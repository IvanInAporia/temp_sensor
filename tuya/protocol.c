/**********************************Copyright (c)**********************************
**                       Copyright (C), 2015-2026, Tuya Technology
**
**                             http://www.tuya.com
**
*********************************************************************************/
/**
 * @file    protocol.c
 * @author  Tuya Integrated Protocol Development Group
 * @version v2.6.2
 * @date    2026.02.04
 * @brief
 *                       *******Very Important, Please Read!!!********
 *          1. Users implement data delivery/reporting functions in this file
 *          2. DP ID/TYPE and data processing functions need to be implemented by the user according to the actual definition
 *          3. When certain macro definitions are enabled, functions requiring user implementation contain #error prompts inside. Please delete the #error after completing the function.
 */

/****************************** Disclaimer !!! *******************************
Due to the variety of MCU types and compilation environments, this code is for reference only. Users are responsible for the final code quality.
Tuya is not responsible for MCU functional results.
******************************************************************************/

/******************************************************************************
                            Porting Notes:
1: The MCU must directly call the wifi_uart_service() function inside mcu_api.c in the while loop.
2: After normal program initialization is complete, it is recommended not to disable UART interrupts. If disabling interrupts is necessary, the disabled time must be very short. Disabling interrupts may cause UART data packet loss.
3: Do not call reporting functions inside interrupt/timer interrupt handlers.
******************************************************************************/

#include "protocol.h"

#include "wifi.h"

/* PORTED: the two function bodies below are the only ones in this vendor file
   that reach out of it -- one to the board, one to the application's cached DP
   values.  Everything else here is Tuya's. */
#include "bsp.h"
#include "tuya_link.h"

#ifdef WEATHER_ENABLE
/**
 * @var    weather_choose
 * @brief  Weather data parameter selection array
 * @note   Users can customize the required parameters by commenting or uncommenting. Please pay attention to the changes.
 */
const i8* weather_choose[WEATHER_CHOOSE_CNT] = {
    "temp",
    "humidity",
    "condition",
    "pm25",
    /*"pressure",
    "realFeel",
    "uvi",
    "tips",
    "windDir",
    "windLevel",
    "windSpeed",
    "sunRise",
    "sunSet",
    "aqi",
    "so2 ",
    "rank",
    "pm10",
    "o3",
    "no2",
    "co",
    "conditionNum",*/
};
#endif

/******************************************************************************
                              Step 1: Initialization
1: Include "wifi.h" in files that need to use WiFi-related functions.
2: Call the wifi_protocol_init() function from the mcu_api.c file during MCU initialization.
3: Fill the MCU UART single-byte transmission function into the uart_transmit_output function in protocol.c, and delete the #error.
4: In the MCU UART receive function, call the uart_receive_input function from mcu_api.c and pass the received byte as a parameter.
5: After the MCU enters the while loop, call the wifi_uart_service() function from the mcu_api.c file.
******************************************************************************/

/******************************************************************************
                        1: DP Data Point Sequence Type Comparison Table
          **This is auto-generated code. If modifications are made on the development platform, please re-download the MCU_SDK.**
******************************************************************************/
#if defined(CONFIG_TUYA_ENABLE_TEST)
#include "dp_var.code_snippet"
#else
const DOWNLOAD_CMD_S download_cmd[] =
{
  {DPID_TEMP_CURRENT, DP_TYPE_VALUE},
  {DPID_HUMIDITY_VALUE, DP_TYPE_VALUE},
  {DPID_BATTERY_STATE, DP_TYPE_ENUM},
  {DPID_BATTERY_PERCENTAGE, DP_TYPE_VALUE},
};

#endif

/******************************************************************************
                           2: UART Single-Byte Send Function
Please fill the MCU UART send function into this function, and pass the data to be sent as a parameter to the UART send function.
******************************************************************************/

/**
 * @brief  UART transmit data
 * @param[in] {value} The 1-byte data to be sent via UART
 * @return Null
 */
void uart_transmit_output(u8 value) {
#if defined(CONFIG_TUYA_ENABLE_TEST)
    printf("%02X ", value);
#endif
    /* PORTED: blocking single-byte send on the module's UART.  Blocking is what
       the SDK assumes -- it walks a frame byte by byte through here -- and at
       9600 baud a whole frame holds the caller for 7-50 ms.  Affordable only
       because nothing else needs the CPU while the module is powered
       (temp_sensor_main.c). */
    BSP_Wifi_TransmitByte(value);
}

#if defined(CONFIG_MCU_SDK_TEST_ONLY_USE_NEW_DISPATCHER)

PROCESS_BUFFER_OVERFLOW_HANDLE_METHOD_E on_process_buffer_overflow(u8 cmd, u16 expected_len) {
    /**
     * In a production environment, the maximum received command lengths are all known. Buffer overflow errors are often unusual and may indicate potential attack risks.
     * If your product has security requirements, it is recommended to directly reset the module and MCU here.
     * If high availability is required, different return values can be used to choose different handling methods.
     *
     * PROCESS_BUFFER_OVERFLOW_HANDLE_METHOD_DISCARD_WHOLE_COMMAND         Discard the entire command according to the declared length without processing
     * PROCESS_BUFFER_OVERFLOW_HANDLE_METHOD_DISCARD_CURRENT_AND_RX_BUFFER Discard currently received data and all data in the receive buffer
     * PROCESS_BUFFER_OVERFLOW_HANDLE_METHOD_DISCARD_CURRENT               Discard only currently received data
     */
    TUYA_UNUSED(cmd);
    TUYA_UNUSED(expected_len);
    return PROCESS_BUFFER_OVERFLOW_HANDLE_METHOD_DISCARD_CURRENT_AND_RX_BUFFER;
}

bool_t on_first_command_after_overflow(const u8* frame, u16 frame_len) {
    /**
     * In a production environment, buffer overflow may indicate potential attacks.
     * After a buffer overflow, we cannot determine if this command truly comes from the module or is a malicious instruction within the data segment of the previous command.
     * In this function, you have the opportunity to judge whether these commands are safe. Returning FALSE can prevent this command from executing and continue calling this function before the next command executes.
     *
     * For example, if this command attempts to upgrade the MCU or set DP values to dangerous combinations, return FALSE to prevent command execution.
     *
     * If the product is not sensitive to security, directly return TRUE.
     */
    TUYA_UNUSED( frame);
    TUYA_UNUSED( frame_len);
    return TRUE;
}


#endif

/******************************************************************************
                           Step 2: Implement Specific User Functions
1: APP downlink data processing
2: Data reporting processing
******************************************************************************/

/******************************************************************************
                            1: All Data Reporting Processing
The current function handles all data reporting (including deliverable/reportable and report-only)
  Users need to implement according to the actual situation:
  1: Need to implement deliverable/reportable data point reporting.
  2: Need to implement report-only data point reporting.
This function must be called internally by the MCU.
Users can also call this function to implement all data reporting.
******************************************************************************/

// Automatically generated data reporting function

/**
 * @brief  Upload all system DP point information to achieve APP and MCU data synchronization
 * @param  Null
 * @return Null
 * @note   This function is required to be called internally by the SDK. The MCU must implement the data reporting functionality within this function, including report-only and deliverable/reportable data types.
 */
void all_data_update(void){
    /* PORTED: the SDK calls this when the module asks for every DP (command
       0x08, STATE_QUERY_CMD).  The module asks once it has finished its
       handshake with the MCU and again as it comes online -- twice per window
       on the bench, once at "router" and once at "cloud" -- and the module
       cold-boots on every report window, so this runs on every window.

       It must answer from a cache, not from the sensor: it is called from
       inside wifi_uart_service() at a moment no caller controls, and Tuya's
       porting notes forbid doing work of any length in here. */
    Tuya_ReportCachedDps();
}

/******************************************************************************
                                WARNING!!!
                            2: All Data Reporting Processing
Automated code template functions, please implement data processing by the user.
******************************************************************************/
#if defined(CONFIG_TUYA_ENABLE_TEST)
#include "dp_id_handle.code_snippet"
#else
    
#endif

