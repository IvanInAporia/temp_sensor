#pragma once

// Host implementation of bsp.h, plus the knobs a test drives it with.
//
// Time is explicit, not real. BSP_GetTimeMs() only moves when something that
// takes time on the device takes time here too: BSP_DelayMs, BSP_Idle (one
// millisecond, standing in for the target's SysTick floor) and BSP_McuSleep.
// So a test never waits, and a timeout either expires or does not for reasons
// the test chose.

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Back to power-on: clock at zero, no reading staged, battery flat, button up,
 * module unpowered, all counters cleared. Call in SetUp().
 */
void bspMockReset(void);

/* --- Clock ---------------------------------------------------------------- */

void     bspMockAdvanceMs(uint32_t ms);
uint32_t bspMockNowMs(void);

/* --- Watchdog ------------------------------------------------------------- */

int bspMockWatchdogRefreshCalls(void);

/* The longest stretch of awake time between two feeds since the last
 * bspMockWatchdogStartMeasuring (or the reset). A sleep counts as fed, as
 * bsp.h has the real BSP_McuSleep feed it through the sleep.
 */
uint32_t bspMockWatchdogLongestGapMs(void);
void     bspMockWatchdogStartMeasuring(void);

/* --- Retained words ------------------------------------------------------- */

/* bspMockReset is a power-on and clears them; TempSensorInit alone is a reset
 * that keeps the power, and leaves them alone, as the device does. */
void     bspMockSetRetained(uint8_t index, uint32_t value);
uint32_t bspMockRetained(uint8_t index);

/* --- Sleep ---------------------------------------------------------------- */

int      bspMockSleepCalls(void);
uint32_t bspMockLastSleepRequestMs(void);

/* Total time asked for across every BSP_McuSleep call. The cycle's cadence is
 * this, not wall time, so this is what a "did it sleep five minutes" assertion
 * looks at.
 */
uint32_t bspMockTotalSleepRequestedMs(void);


/* --- Sensor bus ----------------------------------------------------------- */

/* Stage the six bytes the next BSP_Sensor_I2cRead hands back. Tests build
 * these with the helper in sensor_frame.h so the CRCs are the driver's own.
 */
void bspMockSensorSetResponse(const uint8_t* bytes, uint16_t len);

/* Make the next write (or read) fail, as a NACK or bus error would. */
void bspMockSensorSetWriteFails(bool fails);
void bspMockSensorSetReadFails(bool fails);

/* Lock the bus up: every transfer fails until BSP_Sensor_RecoverBus is called,
 * as a controller stuck "busy" after a glitch does on the device.
 */
void bspMockSensorLockBus(void);
int  bspMockSensorRecoverCalls(void);

int     bspMockSensorWriteCalls(void);
int     bspMockSensorReadCalls(void);
uint8_t bspMockSensorLastAddress(void);

/* The command bytes of the most recent write, so a test can assert which
 * measurement mode was triggered.
 */
uint8_t bspMockSensorLastCommandMsb(void);
uint8_t bspMockSensorLastCommandLsb(void);

/* --- Battery -------------------------------------------------------------- */

void bspMockSetBatteryMv(uint16_t mv);

/* The clock reading at the moment BSP_Battery_ReadMv was last called. Compared
 * against bspMockWifiLastPowerOnMs to assert that the cell is measured before
 * the module loads it, which is the difference between reading the charge and
 * reading the sag.
 */
uint32_t bspMockBatteryLastReadMs(void);
int      bspMockBatteryReadCalls(void);

/* What BSP_Battery_StopSagMonitor returns for a monitor that was running: the
 * lowest single reading of the window (DP 101), and the lowest point of its
 * running average (DP 103, and what the gauge uses). 0 is the device's "saw
 * nothing", and both start at 0.
 */
void bspMockSetSagMinMv(uint16_t mv);
void bspMockSetSagAverageMinMv(uint16_t mv);

bool bspMockSagMonitorRunning(void);

/* Whether the monitor ran for the whole of the last window: already running
 * when the supply came up, still running when it went down.
 */
bool bspMockSagMonitorCoveredLastWindow(void);

/* BSP_McuSleep calls entered with the monitor still running. The real BSP
 * stops it there, and so does this one, but the cycle is meant to have stopped
 * it already: anything above zero is a path out of a window that forgot.
 */
int bspMockSleepsWithSagMonitorRunning(void);

/* --- Button --------------------------------------------------------------- */

/* The mock deals in presses the real BSP has already debounced
 * (BSP_Wifi_TakeButtonPress): bounce, glitches and a stuck button are the
 * debounce's, and button_debounce_test has those. What is left to script is
 * when the press happens.
 */

/* A press, confirmed now: BSP_Wifi_TakeButtonPress reports it, and a sleep
 * that starts before then ends before it starts. */
void bspMockPressButton(void);

/* Kept for the tests' reading order. A held button is one press, so letting go
 * changes nothing the application can see. */
void bspMockReleaseButton(void);

/* A press part-way into the next sleep that is longer than `after_ms`: the
 * sleep ends there and returns `after_ms`, advancing the clock by it, exactly
 * as the real one does from the RTC. */
void bspMockPressButtonDuringNextSleep(uint32_t after_ms);

/* A press while the next Wi-Fi window is open: confirmed when the rail comes
 * up. */
void bspMockPressButtonDuringNextWindow(void);

/* --- Wi-Fi module --------------------------------------------------------- */

bool     bspMockWifiIsPowered(void);
int      bspMockWifiPowerOnCalls(void);
int      bspMockWifiPowerOffCalls(void);
uint32_t bspMockWifiLastPowerOnMs(void);

/* How long the module spent powered across the last completed window. */
uint32_t bspMockWifiLastWindowMs(void);

/* Every byte handed to BSP_Wifi_TransmitByte since the reset. */
int     bspMockWifiTxCount(void);
uint8_t bspMockWifiTxAt(int index);

/* Receive arming, so a test can assert the UART is left idle when the module
 * is not powered.
 */
bool bspMockWifiRxArmed(void);
int  bspMockWifiRxArmCalls(void);
int  bspMockWifiAbortReceiveCalls(void);

/* Deliver one byte to whatever BSP_Wifi_Receive last armed, as the UART
 * interrupt would.
 */
void bspMockWifiDeliverByte(uint8_t value);

/* Fire the registered RX error callback, as an overrun would. */
void bspMockWifiRaiseRxError(void);

/* --- Misc ----------------------------------------------------------------- */

void bspMockSetResetReason(int reason);
bool bspMockSpareIsSet(void);
int  bspMockResetCalls(void);

#ifdef __cplusplus
}
#endif
