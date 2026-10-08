/**********************************Copyright (c)**********************************
**                       Copyright (C), 2015-2026, Tuya Technology
**
**                             http://www.tuya.com
**
*********************************************************************************/
/**
 * @file    protocol.h
 * @author  Tuya Comprehensive Protocol Development Team
 * @version v2.6.2
 * @date    2026.02.04
 * @brief                
 *                       *******Very important, must read!!!********
 *          1. Users implement data downlink/reporting functions in this file
 *          2. DP ID/TYPE and data processing functions need to be implemented by the user according to actual definitions
 *          3. After enabling certain macro definitions, there are #err prompts inside functions that require user implementation. Please delete the #err after completing the function.
 */

/****************************** Disclaimer !!! *******************************
Due to the variety of MCU types and compilation environments, this code is for reference only. Users are responsible for the final code quality.
Tuya is not responsible for the functional results of the MCU.
******************************************************************************/

#ifndef __PROTOCOL_H_
#define __PROTOCOL_H_

#include "tuya_type.h"

/******************************************************************************
                           User-related Information Configuration
******************************************************************************/
/******************************************************************************
                       1: Modify Product Information
******************************************************************************/
#if defined(CONFIG_TUYA_ENABLE_TEST)
#include "dp_id.code_snippet"
#else
#define PRODUCT_KEY "smeeba2wkiop1su7"    //The unique product identification generated after product is created on development platform

#endif
#define MCU_VER "1.0.0"         //User's software version, used for MCU firmware update. Modify for MCU update version.

/*  Module working mode selection, only one of three. Anti-mis-touch mode is recommended. */
//#define CONFIG_MODE     CONFIG_MODE_DEFAULT             //Default working mode
//#define CONFIG_MODE     CONFIG_MODE_LOWPOWER            //Safe mode (low power consumption network configuration)
#define CONFIG_MODE     CONFIG_MODE_SPECIAL             //Anti-mis-touch mode (special network configuration method)
/* PORTED: note only -- kept as generated.  In this mode the reset the pairing
   button sends (mcu_reset_wifi, tuya_link.c) opens a three-minute pairing
   window, and a module that is not paired within it goes back to the network
   it already had.  A stray press therefore costs nothing.  The three minutes
   are why WIFI_PAIRING_TIMEOUT_MS (temp_sensor_main.c) is three minutes too. */

/*  Set the network configuration mode opening time for low-power network configuration mode and special network configuration mode. If this macro is commented, it will be handled as three minutes. Supported data range: 3~10 minutes. */
// #define CONFIG_MODE_DELAY_TIME    10         //Network configuration mode opening time Unit: minutes

/*  Choose smart mode and AP mode. If both macros are commented, smart mode and AP mode will be able to switch mutually. */
//#define CONFIG_MODE_CHOOSE        0         //The module supports both AP connection network configuration and EZ network configuration without user switching. Corresponding network configuration status 0x06
//#define CONFIG_MODE_CHOOSE        1         //Only AP network configuration mode

/*  Enable the infrared function of the module and inform the module which I/O pins are used for infrared transceiver. Without this field, the infrared capability is disabled by default. */
//#define ENABLE_MODULE_IR_FUN                  //Enable module infrared function
#ifdef ENABLE_MODULE_IR_FUN
#define MODULE_IR_PIN_TX      5       //Infrared transmit pin
#define MODULE_IR_PIN_RX      12      //Infrared receive pin
#endif

/*  Whether the module enables the low-power mode that maintains a long connection. Without this field, the low-power mode is disabled by default. */
//#define LONG_CONN_LOWPOWER        0         //Disable low-power mode
//#define LONG_CONN_LOWPOWER        1         //Enable low-power mode

