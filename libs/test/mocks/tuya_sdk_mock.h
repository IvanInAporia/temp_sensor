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

int     tuyaSdkMockSetWifiModeCalls(void);
uint8_t tuyaSdkMockLastWifiMode(void);

/* Bytes handed to uart_receive_input(), i.e. what the receive interrupt path
 * actually pushed into the SDK.
 */
int     tuyaSdkMockRxByteCount(void);
uint8_t tuyaSdkMockRxByteAt(int index);

/* --- Captured DP reports -------------------------------------------------- */

typedef struct {
    uint8_t  dpid;
    bool     is_enum;
    uint32_t value; /* the raw value as it would go on the wire */
} TuyaSdkMockDp;

int  tuyaSdkMockDpCount(void);
bool tuyaSdkMockDpAt(int index, TuyaSdkMockDp* out);

/* The most recent value reported for `dpid`, or false if it was never sent. */
bool tuyaSdkMockLastDpValue(uint8_t dpid, uint32_t* out_value);

#ifdef __cplusplus
}
#endif
