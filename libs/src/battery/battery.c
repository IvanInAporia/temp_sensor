#include "battery.h"

/* Discharge curve for one NiMH AA at the load this board actually presents.
 *
 * ***These numbers are a datasheet-shaped estimate, not bench data.*** They put
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
 * The empty anchor is 1.10 V, which is where the cell is done, NOT where the
 * board stops working: the TPS61021A boosts from 0.5 V, so the MCU keeps
 * running well past 0 %. That is deliberate. A gauge that read 0 % only once
 * the firmware was about to die would never get a last report out.
 */
typedef struct {
    uint16_t mv;
    uint8_t  percent;
} BatteryAnchor;

static const BatteryAnchor kCurve[] = {
    { 1400u, 100u }, /* rested after a full charge */
    { 1330u,  90u },
    { 1290u,  70u },
    { 1265u,  50u }, /* middle of the plateau */
    { 1240u,  30u },
    { 1200u,  15u },
    { 1150u,   5u }, /* the knee */
    { 1100u,   0u },
};

#define CURVE_LEN (sizeof(kCurve) / sizeof(kCurve[0]))

/* Percentage thresholds for the three-level DP. Chosen so "low" lands on the
 * knee, where the remaining runtime really does start shrinking, rather than
 * somewhere on the plateau where it would mean almost nothing.
 */
#define STATE_LOW_MAX_PERCENT    20u
#define STATE_MIDDLE_MAX_PERCENT 60u

uint8_t Battery_PercentFromMv(uint16_t mv)
{
    /* Above the full anchor: a cell straight off the charger carries a surface
     * charge well over 1.4 V that disappears within the hour. Clamping is the
     * whole handling -- extrapolating would report more than 100 %. */
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
             * room over: the widest span is 70 mV x 20 points. */
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