/******************************************************************************
                          2: Does the MCU need to support firmware update?
If MCU firmware update support is needed, please enable this macro.
The MCU can call the mcu_firm_update_query() function in the mcu_api.c file to obtain the current MCU firmware update status.
                        ********WARNING!!!**********
The current receive buffer size is for the closed firmware update function. Firmware update package can be selected, default 256 bytes.
If this function needs to be enabled, the UART receive buffer will become larger.
******************************************************************************/
//#define         SUPPORT_MCU_FIRM_UPDATE                 //Enable MCU firmware update function (disabled by default)
/*  Firmware package size selection  */
#ifdef SUPPORT_MCU_FIRM_UPDATE
#define PACKAGE_SIZE                   0        //Package size is 256 bytes
//#define PACKAGE_SIZE                   1        //Package size is 512 bytes
//#define PACKAGE_SIZE                   2        //Package size is 1024 bytes
#endif
/******************************************************************************
                         3: Define Transmit/Receive Buffers:
                    If the current MCU's RAM is insufficient, it can be modified to 24.
******************************************************************************/
#ifndef SUPPORT_MCU_FIRM_UPDATE
/* PORTED: raised from Tuya's 16/24 defaults.  Neither is a knob to shave RAM
   with.

   WIFI_UART_RECV_BUF_LMT sizes the ISR ring (+PROTOCOL_HEAD, so 39 bytes here).
   uart_receive_input() drops a byte without a word when the ring is full -- no
   UART error, nothing any counter can see -- so the ring has to cover the
   longest gap between wifi_uart_service() calls.  The longest gap in this
   firmware is all_data_update(): uart_transmit_output blocks a byte at a time,
   so four DP frames (57 bytes) hold the loop for ~60 ms while the module is
   free to send a heartbeat or a status change.  Those are 7-8 bytes each; 39
   bytes is ~40 ms of continuous wire time at 9600 8N1, which the module never
   sends.  Tuya's 16 gives 23 bytes.

   WIFI_DATA_PROCESS_LMT sizes frame assembly.  Every DP on this product is
   report-only, so the module never issues DP data and the largest inbound
   frame is an 8-byte status report.  A frame too long for the buffer is
   discarded rather than wedging the parser (wifi_uart_service, PORTED), so
   this is margin, not a cliff.

   Cost of 32/32 over Tuya's 16/24: 24 bytes of RAM. */
#define WIFI_UART_RECV_BUF_LMT          32              //UART data receive buffer size. Must cover the longest gap between wifi_uart_service() calls.
#define WIFI_DATA_PROCESS_LMT           32              //UART data processing buffer size. Determined based on user DP data size. Must be greater than 24.
#else
#define WIFI_UART_RECV_BUF_LMT          128             //UART data receive buffer size. Can be reduced if MCU RAM is insufficient.

//Please choose an appropriate UART data processing buffer size here (based on the selected MCU firmware update package size above and whether weather service is enabled to decide the buffer size)
#define WIFI_DATA_PROCESS_LMT           300             //UART data processing buffer size. If MCU firmware update is needed, if single package size is 256, buffer must be >260. If weather service is enabled, a larger buffer is needed.
//#define WIFI_DATA_PROCESS_LMT           600             //UART data processing buffer size. If MCU firmware update is needed, if single package size is 512, buffer must be >520. If weather service is enabled, a larger buffer is needed.
//#define WIFI_DATA_PROCESS_LMT           1200            //UART data processing buffer size. If MCU firmware update is needed, if single package size is 1024, buffer must be >1030. If weather service is enabled, a larger buffer is needed.

#endif

/* PORTED: note only.  The longest outgoing frame is the product information
   reply, {"p":"<PID>","v":"1.0.0","m":2}, at 42 data bytes.  Nothing bounds
   the writes into this buffer, so anything added to that reply
   (CONFIG_MODE_DELAY_TIME, CONFIG_MODE_CHOOSE, LONG_CONN_LOWPOWER) has to be
   checked against 48 here. */
#define WIFIR_UART_SEND_BUF_LMT         48              //Determined based on user DP data size. Must be greater than 48.
/******************************************************************************
                        4: Define Module Working Mode
Module self-handling:
          WiFi indicator and WiFi reset button are connected to the WiFi module (enable WIFI_CONTROL_SELF_MODE macro)
          and correctly define WF_STATE_KEY and WF_RESET_KEY.
MCU self-handling:
          WiFi indicator and WiFi reset button are connected to the MCU (disable WIFI_CONTROL_SELF_MODE macro)
          MCU calls mcu_reset_wifi() function in mcu_api.c file where WiFi reset needs to be handled, and can call mcu_get_reset_wifi_flag() function to return the WiFi reset result.
          Or call mcu_set_wifi_mode(WIFI_CONFIG_E mode) function in mcu_api.c file to set WiFi mode, and can call mcu_get_wifi_work_state() function to return the WiFi setting result.
******************************************************************************/
//#define         WIFI_CONTROL_SELF_MODE                       //WiFi self-handling button and LED indicator; If button/LED indicator is externally connected to MCU, please disable this macro.
#ifdef          WIFI_CONTROL_SELF_MODE                      //Module self-handling
  #define     WF_STATE_KEY            14                    //WiFi module status indicator key, please set according to actual GPIO pin.
  #define     WF_RESERT_KEY           0                     //WiFi module reset key, please set according to actual GPIO pin.
