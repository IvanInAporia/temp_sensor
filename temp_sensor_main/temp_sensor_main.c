#include "temp_sensor_main.h"

#include "battery.h"
#include "bsp.h"
#include "hdc3020.h"
#include "report_policy.h"
#include "tuya_link.h"

// --- Why this file is written as straight-line code ---------------------------
//
// There is exactly one thing to do per wake, and nothing that has to happen
// while it is happening. The sensor read is a blocking 15 ms. The Wi-Fi window
// is minutes at worst and the module is the only thing on the board drawing
// current while it is open. Between windows the MCU is stopped. At no point is
// there a second activity competing for the CPU.
//
// So there is no scheduler, no task, no coroutine and no state machine here:
// the cycle is a list of steps in the order they happen, and a wait is a call
// that returns when the wait is over. The only concurrency on the device is the
// LPUART receive interrupt, which does nothing but push a byte into the Tuya
// SDK's ring buffer and re-arm itself (tuya_link.c).
//
// What makes that affordable is Tuya_WaitUntil(): it blocks, but it services
// the Tuya SDK and idles the core on every pass, so "block for 20 seconds
// waiting for the cloud" costs interrupt wakeups rather than a spin. The one
// rule that keeps this honest is that every wait has a deadline. Nothing here
// may wait on the module forever -- it is a separate processor on the far end
// of a UART with no reset line, and if it never answers, the cycle has to end
// anyway and try again in five minutes.

/* How often the sensor is read. Measuring is cheap enough that this is not
 * really a power decision; it sets how quickly a real temperature change can
 * be noticed, and with the median filter needing one extra sample to confirm a
 * step, how quickly it can be reported (up to two periods). */
#define CYCLE_PERIOD_MS (5u * 60u * 1000u)

/* The module has to boot and finish its start-up handshake before anything may
 * be sent to it (Tuya_IsAlive). On the bench the T3-3S took ~3 s from its
 * first heartbeat to its first state report, plus however long it took to
 * boot to that heartbeat; this is several times that. Exceeding it means the
 * module is not there or not talking our protocol: unpowered, unflashed, the
 * UART miswired, or a module firmware that speaks something else. */
#define WIFI_ALIVE_TIMEOUT_MS (15u * 1000u)

/* Association plus cloud handshake on a healthy network. Past this the window
 * is written off and the report is left uncommitted, so a later cycle retries
 * from scratch -- which is the right shape, because the usual reason for
 * missing this deadline is a router that is down. How much later is
 * REPORT_RETRY_MS. */
#define WIFI_CLOUD_TIMEOUT_MS (25u * 1000u)

/* A pairing window instead: provisioning takes as long as somebody standing at
 * the device takes, which is not 25 seconds. Three minutes is also how long the
 * module itself stays in pairing mode before going back to its old network
 * (CONFIG_MODE in ../tuya/protocol.h), so there is nothing to wait for after.
 *
 * Still under the cycle period, deliberately. Nothing breaks if a window
 * overruns it -- the schedule re-bases rather than firing a burst of catch-up
 * cycles (WaitForNextCycle) -- but a window that outlasted the cadence would
 * mean the device stopped measuring for longer than it sleeps, and three
 * minutes is already far longer than any successful pairing takes. */
#define WIFI_PAIRING_TIMEOUT_MS (3u * 60u * 1000u)

/* Held open after the DPs go out. mcu_dp_*_update() only queues bytes; the
 * module still has to send them on and acknowledge. Cutting the supply the
 * instant the last byte leaves the shift register throws all of that away. */
#define WIFI_DRAIN_MS (2u * 1000u)

/* A pairing window drains longer: reaching the cloud is where provisioning
 * starts being useful, not where it ends -- the app still has to finish
 * registering the device. */
#define WIFI_PAIRING_DRAIN_MS (10u * 1000u)

/* Long enough to outlast contact bounce on a plain switch to ground, short
 * enough to be invisible to whoever is holding it. Only ever paid on a cycle
 * that actually saw a press edge. */
#define BUTTON_DEBOUNCE_MS 50u

