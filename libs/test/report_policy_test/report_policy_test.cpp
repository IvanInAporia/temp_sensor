#include <gtest/gtest.h>

#include <initializer_list>
#include <iterator>
#include <string>

extern "C" {
#include "report_policy.h"
}

namespace {

// Times are minutes on the cycle's schedule, from the first sample after boot,
// one sample every five minutes as on the device today.
constexpr int kCycleMin = 5;

constexpr uint32_t kMinuteMs = 60u * 1000u;

// What became of a sample.
enum Outcome {
    Quiet, // no report due
    Sent,  // due, and the window delivered it: committed
    Lost,  // due, but the window never reached the cloud: not committed
};

// One row of a table of samples, in order.
struct Sample {
    int     minute;   // on the cycle's schedule
    int16_t temp_c10; // what the sensor read
    int16_t median;   // the filtered value the policy writes back
    Outcome outcome;
};

class ReportPolicyTest : public ::testing::Test {
protected:
    void SetUp() override { ReportPolicy_Init(&policy); }

    uint32_t At(int minute) const { return clock_base_ms + static_cast<uint32_t>(minute) * kMinuteMs; }

    void ExpectSamples(std::initializer_list<Sample> rows)
    {
        for (const Sample& row : rows)
        {
            int16_t filtered = 0;
            bool    due      = ReportPolicy_Update(&policy, row.temp_c10, At(row.minute), &filtered);

            EXPECT_EQ(row.median, filtered) << "minute " << row.minute;
            EXPECT_EQ(row.outcome != Quiet, due) << "minute " << row.minute;

            if (due && (row.outcome == Sent))
            {
                ReportPolicy_ReportSent(&policy, filtered, At(row.minute));
            }

            next_minute = row.minute + kCycleMin;
        }
    }

    // Every cycle from where the last table stopped, up to and including
    // `last_minute`, reads `temp_c10` and has nothing to report.
    void ExpectQuietThrough(int16_t temp_c10, int last_minute)
    {
        for (; next_minute <= last_minute; next_minute += kCycleMin)
        {
            int16_t filtered = 0;
            EXPECT_FALSE(ReportPolicy_Update(&policy, temp_c10, At(next_minute), &filtered))
                << "minute " << next_minute;
        }
    }

    // The report after boot at minute 0, and the window full at that
    // temperature by minute 10. The next sample is at minute 15.
    void SettleAt(int16_t temp_c10)
    {
        ExpectSamples({
            { 0, temp_c10, temp_c10, Sent },
            { 5, temp_c10, temp_c10, Quiet },
            { 10, temp_c10, temp_c10, Quiet },
        });
    }

