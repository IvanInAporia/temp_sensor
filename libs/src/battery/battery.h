#pragma once

// Battery gauge for the single NiMH cell that feeds the boost converter.
//
// Pure functions over a millivolt reading, so the curve is something the host
// tests can pin down rather than something only a bench discharge can check.
// Taking the reading is the BSP's job (BSP_Battery_ReadMv).

#include <stdbool.h>
#include <stdint.h>

/* Tuya DP 3 (battery_state) is an enum with exactly these three values, in
 * this order. The wire values are the enum indices, so do not reorder.
 */
typedef enum {
    Battery_State_Low    = 0,
    Battery_State_Middle = 1,
    Battery_State_High   = 2,
} Battery_State;

/* State of charge, 0..100, from a cell voltage in millivolts.
 *
 * Clamps at both ends: anything at or above the full-charge anchor reads 100,
 * anything at or below the empty anchor reads 0. A reading of 0 mV (the BSP's
 * "could not measure") therefore reads 0 %, which is the safe direction -- it
 * cannot make a flat cell look charged.
 */
uint8_t Battery_PercentFromMv(uint16_t mv);

/* The three-level indicator the Tuya DP carries, from a percentage.
 *
 * Deliberately stateless and without hysteresis: a report only goes out when
 * the temperature moved, so consecutive reports are minutes to hours apart and
 * a level that sits exactly on a threshold will alternate slowly rather than
 * flap. If a bench discharge shows that alternation is worth suppressing, the
 * fix is hysteresis here, which needs a caller-held previous value.
 */
Battery_State Battery_StateFromPercent(uint8_t percent);