/* How long after a report window that never reached the cloud the next one may
 * open.
 *
 * A failed window is the most expensive thing this device does: about 29 s
 * with the module powered (some 4 s to come alive, then the whole cloud
 * timeout). With the router down every window fails, and retried on every
 * cycle that is the module powered ~10 % of the time -- a flat cell in three
 * or four days.
 *
 * So two quick retries, for a router that is rebooting, and then a pause:
 * three failures in a row says it is down and likely to stay down a while.
 * Through a long outage that is one window every two hours, about 12 a day.
 *
 * Times, not cycles. A shorter CYCLE_PERIOD_MS must not shorten the retry,
 * which is already about as short as a router takes to come back. */
#define REPORT_RETRY_MS (5u * 60u * 1000u)
#define REPORT_PAUSE_MS (2u * 60u * 60u * 1000u)

/* Quick retries before failed report windows start the pause. */
#define REPORT_RETRIES 2u

static ReportPolicy report_policy;

/* When the next scheduled cycle is due, as a BSP_GetTimeMs() value. Advanced
 * by exactly one period per cycle rather than re-based off the wake time, so a
 * long cycle body does not push the whole series later. */
static uint32_t next_cycle_ms;

/* Quick retries left before the pause. Only a window that gets through refills
 * them, so once a pause runs out the device tries once and, if that fails too,
 * pauses again.
 *
 * Kept in RAM, so the board's Reset button is how to make the device try again
 * right now. The Wi-Fi button is not: it always opens a window, but a pairing
 * one, which takes the module off its network. */
static uint8_t report_retries_left;

/* Report windows are held back until next_cycle_ms reaches this.
 *
 * On the schedule rather than the clock. On the device a cycle starts a few
 * milliseconds after its slot, by a different few each time -- the RTC wakeup
 * and the clock that measures it are both coarse -- so a retry timed off the
 * clock to land five minutes on would miss the cycle five minutes on whenever
 * the failed one had started later than that one, and slip a whole period.
 *
 * A cycle the button brings forward shares the slot before it, so a report
 * window that fails in one -- possible only on a press the debounce rejects --
 * may be retried a little early. Harmless. */
static uint32_t report_hold_until_ms;

/* A report window found the module waiting to be paired: it never was, or it
 * was removed in the app. No report window opens while this is set -- each
 * would only find the module still waiting -- not even the ones the policy
 * keeps asking for until the first report after boot lands. Only a pairing
 * window that gets through clears it.
 *
 * In RAM, like the backoff. After a reset the first report window finds out
 * again, at the cost of the ~4 s the module takes to say so. */
static bool module_unpaired;

/* The lowest the module's load pulled the cell to, in mV, over the windows
 * since the last one that reached the cloud -- or 0 if none was measured. The
 * next window carries it out (DP 101), so a report says how the windows before
 * it went, failed ones included. A window whose sag browned the module out is
 * the one most worth knowing about, and the one that could not say so itself.
 *
 * In RAM, like the backoff. A battery swap resets it, which is right: it
 * belongs to the cell it was measured on. */
static uint16_t battery_min_mv;

/* The cell's collapse voltage, estimated over its windows (battery.h). It moves
 * the battery percentage's 0 % point and says whether the cell is damaged.
 *
 * In RAM, so the reset a battery swap normally causes clears it, and so does
 * a swap the MCU rides through: see Battery_IsNewCell. */
static Battery_SagTrend sag_trend;

/* The last resting voltage that could be read, for Battery_IsNewCell. 0 until
 * the first. */
static uint16_t previous_rest_mv;

static uint16_t LowerMv(uint16_t a_mv, uint16_t b_mv);
static bool PairingRequested(void);
static bool ReportBackoffOver(void);
static bool RunWifiWindow(bool report_due, bool pairing, int16_t temp_c10, uint16_t rest_mv);
static void UpdateBackoff(bool reached_cloud, bool pairing);
static void WaitForNextCycle(void);

