#pragma once

// The Wi-Fi button's debounce, one millisecond sample at a time.
//
// In the BSP rather than the application because it has to run while the press
// is happening, whatever the MCU is doing then -- asleep, reading the sensor, or
// minutes into a Wi-Fi window -- and only the board's 1 ms tick is there for all
// of those (bsp_stm32.c). Until 2026-11-12 the application judged the level
// itself, once 50 ms after a cycle started, and a press made while a window was
// open was long over by then. Re-pairing a board right after a flash, three of
// about four presses opened no window.
//
// Here rather than inside bsp_stm32.c so the host tests can feed it samples
// (button_debounce_test). Header-only, so the CubeIDE project needs no new
// source folder: bsp/ is already on its include path.

#include <stdbool.h>
#include <stdint.h>

/* How long the pin has to read "pressed" without a break to count as a press.
 * Contact bounce is over in a few milliseconds and a noise pulse on the weak
 * internal pull-up in microseconds; a person's press, even a quick tap, lasts
 * 50 ms or more. */
#define BUTTON_DEBOUNCE_MS 20u

/* One 1 ms sample: `pressed` is the pin's level. `*low_ms` is the debouncer's
 * whole state, how long the pin has read pressed without a break, up to
 * BUTTON_DEBOUNCE_MS where it stops. Returns true on the one sample that
 * completes a press.
 *
 * Stopping at BUTTON_DEBOUNCE_MS is what makes a held -- or stuck -- button one
 * press: it cannot complete again until a sample reads released and starts the
 * count over. Starting the state at BUTTON_DEBOUNCE_MS means the same for a
 * button that is already down when the count begins.
 */
static inline bool ButtonDebounce_Sample(uint8_t* low_ms, bool pressed)
{
    if (!pressed)
    {
        *low_ms = 0u;

        return false;
    }

    if (*low_ms >= BUTTON_DEBOUNCE_MS)
    {
        return false;
    }

    (*low_ms)++;

    return (*low_ms == BUTTON_DEBOUNCE_MS);
}
