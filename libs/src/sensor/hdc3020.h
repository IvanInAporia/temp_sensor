#pragma once

// TI HDC3020 temperature + relative humidity sensor, trigger-on-demand only.
//
// The part is permanently powered on this board (+3V3, RESET# strapped high),
// so there is no power-up sequence to run and no state to restore: it boots
// into sleep mode, a trigger wakes it for one conversion, and it drops back to
// sleep on its own. That makes the whole driver one blocking call.
//
// Everything the driver does not use is deliberately absent -- auto-measurement
// mode, the ALERT limits, the heater, the programmable offsets, the NVM
// commands. On a 16 KB part an unused command table is not free, and none of
// them have a caller.

#include <stdbool.h>
#include <stdint.h>

/* 7-bit address with ADDR and ADDR1 both strapped to ground, which is how U3 is
 * wired (easyeda/review-notes.md 3.1). The other three strappings give 0x45,
 * 0x46 and 0x47.
 */
#define HDC3020_I2C_ADDRESS 0x44u

typedef struct {
    int16_t temp_c10; /* temperature in 0.1 C, e.g. 234 = 23.4 C */
    uint8_t rh_pct;   /* relative humidity in whole percent, 0..100 */
} Hdc3020_Reading;

/* Take one temperature + humidity reading and convert it.
 *
 * Blocks for the conversion (~13 ms in the low-power mode used here). Returns
 * false -- leaving `out` untouched -- if the bus transfer failed or either CRC
 * did not match, so a caller can tell "no reading" from "a reading of zero".
 *
 * A failure is not worth retrying inside the driver: the cycle that asked for
 * it runs again in minutes, and a sensor that has gone quiet will not answer a
 * second time 10 ms later either.
 */
bool Hdc3020_Read(Hdc3020_Reading* out);

/* CRC-8/NRSC-5 over `len` bytes: poly 0x31, init 0xFF, no reflection, no final
 * XOR. Exposed only so the host tests can build valid frames for the mock and
 * check the implementation against the datasheet's worked example
 * (CRC(0xABCD) == 0x6F).
 */
uint8_t Hdc3020_Crc8(const uint8_t* data, uint8_t len);
