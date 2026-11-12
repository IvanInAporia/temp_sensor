#pragma once

// Host stand-in for the vendor Tuya MCU SDK (mcu_api.c / system.c / protocol.c).
//
// The SDK is a UART parser and a frame builder. None of that is this project's
// code and none of it is what the tests are about: what is under test is the
// glue above it (tuya_link.c) and the cycle above that -- when the module is
// powered, how long each wait is given, what is sent once the cloud is up, and
// that the supply is cut on every path out.
//
// So the SDK's entry points are captured rather than implemented, and the one
// thing the glue reads back from it -- the Wi-Fi work state -- becomes
// something a test scripts directly.

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Clear every counter and capture, and put the work state back to "module has
 * said nothing" (WIFI_SATE_UNKNOW). Call in SetUp().
 */
void tuyaSdkMockReset(void);

/* Force the value mcu_get_wifi_work_state() returns from now on. */
void tuyaSdkMockSetWifiState(uint8_t state);

/* Script the module's boot: the work state becomes `state` once
 * wifi_uart_service() has been called `calls` times.
 *
 * This is how a test says "the module reaches the cloud after a while" without
 * a clock, and how it says "it never does" -- just never schedule it. The glue
 * calls wifi_uart_service() once per pass of its wait loop, so `calls` is
 * effectively "how many polls it took".
 */
void tuyaSdkMockSetWifiStateAfter(int calls, uint8_t state);

int tuyaSdkMockServiceCalls(void);
int tuyaSdkMockProtocolInitCalls(void);

/* mcu_reset_wifi() calls, i.e. how many times the module was asked to pair --
 * including any tuyaSdkMockLoseResets threw away.
 */
int tuyaSdkMockResetWifiCalls(void);

/* --- How the module takes the reset command ------------------------------------ */

typedef enum {
    /* Answers on the next wifi_uart_service() pass, restarts, says it is
     * pairing (AP_STATE) a few passes later, and then boots as scripted from
     * there: a scripted cloud is somebody pairing it. The default. */
    TUYA_MOCK_RESET_PAIRS,

    /* Answers and restarts, but boots as scripted with no pairing in between:
     * back on the network it had. */
    TUYA_MOCK_RESET_IGNORED,

    /* Never gets it: no answer, no restart, and its scripted boot carries on.
     * The SDK's cached state is forgotten all the same, as the real
     * mcu_reset_wifi() forgets it before it transmits. */
    TUYA_MOCK_RESET_LOST,
} TuyaSdkMockResetAnswer;

/* How every reset command is taken from now on. Starts TUYA_MOCK_RESET_PAIRS. */
void tuyaSdkMockSetResetAnswer(TuyaSdkMockResetAnswer answer);

/* The next `count` reset commands are lost on the way, whatever the answer
 * above; the ones after them are taken as it says. */
void tuyaSdkMockLoseResets(int count);

/* Bytes handed to uart_receive_input(), i.e. what the receive interrupt path
 * actually pushed into the SDK.
 */
int     tuyaSdkMockRxByteCount(void);
uint8_t tuyaSdkMockRxByteAt(int index);

/* --- Captured DP reports -------------------------------------------------- */

typedef struct {
    uint8_t  dpid;
    bool     is_enum;
    bool     sync;  /* sent as a synchronous report (0x22), asking for an answer */
    uint32_t value; /* the raw value as it would go on the wire; a string's length */
} TuyaSdkMockDp;

int  tuyaSdkMockDpCount(void);
bool tuyaSdkMockDpAt(int index, TuyaSdkMockDp* out);

/* The most recent value reported for `dpid`, or false if it was never sent. */
bool tuyaSdkMockLastDpValue(uint8_t dpid, uint32_t* out_value);

/* String DPs, in the order they went out. The text is kept, terminated, up to
 * 47 characters; a test reads back what the cloud log would show. */
int         tuyaSdkMockStringCount(void);
const char* tuyaSdkMockStringAt(int index);

/* --- The module's answer to a synchronous report ------------------------------ */

typedef enum {
    TUYA_MOCK_SYNC_SILENT,  /* never answers: a module firmware without 0x22 */
    TUYA_MOCK_SYNC_TAKEN,   /* answers 1 */
    TUYA_MOCK_SYNC_REFUSED, /* answers 0: could not deliver it */
} TuyaSdkMockSyncAnswer;

/* How the module answers every synchronous report from now on. The answer
 * arrives on the next wifi_uart_service() pass, through Tuya_OnSyncResult --
 * where the real SDK's get_upload_syn_result (protocol.c) sends it. Starts
 * TUYA_MOCK_SYNC_SILENT, which is how the module behaved before the cycle
 * asked for answers.
 */
void tuyaSdkMockSetSyncAnswer(TuyaSdkMockSyncAnswer answer);

#ifdef __cplusplus
}
#endif
