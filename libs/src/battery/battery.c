#include "battery.h"

/* Discharge curve for one NiMH AA at the load this board actually presents.
 *
 * ***Apart from the full anchor, these numbers are a datasheet-shaped estimate,
 * not bench data.*** They put
 * the curve in the right place and give the gauge a shape to interpolate along;
 * a discharge run on the real board should replace them, and the table is laid
 * out as anchors precisely so that is an edit and not a rewrite.
 *
 * Two things about NiMH drive the shape:
 *
 *  - The plateau is almost flat. Roughly 70 % of the capacity sits between
 *    1.30 V and 1.24 V, so 60 mV has to stretch over most of the scale and the
 *    anchors are packed tightly there. Voltage is a poor gauge for this
 *    chemistry and the middle of this table is the least trustworthy part of
 *    it; that is the chemistry, not the code.
 *  - The cell is measured almost unloaded. Average draw between reports is
 *    microamps, so these are near open-circuit values. They do not apply to a
 *    reading taken while the Wi-Fi module is powered -- see BSP_Battery_ReadMv.
 *
 * The full anchor is field data, not datasheet: a charged cell settles there
 * once its surface charge has gone. Three cells -- two Eneloops and a Duracell
 * -- all read 1.32 V on a DMM after resting a few days off the charger, and
 * 1322 to 1335 mV on this ADC. Straight off the charger a cell reads up to
 * 1.40 V for a few hours; that is the same full cell, so it reads 100 too. At
 * the 1.40 V the anchor used to be, the gauge never showed 100 % at all.
 *
 * The empty anchor is 1.10 V, which is where the cell is done, NOT where the
 * board stops working: the TPS61021A boosts from 0.5 V, so the MCU keeps
 * running well past 0 %. That is deliberate. A gauge that read 0 % only once
 * the firmware was about to die would never get a last report out. A cell
 * whose sag stops the reports earlier moves its own 0 % point up -- see
 * Battery_UsablePercentFromMv.
 */
typedef struct {
    uint16_t mv;
    uint8_t  percent;
} BatteryAnchor;

static const BatteryAnchor kCurve[] = {
    { 1320u, 100u }, /* full, rested: see above */
    { 1290u,  70u },
    { 1265u,  50u }, /* middle of the plateau */
    { 1240u,  30u },
    { 1200u,  15u },
    { 1150u,   5u }, /* the knee */
    { 1100u,   0u },
};

#define CURVE_LEN (sizeof(kCurve) / sizeof(kCurve[0]))

/* Percentage thresholds for the three-level DP, applied to the gauge
 * (Battery_UsablePercentFromMv): the share of the charge the cell can still
 * deliver to a report. A healthy Eneloop turns low at about 1240 mV resting,
 * with ~18 % of its full charge left above its 0 % point -- about seven weeks
 * at 6 reports a day, if the curve's anchors hold. A cell that sags more turns
 * low at a higher voltage. Kept at 20 % until a measured discharge checks the
 * curve: a late "low" costs missed reports, an early one only a recharge.
 */
#define STATE_LOW_MAX_PERCENT    20u
#define STATE_MIDDLE_MAX_PERCENT 60u

uint8_t Battery_PercentFromMv(uint16_t mv)
{
    /* Above the full anchor: a cell straight off the charger carries a surface
     * charge that takes it to 1.40 V for a few hours. Clamping is the whole
     * handling -- extrapolating would report more than 100 %. */
    if (mv >= kCurve[0].mv)
    {
        return kCurve[0].percent;
    }

    for (unsigned i = 1u; i < CURVE_LEN; i++)
    {
        if (mv >= kCurve[i].mv)
        {
            /* Linear between the two anchors that bracket the reading. Both
             * spans are small and positive, so this stays in 16-bit range with
             * room over: the largest product is 30 mV x 30 points. */
            uint16_t span_mv      = (uint16_t)(kCurve[i - 1u].mv - kCurve[i].mv);
            uint16_t span_percent = (uint16_t)(kCurve[i - 1u].percent - kCurve[i].percent);
            uint16_t above_mv     = (uint16_t)(mv - kCurve[i].mv);

            return (uint8_t)(kCurve[i].percent +
                             (uint16_t)(((uint32_t)above_mv * span_percent + (span_mv / 2u)) / span_mv));
        }
    }

    /* Below the empty anchor, and the 0 mV the BSP returns when it could not
     * measure at all. Both read flat, which is the direction that cannot
     * mislead. */
    return 0u;
}

