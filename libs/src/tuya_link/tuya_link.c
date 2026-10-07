#include "tuya_link.h"

#include "bsp.h"
#include "mcu_api.h"
#include "protocol.h"
#include "wifi.h"

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
    bool          valid;
} dps;

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
    dps.valid = false;

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
}

bool Tuya_WaitUntil(bool (*condition)(void), uint32_t timeout_ms)
{
    uint32_t start_ms = BSP_GetTimeMs();

    for (;;)
    {
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

void Tuya_StartPairing(void)
{
    mcu_set_wifi_mode(SMART_CONFIG);
}

void Tuya_SetDps(int16_t temp_c10,
                 uint8_t rh_pct,
                 uint8_t battery_pct,
                 Battery_State battery_state)
{
    /* Written from the cycle only, and only with the module unpowered, so the
     * SDK cannot be halfway through reading the set while it changes: nothing
     * calls all_data_update() when there is no module to answer. */
    dps.temp_c10      = temp_c10;
    dps.rh_pct        = rh_pct;
    dps.battery_pct   = battery_pct;
    dps.battery_state = battery_state;
    dps.valid         = true;
}

void Tuya_ReportCachedDps(void)
{
    if (!dps.valid)
    {
        /* The module can query before this device has measured anything --
         * it queries on every reconnect, and a pairing window can open from
         * the button before the first cycle completes. Reporting nothing is
         * right; reporting zeroes would put -45 C and 0 % into the app's
         * history. */
        return;
    }

    /* DP 1 is a signed value with range -200..600, i.e. 0.1 C units, which is
     * exactly what temp_c10 already is -- no scaling here. Cast through int32
     * so a sub-zero temperature sign-extends before int_to_byte() takes it
     * apart; casting an int16 straight to unsigned long would send 0xFFCE-ish
     * garbage in the top two bytes on some toolchains. */
    (void) mcu_dp_value_update(DPID_TEMP_CURRENT, (unsigned long) (int32_t) dps.temp_c10);

    (void) mcu_dp_value_update(DPID_HUMIDITY_VALUE, (unsigned long) dps.rh_pct);
    (void) mcu_dp_enum_update(DPID_BATTERY_STATE, (unsigned char) dps.battery_state);
    (void) mcu_dp_value_update(DPID_BATTERY_PERCENTAGE, (unsigned long) dps.battery_pct);
}
