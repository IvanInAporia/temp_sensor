#pragma once

// Board Support Package (BSP) -- temp_sensor
//
// This header is the portability boundary. Everything above it (libs/src/*,
// temp_sensor_main/, the ported Tuya SDK) is board-agnostic C that compiles
// unchanged for the host test harness; everything below it is one
// implementation per board:
//
//   temp_sensor_STM32L010F4/Core/Src/bsp_stm32.c   the v2 PCB
//   libs/test/mocks/bsp_mock.c                     the host tests
//
// Changing the hardware means writing one new file against this header. Keep it
// that way: nothing above this line may include "main.h", a HAL header or a
// CMSIS header, and no pin number, peripheral handle or register name belongs
// on this side of it.

#include <stdbool.h>
#include <stdint.h>

/* Why the MCU last started. The values go out in the diagnostic DP
 * (diag_record.h), so append rather than reorder.
 */
typedef enum {
    BSP_Reset_None,      /* no flag set: should not happen */
    BSP_Reset_LowPower,
    BSP_Reset_IWatchdog, /* the watchdog: something stopped making progress */
    BSP_Reset_Software,
    BSP_Reset_PowerOn,   /* power-on or brown-out: a battery swap, or a sag deep enough */
    BSP_Reset_Pin,       /* the Reset button, or a debugger */
    BSP_Reset_Fault,     /* a HardFault or a failed HAL call, turned into a reset */
} BSP_ResetType;

/* Callback function type for any BSP operation that needs a callback. Runs in
 * interrupt context.
 */
typedef void (*BSP_void_callback_t)(void);

/* Bring the board up. Call once, before anything else here.
 */
void BSP_Init(void);

/* --- Time, idling and sleep -------------------------------------------------
 *
 * Two very different waits, and the difference is the whole power budget:
 *
 *   BSP_Idle()     parks the core for microseconds-to-milliseconds while the
 *                  Wi-Fi module is powered and talking. The module is drawing
 *                  tens of milliamps at that point, so the core's own
 *                  consumption does not matter and everything stays running.
 *   BSP_McuSleep() is the between-cycles sleep, minutes long, with the module
 *                  unpowered. Here the core IS the budget, so the implementation
 *                  is expected to reach the deepest state that still keeps SRAM
 *                  and the wake sources.
 */

/* Milliseconds since boot. Free-running, wraps every ~49 days; always compare
 * differences (now - then), never absolute values.
 *
 * The clock does not run on its own across BSP_McuSleep() -- the core is
 * stopped -- so the implementation folds the slept time back in before
 * returning. The value therefore stays a usable wall clock from one cycle to
 * the next.
 */
uint32_t BSP_GetTimeMs(void);

/* Busy-wait. Only for the short hardware settling delays the drivers need
 * (sensor conversion, rail turn-on); never for pacing the cycle.
 */
void BSP_DelayMs(uint32_t ms);

/* Park the core until the next interrupt, without touching clocks or
 * peripherals, and return as soon as anything fires.
 *
 * This is what the blocking waits in the cycle spin on (Tuya_WaitUntil), so the
 * MCU is not burning a millisecond of run time per poll while the module boots.
 * The implementation must guarantee a bounded return even when no peripheral
 * interrupt arrives -- a 1 ms tick is enough -- because the callers treat this
 * as "wait a little, then re-test my condition" and would otherwise stall on
 * the classic check-then-sleep race.
 */
void BSP_Idle(void);

/* Sleep the microcontroller for minimum power usage, for up to
 * `sleep_time_ms`, and return the milliseconds actually slept.
 *
 * The return value is not decoration: the Wi-Fi button is a wake source, so a
 * sleep can end early, and the caller paces the next cycle off what really
 * elapsed rather than off what it asked for (temp_sensor_main.c). An
 * implementation with no early wake source may simply return `sleep_time_ms`.
 *
 * BSP_GetTimeMs() is advanced by the same amount before this returns.
 *
 * Keeps the watchdog fed itself, however long the sleep: the caller cannot,
 * since it is not running. A sleep that never ends -- a wakeup that never
 * fires -- is one of the things the watchdog is there to catch.
 */
uint32_t BSP_McuSleep(uint32_t sleep_time_ms);

