#pragma once

// Battery gauge for the single NiMH cell that feeds the boost converter.
//
// Pure functions over millivolt readings, so the curve is something the host
// tests can pin down rather than something only a bench discharge can check.
// Taking the readings is the BSP's job (BSP_Battery_ReadMv for the cell at
// rest, BSP_Battery_StopSagMonitor for its lowest under load).

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

/* --- The sag: what the cell can still deliver -------------------------------
 *
 * A cell can have plenty of charge and still be unable to power a report: the
 * boost draws roughly constant power, and a cell whose path has too much
 * resistance collapses under it. How far a window pulls the cell down says
 * where that happens. See battery.c for the derivation.
 */

/* The resting voltage, in mV, below which this cell on this board could no
 * longer power a transmit burst -- its collapse voltage -- from one window:
 * the resting voltage just before it and the lowest the window pulled the cell
 * to. 0 when there is nothing to go on: no sag, a 0 for either reading, or a
 * resting reading above 2 V, which no AA gives -- so the result is at most
 * 2000 mV.
 *
 * A cell pulled to half its resting voltage or below is already past the most
 * it can deliver, and reads as collapsing at its resting voltage. Otherwise
 * the value reads at most 9 mV high between 1.05 and 1.35 V, never low.
 */
uint16_t Battery_CollapseMv(uint16_t rest_mv, uint16_t min_mv);

/* A running estimate of a cell's collapse voltage over its windows, which no
 * single window can raise.
 *
 * Each window is paired with the one before it, and only the lower of the two
 * -- the less alarming -- goes in: a lone deep window is never seen, while two
 * in a row are. So the trend starts with a cell's second window, from the lower
 * of its first two. After that each one is weighed at 1/4 and pulls it 15 mV at
 * most.
 *
 * The caller resets it when the cell changes. 0 mV means fewer than two windows
 * have been measured on this cell.
 */
typedef struct {
    uint16_t quarter_mv;
    uint16_t last_mv;    /* the previous window's collapse voltage, 0 for none */
} Battery_SagTrend;

void     Battery_SagTrendReset(Battery_SagTrend* trend);
/* collapse_mv as Battery_CollapseMv returns it; 0 is ignored. */
void     Battery_SagTrendAdd(Battery_SagTrend* trend, uint16_t collapse_mv);
uint16_t Battery_SagTrendMv(const Battery_SagTrend* trend);

/* State of charge, 0..100, of what the cell can still deliver to a report.
 *
 * Battery_PercentFromMv with the 0 % point moved up to 50 mV above the collapse
 * voltage, and the rest of the curve stretched back over the scale -- so a
 * full cell still reads 100, and 0 % arrives while reports still get out. A
 * collapse voltage of 0 (not known) or low enough to leave the 0 % point where
 * it was gives Battery_PercentFromMv unchanged.
 */
uint8_t Battery_UsablePercentFromMv(uint16_t rest_mv, uint16_t collapse_mv);

/* Whether a cell with this collapse voltage sags far more than a healthy one:
 * probably old or damaged, or held by a poor contact. False for 0.
 */
bool Battery_IsDamaged(uint16_t collapse_mv);

/* Whether the resting voltage rose enough since the previous reading that the
 * cell must have been swapped. A cell in use never rises like that. The MCU
 * normally resets on a swap anyway, but the 3.3 V rail's capacitors can carry it
 * through a quick one. False if either reading is 0.
 */
bool Battery_IsNewCell(uint16_t previous_rest_mv, uint16_t rest_mv);