/******************************************************************************
                                WARNING!!!
This section of functions should not be modified by the user!!
******************************************************************************/

/**
 * @brief  DP download (downlink) processing function
 * @param[in] {dpid} DPID number
 * @param[in] {value} DP data buffer address
 * @param[in] {length} DP data length
 * @return DP processing result
 * -           0(ERROR): Failure
 * -           1(SUCCESS): Success
 * @note   This function cannot be modified by the user.
 */
u8 dp_download_handle(u8 dpid, const u8 value[], u16 length) {
    /*********************************
    This function handles calls for deliverable/reportable data.
    Specific downlink data processing needs to be implemented within the function.
    After completion, the user needs to feed the processing result back to the APP side, otherwise the APP will consider the downlink failed.
    ***********************************/
    u8 ret = SUCCESS;
    switch (dpid) {
#if defined(CONFIG_TUYA_ENABLE_TEST)
#include "dp_handle.code_snippet"
#else
        
#endif
        default:
            break;
    }
    return ret;
}

/**
 * @brief  Get the total number of all DP commands
 * @param[in] Null
 * @return Total number of download commands
 * @note   This function cannot be modified by the user.
 */
u8 get_download_cmd_total(void) {
    return (sizeof(download_cmd) / sizeof(download_cmd[0]));
}

/******************************************************************************
                                WARNING!!!
This code is called internally by the SDK. Please implement the data within the function according to the actual DP data.
******************************************************************************/

#ifdef SUPPORT_MCU_FIRM_UPDATE
/**
 * @brief  Upgrade package size selection
 * @param[in] {package_sz} Upgrade package size
 * @ref           0x00: 256byte (default)
 * @ref           0x01: 512byte
 * @ref           0x02: 1024byte
 * @return Null
 * @note   The MCU needs to implement this function.
 */
void upgrade_package_choose(u8 package_sz) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error "Please implement the upgrade package size selection code yourself. Delete this line after completion."
#endif
    u16 send_len = 0;
    send_len = set_wifi_uart_byte(send_len, package_sz);
    wifi_uart_write_frame(UPDATE_START_CMD, MCU_TX_VER, send_len);
}

/**
 * @brief  MCU enters firmware upgrade mode
 * @param[in] {value} Firmware buffer
 * @param[in] {position} Current data packet position within the firmware
 * @param[in] {length} Current firmware packet length (when the firmware packet length is 0, it indicates the firmware packet transmission is complete)
 * @return Null
 * @note   The MCU needs to implement this function.
 */
u8 mcu_firm_update_handle(const u8 value[], u32 position, u16 length) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error "Please complete the MCU firmware upgrade code yourself. Delete this line after completion."
#endif
    if (length == 0) {
        // Firmware data transmission completed

    } else {
        // Firmware data processing
    }

    return SUCCESS;
}
#endif

#ifdef SUPPORT_GREEN_TIME
/**
 * @brief  Acquired Greenwich Mean Time
 * @param[in] {time} Acquired Greenwich Mean Time data
 * @return Null
 * @note   The MCU needs to implement this function.
 */
void mcu_get_greentime(u8 time[]) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error "Please complete the relevant code yourself and delete this line"
#endif
    /*
    time[0] is the flag for successful time acquisition, 0 indicates failure, 1 indicates success
    time[1] is the year, 0x00 represents year 2000
    time[2] is the month, from 1 to 12
    time[3] is the date, from 1 to 31
    time[4] is the hour, from 0 to 23
    time[5] is the minute, from 0 to 59
    time[6] is the second, from 0 to 59
    */
    if (time[0] == 1) {
        // Successfully received Greenwich data from the WiFi module
        TUYA_DBG_EXEC(TUYA_PRINT("Got Greenwich time 20%02d-%02d-%02d %02d:%02d:%02d", time[1], time[2], time[3], time[4], time[5], time[6]));
    } else {
        // Error acquiring Greenwich time, possibly because the current WiFi module is not connected to the internet
        TUYA_DBG_EXEC(TUYA_PRINT("Got Greenwich time FAILED!"));
    }
}
#endif

#ifdef SUPPORT_MCU_RTC_CHECK
/**
 * @brief  MCU corrects local RTC clock
 * @param[in] {time} Acquired Greenwich Mean Time data
 * @return Null
 * @note   The MCU needs to implement this function.
 */
void mcu_write_rtctime(u8 time[]) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error "Please complete the RTC clock write code yourself and delete this line"
#endif
    /*
    Time[0] is the flag for successful time acquisition, 0 indicates failure, 1 indicates success
    Time[1] is the year, 0x00 represents year 2000
    Time[2] is the month, from 1 to 12
    Time[3] is the date, from 1 to 31
    Time[4] is the hour, from 0 to 23
    Time[5] is the minute, from 0 to 59
    Time[6] is the second, from 0 to 59
    Time[7] is the day of week, from 1 to 7, where 1 represents Monday
   */
    if (time[0] == 1) {
        // Successfully received local clock data from the WiFi module
        TUYA_DBG_EXEC(TUYA_PRINT("Got local time 20%02d-%02d-%02d %02d:%02d:%02d weekday: %d", time[1], time[2], time[3], time[4], time[5], time[6], time[7]));
    } else {
        // Error acquiring local clock data, possibly because the current WiFi module is not connected to the internet
        TUYA_DBG_EXEC(TUYA_PRINT("Got Greenwich time FAILED!"));
    }
}
#endif

#ifdef WIFI_TEST_ENABLE
/**
 * @brief  WiFi function test feedback
 * @param[in] {result} WiFi function test result
 * @ref       0: Failure
 * @ref       1: Success
 * @param[in] {rssi} On test success, represents WiFi signal strength / On test failure, represents error type
 * @return Null
 * @note   The MCU needs to implement this function.
 */
void wifi_test_result(u8 result, u8 rssi) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error "Please implement the WiFi function test success/failure code yourself. Delete this line after completion."
#endif
    if (result == 0) {
        // Test failed
        TUYA_DBG_EXEC(TUYA_PRINT("WiFi scan test FAILED!"));
        if (rssi == 0x00) {
            // Router named 'tuya_mdev_test' not found, please check
            TUYA_DBG_EXEC(TUYA_PRINT("ap with ssid 'tuya_mdev_test' not found."));
        } else if (rssi == 0x01) {
            // Module not authorized
            TUYA_DBG_EXEC(TUYA_PRINT("module may not be authorized."));
        }
    } else {
        // Test successful
        // rssi is signal strength (0-100, 0 worst signal, 100 best signal)
        TUYA_DBG_EXEC(TUYA_PRINT("WiFi scan test pass. Signal strength: %d%%", rssi));
    }
}
#endif

#ifdef WEATHER_ENABLE
/**
 * @brief  MCU opens weather service
 * @param  Null
 * @return Null
 */
void mcu_open_weather(void) {
    i32 i = 0;
    i8 buffer[13] = {0};
    u8 weather_len = 0;
    u16 send_len = 0;

    weather_len = sizeof(weather_choose) / sizeof(weather_choose[0]);

    for (i = 0; i < weather_len; i++) {
        buffer[0] = sprintf(buffer + 1, "w.%s", weather_choose[i]);
        send_len = set_wifi_uart_buffer(send_len, (u8*)buffer, buffer[0] + 1);
    }

#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error "Please follow the prompts to improve the code for opening the weather service yourself. Delete this line after completion."
#endif
    /*
    //When the obtained parameters include time-related parameters (e.g., sunrise, sunset), they need to be used with t.unix or t.local to specify whether the parameter data should be based on Greenwich time or local time.
    buffer[0] = sprintf(buffer+1,"t.unix"); //Greenwich time   or use  buffer[0] = sprintf(buffer+1,"t.local"); //Local time
    send_len = set_wifi_uart_buffer(send_len, (u8 *)buffer, buffer[0]+1);
    */

    buffer[0] = sprintf(buffer + 1, "w.date.%d", WEATHER_FORECAST_DAYS_NUM);
    send_len = set_wifi_uart_buffer(send_len, (u8*)buffer, buffer[0] + 1);

    wifi_uart_write_frame(WEATHER_OPEN_CMD, MCU_TX_VER, send_len);
}