#endif

/******************************************************************************
                      5: Does the MCU need to support time calibration function?
If needed, please enable this macro and implement the code in mcu_write_rtctime in the Protocol.c file.
There is a #err prompt inside mcu_write_rtctime. Please delete the #err after completing the function.
The MCU can call mcu_get_system_time() function to initiate time calibration after the WiFi module successfully connects to the network.
******************************************************************************/
//#define         SUPPORT_MCU_RTC_CHECK                //Enable time calibration function

/******************************************************************************
                      6: Does the MCU need to support WiFi function test?
If needed, please enable this macro, and the MCU calls mcu_start_wifitest in the mcu_api.c file where WiFi function test is needed.
And view the test result in the wifi_test_result function in the protocol.c file.
There is a #err prompt inside wifi_test_result. Please delete the #err after completing the function.
******************************************************************************/
/* PORTED: off.  Nothing on this product runs the production scan test, and
   left on it only compiles a result handler that is an #error. */
//#define         WIFI_TEST_ENABLE                //Enable WiFi production test function (scan specified router)

/******************************************************************************
                      7: Whether to enable weather function
If needed, please enable this macro, and implement display and other code in the two user handling functions weather_open_return_handle and weather_data_user_handle in the protocol.c file.
There are #err prompts inside these two functions. Please delete the #err after completing the functions.
Enabling weather function requires a larger UART data buffer size.
******************************************************************************/
//#define         WEATHER_ENABLE                  //Enable weather function
#ifdef          WEATHER_ENABLE
/*  Can be adjusted in the weather_choose array in the protocol.c file, then write the number of opened service types to this macro definition. */
#define         WEATHER_CHOOSE_CNT              4   //Number of selected weather service types needed
/*  When enabling weather service, this macro definition can be set to select the number of forecast days. 1 means today's weather (if forecast is not needed, set to 1), maximum is 7 days (cannot be set to 0 or >7). */
#define         WEATHER_FORECAST_DAYS_NUM       1   //Set the number of weather forecast days
#endif

/******************************************************************************
                      8: Whether to enable WiFi module heartbeat stop function
If needed, please enable this macro, call the wifi_heart_stop function in the mcu_api.c file to stop the heartbeat.
******************************************************************************/
//#define         WIFI_HEARTSTOP_ENABLE           //Enable heartbeat stop function

/******************************************************************************
                      9: Whether to support stream service function
STREM_PACK_LEN is the size of one packet for stream service transmission. Currently, the maximum data part the module UART can cache can reach 1024 bytes. One packet of map data packet data part cannot exceed 1024 bytes. Each packet of map data content is recommended to be 512 bytes per packet.
******************************************************************************/
//#define         WIFI_STREAM_ENABLE              //Support stream service related functions
#ifdef WIFI_STREAM_ENABLE
#define         STREM_PACK_LEN                 256
#endif

/******************************************************************************
                      10: Does the MCU need to support WiFi function test (connect to specified router)?
If needed, please enable this macro, and the MCU calls mcu_start_connect_wifitest in the mcu_api.c file where WiFi function test is needed.
And view the test result in the wifi_connect_test_result function in the protocol.c file.
There is a #err prompt inside wifi_connect_test_result. Please delete the #err after completing the function.
******************************************************************************/
//#define         WIFI_CONNECT_TEST_ENABLE                //Enable WiFi production test function (connect to specified router)

/******************************************************************************
                      11: Does the MCU need to enable the function to get current WiFi connection status?
If needed, please enable this macro, and the MCU calls mcu_get_wifi_connect_status in the mcu_api.c file where obtaining current WiFi connection status is needed.
And view the result in the wifi_test_result function in the protocol.c file.
There is a #err prompt inside wifi_test_result. Please delete the #err after completing the function.
******************************************************************************/
//#define         GET_WIFI_STATUS_ENABLE                  //Enable function to get current WiFi connection status

/******************************************************************************
                      12: Does the MCU need to enable the function to get module MAC address?
If needed, please enable this macro, and the MCU calls mcu_get_module_mac in the mcu_api.c file where obtaining module MAC address is needed.
And view the result in the mcu_get_mac function in the protocol.c file.
There is a #err prompt inside mcu_get_mac. Please delete the #err after completing the function.
******************************************************************************/
//#define         GET_MODULE_MAC_ENABLE                   //Enable function to get module MAC address

