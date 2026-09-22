#include "bsp_mock.h"

#include "bsp.h"

#include <string.h>

#define SENSOR_RESPONSE_MAX 8
#define WIFI_TX_CAPACITY    256

static uint32_t now_ms;

static int      sleep_calls;
static uint32_t last_sleep_request_ms;
static uint32_t total_sleep_requested_ms;
static uint32_t early_wake_ms;
static int      early_wake_remaining;

static uint8_t  sensor_response[SENSOR_RESPONSE_MAX];
static uint16_t sensor_response_len;
static bool     sensor_write_fails;
static bool     sensor_read_fails;
static int      sensor_write_calls;
static int      sensor_read_calls;
static uint8_t  sensor_last_address;
static uint8_t  sensor_last_cmd_msb;
static uint8_t  sensor_last_cmd_lsb;

static uint16_t battery_mv;
static uint32_t battery_last_read_ms;
static int      battery_read_calls;

static bool button_event;
static bool button_down;

static bool     wifi_powered;
static int      wifi_power_on_calls;
static int      wifi_power_off_calls;
static uint32_t wifi_last_power_on_ms;
static uint32_t wifi_last_window_ms;

static uint8_t wifi_tx[WIFI_TX_CAPACITY];
static int     wifi_tx_count;

static uint8_t*            wifi_rx_buffer;
static BSP_void_callback_t wifi_rx_callback;
static BSP_void_callback_t wifi_rx_error_callback;
static int                 wifi_rx_arm_calls;
static int                 wifi_abort_receive_calls;

static BSP_ResetType reset_reason;
static int           reset_calls;
static bool          spare_set;

void bspMockReset(void)
{
    now_ms = 0u;

    sleep_calls              = 0;
    last_sleep_request_ms    = 0u;
    total_sleep_requested_ms = 0u;
    early_wake_ms            = 0u;
    early_wake_remaining     = 0;

    memset(sensor_response, 0, sizeof(sensor_response));
    sensor_response_len = 0u;
    sensor_write_fails  = false;
    sensor_read_fails   = false;
    sensor_write_calls  = 0;
    sensor_read_calls   = 0;
    sensor_last_address = 0u;
    sensor_last_cmd_msb = 0u;
    sensor_last_cmd_lsb = 0u;

    battery_mv           = 0u;
    battery_last_read_ms = 0u;
    battery_read_calls   = 0;

    button_event = false;
    button_down  = false;

    wifi_powered          = false;
    wifi_power_on_calls   = 0;
    wifi_power_off_calls  = 0;
    wifi_last_power_on_ms = 0u;
    wifi_last_window_ms   = 0u;

    memset(wifi_tx, 0, sizeof(wifi_tx));
    wifi_tx_count = 0;

    wifi_rx_buffer           = NULL;
    wifi_rx_callback         = NULL;
    wifi_rx_error_callback   = NULL;
    wifi_rx_arm_calls        = 0;
    wifi_abort_receive_calls = 0;

    reset_reason = BSP_Reset_PowerOn;
    reset_calls  = 0;
    spare_set    = false;
}

void BSP_Init(void)
{
    /* The real one latches the reset reason and parks the board; there is
     * nothing to latch here and bspMockReset already parked it. */
}

/* --- Clock ---------------------------------------------------------------- */

uint32_t BSP_GetTimeMs(void)
{
    return now_ms;
}

void BSP_DelayMs(uint32_t ms)
{
    now_ms += ms;
}

void BSP_Idle(void)
{
    /* One millisecond, which is the floor bsp.h promises: on the target
     * SysTick wakes the core even when nothing else does. Modelling it is what
     * makes a Tuya_WaitUntil timeout expire here at all -- without it the wait
     * loop would spin forever against a clock that never moves. */
    now_ms += 1u;
}

void bspMockAdvanceMs(uint32_t ms)
{
    now_ms += ms;
}

