#include <gtest/gtest.h>

extern "C" {
#include "battery.h"
}

namespace {

// --- Anchors --------------------------------------------------------------
//
// These pin the shape of the curve, not its accuracy: the millivolt values in
// battery.c are a datasheet-shaped estimate for one NiMH cell and are meant to
// be replaced by a bench discharge. What these tests guarantee is that whatever
// numbers are in the table, the gauge interpolates between them monotonically,
// clamps at both ends, and never reports a flat cell as charged.

TEST(BatteryTest, ReportsFullAtTheFullAnchor)
{
    EXPECT_EQ(100u, Battery_PercentFromMv(1400u));
}

TEST(BatteryTest, ClampsAboveTheFullAnchor)
{
    // A cell straight off the charger carries a surface charge well over
    // 1.4 V that is gone within the hour. Extrapolating would report over
    // 100 %.
    EXPECT_EQ(100u, Battery_PercentFromMv(1500u));
    EXPECT_EQ(100u, Battery_PercentFromMv(3300u));
}

TEST(BatteryTest, ReportsEmptyAtAndBelowTheEmptyAnchor)
{
    EXPECT_EQ(0u, Battery_PercentFromMv(1100u));
    EXPECT_EQ(0u, Battery_PercentFromMv(900u));
}

TEST(BatteryTest, AnUnmeasurableCellReadsFlatRatherThanCharged)
{
    // BSP_Battery_ReadMv returns 0 when the internal reference could not be
    // read. Mapping that to 0 % is the direction that cannot mislead.
    EXPECT_EQ(0u, Battery_PercentFromMv(0u));
}

TEST(BatteryTest, InterpolatesBetweenAnchors)
{
    // Halfway between the 1290 mV / 70 % and 1265 mV / 50 % anchors.
    EXPECT_NEAR(60u, Battery_PercentFromMv(1277u), 1);
}

TEST(BatteryTest, IsMonotonicAcrossTheWholeRange)
{
    // The property that matters most in the app: a cell that is discharging
    // must never read as gaining charge, whatever the table says.
    uint8_t previous = Battery_PercentFromMv(900u);

    for (uint16_t mv = 901u; mv <= 1600u; mv++)
    {
        uint8_t percent = Battery_PercentFromMv(mv);

        EXPECT_GE(percent, previous) << "went backwards at " << mv << " mV";
        EXPECT_LE(percent, 100u) << "overflowed at " << mv << " mV";

        previous = percent;
    }
}

TEST(BatteryTest, SpendsMostOfTheScaleOnThePlateau)
{
    // NiMH holds roughly 1.30 V down to 1.24 V for most of its capacity, so
    // that 60 mV band has to carry a large share of the scale. If it did not,
    // the gauge would sit at 100 % for weeks and then collapse.
    uint8_t at_plateau_top    = Battery_PercentFromMv(1300u);
    uint8_t at_plateau_bottom = Battery_PercentFromMv(1240u);

    EXPECT_GE(at_plateau_top - at_plateau_bottom, 40);
}

// --- The three-level indicator -------------------------------------------

TEST(BatteryTest, MapsPercentageToTheDpEnum)
{
    EXPECT_EQ(Battery_State_Low, Battery_StateFromPercent(0u));
    EXPECT_EQ(Battery_State_Low, Battery_StateFromPercent(20u));

    EXPECT_EQ(Battery_State_Middle, Battery_StateFromPercent(21u));
    EXPECT_EQ(Battery_State_Middle, Battery_StateFromPercent(60u));

    EXPECT_EQ(Battery_State_High, Battery_StateFromPercent(61u));
    EXPECT_EQ(Battery_State_High, Battery_StateFromPercent(100u));
}

TEST(BatteryTest, EnumValuesMatchTheTuyaWireCodes)
{
    // The DP is an enum whose wire value is the index of low / middle / high.
    // Reordering the enum would silently relabel every device in the field.
    EXPECT_EQ(0, static_cast<int>(Battery_State_Low));
    EXPECT_EQ(1, static_cast<int>(Battery_State_Middle));
    EXPECT_EQ(2, static_cast<int>(Battery_State_High));
}

TEST(BatteryTest, LowLandsOnTheKneeNotOnThePlateau)
{
    // "Low" is only useful if it means the remaining runtime is genuinely
    // short, so it has to sit past the knee. Anywhere on the plateau -- which
    // is most of the cell's life -- must not read as low.
    EXPECT_NE(Battery_State_Low, Battery_StateFromPercent(Battery_PercentFromMv(1300u)));
    EXPECT_NE(Battery_State_Low, Battery_StateFromPercent(Battery_PercentFromMv(1265u)));
    EXPECT_NE(Battery_State_Low, Battery_StateFromPercent(Battery_PercentFromMv(1240u)));

    // Past the knee it does.
    EXPECT_EQ(Battery_State_Low, Battery_StateFromPercent(Battery_PercentFromMv(1150u)));
    EXPECT_EQ(Battery_State_Low, Battery_StateFromPercent(Battery_PercentFromMv(1100u)));
}

} // namespace