/******************************************************************************
                      13: Does the MCU need to support getting Greenwich Mean Time?
If needed, please enable this macro, and the MCU calls mcu_get_green_time in the mcu_api.c file where obtaining Greenwich Mean Time is needed.
And view the result in the mcu_get_greentime function in the protocol.c file.
There is a #err prompt inside mcu_get_greentime. Please delete the #err after completing the function.
The MCU can call mcu_get_green_time() function to initiate time calibration after the WiFi module successfully connects to the network.
******************************************************************************/
//#define         SUPPORT_GREEN_TIME                //Enable Greenwich Mean Time function

/******************************************************************************
                      14: Does the MCU need to enable synchronous status reporting function?
1) This command is a synchronous instruction. After the MCU data status is reported, it needs to wait for the module to return a result;
2) Each transmission will have a response from the module. Multiple reports cannot be made before the WIFI module responds;
3) If the network is poor and data cannot be reported in time, the module will return failure after 5 seconds. The MCU needs to wait more than 5 seconds.
******************************************************************************/
//#define         MCU_DP_UPLOAD_SYN                   //Enable synchronous status reporting function

/******************************************************************************
                      15: Does the MCU need to enable infrared status notification function?
If needed, please enable this macro,
and view the result and reply in the get_ir_status function in the protocol.c file.
There is a #err prompt inside get_ir_status. Please delete the #err after completing the function.
******************************************************************************/
//#define         GET_IR_STATUS_ENABLE                   //Enable infrared status notification function

/******************************************************************************
                      16: Does the MCU need to enable infrared enter transceiver production test function?
If needed, please enable this macro, and the MCU calls mcu_start_ir_test in the mcu_api.c file where infrared enter transceiver production test is needed.
And view the test result in the ir_tx_rx_test_result function in the protocol.c file.
There is a #err prompt inside ir_tx_rx_test_result. Please delete the #err after completing the function.
******************************************************************************/
//#define         IR_TX_RX_TEST_ENABLE                   //Enable infrared enter transceiver production test function

/******************************************************************************
                      17: File package download function
If needed, please enable this macro, and the package size needs to be selected.
And process data in the file_download_handle function in the protocol.c file.
There is a #err prompt inside file_download_handle. Please delete the #err after completing the function.
******************************************************************************/
//#define         FILE_DOWNLOAD_ENABLE                   //Enable file package download function
//File download package size selection
#ifdef FILE_DOWNLOAD_ENABLE
#define FILE_DOWNLOAD_PACKAGE_SIZE                   0        //Package size is 256 bytes
//#define FILE_DOWNLOAD_PACKAGE_SIZE                   1        //Package size is 512 bytes
//#define FILE_DOWNLOAD_PACKAGE_SIZE                   2        //Package size is 1024 bytes
#endif

/******************************************************************************
                      18: Does the MCU need to support voice module related protocol functions?
This protocol is only applicable to the general connection of voice module VWXR2. Other non-voice module general firmware does not have the related protocol functions in this directory.
If needed, please enable this macro, and the MCU calls the functions get_voice_state/set_voice_MIC_silence/set_speaker_voice/voice_test/voice_awaken_test in the mcu_api.c file where voice module related protocol functions are needed.
And view the results in the related result handling functions in the protocol.c file.
There are #err prompts inside the result handling functions. Please delete the #err after completing the functions.
******************************************************************************/
//#define         VOICE_MODULE_PROTOCOL_ENABLE           //Enable voice module related protocol functions

/******************************************************************************
                      19: Does the MCU need to support module expansion service function?
If needed, please enable this macro, and the MCU calls open_module_time_serve in the mcu_api.c file where module expansion service is needed.
And view the result in the open_module_time_serve_result function in the protocol.c file.
There is a #err prompt inside open_module_time_serve_result. Please delete the #err after completing the function.
******************************************************************************/
//#define         MODULE_EXPANDING_SERVICE_ENABLE        //Enable module expansion service function

/******************************************************************************
                      20: Does the MCU need to support Bluetooth related functions?
If needed, please enable this macro, and the MCU calls mcu_start_BLE_test in the mcu_api.c file where Bluetooth related functions are needed.
And view the test result in the BLE_test_result function in the protocol.c file.
There is a #err prompt inside BLE_test_result. Please delete the #err after completing the function.
******************************************************************************/
//#define         BLE_RELATED_FUNCTION_ENABLE            //Enable Bluetooth related functions


