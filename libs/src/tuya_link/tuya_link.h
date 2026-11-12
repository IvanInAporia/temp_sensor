#pragma once

// Everything the cycle needs from the Tuya T3-3S, in the shape the cycle wants
// to read: power it up, wait for it, hand it its numbers, put it away.
//
// Below this sits the vendor MCU SDK in ../../../tuya/, which is a byte pump
// with a parser attached -- it has no notion of a report window, a timeout or
// a cached reading. Those live here, so that temp_sensor_main.c can stay a
// list of steps and never touch a Tuya symbol.
//
// The module is power-gated with no reset line, so every window is a cold
// boot: it comes up knowing nothing, re-associates, and the SDK's cached Wi-Fi
// state has to be thrown away and rebuilt each time (Tuya_PowerOn).

#include <stdbool.h>
#include <stdint.h>

#include "battery.h"

/* Once at boot, before anything else here. Leaves the module unpowered. */
void Tuya_Init(void);

/* Open a report window: supply on, UART up, receive armed, SDK parser reset.
 *
 * Returns immediately -- the module needs seconds to boot and associate, which
 * is what Tuya_WaitUntil(Tuya_IsCloudConnected, ...) is for.
 */
void Tuya_PowerOn(void);

/* Close the window: receive disarmed, supply off. Safe to call on a window
 * that never got anywhere.
 */
void Tuya_PowerOff(void);

/* One pass of the SDK's parser over whatever the receive interrupt has queued.
 *
 * Must be called often for the whole time the module is powered: this is what
 * answers its heartbeat and its queries, and the module gives up on an MCU
 * that stops answering. All the waits below call it for you; a caller doing
 * its own loop has to call it itself.
 */
void Tuya_Service(void);

/* Service the SDK until `condition` returns true, or until `timeout_ms` has
 * passed. Returns true if the condition was met.
 *
 * Idles the core between polls, so a wait costs interrupt wakeups rather than
 * a spin. This is the blocking primitive the whole cycle is written in terms
 * of -- see temp_sensor_main.c.
 *
 * Feeds the watchdog on every pass, which is safe only because every wait has
 * a deadline: a three-minute pairing window is longer than the watchdog's
 * timeout, and this is where it is spent.
 *
 * A NULL `condition` never becomes true, which is how Tuya_ServiceFor() is
 * built: waiting out a fixed span while keeping the SDK fed.
 */
bool Tuya_WaitUntil(bool (*condition)(void), uint32_t timeout_ms);

/* Keep servicing the SDK for `ms`, ignoring any condition.
 *
 * Used to drain a window after the report goes out: mcu_dp_*_update() only
 * queues bytes, and the module still has an acknowledgement to send back.
 * Cutting the supply the instant the last byte leaves the shift register
 * discards all of that.
 */
void Tuya_ServiceFor(uint32_t ms);

/* True once the module has told us anything at all about its Wi-Fi state.
 *
 * The module only reports its state (command 0x03) after its start-up
 * handshake with the MCU -- heartbeat, product information, working mode --
 * which the SDK answers on its own from inside Tuya_Service(). So this is the
 * point at which the module has booted, knows which product it is, and takes
 * commands. Nothing may be sent to it before this -- a command sent into a
 * module that is still booting is simply lost.
 */
bool Tuya_IsAlive(void);

/* True once the module has reached the Tuya cloud, which is the point at which
 * a DP report is delivered rather than dropped.
 */
bool Tuya_IsCloudConnected(void);

/* True while the module says it has no network -- it never was paired, or it
 * was removed in the app -- so it will not reach the cloud however long it is
 * given. That is any of the three pairing states, or low power (0x05): where
 * such a module goes once it has offered pairing for three minutes with nobody
 * taking it up.
 *
 * Known from Tuya_IsAlive() on: the module's first state report is what says
 * so. False before that, when nothing is known yet.
 */
bool Tuya_IsUnpaired(void);

/* The Wi-Fi state the module last reported, as the SDK numbers it (wifi.h:
 * 0x02 no router, 0x03 router, 0x04 cloud, 0x00/0x01/0x06 pairing, 0x05 low
 * power with no network), or 0xFF if it has said nothing since Tuya_PowerOn.
 * Still readable after Tuya_PowerOff, which is when the diagnostic record
 * (diag_record.h) wants it.
 */
uint8_t Tuya_WifiState(void);

