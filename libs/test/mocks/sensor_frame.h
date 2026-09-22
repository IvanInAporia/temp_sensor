#pragma once

// Builds the six bytes an HDC3020 hands back for one trigger-on-demand read,
// for staging into bspMockSensorSetResponse.
//
// The CRCs come from the driver's own Hdc3020_Crc8 rather than a second
// implementation here. That is on purpose: a duplicate would agree with a
// broken driver, and the one thing a CRC helper must not do is agree with the
// code it is checking. Hdc3020_Crc8 is pinned separately against the
// datasheet's worked example (CRC(0xABCD) == 0x6F) in hdc3020_test.

#include <stdint.h>

#include "hdc3020.h"

/* Raw sensor counts, as the part transmits them, into a wire frame. */
inline void SensorFrameFromRaw(uint16_t temp_raw, uint16_t rh_raw, uint8_t out[6])
{
    out[0] = (uint8_t) (temp_raw >> 8);
    out[1] = (uint8_t) (temp_raw & 0xFFu);
    out[2] = Hdc3020_Crc8(&out[0], 2u);
    out[3] = (uint8_t) (rh_raw >> 8);
    out[4] = (uint8_t) (rh_raw & 0xFFu);
    out[5] = Hdc3020_Crc8(&out[3], 2u);
}

/* The inverse of the datasheet's conversion, so a test can ask for "23.4 C and
 * 55 %" instead of working out which counts produce it.
 *
 * Rounds to the nearest count, so it is exact only to the resolution of a
 * count (~0.0027 C, ~0.0015 %RH). A test that needs an exact tenth should
 * assert with that tolerance, or stage raw counts directly.
 */
inline void SensorFrameFromReading(int16_t temp_c10, uint8_t rh_pct, uint8_t out[6])
{
    /* raw = (T + 45) * 65535 / 175, with T in 0.1 C: (temp_c10 + 450) * 65535 / 1750 */
    int32_t temp_num = ((int32_t) temp_c10 + 450) * 65535;
    uint16_t temp_raw = (uint16_t) ((temp_num + 875) / 1750);

    uint32_t rh_raw32 = (((uint32_t) rh_pct * 65535u) + 50u) / 100u;
    uint16_t rh_raw   = (uint16_t) rh_raw32;

    SensorFrameFromRaw(temp_raw, rh_raw, out);
}