void TempSensorInit(void)
{
    ReportPolicy_Init(&report_policy);
    Tuya_Init();

    /* The first cycle runs immediately -- the wait is at the end of the body --
     * so the schedule starts one period out. */
    next_cycle_ms = BSP_GetTimeMs() + CYCLE_PERIOD_MS;

    /* No backoff carries over a reset, and no pairing state: the first cycle
     * reports, or finds out the module has nowhere to report to. */
    report_retries_left  = REPORT_RETRIES;
    report_hold_until_ms = next_cycle_ms;
    module_unpaired      = false;
    battery_min_mv       = 0u;
    previous_rest_mv     = 0u;
    Battery_SagTrendReset(&sag_trend);
}

void TempSensorRun(void)
{
    for (;;)
    {
        TempSensorRunCycle();
    }
}

void TempSensorRunCycle(void)
{
    /* The button first, because a press is usually why this cycle is running at
     * all: it ends the sleep early. Reading it here also means the debounce
     * delay is paid before the sensor read rather than after, so a press
     * cannot be missed while the HDC3020 converts. */
    bool pairing = PairingRequested();

    /* The battery next, while nothing else is powered. One NiMH cell feeding a
     * 3.3 V boost delivers roughly three times the module's current at the
     * cell, so a reading taken with the Wi-Fi rail up measures the sag and not
     * the charge. */
    uint16_t battery_mv = BSP_Battery_ReadMv();

    if (Battery_IsNewCell(previous_rest_mv, battery_mv))
    {
        /* What was measured belongs to the cell that came out. A false alarm
         * costs little: the next window measures this cell afresh. */
        Battery_SagTrendReset(&sag_trend);
        battery_min_mv = 0u;
    }

    if (battery_mv != 0u)
    {
        previous_rest_mv = battery_mv;
    }

    /* From the windows before this one: this cycle's own is still to come, and
     * the DPs cannot change once the module is up -- it queries them twice
     * per window. One report late, like DP 101. */
    uint16_t      collapse_mv     = Battery_SagTrendMv(&sag_trend);
    uint8_t       battery_pct     = Battery_UsablePercentFromMv(battery_mv, collapse_mv);
    Battery_State battery_state   = Battery_StateFromPercent(battery_pct);
    bool          battery_damaged = Battery_IsDamaged(collapse_mv);

    Hdc3020_Reading reading;
    bool            report_due = false;
    int16_t         temp_c10   = 0;

    if (Hdc3020_Read(&reading))
    {
        report_due = ReportPolicy_Update(&report_policy, reading.temp_c10, &temp_c10);

        /* Cached whether or not a report is due. The module asks for the
         * current readings itself every time it reconnects, and a pairing
         * window can open with no report pending -- there has to be something
         * to answer with, and it should be this cycle's reading rather than
         * whatever the last report left behind. */
        Tuya_SetDps(temp_c10, reading.rh_pct, battery_pct, battery_state, battery_damaged);
    }

    /* A failed sensor read is not an error to handle, it is a cycle with
     * nothing to say: no report is due, the cached values keep last cycle's
     * reading, and the next cycle tries the sensor again. The only thing that
     * still opens a window is the button. */

    /* Every cycle, report due or not -- ReportBackoffOver says why. */
    bool backoff_over = ReportBackoffOver();

    /* A report held back -- by the backoff, or because the module is unpaired
     * -- is not lost. Nothing was committed, so the policy still finds it
     * outstanding once the hold ends, and the readings above are cached for a
     * pairing window in the meantime. The button is never held back -- though
     * its window only ends either hold if it gets through. */
    if ((report_due && backoff_over && !module_unpaired) || pairing)
    {
        bool reached_cloud = RunWifiWindow(report_due, pairing, temp_c10, battery_mv);

        UpdateBackoff(reached_cloud, pairing);
    }

    WaitForNextCycle();
}

/* Whether a report window may open in this cycle -- which it may, except while
 * a retry wait or the pause after failed windows is still running.
 *
 * Called on every cycle, report due or not, for the sake of what it does on
 * the way out.
 */