/* Put the module into pairing mode. Requires Tuya_IsAlive().
 *
 * Sends the reset command, on which the module restarts into pairing over
 * Bluetooth or as an AP hotspot -- whichever the phone uses. The module
 * reports its state afresh once it is back, so Tuya_IsCloudConnected() is
 * false until it has actually been paired, even if it was online a moment
 * before.
 *
 * A plain transmit: whether the module got it is for Tuya_PairingAnswered to
 * say, and sending it again is the only remedy. Each call starts the two
 * answers below over.
 *
 * The product runs Tuya's anti-misoperation mode (CONFIG_MODE in
 * ../../../tuya/protocol.h): a module that is not paired within three minutes
 * goes back to the network it already had, so a stray press loses nothing.
 */
void Tuya_StartPairing(void);

/* The module has answered the reset command from the last Tuya_StartPairing.
 * It answers as soon as the command arrives, before it restarts, so this says
 * the command got there -- not that pairing has begun.
 */
bool Tuya_PairingAnswered(void);

/* The module has said it is pairing -- reported 0x00, 0x01 or 0x06 -- since
 * the last Tuya_StartPairing. Low power (0x05) does not count: the module is
 * then unpaired, but not offering pairing to anyone. Kept rather than read off
 * the current state, because a pairing that gets through leaves that state
 * behind: by the end it says "cloud".
 */
bool Tuya_PairingEntered(void);

/* Cache one complete set of readings. Does not transmit.
 *
 * Cached rather than sent straight through because the module asks for the
 * full set itself, on its own schedule, whenever it reconnects -- and it
 * reconnects on every window. The SDK calls all_data_update() for that, from
 * inside Tuya_Service(), at a moment no caller controls. There has to be an
 * answer ready.
 *
 * battery_damaged is DP 102: the cell sags far more than a healthy one.
 */
void Tuya_SetDps(int16_t temp_c10,
                 uint8_t rh_pct,
                 uint8_t battery_pct,
                 Battery_State battery_state,
                 bool battery_damaged);

/* Cache the cell's lowest voltage under load, in millivolts. Does not
 * transmit.
 *
 *  - min_mv, the lowest single reading, is DP 101. It goes out only below
 *    1000 mV, where the sag starts to matter.
 *  - average_min_mv, the lowest point of the running average, is DP 103. It
 *    goes out whatever its value, while the history it builds is what the
 *    gauge's thresholds will be set from.
 *
 * 0 means nothing has been measured, and reports nothing. Apart from the
 * reading set above because it is a measurement of report windows, not of the
 * room: it has its own "nothing yet", and goes out with or without a reading.
 * Like Tuya_SetDps, call it only with the module unpowered.
 */
void Tuya_SetBatteryMinMv(uint16_t min_mv, uint16_t average_min_mv);

/* Cache the diagnostic text, DP 104 (diag_record.h), or `len` 0 for none.
 * At most DIAG_TEXT_MAX bytes; anything longer is cut. Does not transmit, and
 * like the others, call it only with the module unpowered.
 */
void Tuya_SetDiag(const char* text, uint8_t len);

/* Push the cached readings to the module.
 *
 * Called by the SDK itself (all_data_update in ../../../tuya/protocol.c)
 * whenever the module asks for everything, and by the cycle once the cloud is
 * up. Sends no readings until Tuya_SetDps() has supplied a set, so a query
 * arriving before the first measurement reports nothing rather than zeroes --
 * and, the same way, no DP 101 while its value is 0 or 1000 mV and above, no
 * DP 103 while its value is 0, and no DP 104 while there is no text.
 */
void Tuya_ReportCachedDps(void);

/* The same push, but the temperature (DP 1) goes out last, as a synchronous
 * report (cmd 0x22): the module answers it (0x23) once the cloud has taken it,
 * or with a refusal if it could not deliver it within 5 s. Everything else
 * goes ahead of it as plain reports on the same connection, so that one
 * answer covers the whole push.
 *
 * Only one: the SDK allows no second synchronous report before the first is
 * answered (protocol.h, item 14).
 *
 * Returns at once; Tuya_ReportAnswered says when the answer is in, and
 * Tuya_ReportConfirmed whether it said yes. With no readings cached there is
 * nothing to confirm, and this is plain Tuya_ReportCachedDps.
 */
void Tuya_ReportCachedDpsWithAck(void);

/* The push from Tuya_ReportCachedDpsWithAck has been answered. False before
 * any such push in this window. */
bool Tuya_ReportAnswered(void);

/* ...and the answer was not a refusal. */
bool Tuya_ReportConfirmed(void);

/* The module's answer to one synchronous report: 1 taken, 0 refused. Called
 * by the SDK (get_upload_syn_result in ../../../tuya/protocol.c), from inside
 * Tuya_Service(). */
void Tuya_OnSyncResult(uint8_t result);