Battery_State Battery_StateFromPercent(uint8_t percent)
{
    if (percent <= STATE_LOW_MAX_PERCENT)
    {
        return Battery_State_Low;
    }

    if (percent <= STATE_MIDDLE_MAX_PERCENT)
    {
        return Battery_State_Middle;
    }

    return Battery_State_High;
}

/* --- The sag: what the cell can still deliver -------------------------------
 *
 * The boost draws roughly constant power P from a cell whose path -- cell,
 * holder, traces -- has resistance R. So at the bottom of a transmit burst
 *
 *     P.R = (Vrest - Vmin) . Vmin
 *
 * with no current measurement needed. The most the path can deliver is at
 * Vmin = Vrest / 2; once Vrest^2 < 4.P.R it delivers less than the boost asks
 * for, and the input collapses however much charge is left. So the cell stops
 * powering reports at a resting voltage of
 *
 *     Vc = 2.sqrt(P.R)
 *
 * its collapse voltage. Measured on the office board: a healthy Eneloop at
 * 1.286 V pulled to 0.945 V, Vc ~1.135 V. The failed Eneloop on the same board
 * at 1.29 V pulled to 0.58 V -- below half, so already past the most it can
 * deliver -- which matches the 1.28 V it stopped working at on the front board.
 */

/* The 0 % point sits this far above the collapse voltage, so the gauge reads
 * empty while reports still get out rather than once they have stopped. It
 * covers about a 10 % error in P.R. */
#define COLLAPSE_MARGIN_MV 50u

/* From this collapse voltage up, the cell sags far more than a healthy one.
 * Midway between the office board's healthy Eneloop (~1135 mV) and the failed
 * one (~1285-1290 mV): a cell like that stops powering reports at a resting
 * voltage where a healthy one still has a good part of its charge. The
 * collapse voltage never exceeds the resting voltage, so a cell can only get
 * here while it still rests at 1.21 V or more -- an ordinary cell running flat
 * does not trip it. */
#define DAMAGED_COLLAPSE_MV 1210u

/* The trend weighs each window at 1/4, and no window may pull it further than
 * this before the weighting -- at most 15 mV a window. About three times the
 * variation expected between windows (~+-20 mV); the DP 101 history will show
 * the real one. */
#define TREND_STEP_LIMIT_MV 60

/* A cell in use never rises this much between two cycles; a charged cell
 * replacing a used one does. */
#define NEW_CELL_RISE_MV 50u

/* No AA rests above this: a fresh lithium cell is ~1.8 V, a NiMH off the
 * charger ~1.45 V. A resting reading above it is a bad reference read, not a
 * cell, and its window gives no sample. Two in a row would otherwise set the
 * trend to them -- and at 15 mV a window, a good cell would read damaged for
 * weeks. It also keeps every value downstream small enough for the trend's
 * 16-bit quarter-millivolts. */
#define CELL_MAX_MV 2000u

uint16_t Battery_CollapseMv(uint16_t rest_mv, uint16_t min_mv)
{
    /* No sag seen, or nothing to measure it against -- a 0 from either reading
     * included, and a resting reading no cell gives. */
    if ((min_mv == 0u) || (min_mv >= rest_mv) || (rest_mv > CELL_MAX_MV))
    {
        return 0u;
    }

    /* At or past the most the path can deliver. P.R turns back down from here:
     * taken at face value, a deeper sag would read as a healthier cell. */
    if (((uint32_t) min_mv * 2u) <= rest_mv)
    {
        return rest_mv;
    }

    /* P.R in mV^2: at most (rest / 2)^2, so 32 bits with room to spare.
     *
     * Then 2.sqrt(P.R) without the square root, by its tangent at 1.2 V:
     * 600 + P.R / 600. The tangent of a concave curve lies above it, so this
     * only ever reads high -- the cautious direction -- by (Vc - 1200)^2 / 2400
     * mV: 4 mV at most between 1.10 and 1.30 V, 9 mV at 1.05 and 1.35 V. Below
     * ~1.05 V the value no longer moves the gauge. */
    uint32_t pr_mv2      = (uint32_t)(rest_mv - min_mv) * min_mv;
    uint32_t collapse_mv = 600u + (pr_mv2 / 600u);

    /* Just short of half the resting voltage the tangent overshoots it by a
     * few mV. Capped, so the value never drops on the way into the clamp
     * above: a deeper sag must never read better. */
    if (collapse_mv > rest_mv)
    {
        collapse_mv = rest_mv;
    }

    return (uint16_t) collapse_mv;
}