static bool ReportBackoffOver(void)
{
    /* Unsigned subtraction, as in WaitForNextCycle: a deadline already behind
     * us wraps to a huge number, so "more left than the longest wait there is"
     * is how a passed deadline shows up. */
    uint32_t remaining_ms = report_hold_until_ms - next_cycle_ms;

    if ((remaining_ms != 0u) && (remaining_ms <= REPORT_PAUSE_MS))
    {
        return false;
    }

    /* Over, so brought up to now. Left where it was, a passed deadline would
     * fall further behind on every quiet cycle, and some 49.7 days on the clock
     * would wrap round to it and read it as a pause just started. */
    report_hold_until_ms = next_cycle_ms;

    return true;
}

/* After a window: one that got through clears the backoff, a failed report
 * window extends it.
 */
static void UpdateBackoff(bool reached_cloud, bool pairing)
{
    if (reached_cloud)
    {
        /* Whatever kind of window it was: the network works, and anything that
         * was outstanding has just been delivered with it. */
        report_retries_left  = REPORT_RETRIES;
        report_hold_until_ms = next_cycle_ms;

        return;
    }

    if (pairing)
    {
        /* Says nothing about the router -- the module spent the window waiting
         * for a phone, not trying its network -- so it neither counts as a
         * failure nor ends a pause. */
        return;
    }

    uint32_t wait_ms;

    if (report_retries_left > 0u)
    {
        report_retries_left--;
        wait_ms = REPORT_RETRY_MS;
    }
    else
    {
        wait_ms = REPORT_PAUSE_MS;
    }

    /* From this cycle's slot, not from when the window gave up: timed from
     * ~29 s later, the first retry would miss the next cycle at today's period
     * and wait out one more. */
    report_hold_until_ms = next_cycle_ms + wait_ms;
}

/* One report and/or pairing window, start to finish. Returns whether the
 * module reached the cloud. rest_mv is this cycle's resting cell voltage, to
 * set the window's sag against.
 *
 * Every step has a deadline and every exit path goes through Tuya_PowerOff(),
 * because leaving the module powered is the one mistake that empties the cell
 * outright -- tens of milliamps against a budget built on microamps.
 */
static bool RunWifiWindow(bool report_due, bool pairing, int16_t temp_c10, uint16_t rest_mv)
{
    bool reached_cloud = false;

    /* The windows before this one: its own minimum is still to be measured.
     * Cached now, with the module still unpowered, like the readings. */
    Tuya_SetBatteryMinMv(battery_min_mv);

    /* Before the supply, so the module's inrush counts too. */
    BSP_Battery_StartSagMonitor();

    Tuya_PowerOn();

    if (Tuya_WaitUntil(Tuya_IsAlive, WIFI_ALIVE_TIMEOUT_MS))
    {
        if (!pairing && Tuya_IsUnpaired())
        {
            /* No network to try, so the cloud timeout would be waited out for
             * nothing: the window ends here, some 4 s in rather than 29.
             *
             * UpdateBackoff counts it as a failed window. That is never seen:
             * only a window that gets through clears module_unpaired, and that
             * clears the backoff too. */
            module_unpaired = true;
        }
        else
        {
            if (pairing)
            {
                /* Only now: this is a plain transmit, and one sent into a
                 * module that has not finished booting is simply lost. */
                Tuya_StartPairing();
            }

            uint32_t cloud_timeout_ms = pairing ? WIFI_PAIRING_TIMEOUT_MS : WIFI_CLOUD_TIMEOUT_MS;

            if (Tuya_WaitUntil(Tuya_IsCloudConnected, cloud_timeout_ms))
            {
                /* Unconditional, not gated on report_due: the cloud is up and
                 * the cached readings are this cycle's, so a pairing window
                 * pushes the current state too. That is what makes the button
                 * confirm to whoever pressed it that the device works. */
                Tuya_ReportCachedDps();

                Tuya_ServiceFor(pairing ? WIFI_PAIRING_DRAIN_MS : WIFI_DRAIN_MS);

                /* Committed only here, on the one path where the value reached
                 * the cloud. Every other way out of this function leaves the
                 * policy alone, so the next cycle still finds the change
                 * outstanding and tries again -- which matters because there
                 * is no heartbeat to repair a dropped report later. */
                if (report_due)
                {
                    ReportPolicy_ReportSent(&report_policy, temp_c10);
                }

                /* Paired, whatever an earlier window found. While the flag is
                 * set only a pairing window gets this far. */
                module_unpaired = false;
                reached_cloud   = true;
            }
        }
    }

    Tuya_PowerOff();

    /* After the supply, so the monitor covered all of it. Never left running:
     * its interrupt fires every ~170 us. */
    uint16_t window_min_mv = BSP_Battery_StopSagMonitor();

    /* Every window counts, failed ones included: one whose sag browned the
     * module out is the strongest evidence there is. */
    Battery_SagTrendAdd(&sag_trend, Battery_CollapseMv(rest_mv, window_min_mv));

    if (reached_cloud)
    {
        /* What was cached has gone out, so the next report starts from this
         * window alone. */
        battery_min_mv = window_min_mv;
    }
    else
    {
        battery_min_mv = LowerMv(battery_min_mv, window_min_mv);
    }

    return reached_cloud;
}