/**
 * @brief  User-defined processing function for weather service opening return result
 * @param[in] {res} Weather service opening return result
 * @ref       0: Failure
 * @ref       1: Success
 * @param[in] {err} Error code
 * @return Null
 * @note   The MCU needs to implement this function.
 */
void weather_open_return_handle(u8 res, u8 err) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error "Please complete the processing code for the weather service opening return data yourself. Delete this line after completion."
#endif
    u8 err_num = 0;

    if (res == 1) {
        // Weather service opening return successful
    } else if (res == 0) {
        // Weather service opening return failed
        // Get error code
        err_num = err;
    }
}

/**
 * @brief  User-defined weather data processing function
 * @param[in] {name} Parameter name
 * @param[in] {type} Parameter type
 * @ref       0: int type
 * @ref       1: string type
 * @param[in] {data} Parameter value address
 * @param[in] {day} Which day's weather  0: today  Range: 0~6
 * @ref       0: Today
 * @ref       1: Tomorrow
 * @return Null
 * @note   The MCU needs to implement this function.
 */
void weather_data_user_handle(i8* name, u8 type, const u8* weather_data, i8 day) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error "Only examples are provided here. Please improve the weather data processing code yourself. Delete this line after completion."
#endif
    i32 value_int;
    i8 value_string[50];  // Since some parameter content is large, default is 50. You can reduce this value appropriately based on your defined parameters.

    tuya_memset(value_string, '\0', sizeof(value_string));

    // First, get the data type
    if (type == 0) {  // Parameter is INT type
        value_int = weather_data[0] << 24 | weather_data[1] << 16 | weather_data[2] << 8 | weather_data[3];
    } else if (type == 1) {
        tuya_strncpy(value_string, weather_data, sizeof(value_string));
    }

    // Note: Obtain parameter values based on the selected parameter types!!!
    if (tuya_strcmp(name, "temp") == 0) {
        TUYA_DBG_EXEC(TUYA_PRINT("day:%d temp value is:%d\r\n", day, value_int));  // int type
    } else if (tuya_strcmp(name, "humidity") == 0) {
        TUYA_DBG_EXEC(TUYA_PRINT("day:%d humidity value is:%d\r\n", day, value_int));  // int type
    } else if (tuya_strcmp(name, "pm25") == 0) {
        TUYA_DBG_EXEC(TUYA_PRINT("day:%d pm25 value is:%d\r\n", day, value_int));  // int type
    } else if (tuya_strcmp(name, "condition") == 0) {
        TUYA_DBG_EXEC(TUYA_PRINT("day:%d condition value is:%s\r\n", day, value_string));  // string type
    }
}
#endif

#ifdef MCU_DP_UPLOAD_SYN
/**
 * @brief  Status synchronous upload result
 * @param[in] {result} Result
 * @ref       0: Failure
 * @ref       1: Success
 * @return Null
 * @note   The MCU needs to implement this function.
 */
void get_upload_syn_result(u8 result) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error "Please complete the status synchronous upload result code yourself and delete this line"
#endif

    if (result == 0) {
        // Synchronous upload error
        TUYA_DBG_EXEC(TUYA_PRINT("Sync report dp FAILED."));
    } else {
        // Synchronous upload successful
        TUYA_DBG_EXEC(TUYA_PRINT("Sync report dp SUCCESSFUL."));
    }
}
#endif

#ifdef GET_WIFI_STATUS_ENABLE
/**
 * @brief  Get WiFi status result
 * @param[in] {result} Indicates WiFi working status
 * @ref       0x00: wifi status 1 smartconfig configuration status
 * @ref       0x01: wifi status 2 AP configuration status
 * @ref       0x02: wifi status 3 WiFi configured but not connected to router
 * @ref       0x03: wifi status 4 WiFi configured and connected to router
 * @ref       0x04: wifi status 5 Connected to router and connected to cloud
 * @ref       0x05: wifi status 6 WiFi device in low power mode
 * @ref       0x06: wifi status 7 WiFi device in smartconfig & AP configuration status
 * @return Null
 * @note   The MCU needs to implement this function.
 */
void get_wifi_status(u8 result) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error "Please complete the get WiFi status result code yourself and delete this line"
#endif
    switch (result) {
        case 0:
            // wifi working status 1
            TUYA_DBG_EXEC(TUYA_PRINT("wifi under smartconfig mode."));
            break;

        case 1:
            // wifi working status 2
            TUYA_DBG_EXEC(TUYA_PRINT("wifi under ap config mode."));
            break;

        case 2:
            // wifi working status 3
            TUYA_DBG_EXEC(TUYA_PRINT("wifi configured but not connected to router."));
            break;

        case 3:
            // wifi working status 4
            TUYA_DBG_EXEC(TUYA_PRINT("wifi configured and connected to router."));
            break;

        case 4:
            // wifi working status 5
            TUYA_DBG_EXEC(TUYA_PRINT("wifi connected to cloud."));
            break;

        case 5:
            // wifi working status 6
            TUYA_DBG_EXEC(TUYA_PRINT("wifi under low power mode."));
            break;

        case 6:
            // wifi working status 7
            TUYA_DBG_EXEC(TUYA_PRINT("wifi under smartconfig & ap config mode."));
            break;

        default:
            break;
    }
}
#endif

#ifdef WIFI_STREAM_ENABLE
/**
 * @brief  Stream service send result
 * @param[in] {result} Result
 * @ref       0x00: Success
 * @ref       0x01: Stream service function not enabled
 * @ref       0x02: Stream server connection not successful
 * @ref       0x03: Data push timeout
 * @ref       0x04: Transmitted data length error
 * @return Null
 * @note   The MCU needs to implement this function.
 */
void stream_trans_send_result(u8 result) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error "Only examples are provided here. Please improve the stream service send result processing code yourself. Delete this line after completion."
#endif
    switch (result) {
        case 0x00:
            // Success
            TUYA_DBG_EXEC(TUYA_PRINT("stream send SUCCESSFUL."));
            break;

        case 0x01:
            // Stream service function not enabled
            TUYA_DBG_EXEC(TUYA_PRINT("stream not enable."));
            break;

        case 0x02:
            // Stream server connection not successful
            TUYA_DBG_EXEC(TUYA_PRINT("server connect timeout."));
            break;

        case 0x03:
            // Data push timeout
            TUYA_DBG_EXEC(TUYA_PRINT("data push timeout"));
            break;

        case 0x04:
            // Transmitted data length error
            TUYA_DBG_EXEC(TUYA_PRINT("data length invalid"));
            break;

        default:
            break;
    }
}

/**
 * @brief  Multi-map stream service send result
 * @param[in] {result} Result
 * @ref       0x00: Success
 * @ref       0x01: Failure
 * @return Null
 * @note   The MCU needs to implement this function.
 */
void maps_stream_trans_send_result(u8 result) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error "Only examples are provided here. Please improve the multi-map stream service send result processing code yourself. Delete this line after completion."
#endif
    switch (result) {
        case 0x00:
            // Success
            TUYA_DBG_EXEC(TUYA_PRINT("map stream send SUCCESSFUL."));
            break;

        case 0x01:
            // Failure
            TUYA_DBG_EXEC(TUYA_PRINT("map stream send FAILED."));
            break;

        default:
            break;
    }
}
#endif

#ifdef WIFI_CONNECT_TEST_ENABLE
/**
 * @brief  Router information reception result notification
 * @param[in] {result} Whether the module successfully received correct router information
 * @ref       0x00: Failure
 * @ref       0x01: Success
 * @return Null
 * @note   The MCU needs to implement this function.
 */
void wifi_connect_test_result(u8 result) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error "Please implement the WiFi function test success/failure code yourself. Delete this line after completion."
#endif
    if (result == 0) {
        // Router information reception failed. Please check if the sent router information packet is a complete JSON data packet.
        TUYA_DBG_EXEC(TUYA_PRINT("wifi connect test FAILED."));
    } else {
        // Router information reception successful. For production test results, please note the WiFi working status in the WIFI_STATE_CMD command.
        TUYA_DBG_EXEC(TUYA_PRINT("wifi connect test PASSED."));
    }
}
#endif