/*******************************************************************************
                      21. DP with Type Function
********************************************************************************/
// #define         DP_WITH_TYPE_ENABLE  // Enable DP with Type

/*******************************************************************************
                      22. Record-Type DP Function
********************************************************************************/
// #define       RECORD_TYPE_DP_ENABLE       // Enable Record-Type DP Function

/*******************************************************************************
                      23. Soft Bus Service
May need to appropriately increase the receive data buffer size based on received data size.
********************************************************************************/
// #define SBUS_SERVICE_ENABLE // Enable Soft Bus Service

/*******************************************************************************
                      24. Finished Product Production Test Service
********************************************************************************/
// #define PRODUCT_TEST_ENABLE // Enable Finished Product Test

/*******************************************************************************
                      25. CN IoT Specific Functions
********************************************************************************/
// #define CN_IOT_EXTENDED_ENABLE

/*******************************************************************************
                      26. Fan Product Test Service
********************************************************************************/
// #define FAN_PRODUCT_SERVICE_ENABLE

/*******************************************************************************
                      27. User-Defined Commands
********************************************************************************/
// #define USER_DEFINE_COMMAND_ENABLE

/*******************************************************************************
                      27. Matter Class Common Protocol
********************************************************************************/
// #define MATTER_COMMON_FUNCTION_ENABLE

/*******************************************************************************
                      28. Cloud Storage Service
May need to appropriately increase send and receive buffer sizes.
********************************************************************************/
// #define CLOUD_STORAGE_FUNCTION_ENABLE

/*******************************************************************************
                      29. AP Transparent Transmission Test
********************************************************************************/
// #define AP_TRANSPARENT_TRANS_TEST_ENABLE

/*******************************************************************************
                      30. Factory Reset Function
********************************************************************************/
// #define FACTORY_RECOVERY_FUNCTION_ENABLE

/*******************************************************************************
                      31. Get DP Cache Command
********************************************************************************/
// #define DP_CACHE_ENABLE

/*******************************************************************************
                      32. Hibernate Function
********************************************************************************/
// #define HIBERNATE_ENABLE

/*******************************************************************************
                      33. Offline Voice Commands
********************************************************************************/
// #define OFFLINE_VOICE_CTRL_ENABLE




/******************************************************************************
                        1: Redefine dp data point sequence number
          **This is auto-generated code. If there are related modifications on the development platform, please re-download the MCU_SDK**         
******************************************************************************/
#if defined(CONFIG_TUYA_ENABLE_TEST)
#include "dp_define.code_snippet"
#else
//Temperature(Only report)
#define DPID_TEMP_CURRENT 1
//Humidity(Only report)
#define DPID_HUMIDITY_VALUE 2
//Battery level state(Only report)
#define DPID_BATTERY_STATE 3
//Battery level(Only report)
#define DPID_BATTERY_PERCENTAGE 4

#endif

/**
 * @brief  UART transmit data
 * @param[in] {value} 1 byte data to be transmitted by UART
 * @return Null
 */
void uart_transmit_output(u8 value);


#if defined(CONFIG_MCU_SDK_TEST_ONLY_USE_NEW_DISPATCHER)


/**
 * @brief  Handle buffer overflow callback
 * 
 * When a command header is received and it is found that the required length of the command data is greater than the buffer length, call this function to notify the user.
 * Generally, this situation should not occur.
 * During debugging, it is recommended to check whether the defined buffer length is large enough.
 * In production environment, it is recommended to directly reset the MCU and module, as this may indicate potential attack risk.
 * 
 * After this, the parser and receive buffer will be reset.
 * 
 * @param cmd The received command byte (unreliable)
 * @param expected_len The length declared by the command
 */
PROCESS_BUFFER_OVERFLOW_HANDLE_METHOD_E on_process_buffer_overflow(u8 cmd, u16 expected_len);

/**
 * @brief The first command to be executed after buffer overflow
 * 
 * Callback for the first command to be executed after on_process_buffer_overflow is triggered.
 * In production environment, buffer overflow may indicate potential attack. After this, we cannot determine whether this command truly comes from the module or is part of the data segment of the previous command.
 * If this command attempts to perform dangerous operations (such as firmware update), it is recommended to directly abandon it.
 * 
 * If false is returned, this callback will continue to be called when the next command is received until true is returned.
 * 
 * @param frame Pointer to the command frame
 * @param frame_len Length of the command frame
 * @return FALSE: Abandon this command other: Process this command
 */