/* The lower of two readings in mV, where 0 is "no reading" rather than 0 V.
 */
static uint16_t LowerMv(uint16_t a_mv, uint16_t b_mv)
{
    if ((a_mv == 0u) || ((b_mv != 0u) && (b_mv < a_mv)))
    {
        return b_mv;
    }

    return a_mv;
}

static bool PairingRequested(void)
{
    if (!BSP_Wifi_TakeButtonEvent())
    {
        return false;
    }

    /* An edge on its own is not a press. The pin is held up by the MCU's
     * internal pull-up alone (25-65 kOhm), so it is a long antenna for
     * anything the boost converter is doing, and the switch bounces besides.
     * A real press is still down after the bounce has settled. */
    BSP_DelayMs(BUTTON_DEBOUNCE_MS);

    return BSP_Wifi_ButtonIsPressed();
}

/* Sleep until the next cycle is due, or until the button says otherwise.
 *
 * The sleep is broken up by anything that wakes the MCU, so this is a loop:
 * a wake that is not the button goes straight back to sleep for whatever is
 * left.
 */
static void WaitForNextCycle(void)
{
    for (;;)
    {
        uint32_t remaining_ms = next_cycle_ms - BSP_GetTimeMs();

        /* Unsigned subtraction: a deadline already behind us wraps to a huge
         * number, so "more than one period left" is how a missed deadline
         * shows up, not an impossible state. */
        if ((remaining_ms == 0u) || (remaining_ms > CYCLE_PERIOD_MS))
        {
            next_cycle_ms += CYCLE_PERIOD_MS;

            /* Accumulating keeps the series on its original phase, but only
             * while the body is short against the period. A three-minute
             * pairing window is not, and two of them back to back would leave
             * the schedule far enough behind to fire several cycles with no
             * sleep between them. Re-base instead of catching up: the phase is
             * worth nothing here, and a burst of cycles costs sensor reads and
             * possibly reports. */
            uint32_t next_remaining_ms = next_cycle_ms - BSP_GetTimeMs();

            if ((next_remaining_ms == 0u) || (next_remaining_ms > CYCLE_PERIOD_MS))
            {
                next_cycle_ms = BSP_GetTimeMs() + CYCLE_PERIOD_MS;
            }

            return;
        }

        uint32_t slept_ms = BSP_McuSleep(remaining_ms);

        /* Both halves matter. A short sleep says something woke the MCU that
         * was not the clock, and the button is the only such source on this
         * board; the level then says the press is real and still happening.
         *
         * Testing the level alone would also fire on a sleep that ran its full
         * length with the button simply held down -- a stuck switch -- and
         * would return with the schedule un-advanced, costing a wasted extra
         * cycle every period for as long as it stayed stuck.
         */
        if ((slept_ms < remaining_ms) && BSP_Wifi_ButtonIsPressed())
        {
            /* The press is why we are awake. Run the cycle now and leave the
             * schedule alone: the next scheduled cycle is still due when it
             * was due, and the press has cost the series nothing. */
            return;
        }
    }
}