void Battery_SagTrendReset(Battery_SagTrend* trend)
{
    trend->quarter_mv = 0u;
    trend->last_mv    = 0u;
}

void Battery_SagTrendAdd(Battery_SagTrend* trend, uint16_t collapse_mv)
{
    if (collapse_mv == 0u)
    {
        return;
    }

    /* A window counts only once the next one confirms it. One deep window
     * says nothing a disturbed one would not: on 2026-11-05 a healthy cell's
     * first window, likely while it was still being seated, went as deep as
     * the failed cell's every window. What tells the failed cell apart is
     * that it does it every time. So of each pair of windows in a row, the
     * lower collapse voltage goes in. A real change shows one window later. */
    uint16_t previous_mv = trend->last_mv;

    trend->last_mv = collapse_mv;

    if (previous_mv == 0u)
    {
        /* The cell's first window, with nothing to pair it with yet. */
        return;
    }

    if (previous_mv < collapse_mv)
    {
        collapse_mv = previous_mv;
    }

    if (trend->quarter_mv == 0u)
    {
        /* The cell's first pair, with nothing to weigh it against. Taken
         * whole, so a cell that is bad from the start reads so on its third
         * report. */
        trend->quarter_mv = (uint16_t)(collapse_mv * 4u);
        return;
    }

    int32_t step_mv = (int32_t) collapse_mv - (int32_t) Battery_SagTrendMv(trend);

    if (step_mv > TREND_STEP_LIMIT_MV)
    {
        step_mv = TREND_STEP_LIMIT_MV;
    }
    else if (step_mv < -TREND_STEP_LIMIT_MV)
    {
        step_mv = -TREND_STEP_LIMIT_MV;
    }

    /* Added to quarter-millivolts: a quarter of the step, without dropping the
     * fraction. Samples are never below 600 mV, so the trend cannot fall to
     * the 0 that means "empty". */
    trend->quarter_mv = (uint16_t)((int32_t) trend->quarter_mv + step_mv);
}

uint16_t Battery_SagTrendMv(const Battery_SagTrend* trend)
{
    return (uint16_t)((trend->quarter_mv + 2u) / 4u);
}

uint8_t Battery_UsablePercentFromMv(uint16_t rest_mv, uint16_t collapse_mv)
{
    uint8_t percent = Battery_PercentFromMv(rest_mv);

    if (collapse_mv == 0u)
    {
        return percent;
    }

    uint8_t empty_percent = Battery_PercentFromMv((uint16_t)(collapse_mv + COLLAPSE_MARGIN_MV));

    if (empty_percent == 0u)
    {
        /* The 0 % point is still at or below the curve's own. */
        return percent;
    }

    if (percent <= empty_percent)
    {
        /* Including an empty point at the top of the curve, which leaves no
         * span to divide by. */
        return 0u;
    }

    /* The rest of the curve, stretched back over 0..100: a full cell reads 100
     * however far its sag moved the 0 % point. How much of a normal cell's
     * runtime that is, is for Battery_IsDamaged to say. */
    uint16_t span_percent = (uint16_t)(100u - empty_percent);

    return (uint8_t)((((uint16_t)(percent - empty_percent) * 100u) + (span_percent / 2u)) / span_percent);
}

bool Battery_IsDamaged(uint16_t collapse_mv)
{
    return collapse_mv >= DAMAGED_COLLAPSE_MV;
}

bool Battery_IsNewCell(uint16_t previous_rest_mv, uint16_t rest_mv)
{
    return (previous_rest_mv != 0u) && ((uint32_t) rest_mv >= ((uint32_t) previous_rest_mv + NEW_CELL_RISE_MV));
}