bool_t on_first_command_after_overflow(const u8 * frame, u16 frame_len);


#endif

/**
 * @brief  Upload all dp point information of the system to achieve APP and MCU data synchronization
 * @param  Null
 * @return Null
 * @note   MCU must implement the data reporting function inside this function.
 */
void all_data_update(void);

/**
 * @brief  dp downlink processing function
 * @param[in] {dpid} dpid sequence number
 * @param[in] {value} dp data buffer address
 * @param[in] {length} dp data length
 * @return dp processing result
 * -           0(ERROR): Failure
 * -           1(SUCCESS): Success
 * @note   This function cannot be modified by the user.
 */
u8 dp_download_handle(u8 dpid,const u8 value[], u16 length);

/**
 * @brief  Get the total number of all dp commands
 * @param[in] Null
 * @return Total number of downlink commands
 * @note   This function cannot be modified by the user.
 */
u8 get_download_cmd_total(void);



#ifdef SUPPORT_MCU_FIRM_UPDATE
/**
 * @brief  Upgrade package size selection
 * @param[in] {package_sz} Upgrade package size
 * @ref           0x00: 256byte (default)
 * @ref           0x01: 512byte
 * @ref           0x02: 1024byte
 * @return Null
 * @note   MCU needs to implement this function itself.
 */
void upgrade_package_choose(u8 package_sz);

/**
 * @brief  MCU enters firmware update mode
 * @param[in] {value} Firmware buffer
 * @param[in] {position} Current data packet's position in the firmware
 * @param[in] {length} Current firmware packet length (when firmware packet length is 0, it indicates the firmware packet sending is complete)
 * @return Null
 * @note   MCU needs to implement this function itself.
 */
u8 mcu_firm_update_handle(const u8 value[],u32 position,u16 length);
#endif

#ifdef SUPPORT_GREEN_TIME
/**
 * @brief  Obtained Greenwich Mean Time
 * @param[in] {time} Obtained Greenwich Mean Time data
 * @return Null
 * @note   MCU needs to implement this function itself.
 */
void mcu_get_greentime(u8 time[]);
#endif

#ifdef SUPPORT_MCU_RTC_CHECK
/**
 * @brief  MCU calibrate local RTC clock
 * @param[in] {time} Obtained Greenwich Mean Time data
 * @return Null
 * @note   MCU needs to implement this function itself.
 */
void mcu_write_rtctime(u8 time[]);
#endif

#ifdef WIFI_TEST_ENABLE
/**
 * @brief  WiFi function test feedback
 * @param[in] {result} WiFi function test result
 * @ref       0: Failure
 * @ref       1: Success
 * @param[in] {rssi} If test success, indicates WiFi signal strength / If test failure, indicates error type
 * @return Null
 * @note   MCU needs to implement this function itself.
 */
void wifi_test_result(u8 result,u8 rssi);
#endif

#ifdef WEATHER_ENABLE
/**
* @brief  MCU opens weather service
 * @param  Null
 * @return Null
 */
void mcu_open_weather(void);

/**
 * @brief  Open weather function return user self-handling function
 * @param[in] {res} Result of opening weather function
 * @ref       0: Failure
 * @ref       1: Success
 * @param[in] {err} Error code
 * @return Null
 * @note   MCU needs to implement this function itself.
 */
void weather_open_return_handle(u8 res, u8 err);

/**
 * @brief  Weather data user self-handling function
 * @param[in] {name} Parameter name
 * @param[in] {type} Parameter type
 * @ref       0: int type
 * @ref       1: string type
 * @param[in] {data} Address of parameter value
 * @param[in] {day} Which day's weather 0: indicates today Value range: 0~6
 * @ref       0: Today
 * @ref       1: Tomorrow
 * @return Null
 * @note   MCU needs to implement this function itself.
 */
void weather_data_user_handle(i8* name, u8 type, const u8* weather_data, i8 day);
#endif

#ifdef MCU_DP_UPLOAD_SYN
/**
 * @brief  Status synchronous reporting result
 * @param[in] {result} Result
 * @ref       0: Failure
 * @ref       1: Success
 * @return Null
 * @note   MCU needs to implement this function itself.
 */
void get_upload_syn_result(u8 result);
#endif

