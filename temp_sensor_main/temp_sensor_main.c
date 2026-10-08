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
 * is written off and the report is left uncommitted, so the next cycle retries
 * from scratch -- which is the right shape, because the usual reason for
 * missing this deadline is a router that is down. */
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

static ReportPolicy report_policy;

/* When the next scheduled cycle is due, as a BSP_GetTimeMs() value. Advanced
 * by exactly one period per cycle rather than re-based off the wake time, so a
 * long cycle body does not push the whole series later. */
static uint32_t next_cycle_ms;

static bool PairingRequested(void);
static void RunWifiWindow(bool report_due, bool pairing, int16_t temp_c10);
static void WaitForNextCycle(void);

void TempSensorInit(void)
{
    ReportPolicy_Init(&report_policy);
    Tuya_Init();

    /* The first cycle runs immediately -- the wait is at the end of the body --
     * so the schedule starts one period out. */
    next_cycle_ms = BSP_GetTimeMs() + CYCLE_PERIOD_MS;
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
    uint16_t      battery_mv    = BSP_Battery_ReadMv();
    uint8_t       battery_pct   = Battery_PercentFromMv(battery_mv);
    Battery_State battery_state = Battery_StateFromPercent(battery_pct);

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
        Tuya_SetDps(temp_c10, reading.rh_pct, battery_pct, battery_state);
    }

    /* A failed sensor read is not an error to handle, it is a cycle with
     * nothing to say: no report is due, the cached values keep last cycle's
     * reading, and the next cycle tries the sensor again. The only thing that
     * still opens a window is the button. */

    if (report_due || pairing)
    {
        RunWifiWindow(report_due, pairing, temp_c10);
    }

    WaitForNextCycle();
}

/* One report and/or pairing window, start to finish.
 *
 * Every step has a deadline and every exit path goes through Tuya_PowerOff(),
 * because leaving the module powered is the one mistake that empties the cell
 * outright -- tens of milliamps against a budget built on microamps.
 */
static void RunWifiWindow(bool report_due, bool pairing, int16_t temp_c10)
{
    Tuya_PowerOn();

    if (Tuya_WaitUntil(Tuya_IsAlive, WIFI_ALIVE_TIMEOUT_MS))
    {
        if (pairing)
        {
            /* Only now: this is a plain transmit, and one sent into a module
             * that has not finished booting is simply lost. */
            Tuya_StartPairing();
        }

        uint32_t cloud_timeout_ms = pairing ? WIFI_PAIRING_TIMEOUT_MS : WIFI_CLOUD_TIMEOUT_MS;

        if (Tuya_WaitUntil(Tuya_IsCloudConnected, cloud_timeout_ms))
        {
            /* Unconditional, not gated on report_due: the cloud is up and the
             * cached readings are this cycle's, so a pairing window pushes the
             * current state too. That is what makes the button confirm to
             * whoever pressed it that the device works. */
            Tuya_ReportCachedDps();

            Tuya_ServiceFor(pairing ? WIFI_PAIRING_DRAIN_MS : WIFI_DRAIN_MS);

            /* Committed only here, on the one path where the value reached the
             * cloud. Every other way out of this function leaves the policy
             * alone, so the next cycle still finds the change outstanding and
             * tries again -- which matters because there is no heartbeat to
             * repair a dropped report later. */
            if (report_due)
            {
                ReportPolicy_ReportSent(&report_policy, temp_c10);
            }
        }
    }

    Tuya_PowerOff();
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
