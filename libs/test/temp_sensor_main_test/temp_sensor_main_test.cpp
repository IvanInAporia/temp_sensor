#include <gtest/gtest.h>

extern "C" {
#include "battery.h"
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

// How many wifi_uart_service() calls the scripted module takes to boot and to
// reach the cloud. The glue services once per poll, so these are "polls", and
// they are small so a test runs in microseconds.
constexpr int kBootCalls  = 5;
constexpr int kCloudCalls = 20;

class TempSensorMainTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        bspMockReset();
        tuyaSdkMockReset();

        bspMockSetBatteryMv(1300u); // healthy cell, mid-plateau
        StageReading(234, 55u);     // 23.4 C, 55 %

        TempSensorInit();
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

    int WindowsOpened() const { return bspMockWifiPowerOnCalls(); }

    void RunCycles(int n)
    {
        for (int i = 0; i < n; i++)
        {
            TempSensorRunCycle();
        }
    }

    // Settle the median window at a temperature, with reports delivered.
    void SettleAt(int16_t temp_c10)
    {
        ModuleReachesCloud();
        StageReading(temp_c10, 55u);
        RunCycles(3);
    }
};

// --- The quiet case: no radio ---------------------------------------------

TEST_F(TempSensorMainTest, SaysNothingWhileTheTemperatureHolds)
{
    // This is the whole point of the design: measuring is ~110 uA for 13 ms,
    // reporting is seconds at tens of milliamps. A stable room must cost
    // nothing.
    SettleAt(234);
    int windows_after_settling = WindowsOpened();

    RunCycles(50);

    EXPECT_EQ(windows_after_settling, WindowsOpened());
    EXPECT_EQ(53, bspMockSensorWriteCalls()); // but it kept measuring
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

TEST_F(TempSensorMainTest, SendsAllFourDatapointsTogether)
{
    // Humidity and battery cannot trigger a report of their own, so the only
    // way they ever reach the cloud is by riding along on a temperature one.
    ModuleReachesCloud();
    bspMockSetBatteryMv(1300u);
    StageReading(234, 55u);
    RunCycles(1);

    uint32_t temp = 0, humidity = 0, state = 0, percent = 0;

    EXPECT_TRUE(tuyaSdkMockLastDpValue(DPID_TEMP_CURRENT, &temp));
    EXPECT_TRUE(tuyaSdkMockLastDpValue(DPID_HUMIDITY_VALUE, &humidity));
    EXPECT_TRUE(tuyaSdkMockLastDpValue(DPID_BATTERY_STATE, &state));
    EXPECT_TRUE(tuyaSdkMockLastDpValue(DPID_BATTERY_PERCENTAGE, &percent));

    EXPECT_EQ(55u, humidity);
    EXPECT_EQ(static_cast<uint32_t>(Battery_PercentFromMv(1300u)), percent);
    EXPECT_EQ(static_cast<uint32_t>(Battery_StateFromPercent(Battery_PercentFromMv(1300u))), state);
}

TEST_F(TempSensorMainTest, SendsSubZeroTemperatureAsASignedValue)
{
    // DP 1 has range -200..600 in 0.1 C. The SDK puts an unsigned long on the
    // wire as four big-endian bytes, so the sign has to survive the widening
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

    EXPECT_GE(bspMockWifiLastWindowMs(), 10u * 1000u);
    EXPECT_LT(bspMockWifiLastWindowMs(), 15u * 1000u);
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
    EXPECT_EQ(1, tuyaSdkMockSetWifiModeCalls());
    EXPECT_EQ(SMART_CONFIG, tuyaSdkMockLastWifiMode());
}

TEST_F(TempSensorMainTest, PairingWaitsForTheModuleBeforeAskingItToPair)
{
    // mcu_set_wifi_mode is a plain transmit: one sent into a module that has
    // not finished booting is simply lost, and the user is left holding a
    // button that did nothing.
    ModuleNeverAnswers();
    bspMockPressButton();
    RunCycles(1);

    EXPECT_EQ(1, WindowsOpened());
    EXPECT_EQ(0, tuyaSdkMockSetWifiModeCalls());
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
    // The consequence of having no heartbeat: a change that failed to reach
    // the cloud has to stay outstanding, or it is lost until the temperature
    // happens to move again -- which in a stable room may be never.
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

} // namespace
