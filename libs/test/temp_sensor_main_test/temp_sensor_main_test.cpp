#include <gtest/gtest.h>

#include <functional>
#include <initializer_list>
#include <string>

extern "C" {
#include "battery.h"
#include "bsp.h"
#include "bsp_mock.h"
#include "protocol.h"
#include "report_policy.h"
#include "sensor_frame.h"
#include "temp_sensor_main.h"
#include "tuya_sdk_mock.h"
#include "wifi.h"
}

namespace {

// The whole cycle, end to end, with only the two edges faked: the board
// (bsp_mock) and the vendor Tuya SDK (tuya_sdk_mock). The sensor driver, the
// gauge, the report policy and the Tuya glue in between are the real ones.
//
// What is worth asserting here is not "does it compute the right number" --
// the module tests do that -- but the things that only exist once the steps
// are put in a line: when the radio comes up, how long each wait is given,
// what is measured before the load is switched on, and that the module is
// never left powered.

constexpr uint32_t kCyclePeriodMs = 5u * 60u * 1000u;

// The backoff after failed report windows: two retries this far apart...
constexpr uint32_t kRetryMs = 5u * 60u * 1000u;

// ...then this long between tries until one gets through.
constexpr uint32_t kPauseMs        = 2u * 60u * 60u * 1000u;
constexpr int      kCyclesPerPause = static_cast<int>(kPauseMs / kCyclePeriodMs);
constexpr int      kCyclesPerDay   = static_cast<int>(24u * 60u * 60u * 1000u / kCyclePeriodMs);

// A change under 1.0 C waits this long after the last report...
constexpr uint32_t kHoldOffMs        = 30u * 60u * 1000u;
constexpr int      kCyclesPerHoldOff = static_cast<int>(kHoldOffMs / kCyclePeriodMs);

// ...and a steady room reports this often.
constexpr uint32_t kMaxSilenceMs        = 4u * 60u * 60u * 1000u;
constexpr int      kCyclesPerMaxSilence = static_cast<int>(kMaxSilenceMs / kCyclePeriodMs);

// How many wifi_uart_service() calls the scripted module takes to boot and to
// reach the cloud. The glue services once per poll, so these are "polls", and
// they are small so a test runs in microseconds.
constexpr int kBootCalls  = 5;
constexpr int kCloudCalls = 20;

constexpr Battery_State kLow  = Battery_State_Low;
constexpr Battery_State kHigh = Battery_State_High;

// DP 101 or 103 left out of a report.
constexpr int kNotSent = -1;

// What one report window said about the cell. -1 for a DP it left out.
struct BatteryReport {
    int percent        = -1; // DP 4
    int level          = -1; // DP 3
    int damaged        = -1; // DP 102, 0 or 1
    int min_mv         = -1; // DP 101
    int average_min_mv = -1; // DP 103
};

// One row of a table of a cell's reports, in order. The cell rests at rest_mv
// when the cycle reads it, and the report's window pulls it to min_mv at its
// lowest single reading and to avg_mv at the lowest point of its running
// average. The DPs come from the windows before: a window's own sag shows in
// the row below.
struct ReportRow {
    uint16_t      rest_mv;
    uint16_t      min_mv;
    uint16_t      avg_mv;
    int           percent;  // DP 4
    Battery_State level;    // DP 3
    bool          damaged;  // DP 102
    int           dp101_mv; // the lowest of the windows before, or kNotSent
    int           dp103_mv; // the lowest average of the windows before, or kNotSent
};

class TempSensorMainTest : public ::testing::Test {
protected:
    void SetUp() override { PowerOn(); }

    // A fresh board: everything the mocks hold is reset, the retained words
    // with it, and the firmware starts. Also how a table row starts over.
    void PowerOn()
    {
        bspMockReset();
        tuyaSdkMockReset();

        bspMockSetBatteryMv(1300u); // healthy cell, mid-plateau
        StageReading(234, 55u);     // 23.4 C, 55 %

        TempSensorInit();
    }

    // A reset that keeps the power on -- the Reset button, the watchdog, a
    // fault: the firmware starts over, the mocks (the board, the module, the
    // retained words) carry on.
    void ResetKeepingPower(BSP_ResetType reason)
    {
        bspMockSetResetReason(reason);
        TempSensorInit();
    }

    // The DP 104 text of the last string DP sent after `strings_before` were,
    // or "" if none was.
    std::string DiagSentSince(int strings_before) const
    {
        int count = tuyaSdkMockStringCount();
        return (count > strings_before) ? std::string(tuyaSdkMockStringAt(count - 1)) : std::string();
    }

    // What the sensor answers with from now on.
    void StageReading(int16_t temp_c10, uint8_t rh_pct)
    {
        uint8_t frame[6];
        SensorFrameFromReading(temp_c10, rh_pct, frame);
        bspMockSensorSetResponse(frame, sizeof(frame));
    }

    // The module boots and reaches the cloud, as it does on a healthy network.
    void ModuleReachesCloud() { tuyaSdkMockSetWifiStateAfter(kCloudCalls, WIFI_CONN_CLOUD); }

    // The module boots and talks, but never gets past the router.
    void ModuleBootsButNeverConnects()
    {
        tuyaSdkMockSetWifiStateAfter(kBootCalls, WIFI_NOT_CONNECTED);
    }

    // The module is dead: unpowered, unflashed, or the UART is miswired.
    void ModuleNeverAnswers() { tuyaSdkMockSetWifiState(WIFI_SATE_UNKNOW); }

    // The module boots waiting to be paired: it never was, or it was removed
    // in the app. AP_STATE is what the simulator capture shows.
    void ModuleIsUnpaired() { tuyaSdkMockSetWifiStateAfter(kBootCalls, AP_STATE); }

    int WindowsOpened() const { return bspMockWifiPowerOnCalls(); }

    void RunCycles(int n)
    {
        for (int i = 0; i < n; i++)
        {
            TempSensorRunCycle();
        }
    }

    // How many windows the next `n` cycles open.
    int WindowsDuring(int n)
    {
        int before = WindowsOpened();
        RunCycles(n);
        return WindowsOpened() - before;
    }

    // Settle the median window at a temperature, with reports delivered.
    void SettleAt(int16_t temp_c10)
    {
        ModuleReachesCloud();
        StageReading(temp_c10, 55u);
        RunCycles(3);
    }

    // Settle at 23.4 C, take the router down, and let the temperature step to
    // 28.0 C. Returns with the report due on the very next cycle.
    void RouterDownWithAChangeToReport()
    {
        SettleAt(234);
        ModuleBootsButNeverConnects();
        StageReading(280, 55u);
        RunCycles(1); // one cycle of median lag
    }

    // ...and run until the third failed window has started the pause. Returns
    // when that window opened.
    uint32_t PausedAfterThreeFailures()
    {
        RouterDownWithAChangeToReport();
        EXPECT_EQ(3, WindowsDuring(3));
        return bspMockWifiLastPowerOnMs();
    }

    // One more report window: the temperature steps to the other of two
    // values, and the report goes out after one cycle of median lag.
    void ReportAgain()
    {
        warm_ = !warm_;
        StageReading(warm_ ? 280 : 234, 55u);
        EXPECT_EQ(1, WindowsDuring(2));
    }

    // How many times `dpid` has been reported so far.
    int DpReports(uint8_t dpid) const
    {
        int           count = 0;
        TuyaSdkMockDp dp;

        for (int i = 0; tuyaSdkMockDpAt(i, &dp); i++)
        {
            if (dp.dpid == dpid)
            {
                count++;
            }
        }

        return count;
    }

    // The battery DPs reported from the `first` captured DP on.
    BatteryReport BatteryReportSince(int first) const
    {
        BatteryReport report;
        TuyaSdkMockDp dp;

        for (int i = first; tuyaSdkMockDpAt(i, &dp); i++)
        {
            int value = static_cast<int>(dp.value);

            switch (dp.dpid)
            {
            case DPID_BATTERY_PERCENTAGE:  report.percent = value; break;
            case DPID_BATTERY_STATE:       report.level   = value; break;
            case DPID_BATTERY_DAMAGED:     report.damaged = value; break;
            case DPID_BATTERY_MIN_VOLTAGE: report.min_mv  = value; break;
            case DPID_BATTERY_AVG_MIN_VOLTAGE: report.average_min_mv = value; break;
            default: break;
            }
        }

        return report;
    }

    // The cell rests at `rest_mv`, and the next report's window pulls it to
    // `min_mv` at its lowest and to `avg_mv` at the lowest of its running
    // average. Runs until that report has gone out -- the one after boot, the
    // first time -- and returns what it said.
    BatteryReport Report(uint16_t rest_mv, uint16_t min_mv, uint16_t avg_mv)
    {
        bspMockSetBatteryMv(rest_mv);
        bspMockSetSagMinMv(min_mv);
        bspMockSetSagAverageMinMv(avg_mv);

        int first = tuyaSdkMockDpCount();

        if (WindowsOpened() == 0)
        {
            SettleAt(234);
        }
        else
        {
            ReportAgain();
        }

        return BatteryReportSince(first);
    }

