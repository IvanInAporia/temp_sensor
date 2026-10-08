#pragma once

// Everything the cycle needs from the Tuya T3-3S, in the shape the cycle wants
// to read: power it up, wait for it, hand it four numbers, put it away.
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

/* Put the module into pairing mode. Requires Tuya_IsAlive().
 *
 * Sends the reset command, on which the module restarts into pairing over
 * Bluetooth or as an AP hotspot -- whichever the phone uses. The module
 * reports its state afresh once it is back, so Tuya_IsCloudConnected() is
 * false until it has actually been paired, even if it was online a moment
 * before.
 *
 * The product runs Tuya's anti-misoperation mode (CONFIG_MODE in
 * ../../../tuya/protocol.h): a module that is not paired within three minutes
 * goes back to the network it already had, so a stray press loses nothing.
 */
void Tuya_StartPairing(void);

/* Cache one complete set of readings. Does not transmit.
 *
 * Cached rather than sent straight through because the module asks for the
 * full set itself, on its own schedule, whenever it reconnects -- and it
 * reconnects on every window. The SDK calls all_data_update() for that, from
 * inside Tuya_Service(), at a moment no caller controls. There has to be an
 * answer ready.
 */
void Tuya_SetDps(int16_t temp_c10,
                 uint8_t rh_pct,
                 uint8_t battery_pct,
                 Battery_State battery_state);

/* Push the cached readings to the module.
 *
 * Called by the cycle once the cloud is up, and by the SDK itself
 * (all_data_update in ../../../tuya/protocol.c) whenever the module asks for
 * everything. Does nothing until Tuya_SetDps() has supplied a set, so a query
 * arriving before the first measurement reports nothing rather than zeroes.
 */
void Tuya_ReportCachedDps(void);