#ifdef GET_MODULE_MAC_ENABLE
/**
 * @brief  Get module MAC result
 * @param[in] {mac} Module MAC data
 * @ref       mac[0]: Flag for successful MAC acquisition, 0x00 indicates success, 0x01 indicates failure
 * @ref       mac[1]~mac[6]: If the MAC acquisition flag mac[0] is success, represents the valid MAC address of the module
 * @return Null
 * @note   The MCU needs to implement this function.
 */
void mcu_get_mac(u8 mac[]) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error "Please complete the MAC acquisition code yourself and delete this line"
#endif
    /*
    mac[0] is the flag for successful MAC acquisition, 0x00 indicates success, 0x01 indicates failure
    mac[1]~mac[6]: If the MAC acquisition flag mac[0] is success, represents the valid MAC address of the module
   */

    if (mac[0] == 1) {
        // MAC acquisition error
        TUYA_DBG_EXEC(TUYA_PRINT("FAILED to get module MAC addr."));
    } else {
        // Successfully received MAC address from WiFi module
        TUYA_DBG_EXEC(TUYA_PRINT("Got module mac addr %02X:%02X:%02X:%02X:%02X:%02X.", mac[1], mac[2], mac[3], mac[4], mac[5], mac[6]));
    }
}
#endif

#ifdef GET_IR_STATUS_ENABLE
/**
 * @brief  Get infrared status result
 * @param[in] {result} Indicates infrared status
 * @ref       0x00: infrared status 1 Sending infrared code
 * @ref       0x01: infrared status 2 Infrared code sending finished
 * @ref       0x02: infrared status 3 Infrared learning started
 * @ref       0x03: infrared status 4 Infrared learning ended
 * @return Null
 * @note   The MCU needs to implement this function.
 */
void get_ir_status(u8 result) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error "Please complete the infrared status code yourself and delete this line"
#endif
    switch (result) {
        case 0:
            // infrared status 1
            TUYA_DBG_EXEC(TUYA_PRINT("IR sending"));
            break;

        case 1:
            // infrared status 2
            TUYA_DBG_EXEC(TUYA_PRINT("IR send finish"));
            break;

        case 2:
            // infrared status 3
            TUYA_DBG_EXEC(TUYA_PRINT("IR learn start"));
            break;

        case 3:
            // infrared status 4
            TUYA_DBG_EXEC(TUYA_PRINT("IR learn end"));
            break;

        default:
            break;
    }

    wifi_uart_write_frame(GET_IR_STATUS_CMD, MCU_TX_VER, 0);
}
#endif

#ifdef IR_TX_RX_TEST_ENABLE
/**
 * @brief  Infrared enter send/receive production test result notification
 * @param[in] {result} Whether the module successfully received correct information
 * @ref       0x00: Failure
 * @ref       0x01: Success
 * @return Null
 * @note   The MCU needs to implement this function.
 */
void ir_tx_rx_test_result(u8 result) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error "Please implement the infrared enter send/receive production test function success/failure code yourself. Delete this line after completion."
#endif
    if (result == 0) {
        // Infrared enter send/receive production test successful
    } else {
        // Infrared enter send/receive production test failed. Please check the sent data packet.
    }
}
#endif

#ifdef FILE_DOWNLOAD_ENABLE
/**
 * @brief  File download package size selection
 * @param[in] {package_sz} File download package size
 * @ref       0x00: 256 byte (default)
 * @ref       0x01: 512 byte
 * @ref       0x02: 1024 byte
 * @return Null
 * @note   The MCU needs to implement this function.
 */
void file_download_package_choose(u8 package_sz) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error "Please implement the file download package size selection code yourself. Delete this line after completion."
#endif
    u16 send_len = 0;
    send_len = set_wifi_uart_byte(send_len, package_sz);
    wifi_uart_write_frame(FILE_DOWNLOAD_START_CMD, MCU_TX_VER, send_len);
}

/**
 * @brief  File package download mode
 * @param[in] {value} Data buffer
 * @param[in] {position} Current data packet position within the file
 * @param[in] {length} Current file package length (when length is 0, indicates file package transmission is complete)
 * @return Data processing result
 * -           0(ERROR): Failure
 * -           1(SUCCESS): Success
 * @note   The MCU needs to implement this function.
 */
u8 file_download_handle(const u8 value[], u32 position, u16 length) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error "Please complete the file package download code yourself. Delete this line after completion."
#endif
    if (length == 0) {
        // File package data transmission completed

    } else {
        // File package data processing
    }

    return SUCCESS;
}
#endif

#ifdef MODULE_EXPANDING_SERVICE_ENABLE
/**
 * @brief  Open module time service notification result
 * @param[in] {value} Data buffer
 * @param[in] {length} Data length
 * @return Null
 * @note   The MCU needs to implement this function.
 */
void open_module_time_serve_result(const unsigned char value[], unsigned short length) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error "Please implement the module time service notification result code yourself. Delete this line after completion."
#endif

    if (0x02 != length) {
        // Data length error
        return;
    }

    if (value[1] == 0) {
        // Service opening successful
    } else {
        // Service opening failed
    }
}

void module_time_sync_handler(const unsigned char cmd_bytes[], unsigned short cmd_length) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error Improve time synchronization code here (0x3402)
#endif
    if (0x09 != cmd_length) {
        // Data length error
        return;
    }

    unsigned char time_type = cmd_bytes[1];  // 0x00:Greenwich time  0x01:Local time
    unsigned char time_data[7];

    tuya_memcpy(time_data, cmd_bytes + 2, cmd_length - 2);
    /*
    Data[0] is year, 0x00 represents year 2000
    Data[1] is month, from 1 to 12
    Data[2] is date, from 1 to 31
    Data[3] is hour, from 0 to 23
    Data[4] is minute, from 0 to 59
    Data[5] is second, from 0 to 15
    Data[6] is day of week, from 1 to 7, 1 represents Monday
    */

    TUYA_DBG_EXEC(TUYA_PRINT("Got local time 20%02d-%02d-%02d %02d:%02d:%02d weekday: %d",
                  time_data[0], time_data[1], time_data[2], time_data[3], time_data[4], time_data[5], time_data[6]));

    // Add time data processing code here, time_type is the time type

    unsigned short send_len = 0;
    send_len = set_wifi_uart_byte(send_len, cmd_length);
    wifi_uart_write_frame(MODULE_EXTEND_FUN_CMD, MCU_TX_VER, send_len);
}

void request_weather_handler(const unsigned char cmd_datas[], unsigned short cmd_data_length) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error Active request for weather service data result return. Delete this error directive after completion.
#endif

    u8 result = cmd_datas[1];
    if (result == 0x00) {
        // Success
        TUYA_DBG_EXEC(TUYA_PRINT("request weather data SUCCESSFUL"));
    } else {
        // Failure
        TUYA_DBG_EXEC(TUYA_PRINT("request weather data FAILED"));
    }
}

void module_start_reset_notify_handler(const unsigned char cmd_bytes[], unsigned short cmd_length) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error Open module reset notification result. Delete this error directive after completion.
#endif

    if (0x02 != cmd_length) {
        // Data length error
        return;
    }

    if (cmd_bytes[1] == 0) {
        // Success
    } else {
        // Failure
    }
}

void module_reset_notify_handler(const unsigned char cmd_bytes[], unsigned short cmd_length) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error Module reset notification. Delete this error directive after completion.
#endif

    if (0x02 != cmd_length) {
        // Data length error
        return;
    }

    switch (cmd_bytes[1]) {
        case 0x00:
            // Module local reset

            break;
        case 0x01:
            // APP remote reset

            break;
        case 0x02:
            // APP restore factory reset

            break;
        default:
            break;
    }

    unsigned short send_len = 0;
    send_len = set_wifi_uart_byte(send_len, MODULE_EXTEND_FUN_SUB_CMD_START_RESET_NOTIFY);
    wifi_uart_write_frame(MODULE_EXTEND_FUN_CMD, MCU_TX_VER, send_len);
}

