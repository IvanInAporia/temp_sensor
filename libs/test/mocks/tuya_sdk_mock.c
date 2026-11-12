#include "tuya_sdk_mock.h"

#include "protocol.h" /* ahead of mcu_api.h, as in tuya_link.c */
#include "mcu_api.h"
#include "tuya_link.h"
#include "wifi.h"

#include <string.h>

/* Past this, reports are counted but not kept, and tuyaSdkMockLastDpValue sees
 * only the first ones: a test of many windows would read stale values. Six DPs
 * a window, so this covers twenty windows. */
#define DP_CAPACITY 128
#define RX_CAPACITY 256

#define STRING_CAPACITY 32
#define STRING_MAX      47

static char strings[STRING_CAPACITY][STRING_MAX + 1];
static int  string_count;

static TuyaSdkMockSyncAnswer sync_answer;

/* Synchronous reports sent and not yet answered. */
static int sync_pending;

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
static int reset_wifi_calls;

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
    reset_wifi_calls    = 0;

    memset(rx_bytes, 0, sizeof(rx_bytes));
    rx_byte_count = 0;

    memset(dps, 0, sizeof(dps));
    dp_count = 0;

    memset(strings, 0, sizeof(strings));
    string_count = 0;

    sync_answer  = TUYA_MOCK_SYNC_SILENT;
    sync_pending = 0;
}

void tuyaSdkMockSetSyncAnswer(TuyaSdkMockSyncAnswer answer)
{
    sync_answer = answer;
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

/* The module starts over from nothing: the cached work state is forgotten and
 * the scripted boot runs again from the top. */
static void ModuleRestarts(void)
{
    wifi_state = WIFI_SATE_UNKNOW;

    if (schedule_set)
    {
        schedule_base  = service_calls;
        schedule_armed = true;
    }
}

void wifi_protocol_init(void)
{
    protocol_init_calls++;

    /* The module that owed an answer has been powered off. */
    sync_pending = 0;

    /* The real one drops the ring and resets the cached work state, which is
     * exactly why Tuya_PowerOn has to call it on every window. Modelled,
     * because a test that asserts "the glue does not believe a cold-booting
     * module is already connected" needs this to actually happen. The module
     * is unpowered between windows, so it associates again from nothing every
     * time. */
    ModuleRestarts();
}

void wifi_uart_service(void)
{
    service_calls++;

    if (schedule_armed && ((service_calls - schedule_base) >= schedule_calls))
    {
        wifi_state     = schedule_state;
        schedule_armed = false;
    }

    /* One answer per pass, as the real one parses one 0x23 frame per answer.
     * On the target, system.c hands it to get_upload_syn_result (protocol.c),
     * which forwards it here unchanged. */
    if ((sync_pending > 0) && (sync_answer != TUYA_MOCK_SYNC_SILENT))
    {
        sync_pending--;
        Tuya_OnSyncResult((sync_answer == TUYA_MOCK_SYNC_TAKEN) ? 1u : 0u);
    }
}

void uart_receive_input(u8 value)
{
    if (rx_byte_count < RX_CAPACITY)
    {
        rx_bytes[rx_byte_count] = (uint8_t) value;
    }

    rx_byte_count++;
}

u8 mcu_get_wifi_work_state(void)
{
    return wifi_state;
}

void mcu_reset_wifi(void)
{
    reset_wifi_calls++;

    /* The real module restarts on the reset command and comes back in pairing
     * mode, and the real mcu_reset_wifi() forgets the cached state with it
     * (PORTED, ../../../tuya/mcu_api.c). So a module that was already online
     * when the button was pressed is not online any more until the scripted
     * boot says so again -- which is what keeps a pairing window from taking
     * the old connection for the new one. */
    ModuleRestarts();
}

static void RecordDp(uint8_t dpid, bool is_enum, uint32_t value)
{
    if (dp_count < DP_CAPACITY)
    {
        dps[dp_count].dpid    = dpid;
        dps[dp_count].is_enum = is_enum;
        dps[dp_count].sync    = false;
        dps[dp_count].value   = value;
    }

    dp_count++;
}

u8 mcu_dp_value_update_syn(u8 dpid, u32 value)
{
    RecordDp((uint8_t) dpid, false, (uint32_t) value);

    if (dp_count <= DP_CAPACITY)
    {
        dps[dp_count - 1].sync = true;
    }

    sync_pending++;

    return SUCCESS;
}

u8 mcu_dp_string_update(u8 dpid, const u8 value[], u16 len)
{
    RecordDp((uint8_t) dpid, false, (uint32_t) len);

    if (string_count < STRING_CAPACITY)
    {
        uint16_t kept = (len > STRING_MAX) ? STRING_MAX : len;

        memcpy(strings[string_count], value, kept);
        strings[string_count][kept] = '\0';
    }

    string_count++;

    return SUCCESS;
}

u8 mcu_dp_value_update(u8 dpid, u32 value)
{
    /* Captured at wire width, which is what lets a test assert the
     * two's-complement form of a sub-zero temperature. */
    RecordDp((uint8_t) dpid, false, (uint32_t) value);

    return SUCCESS;
}

u8 mcu_dp_enum_update(u8 dpid, u8 value)
{
    RecordDp((uint8_t) dpid, true, (uint32_t) value);

    return SUCCESS;
}

u8 mcu_dp_bool_update(u8 dpid, u8 value)
{
    /* The real one sends any non-zero value as 1. */
    RecordDp((uint8_t) dpid, false, (value != 0u) ? 1u : 0u);

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

int tuyaSdkMockResetWifiCalls(void)
{
    return reset_wifi_calls;
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

int tuyaSdkMockStringCount(void)
{
    return string_count;
}

const char* tuyaSdkMockStringAt(int index)
{
    if ((index < 0) || (index >= string_count) || (index >= STRING_CAPACITY))
    {
        return "";
    }

    return strings[index];
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