uint32_t bspMockNowMs(void)
{
    return now_ms;
}

/* --- Sleep ---------------------------------------------------------------- */

uint32_t BSP_McuSleep(uint32_t sleep_time_ms)
{
    sleep_calls++;
    last_sleep_request_ms = sleep_time_ms;
    total_sleep_requested_ms += sleep_time_ms;

    uint32_t slept_ms = sleep_time_ms;

    if ((early_wake_remaining > 0) && (early_wake_ms < sleep_time_ms))
    {
        early_wake_remaining--;
        slept_ms = early_wake_ms;
    }

    now_ms += slept_ms;

    return slept_ms;
}

int bspMockSleepCalls(void)
{
    return sleep_calls;
}

uint32_t bspMockLastSleepRequestMs(void)
{
    return last_sleep_request_ms;
}

uint32_t bspMockTotalSleepRequestedMs(void)
{
    return total_sleep_requested_ms;
}

void bspMockSetEarlyWake(uint32_t ms, int count)
{
    early_wake_ms        = ms;
    early_wake_remaining = count;
}

void BSP_McuReset(void)
{
    reset_calls++;
}

int bspMockResetCalls(void)
{
    return reset_calls;
}

BSP_ResetType BSP_GetResetReason(void)
{
    return reset_reason;
}

void bspMockSetResetReason(int reason)
{
    reset_reason = (BSP_ResetType) reason;
}

/* --- Sensor bus ----------------------------------------------------------- */

bool BSP_Sensor_I2cWrite(uint8_t address, const uint8_t* data, uint16_t len)
{
    sensor_write_calls++;
    sensor_last_address = address;

    if (len >= 2u)
    {
        sensor_last_cmd_msb = data[0];
        sensor_last_cmd_lsb = data[1];
    }

    return !sensor_write_fails;
}

bool BSP_Sensor_I2cRead(uint8_t address, uint8_t* data, uint16_t len)
{
    sensor_read_calls++;
    sensor_last_address = address;

    if (sensor_read_fails || (len > sensor_response_len))
    {
        return false;
    }

    memcpy(data, sensor_response, len);

    return true;
}

void bspMockSensorSetResponse(const uint8_t* bytes, uint16_t len)
{
    if (len > SENSOR_RESPONSE_MAX)
    {
        len = SENSOR_RESPONSE_MAX;
    }

    memcpy(sensor_response, bytes, len);
    sensor_response_len = len;
}

void bspMockSensorSetWriteFails(bool fails)
{
    sensor_write_fails = fails;
}

void bspMockSensorSetReadFails(bool fails)
{
    sensor_read_fails = fails;
}

int bspMockSensorWriteCalls(void)
{
    return sensor_write_calls;
}

int bspMockSensorReadCalls(void)
{
    return sensor_read_calls;
}

uint8_t bspMockSensorLastAddress(void)
{
    return sensor_last_address;
}

uint8_t bspMockSensorLastCommandMsb(void)
{
    return sensor_last_cmd_msb;
}

uint8_t bspMockSensorLastCommandLsb(void)
{
    return sensor_last_cmd_lsb;
}

/* --- Battery -------------------------------------------------------------- */

uint16_t BSP_Battery_ReadMv(void)
{
    battery_read_calls++;
    battery_last_read_ms = now_ms;

    return battery_mv;
}

void bspMockSetBatteryMv(uint16_t mv)
{
    battery_mv = mv;
}

uint32_t bspMockBatteryLastReadMs(void)
{
    return battery_last_read_ms;
}

int bspMockBatteryReadCalls(void)
{
    return battery_read_calls;
}

/* --- Button --------------------------------------------------------------- */

bool BSP_Wifi_TakeButtonEvent(void)
{
    bool event = button_event;

    button_event = false;

    return event;
}

bool BSP_Wifi_ButtonIsPressed(void)
{
    return button_down;
}

void bspMockPressButton(void)
{
    button_event = true;
    button_down  = true;
}