void module_wifi_remote_handler(const unsigned char cmd_datas[], unsigned short cmd_data_length) {
    u8 type = cmd_datas[1];
    u8 cmd = cmd_datas[2];
    u8 cmd_data = cmd_datas[3];
    u16 send_len = 0;
    switch (type) {
        case 0xFF:
            // General category
            break;

        case 0x01:
            // Lighting
            break;

        default:
            break;
    }

    send_len = set_wifi_uart_byte(send_len, MODULE_EXTEND_FUN_SUB_CMD_WIFI_REMOTE);
    wifi_uart_write_frame(MODULE_EXTEND_FUN_CMD, MCU_TX_VER, send_len);
}

void get_module_info_handler(const unsigned char cmd_datas[], unsigned short cmd_data_length) {
    // Get all currently supported data information 0xff
    // AP ssid name 0x01
    // Country code 0x02
    // SN information 0x03
    // FFS authorization record 0x04
    // Module version number 0x05
    // WiFi configuration information 0x06
    // uuid information 0x07
    // Firmware key information 0x08

#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error Get WiFi module information return. Delete this error directive after completion.
#endif

    u8 result = cmd_datas[1];
    const char* json_buf = &cmd_datas[2];

    if (result == 0x01) {
        // Failure

        return;
    }

    // TODO: Parse the json in jsonbuf here to get the return
    /*
    {
      "ap":    "xxxxx";
      "cc":    xx;
      "sn":    "xxxx";
      "ffs":   x,
      "sw":    "x.x.x",
      "wcfg": { "ssid": "xxxx", "pwd": "xxxx", "uuid": "xxxx", "key": "xxxx" }
    }
    */
}

void set_module_log_level_handler(const unsigned char cmd_datas[], unsigned short cmd_data_length) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error Adjust module log level result. Delete this error directive after completion.
#endif

    u8 result = cmd_datas[1];
    if (result == 0x00) {
        // Success
        TUYA_DBG_EXEC(TUYA_PRINT("Adjust module log level SUCCESSFUL."));
    } else {
        // Failure
        TUYA_DBG_EXEC(TUYA_PRINT("Adjust module log level FAILED."));
    }
}

#if defined(RECORD_TYPE_DP_ENABLE)

void module_record_dp_report_handler(const unsigned char cmd_datas[], unsigned short cmd_data_length) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error Record-type DP upload result. Delete this error directive after completion.
#endif

    u8 result = cmd_datas[1];
    switch (result) {
        case 0x00:
            // Successfully uploaded to cloud
            break;

        case 0x01:
            // Upload successful and retained
            break;

        case 0x02:
            // Upload failed
            break;
        case 0x03:
            // Data content illegal

            break;
    }
}

#endif

#if defined(SBUS_SERVICE_ENABLE)
void sbus_service_handler(const unsigned char cmd_datas[], unsigned short cmd_data_length) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error Improve the soft bus service yourself. Delete this error directive after completion.
#endif

    u8 sbus_cmd = cmd_datas[1];
    u8 result;
    u8 data_length;
    const u8* data_ptr;
    const u8* data_end;
    switch (sbus_cmd) {
        case 0x01:
            // Status notification
            result = cmd_datas[2];
            TUYA_DBG_EXEC(TUYA_PRINT("sbus state change to: %d", result));
            switch (result) {
                case 0x00:
                    // Idle
                    TUYA_DBG_EXEC(TUYA_PRINT("Idle"));
                    break;
                case 0x01:
                    // Binding
                    TUYA_DBG_EXEC(TUYA_PRINT("Bind"));
                    break;
                case 0x02:
                    // Connected
                    TUYA_DBG_EXEC(TUYA_PRINT("Connected"));
                    break;
                case 0x03:
                    // Disconnected
                    TUYA_DBG_EXEC(TUYA_PRINT("Disconnected"));
                    break;
                case 0x04:
                    // Unbinding
                    TUYA_DBG_EXEC(TUYA_PRINT("Unbind"));
                    break;
            }
            break;

        case 0x02:
            if (cmd_data_length == 3) {
                // Send feedback
                result = cmd_datas[2];
                if (result == 0x00) {
                    // Success
                    TUYA_DBG_EXEC(TUYA_PRINT("sbus send SUCCESSFUL."));
                } else {
                    // Failure
                    TUYA_DBG_EXEC(TUYA_PRINT("sbus send FAILED."));
                }
            } else {
                // Received message
                data_end = &cmd_datas[0] + cmd_data_length;
                while (data_ptr < data_end) {
                    data_length = *data_ptr;
                    data_ptr += 1;
                    // TODO: Process this message

                    data_ptr += data_length;
                }
            }
            break;
        default:
            TUYA_DBG_EXEC(TUYA_PRINT("Unknownm sbus command %02X", sbus_cmd));
            break;
    }
}

#endif

#endif

#ifdef BLE_RELATED_FUNCTION_ENABLE
/**
 * @brief  Bluetooth function test result
 * @param[in] {value} Data buffer
 * @param[in] {length} Data length
 * @return Null
 * @note   The MCU needs to implement this function.
 */
void BLE_test_result(const u8 value[], u16 length) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error "Please implement the Bluetooth function test result code yourself. Delete this line after completion."
#endif

    u8 sub_cmd = value[0];
    u8 result = value[1];
    u8 rssi = value[2];

    if (0x03 != length) {
        // Data length error
        return;
    }

    if (0x01 != sub_cmd) {
        // Subcommand error
        return;
    }

    if (result == 0) {
        // Test failed
        if (rssi == 0x00) {
            // Beacon named ty_mdev not found, please check
        } else if (rssi == 0x01) {
            // Module not authorized
        }
    } else if (result == 0x01) {
        // Test successful
        // rssi is signal strength (0-100, 0 worst signal, 100 best signal)
    }
}

void ble_connect_state_report(const u8 cmd_datas[], u16 cmd_data_length) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error Handle the Bluetooth connection status actively sent by the module here. Delete this line after completion.
#endif
    u8 state = cmd_datas[1];
    u16 send_len = 0;
    TUYA_DBG_EXEC(TUYA_PRINT("BT state change to %02X", state));
    switch (state) {
        case BT_STATE_UNBINDED_DISCONNECTED:
            break;
        case BT_STATE_UNBINDED_CONNECTED:
            break;
        case BT_STATE_BINDED_DISCONNECTED:
            break;
        case BT_STATE_BINDED_CONNECTED:
            break;
        case BT_STATE_UNKNOWN:
            break;
    }
    send_len = set_wifi_uart_byte(send_len, BLE_RELATIVE_SUB_CMD_DEV_STATE_REPORT);
    wifi_uart_write_frame(BLE_RELATIVE_CMD, MCU_TX_VER, send_len);
}

void ble_connect_state_query_result(const u8 cmd_datas[], u16 cmd_data_length) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error Handle the query returned Bluetooth connection status here. Delete this line after completion.
#endif
    u8 state = cmd_datas[1];
    TUYA_DBG_EXEC(TUYA_PRINT("Got BT state %02X", state));
    switch (state) {
        case BT_STATE_UNBINDED_DISCONNECTED:
            break;
        case BT_STATE_UNBINDED_CONNECTED:
            break;
        case BT_STATE_BINDED_DISCONNECTED:
            break;
        case BT_STATE_BINDED_CONNECTED:
            break;
        case BT_STATE_UNKNOWN:
            break;
    }
}

void ble_beacon_remote_data_notify(const u8 cmd_datas[], u16 cmd_data_length) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error Handle Bluetooth beacon remote control data here. Refer to the documentation for detailed definitions. Delete this line after completion.
#endif
    const u8 category_id = cmd_datas[1];
    const u8 ctrl_cmd = cmd_datas[2];
    const u8 ctrl_datas[4] = {cmd_datas[3], cmd_datas[4], cmd_datas[5], cmd_datas[6]};

    TUYA_DBG_EXEC(TUYA_PRINT("beacon remote cmd: category_id: %02X, cmd: %02X, data: %02X %02X %02X %02X",
                  category_id, ctrl_cmd, ctrl_datas[0], ctrl_datas[1], ctrl_datas[2], ctrl_datas[3]));
    // TODO
}

