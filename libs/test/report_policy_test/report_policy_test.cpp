#include <gtest/gtest.h>

extern "C" {
#include "report_policy.h"
}

namespace {

class ReportPolicyTest : public ::testing::Test {
protected:
    void SetUp() override { ReportPolicy_Init(&policy); }

    // Feed a sample and report it if the policy says so, i.e. the happy path
    // the cycle takes when the window reaches the cloud.
    bool FeedAndReport(int16_t temp_c10)
    {
        int16_t filtered = 0;
        bool due = ReportPolicy_Update(&policy, temp_c10, &filtered);

        if (due)
        {
            ReportPolicy_ReportSent(&policy, filtered);
            last_reported = filtered;
        }

        return due;
    }

    // Feed a sample without committing -- a window that never reached the
    // cloud.
    bool FeedOnly(int16_t temp_c10)
    {
        int16_t filtered = 0;
        return ReportPolicy_Update(&policy, temp_c10, &filtered);
    }

    // Fill the median window at a steady value, with the first sample's report
    // taken as delivered.
    void SettleAt(int16_t temp_c10)
    {
        FeedAndReport(temp_c10);
        FeedAndReport(temp_c10);
        FeedAndReport(temp_c10);
    }

    ReportPolicy policy{};
    int16_t      last_reported = 0;
};

// --- Getting on the map ---------------------------------------------------

TEST_F(ReportPolicyTest, ReportsTheFirstReadingAfterBoot)
{
    // Without this a device switched on in a stable room would never say
    // anything at all, because there is no heartbeat to fall back on.
    int16_t filtered = 0;

    EXPECT_TRUE(ReportPolicy_Update(&policy, 234, &filtered));
    EXPECT_EQ(234, filtered);
}

TEST_F(ReportPolicyTest, KeepsRetryingTheFirstReportUntilOneGetsThrough)
{
    // The first report matters more than any later one: until it lands, the
    // device does not exist in the app.
    EXPECT_TRUE(FeedOnly(234));
    EXPECT_TRUE(FeedOnly(234));
    EXPECT_TRUE(FeedOnly(234));

    EXPECT_TRUE(FeedAndReport(234));

    // ...and stops once it has.
    EXPECT_FALSE(FeedAndReport(234));
}

// --- No heartbeat ---------------------------------------------------------

TEST_F(ReportPolicyTest, SaysNothingWhileTheTemperatureHolds)
{
    SettleAt(234);

    for (int i = 0; i < 100; i++)
    {
        EXPECT_FALSE(FeedAndReport(234)) << "at sample " << i;
    }
}

TEST_F(ReportPolicyTest, IgnoresChangesBelowTheThreshold)
{
    SettleAt(234);

    // REPORT_TEMP_DELTA_C10 is 3, so +2 is not enough, in either direction.
    EXPECT_FALSE(FeedAndReport(236));
    EXPECT_FALSE(FeedAndReport(236));
    EXPECT_FALSE(FeedAndReport(236));

    EXPECT_FALSE(FeedAndReport(232));
    EXPECT_FALSE(FeedAndReport(232));
    EXPECT_FALSE(FeedAndReport(232));
}

TEST_F(ReportPolicyTest, ReportsAtExactlyTheThreshold)
{
    SettleAt(234);

    EXPECT_FALSE(FeedAndReport(237)); // median is still 234
    EXPECT_TRUE(FeedAndReport(237));  // median is now 237, delta 3
    EXPECT_EQ(237, last_reported);
}

TEST_F(ReportPolicyTest, ReportsInBothDirections)
{
    SettleAt(234);

    FeedOnly(200);
    EXPECT_TRUE(FeedAndReport(200));
    EXPECT_EQ(200, last_reported);
}

// --- The spike filter -----------------------------------------------------

TEST_F(ReportPolicyTest, ASingleSpikeBuysNoReport)
{
    SettleAt(234);

    // One sample far outside the threshold, gone by the next. The median of
    // {234, 400, 234} is 234, so nothing leaves the device.
    EXPECT_FALSE(FeedAndReport(400));
    EXPECT_FALSE(FeedAndReport(234));
    EXPECT_FALSE(FeedAndReport(234));
}

TEST_F(ReportPolicyTest, RejectsASpikeOfAnySize)
{
    SettleAt(234);

    // A median rejects an outlier outright rather than averaging it in, so the
    // magnitude does not matter -- which is the reason for choosing one.
    for (int16_t spike : { (int16_t) 1300, (int16_t) -450, (int16_t) 0 })
    {
        EXPECT_FALSE(FeedAndReport(spike)) << "spike " << spike;
        EXPECT_FALSE(FeedAndReport(234));
        EXPECT_FALSE(FeedAndReport(234));
    }
}

TEST_F(ReportPolicyTest, ASustainedChangeIsReportedOneSampleLate)
{
    SettleAt(234);

    // A real step looks exactly like a spike until the sample after it says
    // otherwise. One cycle of lag is the price of the filter.
    EXPECT_FALSE(FeedAndReport(260));
    EXPECT_TRUE(FeedAndReport(260));
    EXPECT_EQ(260, last_reported);
}

TEST_F(ReportPolicyTest, ReportsTheFilteredValueNotTheRawSample)
{
    SettleAt(234);

    FeedOnly(260);
    FeedOnly(999); // one bad sample lands mid-step

    int16_t filtered = 0;
    ASSERT_TRUE(ReportPolicy_Update(&policy, 260, &filtered));

    // Window is {260, 999, 260}. What goes to the cloud is 260, not 999.
    EXPECT_EQ(260, filtered);
}

TEST_F(ReportPolicyTest, AlwaysWritesTheFilteredValueEvenWithNoReportDue)
{
    // The cycle caches this every time, because the module asks for the
    // current readings whenever it reconnects -- including in a pairing window
    // opened with no report pending. So it has to be written on the "nothing
    // to report" path too, and it has to be the filtered value: the window
    // {234, 234, 235} has a median of 234, and 234 is what a pairing window
    // should show.
    SettleAt(234);

    int16_t filtered = -1;
    EXPECT_FALSE(ReportPolicy_Update(&policy, 235, &filtered));
    EXPECT_EQ(234, filtered);

    // ...and it tracks a drifting reading once the window has moved with it.
    ReportPolicy_Update(&policy, 235, &filtered);
    EXPECT_FALSE(ReportPolicy_Update(&policy, 235, &filtered));
    EXPECT_EQ(235, filtered);
}

// --- Failed windows -------------------------------------------------------

TEST_F(ReportPolicyTest, AChangeSurvivesAWindowThatNeverReachedTheCloud)
{
    SettleAt(234);

    FeedOnly(280);
    EXPECT_TRUE(FeedOnly(280)); // due, but the window failed: not committed

    // With no heartbeat, a change dropped here would stay dropped until the
    // temperature happened to move again. It has to still be outstanding.
    EXPECT_TRUE(FeedAndReport(280));
    EXPECT_EQ(280, last_reported);

    EXPECT_FALSE(FeedAndReport(280));
}

TEST_F(ReportPolicyTest, CommittingIsWhatMovesTheComparisonPoint)
{
    SettleAt(234);

    FeedOnly(280);
    FeedOnly(280);

    // Still comparing against 234...
    int16_t filtered = 0;
    EXPECT_TRUE(ReportPolicy_Update(&policy, 280, &filtered));

    ReportPolicy_ReportSent(&policy, filtered);

    // ...and now against 280.
    EXPECT_FALSE(FeedAndReport(281));
}

// --- Robustness -----------------------------------------------------------

TEST_F(ReportPolicyTest, ToleratesNullArguments)
{
    int16_t filtered = 0;

    EXPECT_FALSE(ReportPolicy_Update(nullptr, 234, &filtered));
    EXPECT_FALSE(ReportPolicy_Update(&policy, 234, nullptr));

    ReportPolicy_Init(nullptr);
    ReportPolicy_ReportSent(nullptr, 234);
}

TEST_F(ReportPolicyTest, HoldsOffAfterAResetUntilTheWindowRefills)
{
    // Rebuilding the window is the one case where the policy has reported
    // before but cannot yet tell a step from an outlier.
    SettleAt(234);

    ReportPolicy_Init(&policy);
    FeedAndReport(234); // the post-reset first report

    EXPECT_FALSE(FeedAndReport(400)); // window not full: no outlier may pass
}

} // namespace