/* Reset the microcontroller.
 */
void BSP_McuReset(void);

/* Why the MCU last started. Latched at BSP_Init(), so it stays readable for the
 * whole run rather than only until something clears the flags.
 */
BSP_ResetType BSP_GetResetReason(void);

/* --- Watchdog ---------------------------------------------------------------
 *
 * Started by BSP_Init and never stopped. Nothing in the firmware is meant to
 * wait without a deadline, so anything that stops calling this for long enough
 * -- a hang in a vendor library, a fault loop, a clock that never comes back
 * -- resets the board, and BSP_GetResetReason says so on the next run.
 *
 * The timeout is the implementation's (around 20-40 s on the STM32). Callers
 * feed it from every loop that waits awake; BSP_McuSleep feeds it through the
 * sleep.
 */
void BSP_Watchdog_Refresh(void);

/* --- Retained words ---------------------------------------------------------
 *
 * A few words that survive any reset that keeps the power on: the Reset button,
 * the watchdog, a fault. After a power-on they hold anything, usually zero, so
 * whoever stores something here must be able to tell (diag_record.c checks a
 * checksum).
 */
#define BSP_RETAINED_WORDS 3u

uint32_t BSP_Retained_Read(uint8_t index);
void     BSP_Retained_Write(uint8_t index, uint32_t value);

/* --- Sensor bus -------------------------------------------------------------
 *
 * The HDC3020 is the only device on it, but the address is still a parameter:
 * the part offers four addresses by strapping ADDR/ADDR1, and a board that
 * straps them differently should need no driver change. Both calls block; the
 * transfers are a handful of bytes at 100 kHz.
 *
 * `address` is the 7-bit address, unshifted (0x44 on this board -- both strap
 * pins to ground). Both return false on NACK, bus error or timeout.
 */
bool BSP_Sensor_I2cWrite(uint8_t address, const uint8_t* data, uint16_t len);
bool BSP_Sensor_I2cRead(uint8_t address, uint8_t* data, uint16_t len);

/* Put the bus back to idle after a transfer failed: reset the controller, and
 * clock out a device left holding SDA low partway through a byte.
 *
 * Either can outlast the transfer that caused it. A glitch on the lines can
 * leave the controller believing the bus is busy, and every transfer after
 * that fails the same way until something resets it; a device reset or
 * interrupted mid-byte waits for clocks that never come. Without this, the
 * first sensor failure could be the last reading.
 */
void BSP_Sensor_RecoverBus(void);

/* --- Wi-Fi module (Tuya T3-3S) ----------------------------------------------
 *
 * The module is an appliance behind a power gate. There is no reset line and no
 * status line: CEN is pulled up on the module and only reaches a header, so on
 * this board "asleep" means unpowered and "awake" means a cold boot. Everything
 * else -- pairing state, cloud state -- comes back over the UART and is the
 * Tuya SDK's business, not the BSP's.
 */

/* Close the module's supply gate and bring its UART up, in that order.
 *
 * The order matters and is the implementation's responsibility: the MCU's TX
 * pin idles high, push-pull, and driving it into an unpowered module back-feeds
 * the module's supply through its input protection. Power first, drive second.
 */
void BSP_Wifi_PowerOn(void);

/* Park the UART pins and open the supply gate, in that order -- the inverse of
 * the above, for the same reason. Leaves nothing driving the module's rail.
 */
void BSP_Wifi_PowerOff(void);

/* Send one byte to the module, blocking until it has left the shift register.
 *
 * Byte at a time because that is the interface the Tuya MCU SDK asks for
 * (protocol.c, uart_transmit_output). At 9600 baud a byte is ~1 ms, so a whole
 * SDK frame blocks the caller for 7-16 ms. That is affordable here only because
 * nothing else needs the CPU while the module is powered.
 */
void BSP_Wifi_TransmitByte(uint8_t value);

/* Arm a one-byte interrupt receive. `cb` runs in interrupt context when the
 * byte lands, and must re-arm if it wants the next one.
 *
 * One byte at a time, permanently armed, is what the SDK's ring buffer expects
 * -- it is handed every byte through uart_receive_input(). A gap in the arming
 * loses bytes in silence, with no UART error raised, so the re-arm belongs at
 * the top of the callback, ahead of any parsing.
 */
