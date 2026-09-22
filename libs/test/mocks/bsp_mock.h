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

/* --- Sleep ---------------------------------------------------------------- */

int      bspMockSleepCalls(void);
uint32_t bspMockLastSleepRequestMs(void);

/* Total time asked for across every BSP_McuSleep call. The cycle's cadence is
 * this, not wall time, so this is what a "did it sleep five minutes" assertion
 * looks at.
 */
uint32_t bspMockTotalSleepRequestedMs(void);

/* Make the next `count` sleeps end after `ms` instead of running their full
 * length -- the device's button wake. The mock returns the shorter figure and
 * advances the clock by it, exactly as the real one does from the RTC.
 */
void bspMockSetEarlyWake(uint32_t ms, int count);

/* --- Sensor bus ----------------------------------------------------------- */

/* Stage the six bytes the next BSP_Sensor_I2cRead hands back. Tests build
 * these with the helper in sensor_frame.h so the CRCs are the driver's own.
 */
void bspMockSensorSetResponse(const uint8_t* bytes, uint16_t len);

/* Make the next write (or read) fail, as a NACK or bus error would. */
void bspMockSensorSetWriteFails(bool fails);
void bspMockSensorSetReadFails(bool fails);

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

/* --- Button --------------------------------------------------------------- */

/* Press: latches the edge BSP_Wifi_TakeButtonEvent reports AND holds the level
 * down, which is what a debounce confirms against.
 */
void bspMockPressButton(void);
void bspMockReleaseButton(void);

/* Latch an edge without holding the level -- a bounce or a noise pulse. The
 * event is reported once; the debounce should then reject it.
 */
void bspMockGlitchButton(void);

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