    ReportPolicy policy{};
    uint32_t     clock_base_ms = 0;
    int          next_minute   = 0;
};

// --- Getting on the map ---------------------------------------------------

TEST_F(ReportPolicyTest, ReportsTheFirstReadingAfterBoot)
{
    // Nothing has reached the cloud yet, so there is no hold-off to wait out
    // and nothing to compare against. Without this a device switched on in a
    // steady room would not show up in the app until the 4 h report.
    ExpectSamples({
        // min  temp  median  report
        {   0,  234,   234,   Sent  },
    });
}

TEST_F(ReportPolicyTest, KeepsAskingForTheFirstReportUntilOneGetsThrough)
{
    // Until it lands the device does not exist in the app. How often the
    // cycle actually tries is its backoff's business.
    ExpectSamples({
        // min  temp  median  report
        {   0,  234,   234,   Lost  },
        {   5,  234,   234,   Lost  },
        {  10,  234,   234,   Lost  },
        {  15,  234,   234,   Sent  },
        {  20,  234,   234,   Quiet }, // ...and stops once it has
    });
}

TEST_F(ReportPolicyTest, HoldsOffAfterAResetUntilTheWindowRefills)
{
    // Rebuilding the window is the one case where the policy has reported
    // before but cannot yet tell a step from an outlier.
    SettleAt(234);
    ReportPolicy_Init(&policy);

    ExpectSamples({
        // min  temp  median  report
        {  15,  234,   234,   Sent  }, // the report after the reset
        {  20,  400,   400,   Quiet }, // window not full: no outlier may pass
        {  25,  234,   234,   Quiet }, // window {234, 400, 234}
    });
}

TEST_F(ReportPolicyTest, TheFourHourReportWaitsForTheWindowToo)
{
    // Only reachable when the sensor fails for hours right after the report
    // after boot. 4 h have passed, but an outlier still does not go out.
    ExpectSamples({
        // min  temp  median  report
        {   0,  234,   234,   Sent  }, // the report after boot; then the sensor fails
        { 300,  400,   400,   Quiet }, // back, but the window is not full yet
        { 305,  234,   234,   Sent  }, // window {234, 400, 234}
    });
}

// --- The spike filter -----------------------------------------------------

TEST_F(ReportPolicyTest, ASingleSpikeBuysNoReport)
{
    // One sample 16.6 C off, gone by the next. The filter runs before the
    // rules, so not even the fast path sees it.
    SettleAt(234);

    ExpectSamples({
        // min  temp  median  report
        {  15,  400,   234,   Quiet }, // window {400, 234, 234}
        {  20,  234,   234,   Quiet },
        {  25,  234,   234,   Quiet },
    });
}

TEST_F(ReportPolicyTest, RejectsASpikeOfAnySize)
{
    // A median rejects an outlier outright rather than averaging it in, so the
    // magnitude does not matter -- which is the reason for choosing one.
    SettleAt(234);

    for (int16_t spike : { (int16_t) 1300, (int16_t) -450, (int16_t) 0 })
    {
        int start = next_minute;

        ExpectSamples({
            { start,      spike, 234, Quiet },
            { start + 5,  234,   234, Quiet },
            { start + 10, 234,   234, Quiet },
        });
    }
}

TEST_F(ReportPolicyTest, ASustainedChangeIsReportedOneSampleLate)
{
    // A real step looks exactly like a spike until the sample after it says
    // otherwise. One cycle of lag is the price of the filter.
    SettleAt(234);

    ExpectSamples({
        // min  temp  median  report
        {  15,  260,   234,   Quiet },
        {  20,  260,   260,   Sent  }, // moved 2.6 C: no hold-off
        {  25,  260,   260,   Quiet },
    });
}

TEST_F(ReportPolicyTest, ReportsTheFilteredValueNotTheRawSample)
{
    SettleAt(234);

    ExpectSamples({
        // min  temp  median  report
        {  15,  260,   234,   Quiet },
        {  20,  999,   260,   Sent  }, // window {260, 999, 234}: 999 lands mid-step
    });
}

TEST_F(ReportPolicyTest, AlwaysWritesTheFilteredValueEvenWithNoReportDue)
{
    // The cycle caches this every time, because the module asks for the
    // current readings whenever it reconnects -- including in a pairing window
    // opened with no report pending. So it has to be written on the "nothing
    // to report" path too, held back or below the threshold, and it has to be
    // the filtered value.
    SettleAt(234);

    ExpectSamples({
        // min  temp  median  report
        {  15,  235,   234,   Quiet }, // window {235, 234, 234}
        {  20,  235,   235,   Quiet }, // below the threshold
        {  25,  238,   235,   Quiet },
        {  30,  238,   238,   Sent  }, // over it, and the hold-off is over
        {  35,  242,   238,   Quiet },
        {  40,  242,   242,   Quiet }, // over it, but held back
    });
}

// --- How far, and how long since the last report --------------------------
//
// One row per time since the last report, one column per how far the median
// has moved from it, in 0.1 C. R: a report is due.
//
//   moved 1.0 C or more   at once
//   moved 0.3 C or more   once the 30 min hold-off has passed
//   4 h have passed       whatever the change
//
// Each cell: report 23.4 C, then that many minutes later the median moves by
// that much. A fall reads like a rise; the sweeps after the table check that.

bool DueAfter(int minutes, int moved_c10)
{
    ReportPolicy policy;
    ReportPolicy_Init(&policy);

    int16_t  filtered = 0;
    int16_t  moved    = static_cast<int16_t>(234 + moved_c10);
    uint32_t then_ms  = static_cast<uint32_t>(minutes) * kMinuteMs;

    // Only the time of the report matters, so the window fills at once.
    ReportPolicy_Update(&policy, 234, 0, &filtered);
    ReportPolicy_ReportSent(&policy, 234, 0);
    ReportPolicy_Update(&policy, 234, 0, &filtered);
    ReportPolicy_Update(&policy, 234, 0, &filtered);

    ReportPolicy_Update(&policy, moved, then_ms, &filtered); // the median is still 234
    bool due = ReportPolicy_Update(&policy, moved, then_ms, &filtered);

    EXPECT_EQ(moved, filtered);

    return due;
}

constexpr int kMovedC10[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 46 };

struct DueRow {
    int         minutes;
    const char* due; // one mark per column, spaces ignored
};

const DueRow kDue[] = {
    //  minutes      moved from the last report, in 0.1 C
    //  since it       0  1  2  3  4  5  6  7  8  9 10 11 12 46
    {      0,       "  .  .  .  .  .  .  .  .  .  .  R  R  R  R" }, // a cycle the button brought forward
    {      5,       "  .  .  .  .  .  .  .  .  .  .  R  R  R  R" },
    {     10,       "  .  .  .  .  .  .  .  .  .  .  R  R  R  R" },
    {     25,       "  .  .  .  .  .  .  .  .  .  .  R  R  R  R" }, // the last cycle of the hold-off
    {     30,       "  .  .  .  R  R  R  R  R  R  R  R  R  R  R" },
    {     35,       "  .  .  .  R  R  R  R  R  R  R  R  R  R  R" },
    {     60,       "  .  .  .  R  R  R  R  R  R  R  R  R  R  R" },
    {    235,       "  .  .  .  R  R  R  R  R  R  R  R  R  R  R" }, // the last cycle before the 4 h
    {    240,       "  R  R  R  R  R  R  R  R  R  R  R  R  R  R" },
    {    245,       "  R  R  R  R  R  R  R  R  R  R  R  R  R  R" }, // a 4 h report that never arrived
    {   1440,       "  R  R  R  R  R  R  R  R  R  R  R  R  R  R" }, // a day of outage
};

std::string Marks(const char* row)
{
    std::string marks;

    for (const char* c = row; *c != '\0'; c++)
    {
        if (*c != ' ')
        {
            marks += *c;
        }
    }

    return marks;
}

TEST(ReportPolicyRulesTest, HowFarAndHowLongSinceTheLastReport)
{
    for (const DueRow& row : kDue)
    {
        std::string marks = Marks(row.due);

        ASSERT_EQ(std::size(kMovedC10), marks.size()) << "the row for " << row.minutes << " min";

        for (size_t i = 0; i < marks.size(); i++)
        {
            EXPECT_EQ(marks[i] == 'R', DueAfter(row.minutes, kMovedC10[i]))
                << row.minutes << " min after the last report, moved " << kMovedC10[i];
        }
    }
}

TEST(ReportPolicyRulesTest, AFallIsTreatedLikeARise)
{
    for (int minutes = 0; minutes <= 1440; minutes += kCycleMin)
    {
        for (int moved = 1; moved <= 60; moved++)
        {
            ASSERT_EQ(DueAfter(minutes, moved), DueAfter(minutes, -moved))
                << minutes << " min after the last report, moved " << moved;
        }
    }
}

TEST(ReportPolicyRulesTest, MovingFurtherOrWaitingLongerNeverTakesAReportAway)
{
    for (int minutes = 0; minutes <= 1440; minutes += kCycleMin)
    {
        for (int moved = 0; moved <= 60; moved++)
        {
            if (!DueAfter(minutes, moved))
            {
                continue;
            }

            ASSERT_TRUE(DueAfter(minutes + kCycleMin, moved)) << minutes << " min, moved " << moved;
            ASSERT_TRUE(DueAfter(minutes, moved + 1)) << minutes << " min, moved " << moved;
        }
    }
}

// --- The hold-off ---------------------------------------------------------

TEST_F(ReportPolicyTest, ASmallChangeWaitsOutTheHoldOff)
{
    SettleAt(234); // reported at minute 0

    ExpectSamples({
        // min  temp  median  report
        {  15,  238,   234,   Quiet },
        {  20,  238,   238,   Quiet }, // moved 0.4 C, 20 min after the last report
        {  25,  238,   238,   Quiet },
        {  30,  238,   238,   Sent  }, // the hold-off is over
        {  35,  238,   238,   Quiet },
    });
}

TEST_F(ReportPolicyTest, WhatGoesOutAfterTheHoldOffIsTheCurrentReading)
{
    // Not the one that started the wait, so the app is back within 0.3 C.
    SettleAt(234);

    ExpectSamples({
        // min  temp  median  report
        {  15,  237,   234,   Quiet },
        {  20,  237,   237,   Quiet }, // held
        {  25,  239,   237,   Quiet },
        {  30,  239,   239,   Sent  },
    });
}

TEST_F(ReportPolicyTest, AChangeThatFadesDuringTheHoldOffIsNeverSent)
{
    SettleAt(234);

    ExpectSamples({
        // min  temp  median  report
        {  15,  238,   234,   Quiet },
        {  20,  238,   238,   Quiet }, // held
        {  25,  235,   238,   Quiet },
        {  30,  235,   235,   Quiet }, // the hold-off is over, but 23.5 is within 0.3 C
    });
}

TEST_F(ReportPolicyTest, ABigChangeDoesNotWait)
{
    // A window opened in winter, the heating coming on.
    SettleAt(234);

    ExpectSamples({
        // min  temp  median  report
        {  15,  244,   234,   Quiet },
        {  20,  244,   244,   Sent  }, // moved 1.0 C, 20 min after the last report
        {  25,  233,   244,   Quiet },
        {  30,  233,   233,   Sent  }, // and back down 1.1 C, 10 min after that
    });
}

TEST_F(ReportPolicyTest, AnyReportStartsTheHoldOffAgain)
{
    // A big change goes out at once, but the small one after it waits.
    SettleAt(234);

    ExpectSamples({
        // min  temp  median  report
        {  15,  250,   234,   Quiet },
        {  20,  250,   250,   Sent  }, // fast
        {  25,  254,   250,   Quiet },
        {  30,  254,   254,   Quiet }, // moved 0.4 C from 25.0, 10 min after it
        {  35,  254,   254,   Quiet },
        {  40,  254,   254,   Quiet },
        {  45,  254,   254,   Quiet },
        {  50,  254,   254,   Sent  }, // 30 min after the fast report
    });
}

TEST_F(ReportPolicyTest, AHeatingCycleReportsAtMostOncePerHoldOff)
{
    // The heating swings the room between 23.0 and 23.6 C every 20 min. Every
    // swing crosses 0.3 C, so without the hold-off each would be a report: 144
    // a day. The median follows one sample late.
    ExpectSamples({
        // min  temp  median  report
        {   0,  230,   230,   Sent  }, // after boot
        {   5,  230,   230,   Quiet },
        {  10,  236,   230,   Quiet },
        {  15,  236,   236,   Quiet }, // held
        {  20,  230,   236,   Quiet },
        {  25,  230,   230,   Quiet },
        {  30,  236,   230,   Quiet }, // the hold-off is over, but the median is back at 23.0
        {  35,  236,   236,   Sent  },
        {  40,  230,   236,   Quiet },
        {  45,  230,   230,   Quiet }, // held
        {  50,  236,   230,   Quiet },
        {  55,  236,   236,   Quiet },
        {  60,  230,   236,   Quiet },
        {  65,  230,   230,   Sent  }, // 30 min after the last
    });

    // Over a whole day: one report every 30 or 35 min.
    int reports     = 3; // the three above
    int last_minute = 65;

    for (int minute = 70; minute < 1440; minute += kCycleMin)
    {
        int16_t temp     = ((minute / 10) % 2 == 0) ? 230 : 236;
        int16_t filtered = 0;

        if (ReportPolicy_Update(&policy, temp, At(minute), &filtered))
        {
            ReportPolicy_ReportSent(&policy, filtered, At(minute));

            EXPECT_GE(minute - last_minute, 30) << "minute " << minute;
            EXPECT_LE(minute - last_minute, 35) << "minute " << minute;

            reports++;
            last_minute = minute;
        }
    }

    EXPECT_EQ(48, reports);
}

// --- The 4 h report -------------------------------------------------------

TEST_F(ReportPolicyTest, SaysNothingForFourHoursWhileTheTemperatureHolds)
{
    SettleAt(234);
    ExpectQuietThrough(234, 235);
}

TEST_F(ReportPolicyTest, ASteadyRoomReportsEveryFourHours)
{
    // The app shows the device offline between windows, so the cloud log is
    // the only sign of life -- and the only way the battery state gets out of
    // a room that never changes.
    SettleAt(234); // reported at minute 0
    ExpectQuietThrough(234, 235);

    ExpectSamples({
        // min  temp  median  report
        { 240,  234,   234,   Sent  }, // 4 h on, nothing moved
        { 245,  234,   234,   Quiet },
    });

    ExpectQuietThrough(234, 475);

    ExpectSamples({
        { 480,  234,   234,   Sent  }, // and 4 h after that
    });
}

TEST_F(ReportPolicyTest, ASmallDriftGoesOutWithTheFourHourReport)
{
    // Under 0.3 C never buys a report of its own, so it waits for this one,
    // which sends the current reading.
    SettleAt(234);
    ExpectQuietThrough(236, 235);

    ExpectSamples({
        // min  temp  median  report
        { 240,  236,   236,   Sent  },
        { 245,  234,   236,   Quiet }, // 23.4 is 0.2 C from what was just sent
        { 250,  234,   234,   Quiet },
    });
}

TEST_F(ReportPolicyTest, AnyReportStartsTheFourHoursAgain)
{
    SettleAt(234); // reported at minute 0
    ExpectQuietThrough(234, 95);

    ExpectSamples({
        // min  temp  median  report
        { 100,  238,   234,   Quiet },
        { 105,  238,   238,   Sent  }, // a small change, long after the hold-off
    });

    ExpectQuietThrough(238, 235);

    ExpectSamples({
        { 240,  238,   238,   Quiet }, // 4 h after the report after boot, 2 h 15 min after the last
    });

    ExpectQuietThrough(238, 340);

    ExpectSamples({
        { 345,  238,   238,   Sent  }, // 4 h after the last
    });
}

TEST_F(ReportPolicyTest, AFourHourReportThatNeverArrivedIsStillDue)
{
    SettleAt(234);
    ExpectQuietThrough(234, 235);

    ExpectSamples({
        // min  temp  median  report
        { 240,  234,   234,   Lost  },
        { 245,  234,   234,   Lost  },
        { 250,  234,   234,   Sent  },
        { 255,  234,   234,   Quiet },
    });

    ExpectQuietThrough(234, 485);

    ExpectSamples({
        { 490,  234,   234,   Sent  }, // 4 h after the one that arrived
    });
}

// --- Failed windows -------------------------------------------------------

TEST_F(ReportPolicyTest, AChangeSurvivesAWindowThatNeverReachedTheCloud)
{
    // A change dropped here would wait for the next 4 h report. It has to
    // still be outstanding.
    SettleAt(234);

    ExpectSamples({
        // min  temp  median  report
        {  15,  280,   234,   Quiet },
        {  20,  280,   280,   Lost  },
        {  25,  280,   280,   Sent  },
        {  30,  280,   280,   Quiet },
    });
}

TEST_F(ReportPolicyTest, ASmallChangeHeldAndThenLostIsStillDue)
{
    // A failed window commits nothing, so the hold-off is still counted from
    // the last report that arrived and does not start again.
    SettleAt(234);

    ExpectSamples({
        // min  temp  median  report
        {  15,  238,   234,   Quiet },
        {  20,  238,   238,   Quiet }, // held
        {  25,  238,   238,   Quiet }, // held
        {  30,  238,   238,   Lost  },
        {  35,  238,   238,   Sent  },
    });
}

TEST_F(ReportPolicyTest, CommittingIsWhatMovesTheComparisonPoint)
{
    SettleAt(234);

    ExpectSamples({
        // min  temp  median  report
        {  15,  280,   234,   Quiet },
        {  20,  280,   280,   Lost  }, // still compared against 23.4...
        {  25,  280,   280,   Sent  },
        {  30,  281,   280,   Quiet }, // ...and now against 28.0
        {  35,  281,   281,   Quiet },
    });
}

// --- The clock ------------------------------------------------------------

TEST_F(ReportPolicyTest, TheClockWrappingChangesNothing)
{
    // BSP_GetTimeMs() wraps after some 49.7 days. Here it wraps between
    // minutes 15 and 20.
    clock_base_ms = 0u - 18u * kMinuteMs;

    SettleAt(234); // reported at minute 0

    ExpectSamples({
        // min  temp  median  report
        {  15,  238,   234,   Quiet },
        {  20,  238,   238,   Quiet }, // held across the wrap
        {  25,  238,   238,   Quiet },
        {  30,  238,   238,   Sent  },
    });

    ExpectQuietThrough(238, 265);

    ExpectSamples({
        { 270,  238,   238,   Sent  }, // 4 h after it
    });
}

TEST_F(ReportPolicyTest, AnOutageOfAnyLengthKeepsTheReportDue)
{
    // Nothing is committed while the router is down. Some 49.7 days after the
    // last report that arrived, the time since it would wrap round to a few
    // minutes; it has to stay due anyway.
    SettleAt(234);
    ExpectQuietThrough(234, 235);

    const int kSixtyDaysMin = 60 * 24 * 60;

    for (int minute = 240; minute <= kSixtyDaysMin; minute += kCycleMin)
    {
        int16_t filtered = 0;

        ASSERT_TRUE(ReportPolicy_Update(&policy, 234, At(minute), &filtered)) << "minute " << minute;
    }
}

// --- Robustness -----------------------------------------------------------

TEST_F(ReportPolicyTest, ToleratesNullArguments)
{
    int16_t filtered = 0;

    EXPECT_FALSE(ReportPolicy_Update(nullptr, 234, 0, &filtered));
    EXPECT_FALSE(ReportPolicy_Update(&policy, 234, 0, nullptr));

    ReportPolicy_Init(nullptr);
    ReportPolicy_ReportSent(nullptr, 234, 0);
}

} // namespace
