#include <gtest/gtest.h>

extern "C" {
#include "bsp_mock.h"
#include "hdc3020.h"
#include "sensor_frame.h"
}

namespace {

class Hdc3020Test : public ::testing::Test {
protected:
    void SetUp() override { bspMockReset(); }

    // Stage a frame built from raw counts and take a reading.
    bool ReadRaw(uint16_t temp_raw, uint16_t rh_raw, Hdc3020_Reading* out)
    {
        uint8_t frame[6];
        SensorFrameFromRaw(temp_raw, rh_raw, frame);
        bspMockSensorSetResponse(frame, sizeof(frame));
        return Hdc3020_Read(out);
    }
};

// --- CRC ------------------------------------------------------------------
//
// Pinned against the datasheet rather than against itself. Everything else in
// this file stages frames whose CRCs come from this same function, so if it
// were wrong in a self-consistent way nothing else here would notice.

TEST_F(Hdc3020Test, Crc8MatchesTheDatasheetWorkedExample)
{
    const uint8_t data[2] = { 0xABu, 0xCDu };

    EXPECT_EQ(0x6Fu, Hdc3020_Crc8(data, 2u));
}

TEST_F(Hdc3020Test, Crc8OfZeroesIsNotZero)
{
    // Init 0xFF is what makes this true; an init of 0x00 would give 0x00 and
    // would let an all-zero frame from a dead bus pass as valid data.
    const uint8_t data[2] = { 0x00u, 0x00u };

    EXPECT_NE(0x00u, Hdc3020_Crc8(data, 2u));
}

// --- Conversion -----------------------------------------------------------

TEST_F(Hdc3020Test, ConvertsTheDatasheetEndpoints)
{
    Hdc3020_Reading reading{};

    // T = -45 + 175 * 0 / 65535 = -45.0 C, RH = 0 %.
    ASSERT_TRUE(ReadRaw(0u, 0u, &reading));
    EXPECT_EQ(-450, reading.temp_c10);
    EXPECT_EQ(0u, reading.rh_pct);

    // Full scale: T = -45 + 175 = 130.0 C, RH = 100 %.
    ASSERT_TRUE(ReadRaw(0xFFFFu, 0xFFFFu, &reading));
    EXPECT_EQ(1300, reading.temp_c10);
    EXPECT_EQ(100u, reading.rh_pct);
}

TEST_F(Hdc3020Test, ConvertsMidScale)
{
    Hdc3020_Reading reading{};

    // Half scale is exactly the midpoint of both ranges: 42.5 C and 50 %.
    ASSERT_TRUE(ReadRaw(0x8000u, 0x8000u, &reading));
    EXPECT_EQ(425, reading.temp_c10);
    EXPECT_EQ(50u, reading.rh_pct);
}

TEST_F(Hdc3020Test, ConvertsSubZeroTemperature)
{
    // The offset is applied after an unsigned division, so this is the case
    // that catches a rounding term pulling in the wrong direction below 0 C.
    uint8_t frame[6];
    SensorFrameFromReading(-105, 40u, frame); // -10.5 C
    bspMockSensorSetResponse(frame, sizeof(frame));

    Hdc3020_Reading reading{};
    ASSERT_TRUE(Hdc3020_Read(&reading));

    EXPECT_NEAR(-105, reading.temp_c10, 1);
    EXPECT_EQ(40u, reading.rh_pct);
}

TEST_F(Hdc3020Test, ConvertsARoomTemperatureReading)
{
    uint8_t frame[6];
    SensorFrameFromReading(234, 55u, frame); // 23.4 C, 55 %
    bspMockSensorSetResponse(frame, sizeof(frame));

    Hdc3020_Reading reading{};
    ASSERT_TRUE(Hdc3020_Read(&reading));

    EXPECT_NEAR(234, reading.temp_c10, 1);
    EXPECT_EQ(55u, reading.rh_pct);
}

// --- Bus protocol ---------------------------------------------------------

TEST_F(Hdc3020Test, TriggersLowPowerModeZeroAtTheStrappedAddress)
{
    Hdc3020_Reading reading{};
    ASSERT_TRUE(ReadRaw(0x8000u, 0x8000u, &reading));

    EXPECT_EQ(1, bspMockSensorWriteCalls());
    EXPECT_EQ(1, bspMockSensorReadCalls());

    // 0x2400: trigger-on-demand, LPM0. Chosen for noise, not for power --
    // sensor noise is what turns into radio traffic here (hdc3020.c).
    EXPECT_EQ(0x24u, bspMockSensorLastCommandMsb());
    EXPECT_EQ(0x00u, bspMockSensorLastCommandLsb());

    // ADDR and ADDR1 both to ground on this board.
    EXPECT_EQ(0x44u, bspMockSensorLastAddress());
}

TEST_F(Hdc3020Test, WaitsForTheConversionBeforeReading)
{
    // The part NACKs a read issued before the conversion finishes, so the wait
    // is not optional. 12.5 ms typical, 15 ms worst case.
    Hdc3020_Reading reading{};
    ASSERT_TRUE(ReadRaw(0x8000u, 0x8000u, &reading));

    EXPECT_GE(bspMockNowMs(), 15u);
}

// --- Failure --------------------------------------------------------------

TEST_F(Hdc3020Test, RejectsACorruptTemperatureCrc)
{
    uint8_t frame[6];
    SensorFrameFromRaw(0x8000u, 0x8000u, frame);
    frame[2] ^= 0xFFu; // wreck the temperature CRC only

    bspMockSensorSetResponse(frame, sizeof(frame));

    Hdc3020_Reading reading{};
    EXPECT_FALSE(Hdc3020_Read(&reading));
}

TEST_F(Hdc3020Test, RejectsACorruptHumidityCrc)
{
    // Both halves are checked, not just the first: a bus that corrupted one is
    // no reason to trust the other.
    uint8_t frame[6];
    SensorFrameFromRaw(0x8000u, 0x8000u, frame);
    frame[5] ^= 0xFFu;

    bspMockSensorSetResponse(frame, sizeof(frame));

    Hdc3020_Reading reading{};
    EXPECT_FALSE(Hdc3020_Read(&reading));
}

TEST_F(Hdc3020Test, LeavesTheOutputUntouchedWhenTheReadFails)
{
    // "No reading" has to be distinguishable from "a reading of zero", or a
    // dead sensor would report -45 C to the cloud.
    Hdc3020_Reading reading{};
    reading.temp_c10 = 1234;
    reading.rh_pct   = 99u;

    bspMockSensorSetReadFails(true);

    EXPECT_FALSE(Hdc3020_Read(&reading));
    EXPECT_EQ(1234, reading.temp_c10);
    EXPECT_EQ(99u, reading.rh_pct);
}

TEST_F(Hdc3020Test, DoesNotReadWhenTheTriggerWasNotAcknowledged)
{
    bspMockSensorSetWriteFails(true);

    Hdc3020_Reading reading{};
    EXPECT_FALSE(Hdc3020_Read(&reading));

    // No point waiting out a conversion that was never started.
    EXPECT_EQ(0, bspMockSensorReadCalls());
}

TEST_F(Hdc3020Test, RejectsANullOutput)
{
    EXPECT_FALSE(Hdc3020_Read(nullptr));
}

} // namespace
