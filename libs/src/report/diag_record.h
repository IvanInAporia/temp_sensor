#pragma once

#include <stdbool.h>
#include <stdint.h>

// What the device went through since its last report reached the cloud, kept
// across resets, so the report that finally gets through can say why it was
// quiet.
//
// It goes out as DP 104, a short line of text in the Tuya cloud log:
//
//     r=pin c=108 w=4 u=0 m=02 s=0
//
//   r  why this run started, until a report has carried it: por (power-on or
//      brown-out), pin (Reset button), wdg (watchdog), flt (fault), sw, lp,
//      or - once it has gone out
//   c  cycles run (5 min each): how long the device kept going
//   w  report windows that did not get through
//   u  of those, how many found the module waiting to be paired
//   m  the module's Wi-Fi state as the last of them closed, in hex: 02 no
//      router, 03 router but no cloud, 04 cloud but the report was not
//      confirmed, 00/01/06 pairing, ff the module never spoke
//   s  cycles whose sensor read failed
//
// It is only sent when there is something to say (Diag_IsNotable): after a
// reset, or after windows or sensor reads failed. A normal report leaves it out.
//
// Kept in BSP_Retained words, which survive the Reset button, the watchdog and
// a fault, but not a power-on. A checksum tells a record from what the RAM
// came up with.

/* The module's Wi-Fi state before it has said anything: the SDK's
 * WIFI_SATE_UNKNOW. */
#define DIAG_STATE_NONE 0xFFu

/* `reset` once a report has carried it. */
#define DIAG_RESET_REPORTED 0xFFu

/* The longest text Diag_Format writes, with no terminator. The SDK builds a
 * string DP in a 48-byte buffer with no bounds check, and the DP header takes
 * 4 of those. */
#define DIAG_TEXT_MAX 40u

/* How many words Diag_Store fills. */
#define DIAG_WORDS 3u

typedef struct {
    uint16_t cycles;       /* saturates at 65535 */
    uint16_t sensor_fails; /* saturates at 65535 */
    uint8_t  windows;      /* saturates at 255 */
    uint8_t  unpaired;     /* saturates at 255 */
    uint8_t  module_state; /* or DIAG_STATE_NONE */
    uint8_t  reset;        /* a BSP_ResetType, or DIAG_RESET_REPORTED */
} DiagRecord;

/* Nothing to say: no cycles, no failures, the reset reported. */
void Diag_Clear(DiagRecord* rec);

/* One cycle ran; `have_reading` says whether it read the sensor. */
void Diag_CountCycle(DiagRecord* rec, bool have_reading);

/* A report window did not get through. `unpaired` says it found the module
 * waiting to be paired; `module_state` is the Wi-Fi state the module last
 * reported in it. */
void Diag_CountFailedWindow(DiagRecord* rec, bool unpaired, uint8_t module_state);

/* Whether a report should carry the record: after a reset, or after windows
 * or sensor reads failed. */
bool Diag_IsNotable(const DiagRecord* rec);

/* Into, and back out of, the retained words. Diag_Restore returns false for
 * words that do not hold a record -- a power-on -- and leaves `rec` alone. */
void Diag_Store(const DiagRecord* rec, uint32_t words[DIAG_WORDS]);
bool Diag_Restore(const uint32_t words[DIAG_WORDS], DiagRecord* rec);

/* The line of text above, into `text` (at least DIAG_TEXT_MAX bytes, not
 * terminated). Returns its length. */
uint8_t Diag_Format(const DiagRecord* rec, char* text);