void BSP_Wifi_Receive(uint8_t* received_byte, BSP_void_callback_t cb);

/* Cancel a receive armed by BSP_Wifi_Receive; the callback will not run.
 *
 * Pairs with power-down: an arm left standing over a power cycle makes the next
 * BSP_Wifi_Receive fail as busy, and the module then boots into a receiver that
 * is not listening.
 */
void BSP_Wifi_AbortReceive(void);

/* Set the callback invoked when a UART error (overrun, framing, noise) aborts
 * an armed receive.
 *
 * Needed because an aborting error ends the receive WITHOUT running the
 * completion callback, so the re-arm at the top of that callback never happens
 * and the link stays deaf for the rest of the window. The T3-3S guarantees at
 * least one such error per boot: its ROM loader prints on TX at a different
 * baud rate before the application firmware takes the port to 9600.
 */
void BSP_Wifi_SetRxErrorCallback(BSP_void_callback_t cb);

/* Consume the latched "button went down" event: true if a press edge has been
 * seen since the last call, and clears the latch either way.
 *
 * Latched rather than polled because the press is also what ends a
 * BSP_McuSleep -- it has to be, or the button would do nothing for the minutes
 * between cycles -- and the application only gets to look once it is running
 * again. An edge that was not latched somewhere would be gone by then.
 */
bool BSP_Wifi_TakeButtonEvent(void);

/* True while the button is held down.
 *
 * The level behind the event above, so the application can confirm that a
 * latched edge was a real press and not contact bounce or a noise pulse on a
 * pin held up only by the MCU's weak internal pull-up. Debouncing is the
 * application's job (temp_sensor_main.c), not the BSP's.
 */
bool BSP_Wifi_ButtonIsPressed(void);

/* --- Battery ----------------------------------------------------------------
 *
 * One NiMH cell straight into the boost converter's input, so this is measured
 * below the 3.3 V rail, not on it.
 */

/* Cell voltage in millivolts, or 0 if it could not be measured.
 *
 * Read it with the Wi-Fi rail OFF. A single cell feeding a 3.3 V boost delivers
 * roughly three times the module's current at the cell -- hundreds of
 * milliamps -- so a reading taken during a report measures the sag, not the
 * charge.
 */
uint16_t BSP_Battery_ReadMv(void);

/* Start tracking the lowest cell voltage, for the length of a report window.
 *
 * The other half of BSP_Battery_ReadMv: this one is meant to measure the sag.
 * The module's transmit bursts pull the cell down for a millisecond or less at
 * a time, far too briefly for a reading taken from the cycle to land on one,
 * and how far down they pull it is what says whether the cell -- and the
 * holder it sits in -- can still power a report.
 *
 * So the implementation samples on its own, continuously, from an interrupt,
 * until BSP_Battery_StopSagMonitor. It runs through BSP_Idle, which is the
 * point of it. It never runs through BSP_McuSleep: that stops it first, so a
 * monitor left running cannot keep the converter and its interrupt alive into
 * the sleep.
 *
 * BSP_Battery_ReadMv is not available while it runs.
 */
void BSP_Battery_StartSagMonitor(void);

/* What the monitor saw over a window, in millivolts, each 0 if it saw nothing.
 */
typedef struct {
    /* The lowest single reading. Often one narrow spike, which the 3.3 V
     * rail's capacitors carry the module through. */
    uint16_t min_mv;

    /* The lowest point of a running average over about a millisecond: how far
     * the transmit bursts pull the cell for long enough to matter. See
     * Battery_SagSamples in battery.h. */
    uint16_t average_min_mv;
} BSP_SagReading;

/* Stop the monitor and return what it saw. Safe to call on a monitor that is
 * not running: that returns zeroes.
 */
BSP_SagReading BSP_Battery_StopSagMonitor(void);

/* --- Spare I/O --------------------------------------------------------------
 *
 * PA5 on this board, brought out to a test point and otherwise unused. Kept in
 * the BSP so bench instrumentation (scoping a cycle's phases) needs no new
 * plumbing, and stubbed on a board that has no such pin.
 */
void BSP_Spare_Set(bool on);
