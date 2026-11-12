#include "tuya_link.h"

#include "bsp.h"
#include "diag_record.h"

/* protocol.h ahead of mcu_api.h: it holds the switches mcu_api.h tests, and
 * mcu_api.h does not include it. Behind it, MCU_DP_UPLOAD_SYN is undefined and
 * the synchronous senders are not declared. */
#include "protocol.h"
#include "mcu_api.h"
#include "wifi.h"

/* A string DP is built in the SDK's send buffer with no bounds check: four
 * bytes of DP header and then the text. */
_Static_assert(4u + DIAG_TEXT_MAX <= WIFIR_UART_SEND_BUF_LMT, "DP 104 overruns the SDK's send buffer");

/* The byte the receive interrupt lands in. One byte, permanently re-armed: see
 * BSP_Wifi_Receive. */
static uint8_t rx_byte;

/* Last complete set of readings, and whether there is one yet. Answered to the
 * module whenever it asks, from interrupt-adjacent SDK context, so it is only
 * ever written between windows -- see Tuya_SetDps. */
static struct {
    int16_t       temp_c10;
    uint8_t       rh_pct;
    uint8_t       battery_pct;
    Battery_State battery_state;
    bool          battery_damaged;
    bool          valid;
} dps;

/* DP 101 and DP 103, or 0 for nothing to report. Their own variables rather
 * than part of the set above, because they have their own source and their
 * own "nothing yet" -- see Tuya_SetBatteryMinMv. Written between windows
 * only, like the set. */
static uint16_t battery_min_mv;
static uint16_t battery_average_min_mv;

/* DP 104, or diag_len 0 for none. Written between windows only. */
static char    diag_text[DIAG_TEXT_MAX];
static uint8_t diag_len;

/* The synchronous push: how many answers it asked for (0 before one has gone
 * out), how many are in, and whether any was a refusal. Reset by every push
 * and every Tuya_PowerOn, so an answer can only count in the window that
 * asked. */
static uint8_t acks_expected;
static uint8_t acks_received;
static bool    ack_refused;

/* A pairing state has been reported since the last Tuya_StartPairing. Watched
 * on every Tuya_Service pass: see Tuya_PairingEntered. */
static bool pairing_entered;

/* DP 101 goes out only below this. Above it the boost still has plenty of
 * margin, and the DP would only add noise to the app's history. */
#define BATTERY_MIN_REPORT_BELOW_MV 1000u

/* The module is offering pairing: SmartConfig, AP, or both at once. */
static bool IsPairingState(uint8_t state)
{
    return (state == SMART_CONFIG_STATE) || (state == AP_STATE) || (state == SMART_AND_AP_STATE);
}

static void OnByteReceived(void)
{
    /* Re-arm before parsing, not after.
     *
     * The module is free to send back-to-back bytes, and at 9600 a byte is
     * ~1 ms. Parsing first leaves the UART unarmed for as long as
     * uart_receive_input() takes; re-arming first shrinks that window to a
     * couple of instructions. The byte is taken into a local because the
     * re-arm hands `rx_byte` back to the peripheral. */
    uint8_t value = rx_byte;

    BSP_Wifi_Receive(&rx_byte, OnByteReceived);

    /* Ring-buffer push only -- no parsing happens here. The SDK does the work
     * in wifi_uart_service(), from Tuya_Service(), in thread context. */
    uart_receive_input(value);
}

static void OnRxError(void)
{
    /* An aborting UART error (overrun, framing, noise) ends the receive
     * WITHOUT running the completion callback, so the re-arm at the top of
     * OnByteReceived never happens and the link goes deaf for the rest of the
     * window. This hook is the only thing that gets it back.
     *
     * It is not a rare path: the T3-3S guarantees at least one framing error
     * per window, because its boot ROM prints on TX at a different baud rate
     * before the application firmware takes the port to 9600. */
    BSP_Wifi_Receive(&rx_byte, OnByteReceived);
}

void Tuya_Init(void)
{
    dps.valid              = false;
    battery_min_mv         = 0u;
    battery_average_min_mv = 0u;
    diag_len               = 0u;
    acks_expected          = 0u;
    acks_received          = 0u;
    ack_refused            = false;

    BSP_Wifi_SetRxErrorCallback(OnRxError);

    /* Dormant from boot. The module is released only when a cycle has
     * something to report or somebody presses the pairing button. */
    Tuya_PowerOff();
}