void ble_beacon_remote_bind_unbind_notify(const u8 cmd_datas[], u16 cmd_data_length) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error Handle Bluetooth beacon remote control binding/unbinding events here. Delete this line after completion.
#endif
    const u8 event = cmd_datas[1];     // 0x00: Unbind  0x01: Bind
    const u8 group_id = cmd_datas[2];  // Group ID
    // TODO
}

void ble_sub_dev_data_notify(const u8 cmd_datas[], u16 cmd_data_length) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error Handle Bluetooth sub-device data here. Delete this line after completion.
#endif
    const u8 mac_addr[6] = {cmd_datas[1], cmd_datas[2], cmd_datas[3], cmd_datas[4], cmd_datas[5], cmd_datas[6]};
    const u32 seq = UINT32_NTOH(&cmd_datas[7]);
    const u8* ptr_to_data = &cmd_datas[11];
    const u16 data_length = cmd_data_length - 11;
    // TODO: ...
}

void BLE_relative_fun(const u8 cmd_datas[], u16 cmd_data_length) {
    u8 sub_cmd = cmd_datas[0];
    switch (sub_cmd) {
        case BLE_RELATIVE_SUB_CMD_SCAN_TEST:
            BLE_test_result(cmd_datas, cmd_data_length);
            break;
        case BLE_RELATIVE_SUB_CMD_DEV_STATE_REPORT:
            ble_connect_state_report(cmd_datas, cmd_data_length);
            break;
        case BLE_RELATIVE_SUB_CMD_DEV_STATE_QUERY:
            ble_connect_state_query_result(cmd_datas, cmd_data_length);
            break;
        case BLE_RELATIVE_SUB_CMD_BEACON_RMT_DATA_NOTIFY:
            ble_beacon_remote_data_notify(cmd_datas, cmd_data_length);
            break;
        case BLE_RELATIVE_SUB_CMD_BEACON_RMT_BIND_UNBIND_NOTIFY:
            ble_beacon_remote_bind_unbind_notify(cmd_datas, cmd_data_length);
            break;
        case BLE_RELATIVE_SUB_CMD_SUB_DEV_DATA_NOTIFY:
            ble_sub_dev_data_notify(cmd_datas, cmd_data_length);
            break;
    }
}

#endif

#ifdef VOICE_MODULE_PROTOCOL_ENABLE
/**
 * @brief  Get voice status code result
 * @param[in] {result} Voice status code
 * @ref       0x00: Idle
 * @ref       0x01: MIC mute status
 * @ref       0x02: Wakeup
 * @ref       0x03: Recording
 * @ref       0x04: Recognizing
 * @ref       0x05: Recognition successful
 * @ref       0x06: Recognition failed
 * @return Null
 * @note   The MCU needs to implement this function.
 */
void get_voice_state_result(u8 result) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error "Please implement the get voice status code result processing code yourself. Delete this line after completion."
#endif

    TUYA_DBG_EXEC(TUYA_PRINT("voice state: %d", result));
    switch (result) {
        case 0:
            // Idle
            TUYA_DBG_EXEC(TUYA_PRINT("idle."));
            break;

        case 1:
            // MIC mute status
            TUYA_DBG_EXEC(TUYA_PRINT("mic silence."));
            break;

        case 2:
            // Wakeup
            TUYA_DBG_EXEC(TUYA_PRINT("wakeup."));
            break;

        case 3:
            // Recording
            TUYA_DBG_EXEC(TUYA_PRINT("recording."));
            break;

        case 4:
            // Recognizing
            TUYA_DBG_EXEC(TUYA_PRINT("regonizing."));
            break;

        case 5:
            // Recognition successful
            TUYA_DBG_EXEC(TUYA_PRINT("regonize SUCCESSFUL."));
            break;

        case 6:
            // Recognition failed
            TUYA_DBG_EXEC(TUYA_PRINT("regonize FAILED."));
            break;

        default:
            break;
    }
}

/**
 * @brief  MIC mute setting result
 * @param[in] {result} Voice status code
 * @ref       0x00: MIC enabled
 * @ref       0x01: MIC muted
 * @return Null
 * @note   The MCU needs to implement this function.
 */
void set_voice_MIC_silence_result(u8 result) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error "Please implement the MIC mute setting processing code yourself. Delete this line after completion."
#endif
    if (result == 0) {
        // MIC enabled
        TUYA_DBG_EXEC(TUYA_PRINT("MIC enable"));
    } else {
        // MIC muted
        TUYA_DBG_EXEC(TUYA_PRINT("MIC disable"));
    }
}

/**
 * @brief  Speaker volume setting result
 * @param[in] {result} Volume value
 * @ref       0~10: Volume range
 * @return Null
 * @note   The MCU needs to implement this function.
 */
void set_speaker_voice_result(u8 result) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error "Please implement the speaker volume setting result processing code yourself. Delete this line after completion."
#endif
}

/**
 * @brief  Audio production test result
 * @param[in] {result} Audio production test status
 * @ref       0x00: Close audio production test
 * @ref       0x01: MIC1 audio loop test
 * @ref       0x02: MIC2 audio loop test
 * @return Null
 * @note   The MCU needs to implement this function.
 */
void voice_test_result(u8 result) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error "Please implement the audio production test result processing code yourself. Delete this line after completion."
#endif
    if (result == 0x00) {
        // Close audio production test
    } else if (result == 0x01) {
        // MIC1 audio loop test
    } else if (result == 0x02) {
        // MIC2 audio loop test
    }
}

/**
 * @brief  Wakeup production test result
 * @param[in] {result} Wakeup return value
 * @ref       0x00: Wakeup successful
 * @ref       0x01: Wakeup failed (10s timeout failure)
 * @return Null
 * @note   The MCU needs to implement this function.
 */
void voice_awaken_test_result(u8 result) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error "Please implement the wakeup production test result processing code yourself. Delete this line after completion."
#endif
    if (result == 0x00) {
        // Wakeup successful
    } else if (result == 0x01) {
        // Wakeup failed
    }
}

/**
 * @brief  Voice module extended function
 * @param[in] {value} Data buffer
 * @param[in] {length} Data length
 * @return Null
 * @note   The MCU needs to implement this function.
 */
void voice_module_extend_fun(const u8 value[], u16 length) {
    u8 sub_cmd = value[0];
    u8 play;
    u8 bt_play;
    u16 send_len = 0;

    switch (sub_cmd) {
        case 0x00: {  // Subcommand  MCU function setting
            if (0x02 != length) {
                // Data length error
                return;
            }

            if (value[1] == 0) {
                // Success
            } else {
                // Failure
            }
        } break;

        case 0x01: {  // Subcommand  Status notification
            if (0x02 > length) {
                // Data length error
                return;
            }

            u8 play = 0xff;
            u8 bt_play = 0xff;

            const i8* str_buff = (const i8*)&value[1];
            const i8* str_result = NULL;

            str_result = strstr(str_buff, "play") + tuya_strlen("play") + 2;
            if (NULL == str_result) {
                // Data error
                goto ERR_EXTI;
            }

            if (0 == memcmp(str_result, "true", tuya_strlen("true"))) {
                play = 1;
            } else if (0 == memcmp(str_result, "false", tuya_strlen("false"))) {
                play = 0;
            } else {
                // Data error
                goto ERR_EXTI;
            }

            str_result = strstr(str_buff, "bt_play") + tuya_strlen("bt_play") + 2;
            if (NULL == str_result) {
                // Data error
                goto ERR_EXTI;
            }

            if (0 == memcmp(str_result, "true", tuya_strlen("true"))) {
                bt_play = 1;
            } else if (0 == memcmp(str_result, "false", tuya_strlen("false"))) {
                bt_play = 0;
            } else {
                // Data error
                goto ERR_EXTI;
            }
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error "Please implement the voice module status notification processing code yourself. Delete this line after completion."
#endif
            // MCU settings currently only support "Play/Pause" and "Bluetooth Switch"
            // play    Play/Pause function  1(Play) / 0(Pause)
            // bt_play Bluetooth switch function   1(On)   / 0(Off)

            send_len = 0;
            send_len = set_wifi_uart_byte(send_len, sub_cmd);
            send_len = set_wifi_uart_byte(send_len, 0x00);
            wifi_uart_write_frame(MODULE_EXTEND_FUN_CMD, MCU_TX_VER, send_len);
        } break;

        default:
            break;
    }

    return;

ERR_EXTI:
    send_len = 0;
    send_len = set_wifi_uart_byte(send_len, sub_cmd);
    send_len = set_wifi_uart_byte(send_len, 0x01);
    wifi_uart_write_frame(MODULE_EXTEND_FUN_CMD, MCU_TX_VER, send_len);
    return;
}
#endif