    void ExpectReports(std::initializer_list<ReportRow> rows)
    {
        int n = 0;

        for (const ReportRow& row : rows)
        {
            n++;

            BatteryReport report = Report(row.rest_mv, row.min_mv, row.avg_mv);

            EXPECT_EQ(row.percent, report.percent) << "report " << n << ": DP 4";
            EXPECT_EQ(static_cast<int>(row.level), report.level) << "report " << n << ": DP 3";
            EXPECT_EQ(row.damaged ? 1 : 0, report.damaged) << "report " << n << ": DP 102";
            EXPECT_EQ(row.dp101_mv, report.min_mv) << "report " << n << ": DP 101";
            EXPECT_EQ(row.dp103_mv, report.average_min_mv) << "report " << n << ": DP 103";
        }
    }

    bool warm_ = false;
};

// --- The quiet case: no radio ---------------------------------------------

TEST_F(TempSensorMainTest, SaysNothingWhileTheTemperatureHolds)
{
    // This is the whole point of the design: measuring is ~110 uA for 13 ms,
    // reporting is seconds at tens of milliamps. A stable room costs nothing
    // for the 4 h after its last report.
    SettleAt(234); // reported on the first of these three cycles
    int windows_after_settling = WindowsOpened();

    RunCycles(kCyclesPerMaxSilence - 3);

    EXPECT_EQ(windows_after_settling, WindowsOpened());
    EXPECT_EQ(kCyclesPerMaxSilence, bspMockSensorWriteCalls()); // but it kept measuring
}

TEST_F(TempSensorMainTest, ASpikeDoesNotBringTheRadioUp)
{
    SettleAt(234);
    int windows = WindowsOpened();

    StageReading(400, 55u); // one wild sample
    RunCycles(1);
    StageReading(234, 55u);
    RunCycles(2);

    EXPECT_EQ(windows, WindowsOpened());
}

TEST_F(TempSensorMainTest, ASustainedChangeDoesBringTheRadioUp)
{
    SettleAt(234);
    int windows = WindowsOpened();

    StageReading(280, 55u);
    RunCycles(2); // one cycle of median lag, then the report

    EXPECT_EQ(windows + 1, WindowsOpened());
}

// --- How often: the hold-off and the 4 h report -----------------------------
//
// report_policy_test.cpp has the rules sample by sample. What only shows here
// is that the cycle feeds them the right time and acts on them.

TEST_F(TempSensorMainTest, ASmallChangeWaitsHalfAnHourAfterTheLastReport)
{
    ModuleReachesCloud();
    ASSERT_EQ(1, WindowsDuring(3)); // the report after boot, then the window fills
    uint32_t report_ms = bspMockWifiLastPowerOnMs();

    StageReading(238, 55u); // 0.4 C: over the threshold, under the fast path
    EXPECT_EQ(0, WindowsDuring(kCyclesPerHoldOff - 3));
    ASSERT_EQ(1, WindowsDuring(1));
    EXPECT_EQ(report_ms + kHoldOffMs, bspMockWifiLastPowerOnMs());
}

TEST_F(TempSensorMainTest, ALateWakeDoesNotPushTheHoldOffACycleLater)
{
    // As with the backoff: a hold-off timed off the clock would end a few
    // milliseconds after the cycle 30 min on whenever the report had started
    // later than that cycle, and wait out a whole extra period.
    SettleAt(234);
    StageReading(280, 55u);
    RunCycles(1); // one cycle of median lag

    bspMockAdvanceMs(3u);           // this cycle wakes a little late...
    ASSERT_EQ(1, WindowsDuring(1)); // ...and reports 28.0 C

    StageReading(284, 55u); // 0.4 C on from it
    EXPECT_EQ(0, WindowsDuring(kCyclesPerHoldOff - 1));
    EXPECT_EQ(1, WindowsDuring(1)); // 30 min after the report, on time
}

TEST_F(TempSensorMainTest, ASteadyRoomStillReportsEveryFourHours)
{
    // The app shows the device offline between windows, so the cloud log is
    // the only sign of life -- and the only way the battery state gets out of
    // a room that never changes.
    ModuleReachesCloud();
    ASSERT_EQ(1, WindowsDuring(1)); // the report after boot
    uint32_t report_ms = bspMockWifiLastPowerOnMs();

    EXPECT_EQ(0, WindowsDuring(kCyclesPerMaxSilence - 1));
    ASSERT_EQ(1, WindowsDuring(1));
    EXPECT_EQ(report_ms + kMaxSilenceMs, bspMockWifiLastPowerOnMs());

    EXPECT_EQ(6, WindowsDuring(kCyclesPerDay));
}

TEST_F(TempSensorMainTest, TheFourHourReportCarriesTheBatteryState)
{
    // It is how a low cell reaches the app from a room that never changes.
    ModuleReachesCloud();
    ASSERT_EQ(1, WindowsDuring(kCyclesPerMaxSilence)); // the report after boot, and nothing since

    bspMockSetBatteryMv(1160u); // the cell has run down meanwhile
    int first = tuyaSdkMockDpCount();
    ASSERT_EQ(1, WindowsDuring(1)); // 4 h on

    BatteryReport report = BatteryReportSince(first);

    EXPECT_EQ(7, report.percent); // the plain curve at 1160 mV
    EXPECT_EQ(static_cast<int>(kLow), report.level);
}

TEST_F(TempSensorMainTest, AFourHourReportThatFailsBacksOffLikeAnyOther)
{
    SettleAt(234);
    ModuleBootsButNeverConnects();

    EXPECT_EQ(0, WindowsDuring(kCyclesPerMaxSilence - 3));
    EXPECT_EQ(3, WindowsDuring(3)); // 4 h on, then its two retries
    EXPECT_EQ(0, WindowsDuring(kCyclesPerPause - 1));
    EXPECT_EQ(1, WindowsDuring(1));
}

TEST_F(TempSensorMainTest, ADeadSensorGoesSilentRatherThanRepeatItsLastReading)
{
    // The 4 h report needs a reading like any other. So a cloud log with
    // nothing in it for more than about 4 h says something is wrong, instead
    // of showing an old value as if it were new.
    SettleAt(234);
    bspMockSensorSetReadFails(true);

    EXPECT_EQ(0, WindowsDuring(kCyclesPerDay));
}

TEST_F(TempSensorMainTest, WhatAPairingWindowDeliversCountsAsReported)
{
    // It pushed this cycle's reading, so that is what the app now shows, and
    // the 0.3 C and the 4 h count from it.
    SettleAt(234);
    StageReading(238, 55u);
    RunCycles(2); // moved 0.4 C, held back

    ModuleReachesCloud();
    bspMockPressButton();
    ASSERT_EQ(1, WindowsDuring(1)); // pushes 23.8
    bspMockReleaseButton();

    // Not sent again when the hold-off ends, and the next report is 4 h after
    // this window.
    EXPECT_EQ(0, WindowsDuring(kCyclesPerMaxSilence - 1));
    EXPECT_EQ(1, WindowsDuring(1));
}

TEST_F(TempSensorMainTest, APairingWindowWithNoReadingCommitsNothing)
{
    // With the sensor read failed, what the window pushed was an older
    // cycle's reading, and this cycle has nothing to commit.
    SettleAt(234);
    StageReading(238, 55u);
    RunCycles(1); // one cycle of median lag

    bspMockSensorSetReadFails(true);
    ModuleReachesCloud();
    bspMockPressButton();
    ASSERT_EQ(1, WindowsDuring(1));
    bspMockReleaseButton();
    bspMockSensorSetReadFails(false);

    // Still measured from the report after boot: the 0.4 C change waits for
    // its hold-off, 30 min after that one, and no longer.
    EXPECT_EQ(0, WindowsDuring(kCyclesPerHoldOff - 5));
    EXPECT_EQ(1, WindowsDuring(1));
}

// --- What reaches the cloud -----------------------------------------------

TEST_F(TempSensorMainTest, ReportsTheFirstReadingAfterBoot)
{
    ModuleReachesCloud();
    RunCycles(1);

    EXPECT_EQ(1, WindowsOpened());

    uint32_t value = 0;
    ASSERT_TRUE(tuyaSdkMockLastDpValue(DPID_TEMP_CURRENT, &value));
    EXPECT_NEAR(234, static_cast<int32_t>(value), 1);
}

TEST_F(TempSensorMainTest, SendsEveryReadingTogether)
{
    // Humidity and battery cannot trigger a report of their own, so the only
    // way they ever reach the cloud is by riding along on a temperature one.
    ModuleReachesCloud();
    bspMockSetBatteryMv(1300u);
    StageReading(234, 55u);
    RunCycles(1);

    uint32_t temp = 0, humidity = 0, state = 0, percent = 0, damaged = 1;

    EXPECT_TRUE(tuyaSdkMockLastDpValue(DPID_TEMP_CURRENT, &temp));
    EXPECT_TRUE(tuyaSdkMockLastDpValue(DPID_HUMIDITY_VALUE, &humidity));
    EXPECT_TRUE(tuyaSdkMockLastDpValue(DPID_BATTERY_STATE, &state));
    EXPECT_TRUE(tuyaSdkMockLastDpValue(DPID_BATTERY_PERCENTAGE, &percent));
    EXPECT_TRUE(tuyaSdkMockLastDpValue(DPID_BATTERY_DAMAGED, &damaged));

    // The first report after boot: no window has measured the sag yet, so the
    // plain curve, and not damaged.
    EXPECT_EQ(55u, humidity);
    EXPECT_EQ(80u, percent); // the plain curve at 1300 mV
    EXPECT_EQ(static_cast<uint32_t>(kHigh), state);
    EXPECT_EQ(0u, damaged);
}

TEST_F(TempSensorMainTest, SendsSubZeroTemperatureAsASignedValue)
{
    // DP 1 has range -200..600 in 0.1 C. The SDK puts a u32 on the wire as
    // four big-endian bytes, so the sign has to survive the widening
    // or a freezing room reads as +6553 C in the app.
    ModuleReachesCloud();
    StageReading(-105, 55u); // -10.5 C
    RunCycles(1);

    uint32_t value = 0;
    ASSERT_TRUE(tuyaSdkMockLastDpValue(DPID_TEMP_CURRENT, &value));

    EXPECT_NEAR(-105, static_cast<int32_t>(value), 1);
    EXPECT_GT(value, 0xFFFF0000u) << "not two's complement over 32 bits";
}

TEST_F(TempSensorMainTest, ReportsTheFilteredValueNotTheRawSample)
{
    SettleAt(234);

    StageReading(280, 55u);
    RunCycles(1);
    StageReading(999, 55u); // a spike lands during the step
    RunCycles(1);
    StageReading(280, 55u);
    RunCycles(1);

    uint32_t value = 0;
    ASSERT_TRUE(tuyaSdkMockLastDpValue(DPID_TEMP_CURRENT, &value));
    EXPECT_NEAR(280, static_cast<int32_t>(value), 1);
}

// --- Power discipline -----------------------------------------------------

TEST_F(TempSensorMainTest, ReadsTheCellBeforeTheModuleLoadsIt)
{
    // One NiMH cell feeding a 3.3 V boost delivers roughly three times the
    // module's current at the cell. A reading taken during a report measures
    // the sag, not the charge -- and would report a healthy cell as flat.
    ModuleReachesCloud();
    RunCycles(1);

    ASSERT_EQ(1, bspMockWifiPowerOnCalls());
    EXPECT_LT(bspMockBatteryLastReadMs(), bspMockWifiLastPowerOnMs());
}

TEST_F(TempSensorMainTest, LeavesTheModuleUnpoweredAfterASuccessfulWindow)
{
    ModuleReachesCloud();
    RunCycles(5);

    EXPECT_FALSE(bspMockWifiIsPowered());

    // Every window closes, and one extra power-off leads them all: Tuya_Init
    // parks the module at startup, before any window has opened.
    EXPECT_EQ(bspMockWifiPowerOnCalls() + 1, bspMockWifiPowerOffCalls());
}

TEST_F(TempSensorMainTest, LeavesTheModuleUnpoweredWhenItNeverAnswers)
{
    // The one mistake that empties the cell outright. Every path out of the
    // window has to cut the supply, including the ones nothing went right on.
    ModuleNeverAnswers();
    RunCycles(1);

    ASSERT_EQ(1, WindowsOpened());
    EXPECT_FALSE(bspMockWifiIsPowered());
    EXPECT_GE(bspMockWifiPowerOffCalls(), 1);
}

TEST_F(TempSensorMainTest, LeavesTheModuleUnpoweredWhenTheCloudIsUnreachable)
{
    ModuleBootsButNeverConnects();
    RunCycles(1);

    ASSERT_EQ(1, WindowsOpened());
    EXPECT_FALSE(bspMockWifiIsPowered());
}

TEST_F(TempSensorMainTest, GivesUpOnADeadModuleWithinTheAliveTimeout)
{
    // A dead module must not hold the window open for the cloud timeout on
    // top: nothing is going to change in those 25 seconds.
    ModuleNeverAnswers();
    RunCycles(1);

    EXPECT_GE(bspMockWifiLastWindowMs(), 15u * 1000u);
    EXPECT_LT(bspMockWifiLastWindowMs(), 20u * 1000u);
}

TEST_F(TempSensorMainTest, GivesUpOnAnUnreachableCloudWithinItsTimeout)
{
    ModuleBootsButNeverConnects();
    RunCycles(1);

    EXPECT_GE(bspMockWifiLastWindowMs(), 25u * 1000u);
    EXPECT_LT(bspMockWifiLastWindowMs(), 40u * 1000u);
}

TEST_F(TempSensorMainTest, LeavesTheUartIdleBetweenWindows)
{
    // An arm left standing across a power cycle makes the next one fail as
    // busy, and the module then boots into a receiver that is not listening.
    ModuleReachesCloud();
    RunCycles(1);

    EXPECT_FALSE(bspMockWifiRxArmed());
    EXPECT_GE(bspMockWifiAbortReceiveCalls(), 1);
}

TEST_F(TempSensorMainTest, ResetsTheSdkParserOnEveryWindow)
{
    // The module cold-boots every time, so a carried-over parser state or a
    // cached "connected" would have the cycle report into a module that is
    // still booting.
    ModuleReachesCloud();
    SettleAt(234);
    StageReading(280, 55u);
    RunCycles(2);

    EXPECT_EQ(WindowsOpened(), tuyaSdkMockProtocolInitCalls());
}

// --- The cell's minimum under load ----------------------------------------
//
// The module's transmit bursts pull the cell down for a millisecond or less
// at a time, and how far is what says whether the cell -- and its holder --
// can still power a report. The BSP samples it through every window, and the
// next window carries the lowest single reading it saw (DP 101) and the lowest
// point of its running average (DP 103).

TEST_F(TempSensorMainTest, TheSagMonitorCoversTheWholeWindow)
{
    // Already running when the supply comes up, so the module's inrush
    // counts, and stopped only once the supply is down again.
    ModuleReachesCloud();
    RunCycles(1);

    ASSERT_EQ(1, WindowsOpened());
    EXPECT_TRUE(bspMockSagMonitorCoveredLastWindow());
    EXPECT_FALSE(bspMockSagMonitorRunning());
}

TEST_F(TempSensorMainTest, NoWindowLeavesTheSagMonitorRunningIntoTheSleep)
{
    // Its interrupt fires every ~170 us. The BSP stops it before a sleep
    // whatever happens, but every way out of a window should already have.
    ModuleNeverAnswers();
    RunCycles(1); // the report after boot, into a dead module
    ModuleBootsButNeverConnects();
    RunCycles(1); // its retry, into a router that is down
    ModuleReachesCloud();
    RunCycles(1); // the second retry gets through

    ModuleIsUnpaired();
    bspMockPressButton();
    RunCycles(1); // a pairing window nobody finishes
    bspMockReleaseButton();

    ASSERT_EQ(4, WindowsOpened());
    EXPECT_EQ(0, bspMockSleepsWithSagMonitorRunning());
}

TEST_F(TempSensorMainTest, TheFirstWindowAfterBootHasNoMinimumToReport)
{
    // A window carries the windows before it -- its own is still being
    // measured -- and after a reset there are none. Nothing goes out, rather
    // than a 0 mV that would read as a dead cell.
    bspMockSetSagMinMv(960u);
    bspMockSetSagAverageMinMv(1100u);
    ModuleReachesCloud();
    RunCycles(1);

    ASSERT_EQ(1, WindowsOpened());

    uint32_t value = 0;
    EXPECT_FALSE(tuyaSdkMockLastDpValue(DPID_BATTERY_MIN_VOLTAGE, &value));
    EXPECT_FALSE(tuyaSdkMockLastDpValue(DPID_BATTERY_AVG_MIN_VOLTAGE, &value));
}

TEST_F(TempSensorMainTest, EachReportCarriesThePreviousWindowsMinimum)
{
    bspMockSetSagMinMv(960u);
    SettleAt(234); // one window: the report after boot

    bspMockSetSagMinMv(940u);
    StageReading(280, 55u);
    ASSERT_EQ(1, WindowsDuring(2)); // one cycle of median lag, then the report

    uint32_t value = 0;
    ASSERT_TRUE(tuyaSdkMockLastDpValue(DPID_BATTERY_MIN_VOLTAGE, &value));
    EXPECT_EQ(960u, value);

    StageReading(320, 55u);
    ASSERT_EQ(1, WindowsDuring(2));

    ASSERT_TRUE(tuyaSdkMockLastDpValue(DPID_BATTERY_MIN_VOLTAGE, &value));
    EXPECT_EQ(940u, value);
}

TEST_F(TempSensorMainTest, AReportCarriesTheLowestOfTheFailedWindowsBeforeIt)
{
    // A window whose sag browned the module out never gets to say so. The
    // first one after it that does get through has to -- for both minima,
    // each the lowest of its own kind.
    bspMockSetSagMinMv(1000u);
    bspMockSetSagAverageMinMv(1200u);
    SettleAt(234);

    ModuleBootsButNeverConnects();
    StageReading(280, 55u);
    RunCycles(1); // one cycle of median lag

    bspMockSetSagMinMv(850u);
    bspMockSetSagAverageMinMv(1100u);
    ASSERT_EQ(1, WindowsDuring(1)); // fails
    bspMockSetSagMinMv(900u);
    bspMockSetSagAverageMinMv(1050u);
    ASSERT_EQ(1, WindowsDuring(1)); // its retry fails too

    ModuleReachesCloud();
    bspMockSetSagMinMv(990u);
    bspMockSetSagAverageMinMv(1190u);
    ASSERT_EQ(1, WindowsDuring(1)); // the second retry gets through

    uint32_t value = 0;
    ASSERT_TRUE(tuyaSdkMockLastDpValue(DPID_BATTERY_MIN_VOLTAGE, &value));
    EXPECT_EQ(850u, value);
    ASSERT_TRUE(tuyaSdkMockLastDpValue(DPID_BATTERY_AVG_MIN_VOLTAGE, &value));
    EXPECT_EQ(1050u, value);

    // Delivered, so forgotten: the next report starts again from the window
    // that delivered it.
    StageReading(320, 55u);
    ASSERT_EQ(1, WindowsDuring(2));

    ASSERT_TRUE(tuyaSdkMockLastDpValue(DPID_BATTERY_MIN_VOLTAGE, &value));
    EXPECT_EQ(990u, value);
    ASSERT_TRUE(tuyaSdkMockLastDpValue(DPID_BATTERY_AVG_MIN_VOLTAGE, &value));
    EXPECT_EQ(1190u, value);
}

TEST_F(TempSensorMainTest, OnlyAMinimumBelowOneVoltIsReported)
{
    // Above that the boost still has plenty of margin, and the DP would only
    // be noise in the app's history.
    bspMockSetSagMinMv(1000u);
    SettleAt(234);

    bspMockSetSagMinMv(999u);
    StageReading(280, 55u);
    ASSERT_EQ(1, WindowsDuring(2)); // carries 1000 mV

    uint32_t value = 0;
    EXPECT_FALSE(tuyaSdkMockLastDpValue(DPID_BATTERY_MIN_VOLTAGE, &value));

    StageReading(320, 55u);
    ASSERT_EQ(1, WindowsDuring(2)); // carries 999 mV

    ASSERT_TRUE(tuyaSdkMockLastDpValue(DPID_BATTERY_MIN_VOLTAGE, &value));
    EXPECT_EQ(999u, value);
}

TEST_F(TempSensorMainTest, TheAveragedMinimumIsReportedWhateverItsValue)
{
    // DP 103 has no threshold: the gauge's will be set from its history, so
    // every value is worth having -- a healthy cell's most of all.
    bspMockSetSagMinMv(1100u);
    bspMockSetSagAverageMinMv(1250u);
    SettleAt(234);

    StageReading(280, 55u);
    ASSERT_EQ(1, WindowsDuring(2));

    uint32_t value = 0;
    EXPECT_FALSE(tuyaSdkMockLastDpValue(DPID_BATTERY_MIN_VOLTAGE, &value));
    ASSERT_TRUE(tuyaSdkMockLastDpValue(DPID_BATTERY_AVG_MIN_VOLTAGE, &value));
    EXPECT_EQ(1250u, value);
}

// --- The battery gauge under load -------------------------------------------
//
// How far the windows pull the cell down moves the gauge's 0 % point and says
// whether the cell is damaged (DP 102). Each table is one cell's reports in
// order. Like DP 101, one report late: a window's own sag is still being
// measured while it reports, so it shows in the row below. And a window only
// counts once the next one confirms it, so the gauge needs two windows on a
// cell before it moves. battery_test.cpp has the arithmetic behind each number.
//
// The gauge goes by a window's averaged minimum ("avg", DP 103), not its
// lowest single reading ("min", DP 101). Most tables give a window the same
// number for both, so the arithmetic in battery_test.cpp applies as it is;
// ASpikeShowsInDp101ButNotInTheGauge sets them apart.

TEST_F(TempSensorMainTest, TheFailedCellReadsEmptyAndDamagedFromItsThirdReport)
{
    // The failed Eneloop: 1.29 V at rest, pulled to 0.58 V.
    ExpectReports({
        // rest   min   avg  gauge  level   DP 102  DP 101    DP 103
        { 1290,   580,  580,   70,  kHigh,  false,  kNotSent, kNotSent }, // after boot: nothing measured yet
        { 1290,   580,  580,   70,  kHigh,  false,  580,      580      }, // one window, waiting for a second
        { 1290,   580,  580,    0,  kLow,   true,   580,      580      },
        { 1290,   580,  580,    0,  kLow,   true,   580,      580      },
    });
}

TEST_F(TempSensorMainTest, OneDeepWindowDoesNotMoveTheGauge)
{
    // The healthy Eneloop, and one window as deep as the failed cell's.
    ExpectReports({
        // rest   min   avg  gauge  level   DP 102  DP 101    DP 103
        { 1286,   945,  945,   67,  kHigh,  false,  kNotSent, kNotSent }, // after boot: the plain curve
        { 1286,   945,  945,   67,  kHigh,  false,  945,      945      }, // one window: still the plain curve
        { 1286,   580,  580,   63,  kHigh,  false,  945,      945      }, // the deep window
        { 1286,   945,  945,   63,  kHigh,  false,  580,      580      }, // it shows in DP 101 and 103 only
        { 1286,   945,  945,   63,  kHigh,  false,  945,      945      },
        { 1286,   945,  945,   63,  kHigh,  false,  945,      945      },
    });
}

TEST_F(TempSensorMainTest, ASpikeShowsInDp101ButNotInTheGauge)
{
    // The brand-new cell on the scope (2026-11-06): one narrow spike to
    // 0.49 V at the start of the radio's activity, then transmit bursts to
    // ~0.8 V on every other pair, which average out at ~1.01 V
    // (battery_test.cpp, TheSagMonitorsSamples). By its lowest single reading
    // the cell would read 0 % and damaged from the third report; by the
    // average it reads full.
    ExpectReports({
        // rest   min   avg  gauge  level   DP 102  DP 101    DP 103
        { 1336,   491, 1011,  100,  kHigh,  false,  kNotSent, kNotSent },
        { 1336,   491, 1011,  100,  kHigh,  false,  491,      1011     },
        { 1336,   491, 1011,  100,  kHigh,  false,  491,      1011     }, // collapse 1147 mV: healthy
        { 1336,   491, 1011,  100,  kHigh,  false,  491,      1011     },
    });
}

TEST_F(TempSensorMainTest, ACellSwappedWithoutAResetStartsOver)
{
    // The 3.3 V rail's capacitors can carry the sleeping MCU through a quick
    // swap, so nothing in RAM is cleared. The jump in resting voltage is what
    // says the cell changed, and the old cell's sag and DP 101 and 103 minima
    // go with it.
    ExpectReports({
        // rest   min   avg  gauge  level   DP 102  DP 101    DP 103
        { 1290,   580,  580,   70,  kHigh,  false,  kNotSent, kNotSent }, // the failed Eneloop
        { 1290,   580,  580,   70,  kHigh,  false,  580,      580      },
        { 1290,   580,  580,    0,  kLow,   true,   580,      580      },
        { 1400,  1100, 1100,  100,  kHigh,  false,  kNotSent, kNotSent }, // a cell off the charger
        { 1400,  1100, 1100,  100,  kHigh,  false,  kNotSent, 1100     }, // DP 101 only below 1 V; DP 103 always
        { 1400,  1100, 1100,  100,  kHigh,  false,  kNotSent, 1100     },
    });
}

TEST_F(TempSensorMainTest, ARisingCellIsNotTakenForANewOne)
{
    // A cell recovering a little -- 40 mV here -- keeps its trend.
    ExpectReports({
        // rest   min   avg  gauge  level   DP 102  DP 101    DP 103
        { 1290,   580,  580,   70,  kHigh,  false,  kNotSent, kNotSent },
        { 1290,   580,  580,   70,  kHigh,  false,  580,      580      },
        { 1290,   580,  580,    0,  kLow,   true,   580,      580      },
        { 1330,   580,  580,    0,  kLow,   true,   580,      580      },
    });
}

TEST_F(TempSensorMainTest, WindowsThatNeverReachedTheCloudStillCount)
{
    // A window that fails never reports -- here the router is down; on the
    // front board the cell browned the module out. Its sag still counts, so
    // after two such windows the retry that does get through already reads
    // empty and damaged.
    bspMockSetBatteryMv(1290u);
    bspMockSetSagMinMv(580u);
    bspMockSetSagAverageMinMv(580u);
    ModuleBootsButNeverConnects();
    ASSERT_EQ(1, WindowsDuring(1)); // the report after boot, lost
    ASSERT_EQ(1, WindowsDuring(1)); // its retry, five minutes later, lost too

    ModuleReachesCloud();
    int first = tuyaSdkMockDpCount();
    ASSERT_EQ(1, WindowsDuring(1)); // the second retry

    BatteryReport report = BatteryReportSince(first);

    EXPECT_EQ(0, report.percent);
    EXPECT_EQ(static_cast<int>(kLow), report.level);
    EXPECT_EQ(1, report.damaged);
    EXPECT_EQ(580, report.min_mv);
    EXPECT_EQ(580, report.average_min_mv);
}

TEST_F(TempSensorMainTest, AFailedRestingReadReadsFlatButKeepsTheTrend)
{
    // BSP_Battery_ReadMv returns 0 when it could not read its reference. That
    // report reads 0 % -- the direction that cannot mislead -- but it is not
    // taken for a new cell, and its window adds nothing to the trend.
    ExpectReports({
        // rest   min   avg  gauge  level   DP 102  DP 101    DP 103
        { 1286,   945,  945,   67,  kHigh,  false,  kNotSent, kNotSent },
        { 1286,   945,  945,   67,  kHigh,  false,  945,      945      },
        { 1286,   945,  945,   63,  kHigh,  false,  945,      945      },
        {    0,   945,  945,    0,  kLow,   false,  945,      945      }, // the read fails
        { 1286,   945,  945,   63,  kHigh,  false,  945,      945      }, // 63, not 67: the trend held
        { 1286,   945,  945,   63,  kHigh,  false,  945,      945      },
    });
}

TEST_F(TempSensorMainTest, ABadReferenceReadDoesNotMarkTheCellDamaged)
{
    // A resting reading far above any cell, from a bad reference read. It is
    // taken for a new cell -- a false alarm that costs only the trend -- but
    // not as the first window of one: that would read 0 % and damaged for
    // weeks.
    ExpectReports({
        // rest   min   avg  gauge  level   DP 102  DP 101    DP 103
        { 1286,   945,  945,   67,  kHigh,  false,  kNotSent, kNotSent },
        { 1286,   945,  945,   67,  kHigh,  false,  945,      945      },
        { 1286,   945,  945,   63,  kHigh,  false,  945,      945      },
        { 4000,   945,  945,  100,  kHigh,  false,  kNotSent, kNotSent }, // the bad read
        { 1286,   945,  945,   67,  kHigh,  false,  945,      945      }, // starting over: the plain curve
        { 1286,   945,  945,   67,  kHigh,  false,  945,      945      }, // one window on the "new" cell
        { 1286,   945,  945,   63,  kHigh,  false,  945,      945      },
    });
}

// --- Cadence --------------------------------------------------------------

TEST_F(TempSensorMainTest, SleepsOutTheRestOfEachPeriod)
{
    SettleAt(234);

    uint32_t before = bspMockNowMs();
    RunCycles(1);

    // A quiet cycle is a sensor read and a sleep, so the period is what
    // elapses -- within the milliseconds the read itself costs.
    EXPECT_NEAR(kCyclePeriodMs, bspMockNowMs() - before, 100u);
}

TEST_F(TempSensorMainTest, DoesNotLetALongWindowPushTheScheduleLater)
{
    // The schedule accumulates rather than re-basing, so the cycle after an
    // expensive one is still due on the original phase.
    ModuleBootsButNeverConnects(); // ~25 s window
    RunCycles(1);

    uint32_t after_expensive_cycle = bspMockNowMs();

    EXPECT_NEAR(kCyclePeriodMs, after_expensive_cycle, 1000u);
}

// --- The pairing button ---------------------------------------------------

TEST_F(TempSensorMainTest, AButtonPressOpensAPairingWindow)
{
    SettleAt(234);
    int windows = WindowsOpened();

    ModuleReachesCloud();
    bspMockPressButton();
    RunCycles(1);

    EXPECT_EQ(windows + 1, WindowsOpened());
    EXPECT_EQ(1, tuyaSdkMockResetWifiCalls());
}

TEST_F(TempSensorMainTest, AReportWindowDoesNotAskTheModuleToPair)
{
    // The reset that starts pairing also drops the module off its network for
    // up to three minutes. Only the button may send it.
    ModuleReachesCloud();
    RunCycles(1);

    ASSERT_EQ(1, WindowsOpened());
    EXPECT_EQ(0, tuyaSdkMockResetWifiCalls());
}

TEST_F(TempSensorMainTest, PairingWaitsForTheModuleBeforeAskingItToPair)
{
    // mcu_reset_wifi is a plain transmit: one sent into a module that has not
    // finished booting is simply lost, and the user is left holding a button
    // that did nothing.
    ModuleNeverAnswers();
    bspMockPressButton();
    RunCycles(1);

    EXPECT_EQ(1, WindowsOpened());
    EXPECT_EQ(0, tuyaSdkMockResetWifiCalls());
}

TEST_F(TempSensorMainTest, AButtonGlitchIsNotAPress)
{
    // The pin is held up by the MCU's internal pull-up alone, so it is a long
    // antenna. An edge with no level behind it must not cost a window.
    SettleAt(234);
    int windows = WindowsOpened();

    bspMockGlitchButton();
    RunCycles(1);

    EXPECT_EQ(windows, WindowsOpened());
}

TEST_F(TempSensorMainTest, APairingWindowAlsoPushesTheCurrentReadings)
{
    // What makes the button confirm to whoever pressed it that the device
    // works, rather than just joining a network in silence.
    SettleAt(234);

    ModuleReachesCloud();
    StageReading(235, 60u); // below the report threshold
    bspMockPressButton();
    RunCycles(1);

    uint32_t humidity = 0;
    ASSERT_TRUE(tuyaSdkMockLastDpValue(DPID_HUMIDITY_VALUE, &humidity));
    EXPECT_EQ(60u, humidity);
}

TEST_F(TempSensorMainTest, APairingWindowWaitsFarLongerThanAReportWindow)
{
    // Provisioning takes as long as somebody standing at the device takes, so
    // the 25 s a report gets would fail almost every pairing attempt. It still
    // stays under the cycle period -- see WIFI_PAIRING_TIMEOUT_MS.
    ModuleBootsButNeverConnects();
    RunCycles(1);
    uint32_t report_window_ms = bspMockWifiLastWindowMs();

    bspMockPressButton();
    RunCycles(1);
    uint32_t pairing_window_ms = bspMockWifiLastWindowMs();

    EXPECT_GT(pairing_window_ms, 4u * report_window_ms);
    EXPECT_LT(pairing_window_ms, kCyclePeriodMs);
}

TEST_F(TempSensorMainTest, AButtonPressDoesNotDisturbTheMeasurementSchedule)
{
    SettleAt(234);
    uint32_t before = bspMockNowMs();

    bspMockSetEarlyWake(1000u, 1); // the press ends the sleep after 1 s
    bspMockPressButton();
    ModuleReachesCloud();
    RunCycles(1);
    bspMockReleaseButton();

    // The pairing cycle ran early, and then the loop went back to waiting out
    // the period that was already running -- so BOTH cycles fall inside one
    // period and the series keeps its phase. A press that re-based the
    // schedule would show up here as two periods.
    RunCycles(1);

    EXPECT_NEAR(kCyclePeriodMs, bspMockNowMs() - before, 2000u);
}

TEST_F(TempSensorMainTest, AHeldButtonDoesNotCostAnExtraCycleEveryPeriod)
{
    // A stuck switch holds the level down forever but produces exactly one
    // falling edge, so it must not keep looking like a fresh early wake. The
    // sleep runs its full length; only a sleep that ended EARLY is a press
    // worth returning on.
    SettleAt(234);
    bspMockPressButton();
    ModuleReachesCloud();
    RunCycles(1); // the one real pairing cycle

    int sleeps_before = bspMockSleepCalls();
    uint32_t before   = bspMockNowMs();

    RunCycles(4); // button still held down throughout

    EXPECT_EQ(sleeps_before + 4, bspMockSleepCalls());
    EXPECT_NEAR(4u * kCyclePeriodMs, bspMockNowMs() - before, 2000u);
}

// --- Failures -------------------------------------------------------------

TEST_F(TempSensorMainTest, ASensorFailureIsACycleWithNothingToSay)
{
    SettleAt(234);
    int windows = WindowsOpened();

    int sleeps_before = bspMockSleepCalls();

    bspMockSensorSetReadFails(true);
    RunCycles(3);

    EXPECT_EQ(windows, WindowsOpened());

    // Still on schedule: a cycle with nothing to say is a cycle, not a stall.
    EXPECT_EQ(sleeps_before + 3, bspMockSleepCalls());
}

TEST_F(TempSensorMainTest, ASensorThatComesBackResumesReporting)
{
    SettleAt(234);

    bspMockSensorSetReadFails(true);
    RunCycles(2);

    bspMockSensorSetReadFails(false);
    StageReading(300, 55u);
    ModuleReachesCloud();
    RunCycles(2);

    uint32_t value = 0;
    ASSERT_TRUE(tuyaSdkMockLastDpValue(DPID_TEMP_CURRENT, &value));
    EXPECT_NEAR(300, static_cast<int32_t>(value), 1);
}

TEST_F(TempSensorMainTest, ADroppedReportIsRetriedOnTheNextCycle)
{
    // A change that failed to reach the cloud has to stay outstanding.
    // Otherwise it would wait for the next 4 h report, far too late for a
    // change worth sending.
    SettleAt(234);

    ModuleBootsButNeverConnects();
    StageReading(280, 55u);
    RunCycles(2); // the report is due on the second, and the window fails

    int windows_after_failure = WindowsOpened();

    ModuleReachesCloud();
    RunCycles(1);

    EXPECT_EQ(windows_after_failure + 1, WindowsOpened());

    uint32_t value = 0;
    ASSERT_TRUE(tuyaSdkMockLastDpValue(DPID_TEMP_CURRENT, &value));
    EXPECT_NEAR(280, static_cast<int32_t>(value), 1);
}

TEST_F(TempSensorMainTest, StopsRetryingOnceTheReportLands)
{
    SettleAt(234);

    ModuleBootsButNeverConnects();
    StageReading(280, 55u);
    RunCycles(2);

    ModuleReachesCloud();
    RunCycles(1);

    int windows = WindowsOpened();

    RunCycles(10);

    EXPECT_EQ(windows, WindowsOpened());
}

// --- Backing off after failed windows -------------------------------------
//
// A failed window keeps the module powered for about 29 s, and with the
// router down every window fails. Retried on every cycle, that is ~10 % of
// the time at tens of milliamps -- a flat cell in three or four days.

TEST_F(TempSensorMainTest, RetriesTwiceFiveMinutesApartThenPausesForTwoHours)
{
    RouterDownWithAChangeToReport();

    ASSERT_EQ(1, WindowsDuring(1));
    uint32_t first_ms = bspMockWifiLastPowerOnMs();

    ASSERT_EQ(1, WindowsDuring(1));
    EXPECT_EQ(first_ms + kRetryMs, bspMockWifiLastPowerOnMs());

    ASSERT_EQ(1, WindowsDuring(1));
    uint32_t third_ms = bspMockWifiLastPowerOnMs();
    EXPECT_EQ(first_ms + 2u * kRetryMs, third_ms);

    // Three in a row says the router is down and likely to stay down. The
    // radio stays off through the pause, but the measuring carries on.
    int reads_before = bspMockSensorWriteCalls();

    EXPECT_EQ(0, WindowsDuring(kCyclesPerPause - 1));
    EXPECT_EQ(reads_before + kCyclesPerPause - 1, bspMockSensorWriteCalls());

    ASSERT_EQ(1, WindowsDuring(1));
    EXPECT_EQ(third_ms + kPauseMs, bspMockWifiLastPowerOnMs());
}

TEST_F(TempSensorMainTest, AFailureAfterThePauseStartsAnotherPause)
{
    // The count stops at three and only a success clears it, so the try at
    // the end of a pause gets no retries of its own.
    PausedAfterThreeFailures();
    RunCycles(kCyclesPerPause - 1);

    ASSERT_EQ(1, WindowsDuring(1));
    uint32_t try_ms = bspMockWifiLastPowerOnMs();

    EXPECT_EQ(0, WindowsDuring(kCyclesPerPause - 1));
    ASSERT_EQ(1, WindowsDuring(1));
    EXPECT_EQ(try_ms + kPauseMs, bspMockWifiLastPowerOnMs());
}

TEST_F(TempSensorMainTest, ALongOutageCostsAWindowEveryTwoHours)
{
    PausedAfterThreeFailures();

    EXPECT_EQ(12, WindowsDuring(kCyclesPerDay));
}

TEST_F(TempSensorMainTest, AReportThatGetsThroughGivesTheRetriesBack)
{
    RouterDownWithAChangeToReport();
    ASSERT_EQ(2, WindowsDuring(2)); // two failures use up both retries...

    ModuleReachesCloud();
    ASSERT_EQ(1, WindowsDuring(1)); // ...and the third try gets through

    // The router goes down again. Without its retries back, the next change
    // would get one window and then a pause; with them, it gets three.
    ModuleBootsButNeverConnects();
    StageReading(330, 55u);
    RunCycles(1); // one cycle of median lag

    EXPECT_EQ(3, WindowsDuring(3));
    EXPECT_EQ(0, WindowsDuring(1));
}

TEST_F(TempSensorMainTest, TheRetriesAfterBootBackOffTheSameWay)
{
    // Until the first report lands there is nothing committed, so the policy
    // asks for one on every cycle. A device switched on with the router down
    // must not take that as licence to try every five minutes.
    ModuleBootsButNeverConnects();

    ASSERT_EQ(3, WindowsDuring(3));
    uint32_t third_ms = bspMockWifiLastPowerOnMs();

    EXPECT_EQ(0, WindowsDuring(kCyclesPerPause - 1));
    ASSERT_EQ(1, WindowsDuring(1));
    EXPECT_EQ(third_ms + kPauseMs, bspMockWifiLastPowerOnMs());
}

TEST_F(TempSensorMainTest, TheButtonStillOpensAWindowDuringThePause)
{
    uint32_t third_ms = PausedAfterThreeFailures();
    RunCycles(2);

    bspMockPressButton();
    ASSERT_EQ(1, WindowsDuring(1));
    bspMockReleaseButton();

    // A pairing window, not a "retry now" -- and one that nobody paired says
    // nothing about the router, so the pause neither ends early nor starts
    // over. The next report window opens when it would have anyway.
    EXPECT_EQ(1, tuyaSdkMockResetWifiCalls());

    EXPECT_EQ(0, WindowsDuring(kCyclesPerPause - 4));
    ASSERT_EQ(1, WindowsDuring(1));
    EXPECT_EQ(third_ms + kPauseMs, bspMockWifiLastPowerOnMs());
}

TEST_F(TempSensorMainTest, APairingWindowThatGetsThroughEndsThePause)
{
    PausedAfterThreeFailures();

    // Readings taken during the pause are cached as usual, so the pairing
    // window pushes those and not the ones the pause started with.
    StageReading(300, 60u);
    RunCycles(2);

    ModuleReachesCloud();
    bspMockPressButton();
    ASSERT_EQ(1, WindowsDuring(1));
    bspMockReleaseButton();

    uint32_t value = 0;
    ASSERT_TRUE(tuyaSdkMockLastDpValue(DPID_TEMP_CURRENT, &value));
    EXPECT_NEAR(300, static_cast<int32_t>(value), 1);

    // It got through, so the network works: the next change goes out at once,
    // not when the pause would have run out.
    StageReading(350, 60u);
    EXPECT_EQ(1, WindowsDuring(2)); // one cycle of median lag, then the report
}

TEST_F(TempSensorMainTest, ALateWakeDoesNotPushTheRetryACycleLater)
{
    // On the device a cycle starts a few milliseconds after it was due, by a
    // different few each time. A retry timed off the clock would miss the
    // cycle five minutes on whenever the failed one had started later than
    // that one, and wait out a whole extra period.
    RouterDownWithAChangeToReport();

    bspMockAdvanceMs(3u);           // this cycle wakes a little late...
    ASSERT_EQ(1, WindowsDuring(1)); // ...and its window fails

    EXPECT_EQ(1, WindowsDuring(1)); // the next, on time, still retries
}

// --- An unpaired module ---------------------------------------------------
//
// A module that was never paired, or was removed in the app, has no network to
// try. A report window would wait out the whole cloud timeout for nothing, and
// until the first report after boot lands, one is due on every cycle.
//
// So a window that finds it unpaired ends at once and the next waits out the
// pause -- but the next does come. Until 2026-11 it never did: one unpaired
// answer silenced the device until someone pressed Reset.

TEST_F(TempSensorMainTest, AReportWindowEndsAsSoonAsTheModuleSaysItIsUnpaired)
{
    ModuleIsUnpaired();
    ASSERT_EQ(1, WindowsDuring(1));

    // Its first state report says so, some 4 s in on the device. Nothing in
    // the cloud timeout after that could change it.
    EXPECT_LT(bspMockWifiLastWindowMs(), 1000u);
    EXPECT_FALSE(bspMockWifiIsPowered());
}

TEST_F(TempSensorMainTest, AnUnpairedDeviceAsksAgainEveryTwoHours)
{
    // Not on every cycle, as the policy keeps asking until the first report
    // after boot lands, and without the two quick retries a down router gets:
    // it will not have been paired five minutes later.
    ModuleIsUnpaired();
    ASSERT_EQ(1, WindowsDuring(1));
    uint32_t first_ms = bspMockWifiLastPowerOnMs();

    int reads_before = bspMockSensorWriteCalls();

    EXPECT_EQ(0, WindowsDuring(kCyclesPerPause - 1));
    ASSERT_EQ(1, WindowsDuring(1));
    EXPECT_EQ(first_ms + kPauseMs, bspMockWifiLastPowerOnMs());
    EXPECT_LT(bspMockWifiLastWindowMs(), 1000u); // still short

    // About 12 short windows a day, and the measuring carries on throughout.
    EXPECT_EQ(12, WindowsDuring(kCyclesPerDay));
    EXPECT_EQ(reads_before + kCyclesPerPause + kCyclesPerDay, bspMockSensorWriteCalls());
}

TEST_F(TempSensorMainTest, ADeviceRemovedInTheAppChecksBackEveryTwoHours)
{
    SettleAt(234);

    // The module comes back from its next boot waiting to be paired. The next
    // change finds that out, in one short window...
    ModuleIsUnpaired();
    StageReading(280, 55u);
    ASSERT_EQ(1, WindowsDuring(2)); // one cycle of median lag, then the report
    EXPECT_LT(bspMockWifiLastWindowMs(), 1000u);
    uint32_t found_ms = bspMockWifiLastPowerOnMs();

    // ...and no change opens another before the pause is out.
    StageReading(320, 55u);
    EXPECT_EQ(0, WindowsDuring(kCyclesPerPause - 1));
    ASSERT_EQ(1, WindowsDuring(1));
    EXPECT_EQ(found_ms + kPauseMs, bspMockWifiLastPowerOnMs());
}

TEST_F(TempSensorMainTest, AModuleThatComesBackOnItsNetworkIsHeardFromAtTheNextCheck)
{
    // The case that used to need the Reset button: the module answers one boot
    // as if it were unpaired, and the next as if nothing had happened.
    SettleAt(234);

    ModuleIsUnpaired();
    StageReading(280, 55u);
    ASSERT_EQ(1, WindowsDuring(2));

    ModuleReachesCloud();

    // The change is still outstanding, and goes out at the next check...
    EXPECT_EQ(0, WindowsDuring(kCyclesPerPause - 1));
    ASSERT_EQ(1, WindowsDuring(1));

    uint32_t value = 0;
    ASSERT_TRUE(tuyaSdkMockLastDpValue(DPID_TEMP_CURRENT, &value));
    EXPECT_NEAR(280, static_cast<int32_t>(value), 1);

    // ...and from then on a change goes out as usual.
    StageReading(330, 55u);
    EXPECT_EQ(1, WindowsDuring(2));
}

TEST_F(TempSensorMainTest, AnyOfThePairingModesCountsAsUnpaired)
{
    // Which one the module reports depends on how it was set to pair:
    // SmartConfig, AP, or both at once.
    const uint8_t states[] = {SMART_CONFIG_STATE, AP_STATE, SMART_AND_AP_STATE};

    for (uint8_t state : states)
    {
        TempSensorInit();
        tuyaSdkMockSetWifiStateAfter(kBootCalls, state);

        EXPECT_EQ(1, WindowsDuring(3)) << "state " << static_cast<int>(state);
    }
}

TEST_F(TempSensorMainTest, APairingWindowIsNotCutShortByAnUnpairedModule)
{
    // Waiting to be paired is what a pairing window is for.
    ModuleIsUnpaired();
    bspMockPressButton();
    ASSERT_EQ(1, WindowsDuring(1));
    bspMockReleaseButton();

    EXPECT_EQ(1, tuyaSdkMockResetWifiCalls());
    EXPECT_GE(bspMockWifiLastWindowMs(), 3u * 60u * 1000u); // the whole pairing timeout
}

TEST_F(TempSensorMainTest, APairingWindowNobodyFinishesChangesNothing)
{
    // The pause that the unpaired answer started runs on as it would have: the
    // pairing window neither ends it nor starts it over.
    ModuleIsUnpaired();
    ASSERT_EQ(1, WindowsDuring(1));
    uint32_t found_ms = bspMockWifiLastPowerOnMs();
    RunCycles(1);

    bspMockPressButton();
    ASSERT_EQ(1, WindowsDuring(1));
    bspMockReleaseButton();

    // Three cycles are behind us: the finding, a quiet one and the button's.
    EXPECT_EQ(0, WindowsDuring(kCyclesPerPause - 3));
    ASSERT_EQ(1, WindowsDuring(1));
    EXPECT_EQ(found_ms + kPauseMs, bspMockWifiLastPowerOnMs());
}

TEST_F(TempSensorMainTest, PairingTheDeviceLetsItReportAgain)
{
    ModuleIsUnpaired();
    RunCycles(1);

    // Readings are cached throughout, so the pairing window pushes the current
    // ones...
    StageReading(300, 60u);
    RunCycles(2);

    ModuleReachesCloud();
    bspMockPressButton();
    ASSERT_EQ(1, WindowsDuring(1));
    bspMockReleaseButton();

    uint32_t value = 0;
    ASSERT_TRUE(tuyaSdkMockLastDpValue(DPID_TEMP_CURRENT, &value));
    EXPECT_NEAR(300, static_cast<int32_t>(value), 1);

    // ...and from then on a change is reported as usual.
    StageReading(350, 60u);
    EXPECT_EQ(1, WindowsDuring(2)); // one cycle of median lag, then the report
}

TEST_F(TempSensorMainTest, AResetFindsOutAgainWithOneShortWindow)
{
    // Kept in RAM, so a battery swap forgets it. Finding out again costs one
    // window of a few seconds, which is why it is not worth the EEPROM.
    ModuleIsUnpaired();
    ASSERT_EQ(1, WindowsDuring(1));
    ASSERT_EQ(0, WindowsDuring(3));

    TempSensorInit(); // the MCU resets; the module is still unpaired

    ASSERT_EQ(1, WindowsDuring(1));
    EXPECT_LT(bspMockWifiLastWindowMs(), 1000u);
    EXPECT_EQ(0, WindowsDuring(3));
}

// --- Confirmed reports -----------------------------------------------------
//
// Reaching the cloud did not always mean the report did: the kitchen board's
// window at 08:34 on 11-10 went online and its DPs never showed in the log. So
// the temperature now goes out last as a synchronous report, and the module's
// answer decides whether the report was delivered. A module that never answers
// one gets the old rule: reaching the cloud is delivering.

constexpr uint32_t kAckTimeoutMs = 6u * 1000u;
constexpr uint32_t kDrainMs      = 2u * 1000u;

TEST_F(TempSensorMainTest, TheTemperatureGoesLastAndAsksForAnAnswer)
{
    ModuleReachesCloud();
    tuyaSdkMockSetSyncAnswer(TUYA_MOCK_SYNC_TAKEN);
    ASSERT_EQ(1, WindowsDuring(1));

    TuyaSdkMockDp dp;
    int           last = tuyaSdkMockDpCount() - 1;

    ASSERT_TRUE(tuyaSdkMockDpAt(last, &dp));
    EXPECT_EQ(DPID_TEMP_CURRENT, dp.dpid);
    EXPECT_TRUE(dp.sync);

    // One, and only one: the SDK allows no second before the first is answered.
    for (int i = 0; i < last; i++)
    {
        ASSERT_TRUE(tuyaSdkMockDpAt(i, &dp));
        EXPECT_FALSE(dp.sync) << "DP " << int(dp.dpid);
    }
}

TEST_F(TempSensorMainTest, TheAnswerDecidesWhetherTheReportWasDelivered)
{
    // The report after boot, on a fresh run, for each way the module can
    // answer. Delivered or not shows in the next cycle: a report that did not
    // get through is retried five minutes later.
    struct Row {
        const char*           module;
        TuyaSdkMockSyncAnswer answer;
        bool                  delivered;
        uint32_t              window_min_ms; // how long the module was powered
        uint32_t              window_max_ms;
    };

    const Row rows[] = {
        {"takes it: off at once, no drain",         TUYA_MOCK_SYNC_TAKEN,   true,  0u,            1000u},
        {"refuses it: off at once, retried",        TUYA_MOCK_SYNC_REFUSED, false, 0u,            1000u},
        {"never answers: the old rule, after 6 s",  TUYA_MOCK_SYNC_SILENT,  true,  kAckTimeoutMs, kAckTimeoutMs + 1000u},
    };

    for (const Row& row : rows)
    {
        PowerOn();
        ModuleReachesCloud();
        tuyaSdkMockSetSyncAnswer(row.answer);

        ASSERT_EQ(1, WindowsDuring(1)) << row.module;
        EXPECT_GE(bspMockWifiLastWindowMs(), row.window_min_ms) << row.module;
        EXPECT_LE(bspMockWifiLastWindowMs(), row.window_max_ms) << row.module;

        EXPECT_EQ(row.delivered ? 0 : 1, WindowsDuring(1)) << row.module << ": a retry?";
    }
}

TEST_F(TempSensorMainTest, WhatTheFirstAnswerSaysAboutTheRest)
{
    // Two reports in one run. Once the module has answered, a missing answer
    // is a failure; once it has failed to answer even once, the run stops
    // asking and goes back to the 2 s drain.
    struct Row {
        const char*           module;
        TuyaSdkMockSyncAnswer first;
        TuyaSdkMockSyncAnswer second;
        bool                  second_asks; // the second report's temperature is synchronous
        bool                  second_delivered;
        uint32_t              second_min_ms;
        uint32_t              second_max_ms;
    };

    const Row rows[] = {
        {"answers both times",         TUYA_MOCK_SYNC_TAKEN,  TUYA_MOCK_SYNC_TAKEN,  true,  true,  0u,            1000u},
        {"answered, then went quiet",  TUYA_MOCK_SYNC_TAKEN,  TUYA_MOCK_SYNC_SILENT, true,  false, kAckTimeoutMs, kAckTimeoutMs + 1000u},
        {"never answers",              TUYA_MOCK_SYNC_SILENT, TUYA_MOCK_SYNC_SILENT, false, true,  kDrainMs,      kDrainMs + 1000u},
        {"quiet, then would answer",   TUYA_MOCK_SYNC_SILENT, TUYA_MOCK_SYNC_TAKEN,  false, true,  kDrainMs,      kDrainMs + 1000u},
    };

    for (const Row& row : rows)
    {
        PowerOn();
        tuyaSdkMockSetSyncAnswer(row.first);
        SettleAt(234); // the report after boot, then two quiet cycles

        tuyaSdkMockSetSyncAnswer(row.second);
        int dps_before = tuyaSdkMockDpCount();
        StageReading(280, 55u);
        ASSERT_EQ(1, WindowsDuring(2)) << row.module; // one cycle of median lag

        bool          asked = false;
        TuyaSdkMockDp dp;

        for (int i = dps_before; tuyaSdkMockDpAt(i, &dp); i++)
        {
            asked = asked || dp.sync;
        }

        EXPECT_EQ(row.second_asks, asked) << row.module;
        EXPECT_GE(bspMockWifiLastWindowMs(), row.second_min_ms) << row.module;
        EXPECT_LE(bspMockWifiLastWindowMs(), row.second_max_ms) << row.module;
        EXPECT_EQ(row.second_delivered ? 0 : 1, WindowsDuring(1)) << row.module << ": a retry?";
    }
}

TEST_F(TempSensorMainTest, ARefusedReportIsSentAgainUntilItIsTaken)
{
    tuyaSdkMockSetSyncAnswer(TUYA_MOCK_SYNC_TAKEN); // so the run knows it answers
    SettleAt(234);

    tuyaSdkMockSetSyncAnswer(TUYA_MOCK_SYNC_REFUSED);
    StageReading(280, 55u);
    ASSERT_EQ(1, WindowsDuring(2));

    tuyaSdkMockSetSyncAnswer(TUYA_MOCK_SYNC_TAKEN);
    int dps_before = tuyaSdkMockDpCount();
    ASSERT_EQ(1, WindowsDuring(1)); // the retry, five minutes on

    uint32_t value = 0;
    ASSERT_TRUE(tuyaSdkMockLastDpValue(DPID_TEMP_CURRENT, &value));
    EXPECT_NEAR(280, static_cast<int32_t>(value), 1);
    EXPECT_GT(tuyaSdkMockDpCount(), dps_before);

    EXPECT_EQ(0, WindowsDuring(3)); // and that one landed
}

TEST_F(TempSensorMainTest, APairingWindowGivesTheAppItsTenSecondsWhateverTheAnswer)
{
    const TuyaSdkMockSyncAnswer answers[] = {TUYA_MOCK_SYNC_TAKEN, TUYA_MOCK_SYNC_REFUSED, TUYA_MOCK_SYNC_SILENT};

    for (TuyaSdkMockSyncAnswer answer : answers)
    {
        PowerOn();
        ModuleReachesCloud();
        tuyaSdkMockSetSyncAnswer(answer);

        bspMockPressButton();
        ASSERT_EQ(1, WindowsDuring(1));
        bspMockReleaseButton();

        EXPECT_GE(bspMockWifiLastWindowMs(), 10u * 1000u) << "answer " << int(answer);
    }
}

TEST_F(TempSensorMainTest, AResetAsksAgain)
{
    // Whether the module answers is kept in RAM: a module swapped for one with
    // newer firmware gets asked.
    tuyaSdkMockSetSyncAnswer(TUYA_MOCK_SYNC_SILENT);
    SettleAt(234);

    ResetKeepingPower(BSP_Reset_Pin);
    tuyaSdkMockSetSyncAnswer(TUYA_MOCK_SYNC_TAKEN);
    ASSERT_EQ(1, WindowsDuring(1));

    TuyaSdkMockDp dp;
    ASSERT_TRUE(tuyaSdkMockDpAt(tuyaSdkMockDpCount() - 1, &dp));
    EXPECT_TRUE(dp.sync);
}

// --- The sensor bus --------------------------------------------------------
//
// A glitch can leave the I2C controller believing the bus is busy, and a
// sensor interrupted mid-byte holds SDA low. Either fails every transfer after
// it, and a device that cannot read reports nothing -- not even the 4 h report.

TEST_F(TempSensorMainTest, AFailedReadPutsTheBusBackToIdleAndAGoodOneLeavesItAlone)
{
    struct Row {
        bool read_ok;
        int  recoveries; // so far
    };

    const Row rows[] = {
        {true,  0},
        {false, 1},
        {false, 2},
        {true,  2},
        {true,  2},
        {false, 3},
    };

    int cycle = 0;

    for (const Row& row : rows)
    {
        cycle++;
        bspMockSensorSetReadFails(!row.read_ok);
        RunCycles(1);
        EXPECT_EQ(row.recoveries, bspMockSensorRecoverCalls()) << "cycle " << cycle;
    }
}

TEST_F(TempSensorMainTest, ALockedUpBusCostsOneReadingNotAllOfThem)
{
    SettleAt(234);

    bspMockSensorLockBus(); // fails every transfer until recovered
    StageReading(280, 55u);
    RunCycles(1);
    EXPECT_EQ(1, bspMockSensorRecoverCalls());

    // The next reads are fine: one cycle of median lag, then the report.
    EXPECT_EQ(1, WindowsDuring(2));
    EXPECT_EQ(1, bspMockSensorRecoverCalls());
}

// --- The watchdog ----------------------------------------------------------
//
// It resets a board that stops making progress, after 20-40 s on the STM32.
// So nothing the cycle does awake may go that long without feeding it --
// including a three-minute pairing window, which is far longer.

TEST_F(TempSensorMainTest, NothingTheCycleDoesAwakeGoesLongUnfed)
{
    bspMockWatchdogStartMeasuring();

    ModuleReachesCloud();
    SettleAt(234); // reports, and the 6 s wait for an answer that never comes

    ModuleBootsButNeverConnects(); // three 29 s windows and a pause
    StageReading(280, 55u);
    RunCycles(4 + kCyclesPerPause);

    ModuleNeverAnswers(); // the 15 s alive timeout
    RunCycles(kCyclesPerPause);

    ModuleIsUnpaired(); // a three-minute pairing window nobody finishes
    bspMockPressButton();
    RunCycles(1);
    bspMockReleaseButton();

    bspMockSensorSetReadFails(true); // the bus recovery
    RunCycles(2);

    EXPECT_GT(bspMockWifiPowerOnCalls(), 6);
    EXPECT_LE(bspMockWatchdogLongestGapMs(), 1000u);
}

// --- The diagnostic record (DP 104) -----------------------------------------
//
// What the device went through since its last report got through, sent with
// the next report that does -- including the one after a reset, which is the
// one that most needs to say what came before it. diag_record_test has the
// text field by field; these are the stories that fill it in.

TEST_F(TempSensorMainTest, TheDiagnosticDpSaysWhatHappenedSinceTheLastReport)
{
    struct Row {
        const char*                story;
        std::function<void()>      before; // everything up to the report
        std::function<void()>      report; // the cycles that send it
        const char*                dp104;  // "" when the report leaves it out
    };

    const Row rows[] = {
        {"the first report after a power-on",
         [] {},
         [this] { ModuleReachesCloud(); RunCycles(1); },
         "r=por c=1 w=0 u=0 m=ff s=0"},

        {"an ordinary report: nothing to say",
         [this] { SettleAt(234); },
         [this] { StageReading(280, 55u); RunCycles(2); },
         ""},

        {"three windows the router never answered",
         [this] { PausedAfterThreeFailures(); },
         [this] { ModuleReachesCloud(); RunCycles(kCyclesPerPause); },
         "r=- c=30 w=3 u=0 m=02 s=0"},

        {"a window that reached the cloud but was refused",
         [this] {
             tuyaSdkMockSetSyncAnswer(TUYA_MOCK_SYNC_TAKEN);
             SettleAt(234);
             tuyaSdkMockSetSyncAnswer(TUYA_MOCK_SYNC_REFUSED);
             StageReading(280, 55u);
             RunCycles(2);
         },
         [this] { tuyaSdkMockSetSyncAnswer(TUYA_MOCK_SYNC_TAKEN); RunCycles(1); },
         "r=- c=5 w=1 u=0 m=04 s=0"},

        {"a window that found the module unpaired",
         [this] { SettleAt(234); ModuleIsUnpaired(); StageReading(280, 55u); RunCycles(2); },
         [this] { ModuleReachesCloud(); RunCycles(kCyclesPerPause); },
         "r=- c=28 w=1 u=1 m=01 s=0"},

        {"a module that never spoke",
         [this] { SettleAt(234); ModuleNeverAnswers(); StageReading(280, 55u); RunCycles(2); },
         [this] { ModuleReachesCloud(); RunCycles(1); },
         "r=- c=5 w=1 u=0 m=ff s=0"},

        {"a pairing window nobody finished: not a failure to report",
         [this] {
             SettleAt(234);
             ModuleIsUnpaired();
             bspMockPressButton();
             RunCycles(1);
             bspMockReleaseButton();
         },
         [this] { ModuleReachesCloud(); StageReading(280, 55u); RunCycles(2); },
         ""},

        {"two sensor reads that failed",
         [this] { SettleAt(234); bspMockSensorSetReadFails(true); RunCycles(2); bspMockSensorSetReadFails(false); },
         [this] { StageReading(280, 55u); RunCycles(2); },
         "r=- c=6 w=0 u=0 m=ff s=2"},

        // The kitchen on 11-11: an outage, then silence until the Reset button.
        // The report after it says the device kept cycling the whole time.
        {"the Reset button pressed in the middle of an outage",
         [this] { PausedAfterThreeFailures(); RunCycles(10); },
         [this] { ResetKeepingPower(BSP_Reset_Pin); ModuleReachesCloud(); RunCycles(1); },
         "r=pin c=17 w=3 u=0 m=02 s=0"},

        {"the watchdog, the same way",
         [this] { PausedAfterThreeFailures(); RunCycles(10); },
         [this] { ResetKeepingPower(BSP_Reset_IWatchdog); ModuleReachesCloud(); RunCycles(1); },
         "r=wdg c=17 w=3 u=0 m=02 s=0"},

        {"a power-on: the record is gone with the power",
         [this] { PausedAfterThreeFailures(); RunCycles(10); },
         [this] {
             for (uint8_t i = 0; i < 3; i++) bspMockSetRetained(i, 0u);
             ResetKeepingPower(BSP_Reset_PowerOn); ModuleReachesCloud(); RunCycles(1);
         },
         "r=por c=1 w=0 u=0 m=ff s=0"},
    };

    for (const Row& row : rows)
    {
        PowerOn();
        row.before();

        int strings_before = tuyaSdkMockStringCount();
        int windows_before = WindowsOpened();
        row.report();

        ASSERT_GT(WindowsOpened(), windows_before) << row.story << ": no report went out";
        EXPECT_EQ(row.dp104, DiagSentSince(strings_before)) << row.story;
    }
}

TEST_F(TempSensorMainTest, TheRecordGoesOutOnceAndStartsOver)
{
    PausedAfterThreeFailures();
    ModuleReachesCloud();
    RunCycles(kCyclesPerPause); // the report that carries it

    int strings_before = tuyaSdkMockStringCount();
    StageReading(330, 55u);
    ASSERT_EQ(1, WindowsDuring(2));
    EXPECT_EQ("", DiagSentSince(strings_before));

    // ...and a reset now has only itself to report.
    ResetKeepingPower(BSP_Reset_Pin);
    strings_before = tuyaSdkMockStringCount();
    ASSERT_EQ(1, WindowsDuring(1));
    EXPECT_EQ("r=pin c=1 w=0 u=0 m=ff s=0", DiagSentSince(strings_before));
}

} // namespace