void Tuya_PowerOn(void)
{
    /* Supply and UART first: the BSP owns the ordering between the two, and
     * there is nothing to receive until the module has power anyway. */
    BSP_Wifi_PowerOn();

    /* The module boots from scratch on every window, so the parser must too.
     * This drops whatever was left in the ring and the frame buffer from
     * last time and puts the cached Wi-Fi state back to WIFI_SATE_UNKNOW,
     * which is what Tuya_IsAlive() tests. Carrying any of it across a
     * window would have the
     * cycle believe the module was already connected while it was still
     * booting. */
    wifi_protocol_init();

    /* No answer from an earlier window counts in this one. */
    acks_expected = 0u;
    acks_received = 0u;
    ack_refused   = false;

    /* Armed last, so nothing can land in the ring between the reset above and
     * the receiver being ready for it. The module's own boot takes tens of
     * milliseconds, so there is no race with the supply coming up. */
    BSP_Wifi_Receive(&rx_byte, OnByteReceived);
}

void Tuya_PowerOff(void)
{
    /* Disarm first. An arm left standing across the power cycle makes the next
     * BSP_Wifi_Receive report busy, and the module would boot into a receiver
     * that is not listening. */
    BSP_Wifi_AbortReceive();

    BSP_Wifi_PowerOff();
}

void Tuya_Service(void)
{
    wifi_uart_service();

    /* Here, on every pass, because the state the module reports is only ever
     * the latest one, and a pairing state lasts at least as long as somebody
     * takes to pair it. Not Tuya_IsUnpaired(): a module asleep in low power
     * has no network either, but it is not offering pairing. */
    if (IsPairingState(mcu_get_wifi_work_state()))
    {
        pairing_entered = true;
    }
}

bool Tuya_WaitUntil(bool (*condition)(void), uint32_t timeout_ms)
{
    uint32_t start_ms = BSP_GetTimeMs();

    for (;;)
    {
        BSP_Watchdog_Refresh();

        Tuya_Service();

        /* Tested after the service call, so a condition that the bytes just
         * parsed have satisfied is seen on this pass rather than after another
         * idle. */
        if ((condition != 0) && condition())
        {
            return true;
        }

        /* ...and the deadline after the condition, so a condition met exactly
         * at the deadline still counts as met. */
        if ((BSP_GetTimeMs() - start_ms) >= timeout_ms)
        {
            return false;
        }

        /* Idle rather than spin. Bounded by the BSP even when the module says
         * nothing at all, so the loop keeps re-testing and the timeout always
         * expires. */
        BSP_Idle();
    }
}

void Tuya_ServiceFor(uint32_t ms)
{
    (void) Tuya_WaitUntil(0, ms);
}

bool Tuya_IsAlive(void)
{
    return (mcu_get_wifi_work_state() != WIFI_SATE_UNKNOW);
}

bool Tuya_IsCloudConnected(void)
{
    return (mcu_get_wifi_work_state() == WIFI_CONN_CLOUD);
}

bool Tuya_IsUnpaired(void)
{
    /* The three pairing modes, and low power (0x05, WIFI_LOW_POWER): where a
     * module with no network goes once it has offered pairing for its three
     * minutes and nobody took it up. The board seen on 2026-11-12 reported it in
     * two report windows after a flash (DP 104 m=05), each waiting out the full
     * cloud timeout as if a router were down, until the button paired it.
     *
     * A module that has a network reports 0x02, 0x03 or 0x04 instead, even when
     * it cannot reach that network -- a router that is down looks like that,
     * not like this. */
    uint8_t state = mcu_get_wifi_work_state();

    return IsPairingState(state) || (state == WIFI_LOW_POWER);
}

uint8_t Tuya_WifiState(void)
{
    return mcu_get_wifi_work_state();
}

void Tuya_StartPairing(void)
{
    /* Reset (0x04), not "reset and select mode" (0x05): 0x05 picks between the
     * old EZ broadcast and AP, and the T3-3S pairs over Bluetooth or AP. 0x04
     * lets the module use its own default, which on a Wi-Fi + Bluetooth module
     * resets both and pairs over either.
     *
     * mcu_reset_wifi() also clears the answer and the cached state (PORTED,
     * ../../../tuya/mcu_api.c), so a pairing state seen from here on is one the
     * module reported after this command, not before it. */
    pairing_entered = false;

    mcu_reset_wifi();
}

bool Tuya_PairingAnswered(void)
{
    return (mcu_get_reset_wifi_flag() == RESET_WIFI_SUCCESS);
}

bool Tuya_PairingEntered(void)
{
    return pairing_entered;
}