#ifdef GET_WIFI_STATUS_ENABLE
/**
 * @brief  Get WiFi status result
 * @param[in] {result} Indicates WiFi working status
 * @ref       0x00: wifi status 1 smartconfig configuration status
 * @ref       0x01: wifi status 2 AP configuration status
 * @ref       0x02: wifi status 3 WIFI configured but not connected to router
 * @ref       0x03: wifi status 4 WIFI configured and connected to router
 * @ref       0x04: wifi status 5 Connected to router and connected to cloud
 * @ref       0x05: wifi status 6 WIFI device in low power mode
 * @ref       0x06: wifi status 7 WIFI device in smartconfig&AP configuration status
 * @return Null
 * @note   MCU needs to implement this function itself.
 */
void get_wifi_status(u8 result);
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
 * @note   MCU needs to implement this function itself.
 */
void stream_trans_send_result(u8 result);

/**
 * @brief  Multi-map stream service send result
 * @param[in] {result} Result
 * @ref       0x00: Success
 * @ref       0x01: Failure
 * @return Null
 * @note   MCU needs to implement this function itself.
 */
void maps_stream_trans_send_result(u8 result);
#endif

#ifdef WIFI_CONNECT_TEST_ENABLE
/**
 * @brief  Router information receive result notification
 * @param[in] {result} Whether the module successfully received correct router information
 * @ref       0x00: Failure
 * @ref       0x01: Success
 * @return Null
 * @note   MCU needs to implement this function itself.
 */
void wifi_connect_test_result(u8 result);
#endif

#ifdef GET_MODULE_MAC_ENABLE
/**
 * @brief  Get module MAC result
 * @param[in] {mac} Module MAC data
 * @ref       mac[0]: Flag for whether obtaining MAC was successful, 0x00 indicates success, 0x01 indicates failure.
 * @ref       mac[1]~mac[6]: If the MAC address flag bit mac[0] is success, then represents the module's valid MAC address.
 * @return Null
 * @note   MCU needs to implement this function itself.
 */
void mcu_get_mac(u8 mac[]);
#endif

#ifdef GET_IR_STATUS_ENABLE
/**
 * @brief  Get infrared status result
 * @param[in] {result} Indicates infrared status
 * @ref       0x00: Infrared status 1 Infrared code transmitting
 * @ref       0x01: Infrared status 2 Infrared code transmission ended
 * @ref       0x02: Infrared status 3 Infrared learning started
 * @ref       0x03: Infrared status 4 Infrared learning ended
 * @return Null
 * @note   MCU needs to implement this function itself.
 */
void get_ir_status(u8 result);
#endif

#ifdef IR_TX_RX_TEST_ENABLE
/**
 * @brief  Infrared enter transceiver production test result notification
 * @param[in] {result} Whether the module successfully received correct information
 * @ref       0x00: Failure
 * @ref       0x01: Success
 * @return Null
 * @note   MCU needs to implement this function itself.
 */
void ir_tx_rx_test_result(u8 result);
#endif

#ifdef FILE_DOWNLOAD_ENABLE
/**
 * @brief  File download package size selection
 * @param[in] {package_sz} File download package size
 * @ref       0x00: 256 byte (default)
 * @ref       0x01: 512 byte
 * @ref       0x02: 1024 byte
 * @return Null
 * @note   MCU needs to implement this function itself.
 */
void file_download_package_choose(u8 package_sz);

/**
 * @brief  File package download mode
 * @param[in] {value} Data buffer
 * @param[in] {position} Current data packet's position in the file
 * @param[in] {length} Current file packet length (when length is 0, it indicates file packet sending is complete)
 * @return Data processing result
 * -           0(ERROR): Failure
 * -           1(SUCCESS): Success
 * @note   MCU needs to implement this function itself.
 */
u8 file_download_handle(const u8 value[],u32 position,u16 length);
#endif

#ifdef MODULE_EXPANDING_SERVICE_ENABLE
/**
 * @brief  Open module time service notification result
 * @param[in] {value} Data buffer
 * @param[in] {length} Data length
 * @return Null
 * @note   MCU needs to implement this function itself.
 */