void bspMockReleaseButton(void)
{
    button_down = false;
}

void bspMockGlitchButton(void)
{
    /* An edge with nothing behind it: the latch fires, the level never goes
     * down. This is what the application's debounce exists to throw away. */
    button_event = true;
    button_down  = false;
}

/* --- Wi-Fi module --------------------------------------------------------- */

void BSP_Wifi_PowerOn(void)
{
    wifi_powered = true;
    wifi_power_on_calls++;
    wifi_last_power_on_ms = now_ms;
}

void BSP_Wifi_PowerOff(void)
{
    if (wifi_powered)
    {
        wifi_last_window_ms = now_ms - wifi_last_power_on_ms;
    }

    wifi_powered = false;
    wifi_power_off_calls++;
}

bool bspMockWifiIsPowered(void)
{
    return wifi_powered;
}

int bspMockWifiPowerOnCalls(void)
{
    return wifi_power_on_calls;
}

int bspMockWifiPowerOffCalls(void)
{
    return wifi_power_off_calls;
}

uint32_t bspMockWifiLastPowerOnMs(void)
{
    return wifi_last_power_on_ms;
}

uint32_t bspMockWifiLastWindowMs(void)
{
    return wifi_last_window_ms;
}

void BSP_Wifi_TransmitByte(uint8_t value)
{
    if (wifi_tx_count < WIFI_TX_CAPACITY)
    {
        wifi_tx[wifi_tx_count] = value;
    }

    wifi_tx_count++;

    /* One byte at 9600 8N1, rounded to the millisecond the device would spend
     * blocked in it. Without this a report would appear to take no time at
     * all, and the drain wait would be the only thing on the clock. */
    now_ms += 1u;
}

int bspMockWifiTxCount(void)
{
    return wifi_tx_count;
}

uint8_t bspMockWifiTxAt(int index)
{
    if ((index < 0) || (index >= wifi_tx_count) || (index >= WIFI_TX_CAPACITY))
    {
        return 0u;
    }

    return wifi_tx[index];
}

void BSP_Wifi_Receive(uint8_t* received_byte, BSP_void_callback_t cb)
{
    wifi_rx_buffer   = received_byte;
    wifi_rx_callback = cb;
    wifi_rx_arm_calls++;
}

void BSP_Wifi_AbortReceive(void)
{
    wifi_rx_buffer   = NULL;
    wifi_rx_callback = NULL;
    wifi_abort_receive_calls++;
}

void BSP_Wifi_SetRxErrorCallback(BSP_void_callback_t cb)
{
    wifi_rx_error_callback = cb;
}

bool bspMockWifiRxArmed(void)
{
    return (wifi_rx_callback != NULL);
}

int bspMockWifiRxArmCalls(void)
{
    return wifi_rx_arm_calls;
}

int bspMockWifiAbortReceiveCalls(void)
{
    return wifi_abort_receive_calls;
}

void bspMockWifiDeliverByte(uint8_t value)
{
    if ((wifi_rx_buffer == NULL) || (wifi_rx_callback == NULL))
    {
        /* Nothing armed: on the device the byte would be lost in silence, and
         * so it is here. */
        return;
    }

    BSP_void_callback_t cb = wifi_rx_callback;

    *wifi_rx_buffer = value;

    cb();
}

void bspMockWifiRaiseRxError(void)
{
    /* An overrun on the device ends the receive before the completion callback
     * can re-arm it, so the arm is dropped here first. Whether the link comes
     * back is then entirely down to the error callback. */
    wifi_rx_buffer   = NULL;
    wifi_rx_callback = NULL;

    if (wifi_rx_error_callback != NULL)
    {
        wifi_rx_error_callback();
    }
}

/* --- Misc ----------------------------------------------------------------- */

void BSP_Spare_Set(bool on)
{
    spare_set = on;
}

bool bspMockSpareIsSet(void)
{
    return spare_set;
}