#if defined(DP_WITH_TYPE_ENABLE)
void dp_type_extended_result(const u8 value[], u16 length) {
    u8 sub_cmd = value[0];
    u8 result;
    u8 source;
    u16 send_len = 0;
    const u8* data_ptr;
    const u8* data_end;
    u16 data_len;
    u8 dp_id;
    u8 dp_type;

#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error Implement DP-with-type related logic here
#endif

    switch (sub_cmd) {
        case DP_WITH_TYPE_EXTEND_SUB_CMD_START:
            result = value[DATA_START + 1];
            if (result == 0x00) {
                // Operation successful

            } else if (result == 0x01) {
                // Operation failed
            }
            break;
        case DP_WITH_TYPE_EXTEND_SUB_CMD_DOWNLOAD:
            // Process DP download
            source = value[DATA_START + 2];
            switch (source) {
                case 0x00:  // Unknown source
                    break;
                case 0x01:  // Local area network
                    break;
                case 0x02:  // Wide area network
                    break;
                case 0x03:  // Local timer
                    break;
                case 0x04:  // WAN scene linkage
                    break;
                case 0x05:  // Reliable channel
                    break;
                case 0x06:  // Bluetooth
                    break;
                case 0x07:  // Local scene linkage
                    break;
                case 0xF0:  // Offline voice module
                    break;
            }
            data_end = &value[DATA_START + length];
            data_ptr = &value[DATA_START + 2];
            while (data_ptr < data_end) {
                dp_id = *data_ptr;
                data_ptr++;
                dp_type = *data_ptr;
                data_ptr++;
                data_len = (u16)data_ptr[0] << 8 | data_ptr[1];
                data_ptr += 2;
                dp_download_handle(dp_id, data_ptr, data_len);
                data_ptr += data_len;
            }

            break;
    }
}
#endif

#if defined(PRODUCT_TEST_ENABLE)

void product_test_fun(const u8 value[], u16 length) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error Production test related logic is highly product-specific and is not implemented here. Please complete the related logic based on the actual product situation.
#endif

    u8 sub_cmd = value[0];
    u16 send_len = 0;
    u16 buf_len;

    u8 result;
    char json_buf[32];
    switch (sub_cmd) {
        case PRODUCT_TEST_SUB_CMD_STATE_NOTIFY:
            switch (value[1]) {
                case 0x00:
                    // Enter production test
                    TUYA_DBG_EXEC(TUYA_PRINT("enter product test."));
                    break;
                case 0x01:
                    // Exit production test
                    TUYA_DBG_EXEC(TUYA_PRINT("exit product test."));
                    break;
            }
            send_len = set_wifi_uart_byte(send_len, sub_cmd);
            send_len = set_wifi_uart_byte(send_len, value[1]);
            send_len = set_wifi_uart_byte(send_len, 0x00);
            wifi_uart_write_frame(PRODUCT_TEST_CMD, MCU_TX_VER, send_len);
            break;
        case PRODUCT_TEST_SUB_CMD_KEY_TEST:
            TUYA_DBG_EXEC(TUYA_PRINT("key test."));

            break;
        case PRODUCT_TEST_SUB_CMD_LED_TEST:
            TUYA_DBG_EXEC(TUYA_PRINT("led test."));
            send_len = set_wifi_uart_byte(send_len, sub_cmd);

            break;
        case PRODUCT_TEST_SUB_CMD_TRANSPARENT_TRANS_TEST:
            break;
        case PRODUCT_TEST_SUB_CMD_MODULE_TEST_STATE_NOTIFY:
            break;
        case PRODUCT_TEST_SUB_CMD_TEST_RESULT_REPORT:
            break;
        default:
            TUYA_DBG_EXEC(TUYA_PRINT("Unknown subcmd %02X", sub_cmd));
            break;
    }
}

#endif

#if defined(CN_IOT_EXTENDED_ENABLE)

void cn_iot_extended_fun(const u8 value[], u16 length) {
    TUYA_UNUSED(value);
    TUYA_UNUSED(length);
}

#endif

#if defined(FAN_PRODUCT_SERVICE_ENABLE)

void fan_product_service_fun(const u8 value[], u16 length) {
    u8 sub_cmd = value[0];
    u8 result;
    switch (sub_cmd) {
        case FAN_PRODUCT_SERVICE_SUB_CMD_TEST:  // Fan function test
            result = value[1];
            if (result == 0x00) {
                TUYA_DBG_EXEC(TUYA_PRINT("Fan product test OK"));
                // Success
            } else {
                // Failure
                TUYA_DBG_EXEC(TUYA_PRINT("Fan product test FAILED"));
            }
            break;
        case FAN_PRODUCT_SERVICE_SUB_CMD_SET_DUTY:  // Duty cycle test
            result = value[1];
            if (result == 0x00) {
                // Success
                TUYA_DBG_EXEC(TUYA_PRINT("Fan product test duty OK"));

            } else {
                // Failure
                TUYA_DBG_EXEC(TUYA_PRINT("Fan product test duty FAILED"));
            }
            break;
    }
}
#endif

#if defined(USER_DEFINE_COMMAND_ENABLE)

void user_defined_fun(const u8 value[], u16 length) {
    TUYA_UNUSED(value);
    TUYA_UNUSED(length);
    // User-defined command
}

#endif

#if defined(MATTER_COMMON_FUNCTION_ENABLE)

void matter_common_fun(const u8 value[], u16 length) {
    u8 sub_cmd = value[0];
    u8 status;
    u8 result;
    u16 send_len = 0;

    switch (sub_cmd) {
        case MATTER_COMMON_CMD_IDENTIFY_STATE_CHANGE_NOTIFY:
            status = value[1];
            TUYA_DBG_EXEC(TUYA_PRINT("Matter indentify state change: %d", status));
            switch (status) {
                case 0x00:
                    // identify start
                    break;
                case 0x01:
                    // identify stop
                    break;
                case 0x02:
                    // identify effect Blink
                    break;
                case 0x03:
                    // identify effect Breathe
                    break;
                case 0x04:
                    // identify effect Okay
                    break;
                case 0x05:
                    // identify effect Channel change
                    break;
                case 0x06:
                    // identify effect Finish
                    break;
                case 0x07:
                    // identify effect Stop
                    break;
            }

            // TODO: Success: 0 Failure: 1
            // result = ?????;

            send_len = set_wifi_uart_byte(send_len, sub_cmd);
            send_len = set_wifi_uart_byte(send_len, result);
            wifi_uart_write_frame(MATTER_COMMON_CMD, MCU_TX_VER, send_len);
            break;

        case MATTER_COMMON_CMD_EVENT_NOTIFY:
            status = value[1];
            TUYA_DBG_EXEC(TUYA_PRINT("Matter event: %d", status));
            switch (status) {
                case 0x00:
                    // Normal power-on startup
                    TUYA_DBG_EXEC(TUYA_PRINT("Power on"));
                    break;
                case 0x01:
                    // Network configuration window open
                    TUYA_DBG_EXEC(TUYA_PRINT("net config window open"));
                    break;
                case 0x02:
                    // Network configuration successful
                    TUYA_DBG_EXEC(TUYA_PRINT("net config SUCCESSFUL."));
                    break;
                case 0x03:
                    // Network configuration failed
                    TUYA_DBG_EXEC(TUYA_PRINT("net config FAILED."));
                    break;
                case 0x04:
                    // Reset (not reported yet)
                    TUYA_DBG_EXEC(TUYA_PRINT("reset."));
                    break;
                case 0x05:
                    // Network configuration window closed
                    TUYA_DBG_EXEC(TUYA_PRINT("net config window close"));
                    break;
                case 0x06:
                    // Remove network configuration information
                    TUYA_DBG_EXEC(TUYA_PRINT("net config info remove"));
                    break;
            }

            // TODO: Success: 0 Failure: 1
            // result = ?????;

            send_len = set_wifi_uart_byte(send_len, sub_cmd);
            send_len = set_wifi_uart_byte(send_len, result);
            wifi_uart_write_frame(MATTER_COMMON_CMD, MCU_TX_VER, send_len);
            break;
    }
}