void open_module_time_serve_result(const unsigned char value[], unsigned short length);
void module_time_sync_handler(const unsigned char cmd_bytes[], unsigned short cmd_length);
void request_weather_handler(const unsigned char cmd_bytes[], unsigned short cmd_length);
void module_start_reset_notify_handler(const unsigned char cmd_bytes[], unsigned short cmd_length);
void module_reset_notify_handler(const unsigned char cmd_bytes[], unsigned short cmd_length);
void module_wifi_remote_handler(const unsigned char cmd_bytes[], unsigned short cmd_length);
void get_module_info_handler(const unsigned char cmd_bytes[], unsigned short cmd_length);
void set_module_log_level_handler(const unsigned char cmd_bytes[], unsigned short cmd_length);
void module_record_dp_report_handler(const unsigned char cmd_bytes[], unsigned short cmd_length);
void sbus_service_handler(const unsigned char cmd_bytes[], unsigned short cmd_length);

#endif

#ifdef BLE_RELATED_FUNCTION_ENABLE

void BLE_relative_fun(const u8 cmd_datas[], u16 cmd_data_length);
#endif

#ifdef VOICE_MODULE_PROTOCOL_ENABLE





/**
 * @brief  Get voice status code result
 * @param[in] {result} Voice status code
 * @ref       0x00: Idle
 * @ref       0x01: MIC mute status
 * @ref       0x02: Awakened
 * @ref       0x03: Recording
 * @ref       0x04: Recognizing
 * @ref       0x05: Recognition successful
 * @ref       0x06: Recognition failed
 * @return Null
 * @note   MCU needs to implement this function itself.
 */
void get_voice_state_result(u8 result);

/**
 * @brief  MIC mute setting
 * @param[in] {result} Voice status code
 * @ref       0x00: MIC enabled
 * @ref       0x01: MIC muted
 * @return Null
 * @note   MCU needs to call this function itself.
 */
void set_voice_MIC_silence_result(u8 result);

/**
 * @brief  Speaker volume setting result
 * @param[in] {result} Volume value
 * @ref       0~10: Volume range
 * @return Null
 * @note   MCU needs to call this function itself.
 */
void set_speaker_voice_result(u8 result);

/**
 * @brief  Audio production test result
 * @param[in] {result} Audio production test status
 * @ref       0x00: Close audio production test
 * @ref       0x01: MIC1 audio loop test
 * @ref       0x02: MIC2 audio loop test
 * @return Null
 * @note   MCU needs to call this function itself.
 */
void voice_test_result(u8 result);

/**
 * @brief  Awaken production test result
 * @param[in] {result} Awaken return value
 * @ref       0x00: Awaken successful
 * @ref       0x01: Awaken failed (10s timeout failure)
 * @return Null
 * @note   MCU needs to call this function itself.
 */
void voice_awaken_test_result(u8 result);

/**
 * @brief  Voice module extended function
 * @param[in] {value} Data buffer
 * @param[in] {length} Data length
 * @return Null
 * @note   MCU needs to implement this function itself.
 */
void voice_module_extend_fun(const u8 value[], u16 length);
#endif

#if defined(DP_WITH_TYPE_ENABLE)
void dp_type_extended_result(const u8 value[], u16 length);
#endif

#if defined(PRODUCT_TEST_ENABLE)
void product_test_fun(const u8 value[], u16 length);
#endif

#if defined(CN_IOT_EXTENDED_ENABLE)
void cn_iot_extended_fun(const u8 value[], u16 length);
#endif

#if defined(FAN_PRODUCT_SERVICE_ENABLE)
void fan_product_service_fun(const u8 value[], u16 length);
#endif


#if defined(USER_DEFINED_COMMAND_ENABLE)
void user_defined_fun(const u8 value[], u16 length);
#endif

#if defined(MATTER_COMMON_FUNCTION_ENABLE)
void matter_common_fun(const u8 value[], u16 length);
#endif

#if defined(CLOUD_STORAGE_FUNCTION_ENABLE)
void cloud_stonage_fun(const u8 value[], u16 length);
#endif

#if defined(AP_TRANSPARENT_TRANS_TEST_ENABLE)
void ap_transparent_trans_test_fun(const u8 value[], u16 length);
#endif

#if defined(FACTORY_RECOVERY_FUNCTION_ENABLE)
void factory_recovery_result();
#endif


#if defined(DP_CACHE_ENABLE)
void dp_cache_get_result(const u8 value[], u16 length);
#endif

#if defined(HIBERNATE_ENABLE)
void hibernate_fun(const u8 value[], u16 length);
#endif

// void offline_voice_fun(const u8 value[], u16 length) ;

#if defined(OFFLINE_VOICE_CTRL_ENABLE)
void offline_voice_ctrl(const u8 cmd_datas[], u16 cmd_data_length);
#endif


#endif
