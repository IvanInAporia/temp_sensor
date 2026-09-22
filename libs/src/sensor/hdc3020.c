#include "hdc3020.h"

#include "bsp.h"

/* Trigger-on-demand, Low Power Mode 0 -- the lowest-noise of the four
 * (+-0.02 %RH) and the slowest at 12.5 ms typical.
 *
 * LPM0 rather than LPM3 because the choice is worth nothing here. The four
 * modes differ by 11 uA of active current over at most 12.5 ms, i.e. about
 * 0.04 uAs per reading; against a cycle that spends seconds of Wi-Fi at tens of
 * milliamps every time the temperature moves, the difference is unmeasurable.
 * Noise, on the other hand, is not free: this firmware only transmits when the
 * reading changes, so sensor noise is what turns into radio traffic.
 */
#define CMD_TRIGGER_LPM0_MSB 0x24u
#define CMD_TRIGGER_LPM0_LSB 0x00u

/* 12.5 ms typical conversion, 15 ms worst case in the datasheet's timing
 * table. Reading early is not destructive -- the part NACKs until it is done --
 * but a NACK is indistinguishable here from a sensor that has fallen off the
 * bus, so wait the worst case and keep a failure meaning something.
 */
#define CONVERSION_TIME_MS 15u

/* Two bytes of temperature, its CRC, two bytes of humidity, its CRC. */
#define RESULT_LEN 6u

uint8_t Hdc3020_Crc8(const uint8_t* data, uint8_t len)
{
    uint8_t crc = 0xFFu;

    for (uint8_t i = 0u; i < len; i++)
    {
        crc ^= data[i];

        for (uint8_t bit = 0u; bit < 8u; bit++)
        {
            /* Bitwise rather than a 256-byte table: this runs twice per cycle,
             * once every five minutes, and the table would cost more flash than
             * the whole driver. */
            crc = (crc & 0x80u) ? (uint8_t)((crc << 1) ^ 0x31u) : (uint8_t)(crc << 1);
        }
    }

    return crc;
}

bool Hdc3020_Read(Hdc3020_Reading* out)
{
    static const uint8_t trigger[2] = { CMD_TRIGGER_LPM0_MSB, CMD_TRIGGER_LPM0_LSB };
    uint8_t result[RESULT_LEN];

    if (out == 0)
    {
        return false;
    }

    if (!BSP_Sensor_I2cWrite(HDC3020_I2C_ADDRESS, trigger, sizeof(trigger)))
    {
        return false;
    }

    BSP_DelayMs(CONVERSION_TIME_MS);

    if (!BSP_Sensor_I2cRead(HDC3020_I2C_ADDRESS, result, RESULT_LEN))
    {
        return false;
    }

    /* Both CRCs, not just one. They cover different halves of the transfer, and
     * a bus that corrupted the temperature is no reason to trust the humidity
     * that followed it. */
    if ((Hdc3020_Crc8(&result[0], 2u) != result[2]) ||
        (Hdc3020_Crc8(&result[3], 2u) != result[5]))
    {
        return false;
    }

    uint16_t temp_raw = (uint16_t)(((uint16_t)result[0] << 8) | result[1]);
    uint16_t rh_raw   = (uint16_t)(((uint16_t)result[3] << 8) | result[4]);

    /* Datasheet 8.3: T[C] = -45 + 175 * raw / 65535, wanted in 0.1 C.
     *
     * Kept in integers all the way: 65535 * 1750 is 114,686,250, which fits an
     * int32 with two decimal orders to spare, so there is no reason to drag
     * soft-float onto a Cortex-M0+ for this. The +32767 is round-to-nearest on
     * the unsigned part, applied before the offset so it does not change
     * direction below 0 C. */
    out->temp_c10 = (int16_t)((int32_t)(((uint32_t)temp_raw * 1750u + 32767u) / 65535u) - 450);

    /* RH[%] = 100 * raw / 65535. Whole percent is all the Tuya DP carries
     * (range 0-100, no scale), so the fraction is dropped here rather than
     * being carried around and thrown away at the report. */
    out->rh_pct = (uint8_t)(((uint32_t)rh_raw * 100u + 32767u) / 65535u);

    return true;
}