void Tuya_SetDps(int16_t temp_c10,
                 uint8_t rh_pct,
                 uint8_t battery_pct,
                 Battery_State battery_state,
                 bool battery_damaged)
{
    /* Written from the cycle only, and only with the module unpowered, so the
     * SDK cannot be halfway through reading the set while it changes: nothing
     * calls all_data_update() when there is no module to answer. */
    dps.temp_c10      = temp_c10;
    dps.rh_pct        = rh_pct;
    dps.battery_pct   = battery_pct;
    dps.battery_state   = battery_state;
    dps.battery_damaged = battery_damaged;
    dps.valid           = true;
}

void Tuya_SetBatteryMinMv(uint16_t min_mv, uint16_t average_min_mv)
{
    /* Same rule as Tuya_SetDps: only between windows. */
    battery_min_mv         = min_mv;
    battery_average_min_mv = average_min_mv;
}

void Tuya_SetDiag(const char* text, uint8_t len)
{
    if (len > DIAG_TEXT_MAX)
    {
        len = DIAG_TEXT_MAX;
    }

    for (uint8_t i = 0u; i < len; i++)
    {
        diag_text[i] = text[i];
    }

    diag_len = len;
}

/* Both pushes. `with_ack` sends the temperature, last, as a synchronous
 * report, and expects its answer.
 *
 * One synchronous report, not one per reading: the SDK's own notes
 * (protocol.h, item 14) say no second one may go out before the module has
 * answered the first, and the module takes up to 5 s to answer when the
 * network is poor. The temperature goes last so that its answer covers
 * everything sent before it on the same connection. */
static void ReportDps(bool with_ack)
{
    /* First, because they do not depend on the reading set below. */
    if ((battery_min_mv != 0u) && (battery_min_mv < BATTERY_MIN_REPORT_BELOW_MV))
    {
        (void) mcu_dp_value_update(DPID_BATTERY_MIN_VOLTAGE, (u32) battery_min_mv);
    }

    /* No threshold: until the gauge is calibrated against it, every value is
     * worth having in the history. */
    if (battery_average_min_mv != 0u)
    {
        (void) mcu_dp_value_update(DPID_BATTERY_AVG_MIN_VOLTAGE, (u32) battery_average_min_mv);
    }

    if (diag_len != 0u)
    {
        (void) mcu_dp_string_update(DPID_DIAG, (const u8*) diag_text, diag_len);
    }

    if (!dps.valid)
    {
        /* The module can query before this device has measured anything --
         * it queries on every reconnect, and a pairing window can open from
         * the button before the first cycle completes. Reporting nothing is
         * right; reporting zeroes would put -45 C and 0 % into the app's
         * history. */
        return;
    }

    (void) mcu_dp_value_update(DPID_HUMIDITY_VALUE, (u32) dps.rh_pct);
    (void) mcu_dp_enum_update(DPID_BATTERY_STATE, (u8) dps.battery_state);
    (void) mcu_dp_value_update(DPID_BATTERY_PERCENTAGE, (u32) dps.battery_pct);

    /* Sent false as well as true, so the app clears the flag once a good cell
     * goes in. */
    (void) mcu_dp_bool_update(DPID_BATTERY_DAMAGED, dps.battery_damaged ? 1u : 0u);

    /* DP 1 is a signed value with range -200..600, i.e. 0.1 C units, which is
     * exactly what temp_c10 already is -- no scaling here. Cast through int32
     * so a sub-zero temperature sign-extends to all 32 bits before
     * mcu_dp_value_update() takes it apart; the cloud reads the four bytes as
     * a two's-complement int32. */
    u32 temp_wire = (u32) (int32_t) dps.temp_c10;

    if (!with_ack)
    {
        (void) mcu_dp_value_update(DPID_TEMP_CURRENT, temp_wire);

        return;
    }

    /* Before sending: the answer can come back from inside the next service
     * pass, and must find the count ready for it. */
    acks_expected = 1u;
    acks_received = 0u;
    ack_refused   = false;

    (void) mcu_dp_value_update_syn(DPID_TEMP_CURRENT, temp_wire);
}

void Tuya_ReportCachedDps(void)
{
    ReportDps(false);
}

void Tuya_ReportCachedDpsWithAck(void)
{
    ReportDps(true);
}

bool Tuya_ReportAnswered(void)
{
    return (acks_expected != 0u) && (acks_received >= acks_expected);
}

bool Tuya_ReportConfirmed(void)
{
    return Tuya_ReportAnswered() && !ack_refused;
}

void Tuya_OnSyncResult(uint8_t result)
{
    if (acks_received < UINT8_MAX)
    {
        acks_received++;
    }

    if (result == 0u)
    {
        ack_refused = true;
    }
}