#endif

#if defined(CLOUD_STORAGE_FUNCTION_ENABLE)

void cloud_stonage_fun(const u8 value[], u16 length) {
    u8 sub_cmd = value[0];
    u8 result;
    u16 send_len = 0;

    switch (sub_cmd) {
        case CLOUD_STONAGE_TRANS_SUB_CMD_UPLOAD_START:
            // TODO: Success: 0 Failure: 1
            result = value[1];
            if (result == 0x00) {
                TUYA_DBG_EXEC(TUYA_PRINT("Cloud stonage start OK"));
            } else {
                TUYA_DBG_EXEC(TUYA_PRINT("Cloud stonage start FAILED"));
            }
            break;
        case CLOUD_STONAGE_TRANS_SUB_CMD_UPLOAD_DATA:
            // TODO:
            result = value[1];
            if (result == 0x00) {
                TUYA_DBG_EXEC(TUYA_PRINT("Cloud stonage upload frame OK"));
            } else {
                TUYA_DBG_EXEC(TUYA_PRINT("Cloud stonage upload frame FAILED"));
            }
            break;
        default:
            TUYA_DBG_EXEC(TUYA_PRINT("Unknown subcmd %02X", sub_cmd));
            break;
    }
}

#endif

#if defined(AP_TRANSPARENT_TRANS_TEST_ENABLE)

void ap_transparent_trans_test_fun(const u8 value[], u16 length) {
    u8 sub_cmd = value[0];
    const u8* buf_ptr;
    u16 buf_len;
    u16 send_len = 0;
    switch (sub_cmd) {
        case AP_TRANSPARENT_TRANS_TEST_SUB_CMD_UPSTREAM_DATA:
            buf_ptr = &value[1];
            buf_len = length - 1;

            // TODO: Process data
            // ...

            send_len = set_wifi_uart_byte(send_len, sub_cmd);
            wifi_uart_write_frame(AP_TRANSPARENT_TRANS_TEST_CMD, MCU_TX_VER, send_len);
            break;

        case AP_TRANSPARENT_TRANS_TEST_SUB_CMD_DOWNSTREAM_DATA:

            // TODO: do something

            break;
    }
}

#endif

#if defined(FACTORY_RECOVERY_FUNCTION_ENABLE)

void factory_recovery_result() {
    TUYA_DBG_EXEC(TUYA_PRINT("Got reset event"));
    // TODO: do something
}

#endif

#if defined(DP_CACHE_ENABLE)

void dp_cache_get_result(const u8 value[], u16 length) {
    u8 result = value[0];
    u8 count;
    const u8* data_start;
    const u8* data_end;
    if (result == 0) {
        // Failure
        return;
    }
    count = value[1];
    data_start = &value[2];
    data_end = &value[length];

#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error Complete the related logic here. data_start points to the first data, data_end points to the element after the last data.
#endif
}

#endif

#if defined(HIBERNATE_ENABLE)

void hibernate_fun(const u8 value[], u16 length) {
    u8 sub_cmd = value[0];
    u8 reason;
    u8 allow_hibernate;
    u16 send_len = 0;

    switch (sub_cmd) {
        case HIBERNATE_SUB_CMD_HIBERNATE_NOTIFY:
            TUYA_DBG_EXEC(TUYA_PRINT(" module request enter hibernate."));
            // TODO: The module requests to enter hibernate. Here you should check if there is any unuploaded data. If yes, reply 0x00 to prevent module hibernation.
            allow_hibernate = 0x01;  // TODO:  0x00: Sleep not allowed, the module will retry after 5 seconds  0x01 Sleep allowed, the module will sleep immediately
            send_len = set_wifi_uart_byte(send_len, HIBERNATE_SUB_CMD_HIBERNATE_NOTIFY);
            send_len = set_wifi_uart_byte(send_len, allow_hibernate);
            wifi_uart_write_frame(HIBERNATE_CMD, MCU_TX_VER, send_len);
            break;
        case HIBERNATE_SUB_CMD_WAKEUP_NOTIFY:
            // TODO: Wakeup
            TUYA_DBG_EXEC(TUYA_PRINT("wakeup."));

            send_len = set_wifi_uart_byte(send_len, HIBERNATE_SUB_CMD_WAKEUP_NOTIFY);
            wifi_uart_write_frame(HIBERNATE_CMD, MCU_TX_VER, send_len);
            break;
        case HIBERNATE_SUB_CMD_HIBERNATE_REQUEST:
            reason = value[1];
            if (reason == 0) {
                // Allow to enter sleep
                TUYA_DBG_EXEC(TUYA_PRINT("Allow to hibernate"));
            } else {
                // Not allowed to enter sleep, reasons:
                // Bit0: Wakeup IO not set to invalid level, 1: Task status; 0: Not in task;
                // Bit1: OTA task, 1: Task status; 0: Not in task;
                // Bit2: Device status synchronization, 1: Task status; 0: Not in task;
                // Bit3: Other task processing, 1: Task status; 0: Not in task;
                // Bit4-bit7: Reserved, default 0;
                TUYA_DBG_EXEC(TUYA_PRINT(
                    "Reject to hibernate because: "
                    "Wakeup IO invalid: %s, "
                    "OTA task: %s, "
                    "sync task: %s, "
                    "other task: %s",
                    reason & 0x01 ? "yes" : "no",
                    reason & 0x02 ? "yes" : "no",
                    reason & 0x04 ? "yes" : "no",
                    reason & 0x08 ? "yes" : "no"));
            }
            break;
    }
}

#endif

#if defined(OFFLINE_VOICE_CTRL_ENABLE)

void offline_voice_ctrl(const u8 cmd_datas[], u16 cmd_data_length) {
    u32 wkup_id = UINT32_NTOH(&cmd_datas[0]);
    u16 send_len = 0;
    u8 result = 0;

    TUYA_DBG_EXEC(TUYA_PRINT("Got offline ctrl event, wakeup id is %08X", wkup_id));

#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error Refer to the protocol documentation and wake word ID definitions to complete the processing for each wake word.
#endif

    switch (wkup_id) {
        // TODO: Refer to the wake word ID definitions in offline_voice_word_<lang>.h (Please include the target language definition header as needed)
        // Wake word definition names follow the format OFFLINE_VOICE_WORD_<lang>_<type>_<action>. For detailed definitions, refer to the comments next to the header definitions or related documentation.
        // It is recommended to update the corresponding DP value after receiving a wake word.

        /*
        EXAMPLE:

        case OFFLINE_VOICE_WORD_CN_CTRL_POWER_ON: // Air conditioner power on
            // Process power on logic
            mcu_dp_bool_update(DP_ID_POWER_ON, 1);
            break;

        case OFFLINE_VOICE_WORD_CN_CTRL_POWER_OFF: // Air conditioner power off
            // Process power off logic
            mcu_dp_bool_update(DP_ID_POWER_ON, 0);
            break;

        */
    }
    result = 0x00;  // TODO: 0x00: Success; 0x01: Failure
    send_len = set_wifi_uart_byte(send_len, result);
    wifi_uart_write_frame(OFFLINE_VOICE_CTRL_CMD, MCU_TX_VER, send_len);
}

#endif
