#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <iterator>

extern "C" {
#include "battery.h"
}

namespace {

// How to read these tests
//
// Each section opens with a table: one row per input, the output it must give
// and, where it matters, where the row comes from. To see what the gauge does
// with a reading, find its row. The loops that follow a table check a property
// over every input in range, which no table can list.
//
// The cells the rows come from, all measured on the office board:
//   - a healthy Eneloop: resting at 1286 mV and pulled to 945 mV by the
//     transmit bursts (DP 101), or 1300 -> 960 mV on the scope;
//   - the Eneloop that stopped working on the front board: resting at 1290 mV
//     and pulled to 580 mV, below half;
//   - three charged cells, rested a few days: 1322 to 1335 mV.

constexpr Battery_State kLow    = Battery_State_Low;
constexpr Battery_State kMiddle = Battery_State_Middle;
constexpr Battery_State kHigh   = Battery_State_High;

// The functions under test that return a percentage, as an int: a failure then
// prints a number rather than a character.
int PercentAtRest(uint16_t rest_mv)
{
    return Battery_PercentFromMv(rest_mv);
}

int PercentUnderLoad(uint16_t rest_mv, uint16_t collapse_mv)
{
    return Battery_UsablePercentFromMv(rest_mv, collapse_mv);
}

// =========================================================================
// The plain curve: resting voltage to percentage
// =========================================================================
//
// Battery_PercentFromMv, and the level DP 3 shows for it. The full anchor is
// field data; the rest of the curve is a datasheet-shaped estimate for one
// NiMH cell, to be replaced by a bench discharge.

struct PlainRow {
    uint16_t      rest_mv;
    int           percent;
    Battery_State level;
    const char*   note;
};

const PlainRow kPlainCurve[] = {
    // rest mV    %   level
    {     0,    0, kLow,    "the BSP could not measure: reads flat, never charged" },
    {   900,    0, kLow,    "below the empty anchor" },
    {  1099,    0, kLow,    "" },
    {  1100,    0, kLow,    "anchor: empty" },
    {  1104,    0, kLow,    "rounded down" },
    {  1105,    1, kLow,    "rounded up: the first 1 %" },
    {  1125,    3, kLow,    "" },
    {  1150,    5, kLow,    "anchor: the knee" },
    {  1175,   10, kLow,    "" },
    {  1200,   15, kLow,    "anchor" },
    {  1214,   20, kLow,    "the highest reading still Low" },
    {  1215,   21, kMiddle, "" },
    {  1240,   30, kMiddle, "anchor: the bottom of the plateau, not yet Low" },
    {  1252,   40, kMiddle, "" },
    {  1265,   50, kMiddle, "anchor: the middle of the plateau" },
    {  1278,   60, kMiddle, "the highest reading still Middle" },
    {  1279,   61, kHigh,   "" },
    {  1286,   67, kHigh,   "the healthy Eneloop today" },
    {  1290,   70, kHigh,   "anchor" },
    {  1300,   80, kHigh,   "" },
    {  1310,   90, kHigh,   "" },
    {  1319,   99, kHigh,   "" },
    {  1320,  100, kHigh,   "anchor: full, rested" },
    {  1322,  100, kHigh,   "Eneloop, front board, rested" },
    {  1330,  100, kHigh,   "Eneloop, office board, rested" },
    {  1335,  100, kHigh,   "Duracell, kitchen board, rested" },
    {  1400,  100, kHigh,   "straight off the charger: clamped, not extrapolated" },
    {  3300,  100, kHigh,   "" },
    { 65535,  100, kHigh,   "" },
};

TEST(BatteryTest, ThePlainCurve)
{
    for (const PlainRow& row : kPlainCurve)
    {
        int percent = PercentAtRest(row.rest_mv);

        EXPECT_EQ(row.percent, percent) << row.rest_mv << " mV: " << row.note;
        EXPECT_EQ(row.level, Battery_StateFromPercent(static_cast<uint8_t>(percent)))
            << row.rest_mv << " mV: " << row.note;
    }
}

TEST(BatteryTest, ThePlainCurveNeverGoesBackwardsOrPast100)
{
    // The property that matters most in the app: a cell that is discharging
    // must never read as gaining charge, whatever the table says.
    int previous = PercentAtRest(0);

    for (uint32_t mv = 1; mv <= UINT16_MAX; mv++)
    {
        int percent = PercentAtRest(static_cast<uint16_t>(mv));

        ASSERT_GE(percent, previous) << "went backwards at " << mv << " mV";
        ASSERT_LE(percent, 100) << "past 100 at " << mv << " mV";

        previous = percent;
    }
}

TEST(BatteryTest, ThePlainCurveSpendsMostOfItsScaleOnThePlateau)
{
    // NiMH holds roughly 1.30 V down to 1.24 V for most of its capacity, so
    // that 60 mV band has to carry a large share of the scale. If it did not,
    // the gauge would sit at 100 % for weeks and then collapse.
    EXPECT_GE(PercentAtRest(1300) - PercentAtRest(1240), 40);
}

// --- The level (DP 3) -------------------------------------------------------

struct LevelRow {
    int           percent;
    Battery_State level;
};

const LevelRow kLevels[] = {
    //  %   level
    {   0, kLow    },
    {  20, kLow    }, // the top of Low
    {  21, kMiddle },
    {  60, kMiddle }, // the top of Middle
    {  61, kHigh   },
    { 100, kHigh   },
};

TEST(BatteryTest, PercentageToLevel)
{
    for (const LevelRow& row : kLevels)
    {
        EXPECT_EQ(row.level, Battery_StateFromPercent(static_cast<uint8_t>(row.percent))) << row.percent << " %";
    }
}

TEST(BatteryTest, LevelsMatchTheTuyaWireCodes)
{
    // The DP is an enum whose wire value is the index of low / middle / high.
    // Reordering the enum would silently relabel every device in the field.
    EXPECT_EQ(0, static_cast<int>(Battery_State_Low));
    EXPECT_EQ(1, static_cast<int>(Battery_State_Middle));
    EXPECT_EQ(2, static_cast<int>(Battery_State_High));
}

// =========================================================================
// The collapse voltage: what one window says
// =========================================================================
//
// Battery_CollapseMv: the resting voltage below which the cell could no longer
// power a report, from its resting voltage and the lowest a window pulled it
// to. "exact" in a note is 2.sqrt(P.R), which the firmware approximates by a
// tangent at 1.2 V rather than take a square root.

struct CollapseRow {
    uint16_t    rest_mv;
    uint16_t    min_mv;
    uint16_t    collapse_mv;
    const char* note;
};

const CollapseRow kCollapse[] = {
    // rest   min  collapse
    { 1290,    0,     0, "the monitor measured nothing" },
    {    0,  900,     0, "the resting read failed" },
    { 1290, 1290,     0, "no sag" },
    { 1290, 1300,     0, "a minimum above the resting voltage" },
    { 2001,  945,     0, "a resting reading no AA gives: a bad reference read" },
    { 2000,  945,  2000, "the highest resting reading still taken as a cell" },
    { 1290, 1289,   602, "a 1 mV sag, exact 72: far from 1.2 V the tangent reads high, where it moves nothing" },
    { 1100,  800,  1000, "exact 980" },
    { 1400, 1100,  1150, "a cell off the charger pulled to 1.10 V, exact 1149" },
    { 1300,  960,  1144, "the healthy Eneloop on the scope, exact 1143" },
    { 1286,  945,  1137, "the healthy Eneloop from DP 101, exact 1135" },
    { 1290,  965,  1122, "exact 1120" },
    { 1390,  965,  1283, "the failed Eneloop recharged, if its resistance holds, exact 1281" },
    { 1290,  646,  1290, "just above half, exact 1290: the tangent's 1293 is capped" },
    { 1290,  645,  1290, "half: the most the path can deliver" },
    { 1290,  580,  1290, "the failed Eneloop: past half, so collapsing at its resting voltage" },
    { 1200,  500,  1200, "past half" },
    { 1290,    1,  1290, "pulled to nothing" },
};

TEST(BatteryTest, TheCollapseVoltage)
{
    for (const CollapseRow& row : kCollapse)
    {
        EXPECT_EQ(row.collapse_mv, Battery_CollapseMv(row.rest_mv, row.min_mv))
            << row.rest_mv << " mV pulled to " << row.min_mv << " mV: " << row.note;
    }
}

double ExactCollapseMv(uint16_t rest_mv, uint16_t min_mv)
{
    return 2.0 * std::sqrt(static_cast<double>(rest_mv - min_mv) * min_mv);
}

TEST(BatteryTest, TheApproximationNeverReadsLowAndStaysClose)
{
    // Above the true value everywhere (less up to 1 mV of integer division),
    // and within 9 mV of it wherever the value can move the gauge.
    for (uint16_t rest_mv = 1000; rest_mv <= 2000; rest_mv += 5)
    {
        for (uint16_t min_mv = static_cast<uint16_t>(rest_mv / 2 + 1); min_mv < rest_mv; min_mv++)
        {
            double exact  = ExactCollapseMv(rest_mv, min_mv);
            double approx = Battery_CollapseMv(rest_mv, min_mv);

            ASSERT_GE(approx + 1.0, exact) << rest_mv << " mV pulled to " << min_mv;

            if ((exact >= 1050.0) && (exact <= 1350.0))
            {
                ASSERT_LE(approx, exact + 9.5) << rest_mv << " mV pulled to " << min_mv;
            }
        }
    }
}

TEST(BatteryTest, ADeeperSagNeverReadsAsAHealthierCell)
{
    // The trap in P.R: it peaks at half the resting voltage and falls again
    // below that. Taken at face value, a cell pulled to 0.3 V would read
    // healthier than one pulled to 0.96 V. Nor does any sag read past the
    // resting voltage.
    for (uint16_t rest_mv = 1; rest_mv <= 2000; rest_mv++)
    {
        uint16_t previous = 0;

        for (int min_mv = rest_mv - 1; min_mv >= 1; min_mv--)
        {
            uint16_t collapse_mv = Battery_CollapseMv(rest_mv, static_cast<uint16_t>(min_mv));

            ASSERT_GE(collapse_mv, previous) << rest_mv << " mV pulled to " << min_mv;
            ASSERT_LE(collapse_mv, rest_mv) << rest_mv << " mV pulled to " << min_mv;

            previous = collapse_mv;
        }
    }
}

TEST(BatteryTest, NoRestingReadingAbove2VIsTakenAsACell)
{
    for (uint32_t rest_mv = 2001; rest_mv <= UINT16_MAX; rest_mv++)
    {
        uint16_t rest = static_cast<uint16_t>(rest_mv);

        ASSERT_EQ(0u, Battery_CollapseMv(rest, 945)) << rest_mv << " mV";
        ASSERT_EQ(0u, Battery_CollapseMv(rest, static_cast<uint16_t>(rest - 1))) << rest_mv << " mV";
    }
}

// =========================================================================
// The trend: the collapse voltage over a cell's windows
// =========================================================================
//
// Battery_SagTrend. Each table is one cell's windows in order: the collapse
// voltage the window measured, and the trend after it. Each window counts for
// a quarter, but no window may pull it more than 60 mV before that -- so 15 mV
// at most.

struct TrendRow {
    uint16_t window_mv;
    uint16_t trend_mv;
};

void ExpectTrend(std::initializer_list<TrendRow> rows)
{
    Battery_SagTrend trend;
    Battery_SagTrendReset(&trend);

    int window = 0;

    for (const TrendRow& row : rows)
    {
        window++;
        Battery_SagTrendAdd(&trend, row.window_mv);

        EXPECT_EQ(row.trend_mv, Battery_SagTrendMv(&trend))
            << "after window " << window << ", which measured " << row.window_mv << " mV";
    }
}

TEST(BatteryTest, ATrendStartsEmpty)
{
    Battery_SagTrend trend;
    Battery_SagTrendReset(&trend);

    EXPECT_EQ(0u, Battery_SagTrendMv(&trend));
}

TEST(BatteryTest, TheFirstWindowSetsTheTrendOutright)
{
    // So a cell that is bad from the start reads so on the next report.
    ExpectTrend({
        // window  trend
        { 1290,    1290 },
    });
}

TEST(BatteryTest, AWindowThatMeasuredNothingLeavesTheTrendAlone)
{
    ExpectTrend({
        // window  trend
        {    0,       0 }, // still empty
        { 1200,    1200 },
        {    0,    1200 },
        {    0,    1200 },
    });
}

TEST(BatteryTest, AWindowCountsForAQuarter)
{
    ExpectTrend({
        // window  trend
        { 1100,    1100 },
        { 1140,    1110 }, // a quarter of 40 mV
        { 1140,    1118 }, // 1117.5: the fraction is kept, not dropped
        { 1140,    1123 },
    });
}

TEST(BatteryTest, AWindowFarAboveTheTrendMovesIt15Mv)
{
    ExpectTrend({
        // window  trend
        { 1100,    1100 },
        { 1300,    1115 }, // a quarter of 60 mV, not of 200
        { 1300,    1130 },
        { 1300,    1145 },
    });
}

TEST(BatteryTest, AWindowFarBelowTheTrendMovesIt15Mv)
{
    ExpectTrend({
        // window  trend
        { 1100,    1100 },
        {  900,    1085 },
        {  900,    1070 },
    });
}

TEST(BatteryTest, ASustainedChangeIsFollowedAllTheWay)
{
    // 200 mV in 21 windows, to within 2 mV.
    Battery_SagTrend trend;
    Battery_SagTrendReset(&trend);
    Battery_SagTrendAdd(&trend, 1100);

    for (int window = 0; window < 21; window++)
    {
        Battery_SagTrendAdd(&trend, 1300);
    }

    EXPECT_NEAR(1300, Battery_SagTrendMv(&trend), 2);
}

TEST(BatteryTest, AResetForgetsTheCell)
{
    Battery_SagTrend trend;
    Battery_SagTrendReset(&trend);
    Battery_SagTrendAdd(&trend, 1290);

    Battery_SagTrendReset(&trend);
    EXPECT_EQ(0u, Battery_SagTrendMv(&trend));

    // The next cell's first window is taken whole, not stepped towards.
    Battery_SagTrendAdd(&trend, 1137);
    EXPECT_EQ(1137u, Battery_SagTrendMv(&trend));
}

TEST(BatteryTest, NoWindowMovesTheTrendMoreThan15MvOrPastItself)
{
    // Fixed pseudo-random runs of windows over everything Battery_CollapseMv
    // can return: mostly a random walk, now and then a jump anywhere, and
    // some windows that measured nothing.
    uint32_t seed = 12345u;

    auto next = [&seed]() {
        seed = (seed * 1664525u) + 1013904223u;
        return seed >> 8;
    };

    for (int run = 0; run < 500; run++)
    {
        Battery_SagTrend trend;
        Battery_SagTrendReset(&trend);

        int window_mv = 600 + static_cast<int>(next() % 1401u);
        int lowest    = 2000;
        int highest   = 0;

        for (int window = 0; window < 100; window++)
        {
            uint32_t roll = next() % 16u;

            if (roll == 0u)
            {
                window_mv = 600 + static_cast<int>(next() % 1401u);
            }
            else
            {
                window_mv = std::clamp(window_mv + static_cast<int>(next() % 161u) - 80, 600, 2000);
            }

            uint16_t measured = (roll == 1u) ? 0u : static_cast<uint16_t>(window_mv);
            int      before   = Battery_SagTrendMv(&trend);

            Battery_SagTrendAdd(&trend, measured);

            int after = Battery_SagTrendMv(&trend);

            if (measured == 0u)
            {
                ASSERT_EQ(before, after) << "run " << run << ", window " << window;
                continue;
            }

            lowest  = std::min(lowest, static_cast<int>(measured));
            highest = std::max(highest, static_cast<int>(measured));

            if (before == 0)
            {
                ASSERT_EQ(measured, after) << "run " << run << ", window " << window;
                continue;
            }

            ASSERT_LE(std::abs(after - before), 15) << "run " << run << ", window " << window;

            // Towards the window, and never past it.
            ASSERT_LE(std::min(before, static_cast<int>(measured)), after) << "run " << run << ", window " << window;
            ASSERT_GE(std::max(before, static_cast<int>(measured)), after) << "run " << run << ", window " << window;

            // So never outside what this cell's windows measured.
            ASSERT_LE(lowest, after) << "run " << run << ", window " << window;
            ASSERT_GE(highest, after) << "run " << run << ", window " << window;
        }
    }
}

// =========================================================================
// The gauge: the percentage under load
// =========================================================================
//
// Battery_UsablePercentFromMv: the plain curve with its 0 % point moved up to
// 50 mV above the collapse voltage, and what is left stretched back over
// 0..100, so a full cell still reads 100.
//
// One row per collapse voltage, one column per resting voltage: each row is
// the curve a cell with that sag reads along as it runs down.
//
//   collapse
//       0   nothing measured yet: the plain curve
//    1050   a small sag: the 0 % point is still the curve's own, 1100 mV
//    1100   0 % at 1150 mV, where the plain curve reads 5 %
//    1137   the healthy Eneloop: 0 % at 1187 mV, plain 12 %
//    1150   0 % at 1200 mV, plain 15 %
//    1200   0 % at 1250 mV, plain 38 %
//    1210   the lowest that is damaged (DP 102): 0 % at 1260 mV, plain 46 %
//    1250   0 % at 1300 mV, plain 80 %: from 100 % to 0 % in 20 mV
//    1270   0 % at the full anchor: empty even when full
//    1290   the failed Eneloop

constexpr uint16_t kGaugeRestMv[] = {
    1100, 1120, 1140, 1160, 1180, 1200, 1220, 1240, 1260, 1280, 1300, 1320, 1400,
};

constexpr size_t kGaugeColumns = std::size(kGaugeRestMv);

struct GaugeRow {
    uint16_t collapse_mv;
    int      percent[kGaugeColumns];
};

const GaugeRow kGauge[] = {
    //             the gauge resting at this many mV:
    // collapse  1100  1120  1140  1160  1180  1200  1220  1240  1260  1280  1300  1320  1400
    {    0, {       0,    2,    4,    7,   11,   15,   23,   30,   46,   62,   80,  100,  100 } },
    { 1050, {       0,    2,    4,    7,   11,   15,   23,   30,   46,   62,   80,  100,  100 } },
    { 1100, {       0,    0,    0,    2,    6,   11,   19,   26,   43,   60,   79,  100,  100 } },
    { 1137, {       0,    0,    0,    0,    0,    3,   13,   20,   39,   57,   77,  100,  100 } },
    { 1150, {       0,    0,    0,    0,    0,    0,    9,   18,   36,   55,   76,  100,  100 } },
    { 1200, {       0,    0,    0,    0,    0,    0,    0,    0,   13,   39,   68,  100,  100 } },
    { 1210, {       0,    0,    0,    0,    0,    0,    0,    0,    0,   30,   63,  100,  100 } },
    { 1250, {       0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,  100,  100 } },
    { 1270, {       0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0 } },
    { 1290, {       0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0 } },
};

TEST(BatteryTest, TheGaugeForEachSag)
{
    for (const GaugeRow& row : kGauge)
    {
        for (size_t i = 0; i < kGaugeColumns; i++)
        {
            EXPECT_EQ(row.percent[i], PercentUnderLoad(kGaugeRestMv[i], row.collapse_mv))
                << "collapsing at " << row.collapse_mv << " mV, resting at " << kGaugeRestMv[i] << " mV";
        }
    }
}

TEST(BatteryTest, WithNothingMeasuredTheGaugeIsThePlainCurve)
{
    for (uint32_t mv = 0; mv <= UINT16_MAX; mv++)
    {
        uint16_t rest_mv = static_cast<uint16_t>(mv);

        ASSERT_EQ(PercentAtRest(rest_mv), PercentUnderLoad(rest_mv, 0)) << mv << " mV";
    }
}

TEST(BatteryTest, TheCurveIsLeftAloneUntilTheZeroPointClimbsPast1104Mv)
{
    // The plain curve reads 0 up to 1104 mV, so a collapse voltage up to 1054
    // leaves the gauge as it was. From 1055 the 0 % point starts to climb.
    for (uint16_t collapse_mv = 1; collapse_mv <= 1054; collapse_mv++)
    {
        for (uint16_t rest_mv = 1050; rest_mv <= 1350; rest_mv++)
        {
            ASSERT_EQ(PercentAtRest(rest_mv), PercentUnderLoad(rest_mv, collapse_mv))
                << "collapsing at " << collapse_mv << " mV, resting at " << rest_mv << " mV";
        }
    }

    EXPECT_EQ(15, PercentUnderLoad(1200, 1054));
    EXPECT_EQ(14, PercentUnderLoad(1200, 1055));
}

TEST(BatteryTest, AFullCellReadsFullUntilTheZeroPointReachesTheFullAnchor)
{
    // The 0 % point is 50 mV above the collapse voltage, so up to 1269 mV
    // (0 % at 1319) a full cell still reads 100. From 1270 the 0 % point is
    // the full anchor itself, and nothing is left above it.
    for (uint16_t rest_mv : { 1320, 1335, 1400 })
    {
        for (uint16_t collapse_mv = 0; collapse_mv <= 1269; collapse_mv++)
        {
            ASSERT_EQ(100, PercentUnderLoad(rest_mv, collapse_mv))
                << "collapsing at " << collapse_mv << " mV, resting at " << rest_mv << " mV";
        }

        for (uint16_t collapse_mv = 1270; collapse_mv <= 2000; collapse_mv++)
        {
            ASSERT_EQ(0, PercentUnderLoad(rest_mv, collapse_mv))
                << "collapsing at " << collapse_mv << " mV, resting at " << rest_mv << " mV";
        }
    }
}

TEST(BatteryTest, AnUnreadableCellReadsFlatWhateverItsSag)
{
    for (uint16_t collapse_mv = 0; collapse_mv <= 2000; collapse_mv++)
    {
        ASSERT_EQ(0, PercentUnderLoad(0, collapse_mv)) << "collapsing at " << collapse_mv << " mV";
    }
}

TEST(BatteryTest, ASagNeverMakesTheGaugeReadHigher)
{
    for (uint16_t rest_mv = 0; rest_mv <= 2100; rest_mv++)
    {
        for (uint16_t collapse_mv = 0; collapse_mv <= 2000; collapse_mv += 7)
        {
            ASSERT_LE(PercentUnderLoad(rest_mv, collapse_mv), PercentAtRest(rest_mv))
                << "collapsing at " << collapse_mv << " mV, resting at " << rest_mv << " mV";
        }
    }
}

TEST(BatteryTest, TheGaugeNeverReadsHigherForADeeperSagOrALowerCell)
{
    // Along a row of the table above, and down a column.
    for (uint16_t rest_mv = 0; rest_mv <= 2100; rest_mv += 5)
    {
        int previous = 100;

        for (uint16_t collapse_mv = 0; collapse_mv <= 2000; collapse_mv++)
        {
            int percent = PercentUnderLoad(rest_mv, collapse_mv);

            ASSERT_LE(percent, previous) << "collapsing at " << collapse_mv << " mV, resting at " << rest_mv << " mV";

            previous = percent;
        }
    }

    for (uint16_t collapse_mv = 0; collapse_mv <= 2000; collapse_mv += 5)
    {
        int previous = 0;

        for (uint16_t rest_mv = 0; rest_mv <= 2100; rest_mv++)
        {
            int percent = PercentUnderLoad(rest_mv, collapse_mv);

            ASSERT_GE(percent, previous) << "collapsing at " << collapse_mv << " mV, resting at " << rest_mv << " mV";
            ASSERT_LE(percent, 100) << "collapsing at " << collapse_mv << " mV, resting at " << rest_mv << " mV";

            previous = percent;
        }
    }
}

// =========================================================================
// A damaged cell (DP 102)
// =========================================================================

struct DamagedRow {
    uint16_t    collapse_mv;
    bool        damaged;
    const char* note;
};

const DamagedRow kDamaged[] = {
    // collapse  damaged
    {    0,      false, "nothing measured" },
    { 1137,      false, "the healthy Eneloop" },
    { 1150,      false, "a cell off the charger pulled to 1.10 V" },
    { 1209,      false, "" },
    { 1210,      true,  "the threshold: midway between the two Eneloops" },
    { 1283,      true,  "the failed Eneloop recharged" },
    { 1290,      true,  "the failed Eneloop" },
    { 2000,      true,  "" },
};

TEST(BatteryTest, DamagedFromACollapseVoltageOf1210Mv)
{
    for (const DamagedRow& row : kDamaged)
    {
        EXPECT_EQ(row.damaged, Battery_IsDamaged(row.collapse_mv)) << row.collapse_mv << " mV: " << row.note;
    }
}

TEST(BatteryTest, ACellRestingBelow1210MvIsNeverDamagedWhateverItsSag)
{
    // Its collapse voltage cannot exceed its resting voltage. A cell that
    // gives out below 1.21 V is empty, not damaged.
    for (uint16_t rest_mv = 0; rest_mv < 1210; rest_mv++)
    {
        for (uint16_t min_mv = 0; min_mv <= rest_mv + 10; min_mv++)
        {
            ASSERT_FALSE(Battery_IsDamaged(Battery_CollapseMv(rest_mv, min_mv)))
                << rest_mv << " mV pulled to " << min_mv << " mV";
        }
    }
}

// =========================================================================
// A new cell
// =========================================================================

struct NewCellRow {
    uint16_t    before_mv;
    uint16_t    now_mv;
    bool        new_cell;
    const char* note;
};

const NewCellRow kNewCell[] = {
    // before    now  new cell
    {  1250,   1300,  true,  "a rise of exactly 50 mV" },
    {  1250,   1299,  false, "49 mV" },
    {  1290,   1400,  true,  "the failed Eneloop swapped for a cell off the charger" },
    {  1240,   1322,  true,  "a used cell swapped for a rested, charged one" },
    {  1286,   1310,  false, "a cell recovering a little" },
    {  1300,   1290,  false, "a cell running down" },
    {  1300,   1300,  false, "" },
    {     0,   1300,  false, "no reading before: the first cycle after a reset" },
    {  1300,      0,  false, "this reading failed" },
    {  1290,   4000,  true,  "a bad reference read: a false alarm, which costs only the trend" },
    {  4000,   1290,  false, "the reading after it" },
    { 65535,  65535,  false, "no overflow at the top" },
};

TEST(BatteryTest, ANewCellIsARiseOf50MvOrMore)
{
    for (const NewCellRow& row : kNewCell)
    {
        EXPECT_EQ(row.new_cell, Battery_IsNewCell(row.before_mv, row.now_mv))
            << row.before_mv << " mV, then " << row.now_mv << " mV: " << row.note;
    }
}

// =========================================================================
// Whole cells: what the app shows
// =========================================================================
//
// Every step at once: the collapse voltage of a window, the trend it seeds,
// and what DP 4 (percentage), DP 3 (level) and DP 102 (damaged) then say. Each
// row is one window, the cell's first -- or what a cell whose windows all sag
// alike settles at.
//
// (*) Not measured: the cell's measured P.R carried to another resting
//     voltage, as if its resistance held.

struct CellRow {
    const char*   cell;
    uint16_t      rest_mv;
    uint16_t      min_mv;
    uint16_t      collapse_mv;
    int           plain_percent;
    int           percent;
    Battery_State level;
    bool          damaged;
};

const CellRow kCells[] = {
    //                                        rest   min  collapse  plain  gauge  level    DP 102
    { "healthy Eneloop off the charger (*)",  1400,  1110,  1136,    100,   100,  kHigh,   false },
    { "healthy Eneloop, full, rested (*)",    1320,   997,  1136,    100,   100,  kHigh,   false },
    { "healthy Eneloop on the scope",         1300,   960,  1144,     80,    77,  kHigh,   false },
    { "healthy Eneloop today, from DP 101",   1286,   945,  1137,     67,    63,  kHigh,   false },
    { "healthy Eneloop at 1265 mV (*)",       1265,   911,  1137,     50,    43,  kMiddle, false },
    { "healthy Eneloop at 1240 mV (*)",       1240,   869,  1137,     30,    20,  kLow,    false },
    { "healthy Eneloop at 1220 mV (*)",       1220,   833,  1137,     23,    13,  kLow,    false },
    { "healthy Eneloop at 1200 mV (*)",       1200,   794,  1137,     15,     3,  kLow,    false },
    { "healthy Eneloop at its 0 % point (*)", 1187,   767,  1136,     12,     0,  kLow,    false },
    { "failed Eneloop",                       1290,   580,  1290,     70,     0,  kLow,    true  },
    { "failed Eneloop recharged (*)",         1390,   965,  1283,    100,     0,  kLow,    true  },

    // Pulled exactly as far as the recharged failed cell, but from 100 mV
    // lower: the opposite verdict. The minimum alone cannot tell them apart.
    { "a cell at 1290 mV pulled to 965 mV",   1290,   965,  1122,     70,    67,  kHigh,   false },
};

TEST(BatteryTest, WholeCells)
{
    for (const CellRow& row : kCells)
    {
        uint16_t collapse_mv = Battery_CollapseMv(row.rest_mv, row.min_mv);

        Battery_SagTrend trend;
        Battery_SagTrendReset(&trend);
        Battery_SagTrendAdd(&trend, collapse_mv);

        uint16_t trend_mv = Battery_SagTrendMv(&trend);
        int      percent  = PercentUnderLoad(row.rest_mv, trend_mv);

        EXPECT_EQ(row.collapse_mv, collapse_mv) << row.cell;
        EXPECT_EQ(row.collapse_mv, trend_mv) << row.cell;
        EXPECT_EQ(row.plain_percent, PercentAtRest(row.rest_mv)) << row.cell;
        EXPECT_EQ(row.percent, percent) << row.cell;
        EXPECT_EQ(row.level, Battery_StateFromPercent(static_cast<uint8_t>(percent))) << row.cell;
        EXPECT_EQ(row.damaged, Battery_IsDamaged(trend_mv)) << row.cell;
    }
}

// --- Whole cells, window by window ----------------------------------------
//
// One cell resting at a fixed voltage, and what each window does to the trend
// and the DPs. In the firmware each row's DPs go out one report later.

struct WindowRow {
    uint16_t      window_mv;
    uint16_t      trend_mv;
    int           percent;
    Battery_State level;
    bool          damaged;
};

void ExpectWindows(uint16_t rest_mv, std::initializer_list<WindowRow> rows)
{
    Battery_SagTrend trend;
    Battery_SagTrendReset(&trend);

    int window = 0;

    for (const WindowRow& row : rows)
    {
        window++;
        Battery_SagTrendAdd(&trend, row.window_mv);

        uint16_t trend_mv = Battery_SagTrendMv(&trend);
        int      percent  = PercentUnderLoad(rest_mv, trend_mv);

        EXPECT_EQ(row.trend_mv, trend_mv) << "after window " << window;
        EXPECT_EQ(row.percent, percent) << "after window " << window;
        EXPECT_EQ(row.level, Battery_StateFromPercent(static_cast<uint8_t>(percent))) << "after window " << window;
        EXPECT_EQ(row.damaged, Battery_IsDamaged(trend_mv)) << "after window " << window;
    }
}

TEST(BatteryTest, AHealthyCellBarelyMovesForOneDeepWindow)
{
    // The healthy Eneloop at 1286 mV, and one window as deep as the failed
    // cell's -- taken alone, that window would read 0 % and damaged.
    ExpectWindows(1286, {
        // window  trend  gauge  level    DP 102
        { 1137,    1137,   63,   kHigh,   false },
        { 1286,    1152,   61,   kHigh,   false }, // pulled to 580 mV
        { 1137,    1148,   61,   kHigh,   false },
        { 1137,    1146,   62,   kHigh,   false },
        { 1137,    1143,   62,   kHigh,   false },
        { 1137,    1142,   62,   kHigh,   false },
        { 1137,    1141,   62,   kHigh,   false },
        { 1137,    1140,   62,   kHigh,   false },
    });
}

TEST(BatteryTest, AHealthyCellThatStartsToSagLikeTheFailedOne)
{
    // From the second window on, every window as deep as the failed cell's:
    // the cell or its holder going bad. DP 102 comes on at the sixth window
    // and the gauge is at 0 % by the eighth -- about a day at six reports a
    // day.
    ExpectWindows(1286, {
        // window  trend  gauge  level    DP 102
        { 1137,    1137,   63,   kHigh,   false },
        { 1286,    1152,   61,   kHigh,   false },
        { 1286,    1167,   58,   kMiddle, false },
        { 1286,    1182,   55,   kMiddle, false },
        { 1286,    1197,   48,   kMiddle, false },
        { 1286,    1212,   37,   kMiddle, true  },
        { 1286,    1227,   18,   kLow,    true  },
        { 1286,    1242,    0,   kLow,    true  },
        { 1286,    1253,    0,   kLow,    true  },
        { 1286,    1261,    0,   kLow,    true  },
        { 1286,    1267,    0,   kLow,    true  },
        { 1286,    1272,    0,   kLow,    true  },
        { 1286,    1276,    0,   kLow,    true  },
    });
}

TEST(BatteryTest, ADeepFirstWindowMarksAHealthyCellDamagedForSixWindows)
{
    // The cost of taking a cell's first window whole: if it happens to be as
    // deep as the failed cell's, the healthy Eneloop reads 0 % for four
    // windows and damaged for six, before it settles.
    ExpectWindows(1286, {
        // window  trend  gauge  level    DP 102
        { 1286,    1286,    0,   kLow,    true  }, // pulled to 580 mV
        { 1137,    1271,    0,   kLow,    true  },
        { 1137,    1256,    0,   kLow,    true  },
        { 1137,    1241,    0,   kLow,    true  },
        { 1137,    1226,   20,   kLow,    true  },
        { 1137,    1211,   38,   kMiddle, true  },
        { 1137,    1196,   49,   kMiddle, false },
        { 1137,    1181,   55,   kMiddle, false },
        { 1137,    1170,   57,   kMiddle, false },
        { 1137,    1162,   59,   kMiddle, false },
    });
}

TEST(BatteryTest, TheFailedCellReadsEmptyAndDamagedFromItsFirstWindow)
{
    ExpectWindows(1290, {
        // window  trend  gauge  level    DP 102
        { 1290,    1290,    0,   kLow,    true  },
        { 1290,    1290,    0,   kLow,    true  },
    });
}

} // namespace
