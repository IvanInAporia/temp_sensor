#include "tuya_sdk_mock.h"

#include "mcu_api.h"
#include "wifi.h"

#include <string.h>

#define DP_CAPACITY 32
#define RX_CAPACITY 256

static uint8_t wifi_state;

/* The scripted boot: `schedule_state` takes effect once wifi_uart_service has
 * been called `schedule_calls` times counting from `schedule_base`.
 *
 * Re-armed by wifi_protocol_init, because that is what the real module forces:
 * it is power-gated, so every report window is a cold boot that has to
 * associate again from nothing. A schedule that only fired once would model a
 * module that stays connected between windows, which this one never is.
 */
static int     schedule_calls;
static uint8_t schedule_state;
static int     schedule_base;
static bool    schedule_set;
static bool    schedule_armed;

static int service_calls;
static int protocol_init_calls;
static int set_wifi_mode_calls;
static uint8_t last_wifi_mode;

static uint8_t rx_bytes[RX_CAPACITY];
static int     rx_byte_count;

static TuyaSdkMockDp dps[DP_CAPACITY];
static int           dp_count;

void tuyaSdkMockReset(void)
{
    wifi_state = WIFI_SATE_UNKNOW;

    schedule_calls = 0;
    schedule_state = WIFI_SATE_UNKNOW;
    schedule_base  = 0;
    schedule_set   = false;
    schedule_armed = false;

    service_calls       = 0;
    protocol_init_calls = 0;
    set_wifi_mode_calls = 0;
    last_wifi_mode      = 0xFFu;

    memset(rx_bytes, 0, sizeof(rx_bytes));
    rx_byte_count = 0;

    memset(dps, 0, sizeof(dps));
    dp_count = 0;
}

void tuyaSdkMockSetWifiState(uint8_t state)
{
    wifi_state     = state;
    schedule_set   = false;
    schedule_armed = false;
}

void tuyaSdkMockSetWifiStateAfter(int calls, uint8_t state)
{
    schedule_calls = calls;
    schedule_state = state;
    schedule_base  = service_calls;
    schedule_set   = true;
    schedule_armed = true;
}

/* --- SDK entry points the glue calls -------------------------------------- */

void wifi_protocol_init(void)
{
    protocol_init_calls++;

    /* The real one drops the ring and resets the cached work state, which is
     * exactly why Tuya_PowerOn has to call it on every window. Modelled,
     * because a test that asserts "the glue does not believe a cold-booting
     * module is already connected" needs this to actually happen. */
    wifi_state = WIFI_SATE_UNKNOW;

    /* ...and the scripted boot restarts with it: the module is unpowered
     * between windows, so it associates again from nothing every time. */
    if (schedule_set)
    {
        schedule_base  = service_calls;
        schedule_armed = true;
    }
}

void wifi_uart_service(void)
{
    service_calls++;

    if (schedule_armed && ((service_calls - schedule_base) >= schedule_calls))
    {
        wifi_state     = schedule_state;
        schedule_armed = false;
    }
}

void uart_receive_input(unsigned char value)
{
    if (rx_byte_count < RX_CAPACITY)
    {
        rx_bytes[rx_byte_count] = (uint8_t) value;
    }

    rx_byte_count++;
}

unsigned char mcu_get_wifi_work_state(void)
{
    return wifi_state;
}

void mcu_set_wifi_mode(unsigned char mode)
{
    set_wifi_mode_calls++;
    last_wifi_mode = (uint8_t) mode;
}

static void RecordDp(uint8_t dpid, bool is_enum, uint32_t value)
{
    if (dp_count < DP_CAPACITY)
    {
        dps[dp_count].dpid    = dpid;
        dps[dp_count].is_enum = is_enum;
        dps[dp_count].value   = value;
    }

    dp_count++;
}

unsigned char mcu_dp_value_update(unsigned char dpid, unsigned long value)
{
    /* Truncated to 32 bits deliberately: that is the width int_to_byte puts on
     * the wire, and `unsigned long` is 64 bits on some hosts. Keeping the
     * capture at wire width is what lets a test assert the two's-complement
     * form of a sub-zero temperature. */
    RecordDp((uint8_t) dpid, false, (uint32_t) value);

    return SUCCESS;
}

unsigned char mcu_dp_enum_update(unsigned char dpid, unsigned char value)
{
    RecordDp((uint8_t) dpid, true, (uint32_t) value);

    return SUCCESS;
}

/* --- Accessors ------------------------------------------------------------ */

int tuyaSdkMockServiceCalls(void)
{
    return service_calls;
}

int tuyaSdkMockProtocolInitCalls(void)
{
    return protocol_init_calls;
}

int tuyaSdkMockSetWifiModeCalls(void)
{
    return set_wifi_mode_calls;
}

uint8_t tuyaSdkMockLastWifiMode(void)
{
    return last_wifi_mode;
}

int tuyaSdkMockRxByteCount(void)
{
    return rx_byte_count;
}

uint8_t tuyaSdkMockRxByteAt(int index)
{
    if ((index < 0) || (index >= rx_byte_count) || (index >= RX_CAPACITY))
    {
        return 0u;
    }

    return rx_bytes[index];
}

int tuyaSdkMockDpCount(void)
{
    return dp_count;
}

bool tuyaSdkMockDpAt(int index, TuyaSdkMockDp* out)
{
    if ((index < 0) || (index >= dp_count) || (index >= DP_CAPACITY) || (out == NULL))
    {
        return false;
    }

    *out = dps[index];

    return true;
}

bool tuyaSdkMockLastDpValue(uint8_t dpid, uint32_t* out_value)
{
    int limit = (dp_count < DP_CAPACITY) ? dp_count : DP_CAPACITY;

    for (int i = limit - 1; i >= 0; i--)
    {
        if (dps[i].dpid == dpid)
        {
            if (out_value != NULL)
            {
                *out_value = dps[i].value;
            }

            return true;
        }
    }

    return false;
}
