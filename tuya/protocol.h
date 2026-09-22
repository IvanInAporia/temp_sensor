/**********************************Copyright (c)**********************************
**                     All rights reserved (C), 2015-2020, Tuya
**
**                             http://www.tuya.com
**
*********************************************************************************/
/**
 * @file    protocol.h
 * @author  Tuya Team
 * @version v1.0.4
 * @date    2020.5.26
 * @brief                *******Very important, be sure to watch!!!********
 *          1. The user implements the data delivery/reporting function in this file.
 *          2. DP ID / TYPE and data processing functions require the user to implement according to the actual definition
 *          3. There are #err hints inside the function that needs the user to implement the code after starting some macro definitions. 
 *             Please delete the #err after completing the function.
 */


#ifndef __PROTOCOL_H_
#define __PROTOCOL_H_



/******************************************************************************
                 User related information configuration
******************************************************************************/
/******************************************************************************
                       1:Modify product information
******************************************************************************/
#define PRODUCT_KEY "smeeba2wkiop1su7"    //The unique product identification generated after product is created on development platform

#define MCU_VER "1.0.0"                                 //User's software version for MCU firmware upgrade, MCU upgrade version needs to be modified


/******************************************************************************
                          2:Does the MCU require a firmware upgrade?                  
If you need to support MCU firmware upgrade, please open this macro
The MCU can call the mcu_firm_update_query() function in the mcu_api.c file to get the current MCU firmware update.

                        ********WARNING!!!**********
The current receive buffer is the size to turn off the firmware update function. 
The firmware upgrade package is 256 bytes.
If you need to enable this function, the serial receive buffer will become larger.
******************************************************************************/
//#define         SUPPORT_MCU_FIRM_UPDATE                 //Enable MCU firmware upgrade function (off by default)
/*  Firmware package size selection  */
#ifdef SUPPORT_MCU_FIRM_UPDATE
#define PACKAGE_SIZE                   0        //The package size is 256 bytes
//#define PACKAGE_SIZE                   1        //The package size is 512 bytes
//#define PACKAGE_SIZE                   2        //The package size is 1024 bytes
#endif

/******************************************************************************
                         3:Define the send and receive buffer:
          If the current RAM of the MCU is not enough, it can be modified to 24
******************************************************************************/
#ifndef SUPPORT_MCU_FIRM_UPDATE
/* PORTED: raised from Tuya's 16/24 defaults.  Both fail silently when they are
   too small, so neither is a knob to shave RAM with.

   WIFI_UART_RECV_BUF_LMT sizes the ISR ring (+PROTOCOL_HEAD, so 39 bytes here).
   uart_receive_input() drops a byte without a word when the ring is full -- no
   UART error, nothing any counter can see -- so the ring has to cover the
   longest gap between wifi_uart_service() calls.  The longest gap in this
   firmware is one outgoing DP report: uart_transmit_output blocks a byte at a
   time, so a 13-byte frame holds the loop for ~13 ms while the module is free
   to talk back.  39 bytes is ~40 ms of wire time at 9600 8N1; Tuya's 16 gives
   23 bytes, ~24 ms, which leaves under half a frame of margin.

   WIFI_DATA_PROCESS_LMT sizes frame assembly.  wifi_uart_service() accepts any
   declared length up to sizeof(buffer) + PROTOCOL_HEAD but can only ever buffer
   sizeof(buffer), so a frame declaring a length in that gap waits forever for
   bytes that cannot fit and wedges the parser for the rest of the window.
   Every DP on this product is report-only, so the module never issues DP data
   and the largest inbound frame is small -- this is margin against that cliff,
   not against a known frame.

   Cost of 32/32 over Tuya's 16/24: 24 bytes of RAM. */
#define WIFI_UART_RECV_BUF_LMT          32              //UART data receiving buffer size, must cover the longest gap between wifi_uart_service() calls
#define WIFI_DATA_PROCESS_LMT           32              //UART data processing buffer size, according to the user DP data size, must be greater than 24
#else
#define WIFI_UART_RECV_BUF_LMT          128             //UART data receiving buffer size, can be reduced if the MCU has insufficient RAM

/*  Select the appropriate UART data processing buffer size here 
    (select the buffer size based on the size selected by the above MCU firmware upgrade package and whether to turn on the weather service)  */
#define WIFI_DATA_PROCESS_LMT           300             //UART data processing buffer size. If the MCU firmware upgrade is required, the single-packet size is 256, the buffer must be greater than 260, or larger if the weather service is enabled
//#define WIFI_DATA_PROCESS_LMT           600             //UART data processing buffer size. If the MCU firmware upgrade is required, the single-packet size is 512, the buffer must be greater than 520, or larger if the weather service is enabled
//#define WIFI_DATA_PROCESS_LMT           1200            //UART data processing buffer size. If the MCU firmware upgrade is required, the single-packet size is 1024, the buffer must be greater than 1030, or larger if the weather service is enabled
#endif
#define WIFIR_UART_SEND_BUF_LMT         48              //According to the user's DP data size, it must be greater than 48

/******************************************************************************
                        4:Define how the module works
Module self-processing:
          The wifi indicator and wifi reset button are connected to the wifi module (turn on the WIFI_CONTROL_SELF_MODE macro)
          And correctly define WF_STATE_KEY and WF_RESET_KEY
MCU self-processing:
          The wifi indicator and wifi reset button are connected to the MCU (turn off the WIFI_CONTROL_SELF_MODE macro)
          The MCU calls the mcu_reset_wifi() function in the mcu_api.c file where it needs to handle the reset wifi, and can call the mcu_get_reset_wifi_flag() function to return the reset wifi result
          or call the mcu_set_wifi_mode(WIFI_CONFIG_E mode) function in the mcu_api.c file in the wifi mode, and call mcu_get_wifi_work_state() to return the setting wifi result.
******************************************************************************/
//#define         WIFI_CONTROL_SELF_MODE                       //Wifi self-processing button and LED indicator; if the MCU external button / LED indicator please turn off the macro
#ifdef          WIFI_CONTROL_SELF_MODE                      //Module self-processing
  #define     WF_STATE_KEY            14                    //Wifi module status indication button, please set according to the actual GPIO pin
  #define     WF_RESERT_KEY           0                     //Wifi module reset button, please set according to the actual GPIO pin
#endif


/******************************************************************************
                      5: Does the MCU need to support the time function?
Open this macro if needed and implement the code in mcu_write_rtctime in the Protocol.c file.
Mcu_write_rtctime has #err hint inside, please delete the #err after completing the function
Mcu can call the mcu_get_system_time() function to initiate the calibration function after the wifi module is properly networked.
******************************************************************************/
//#define         SUPPORT_MCU_RTC_CHECK                //Turn on time calibration

/******************************************************************************
                      6:Does the MCU need to support the wifi function test?                    
Please enable this macro if necessary, and mcu calls mcu_start_wifitest in mcu_api.c file when wifi function test is required.
And view the test results in the protocol_c file wifi_test_result function.
There is a #err hint inside wifi_test_result. Please delete the #err after completing the function.
******************************************************************************/
#define         WIFI_TEST_ENABLE                //Open WIFI production test function (scan designated route)

/******************************************************************************
                      7:Does MCU support wifi module firmware upgrade
Here, the power of the WIFI module is controlled by the MCU. Here, when our MCU needs to upgrade the WIFI
The firmware of the module can pull the latest firmware through the following command. The MCU motherboard is determined according to the reply package of the WIFI module
Determine whether it is necessary to power off the WIFI module. Here the MCU sends 0a command and waits for 5S.
The WIFI module is powered off. When the module replies that the firmware is being updated, the MCU also needs to start a timer when the firmware is being updated.
If the firmware is over 60S and the firmware upgrade is not received successfully, it is also forced to believe that the firmware upgrade fails and power off the WIFI module

If you need to support wifi module firmware upgrade, please turn on this macro
******************************************************************************/
//#define         WIFI_MODULE_UPDATE                 //Turn on WIFI module firmware upgrade

/******************************************************************************
                      8:Does MCU support query the signal strength of the current connection route
If necessary, please enable this macro. When you need to query the signal strength of the current connection route, mcu will call qur_router_strenth in the mcu_api.c file.
And check the test results in the protocol_c file router_strenth_result function.
There is #err hint inside router_strenth_result. After completing this function, please delete #err.
******************************************************************************/
//#define         WIFI_QUERY_ROUTE_RSSI             //Turn on query the signal strength of the current connection route

/******************************************************************************
                      9:Does MCU support get dp cache instruction
For some sensors with configuration or control functions, the dp delivery function needs to be added. When the device is offline, the panel
An instruction is issued, and this instruction will be cached in the cloud, waiting for the device to obtain it actively. The cache command is incremental, for
Commands that have been acquired will not be issued when they are acquired again

If you need to support the dp cache instruction, please turn on this macro
******************************************************************************/
//#define         DO_CACHE_SUPPORT                   //Get dp cache instruction





/******************************************************************************
                        1:dp data point serial number redefinition
          **This is the automatic generation of code, 
            such as the relevant changes in the development platform, 
            please re-download MCU_SDK**         
******************************************************************************/
//Temperature(Only report)
#define DPID_TEMP_CURRENT 1
//Humidity(Only report)
#define DPID_HUMIDITY_VALUE 2
//Battery level state(Only report)
#define DPID_BATTERY_STATE 3
//Battery level(Only report)
#define DPID_BATTERY_PERCENTAGE 4



/**
 * @brief  Send data processing
 * @param[in] {value} Serial port receives byte data
 * @return Null
 */
void uart_transmit_output(unsigned char value);

/**
 * @brief  All dp point information of the system is uploaded to realize APP and muc data synchronization
 * @param  Null
 * @return Null
 * @note   This function SDK needs to be called internally;
 *         The MCU must implement the data upload function in the function;
 *         including only reporting and reportable hair style data.
 */
void all_data_update(void);

/**
 * @brief  Record type data combination report
 * @param[in] {time} The length of time data is 7. The first byte indicates whether the flag bit is transmitted or not. The rest are year, month, day, hour, minute, second.
 * @param[in] {dp_bool}   Bool type dpid number
 * @param[in] {v_bool}    Bool type dp corresponding value
 * @param[in] {dp_enum}   enum type dpid number
 * @param[in] {v_enum}    enum type dp corresponding value
 * @param[in] {dp_value}  value type dpid number
 * @param[in] {v_value}   value type dp corresponding value
 * @param[in] {dp_string} string type dpid number
 * @param[in] {v_string}  string type dp corresponding value
 * @param[in] {len}       string length
 * @return SUCCESS(1) or ERROR(0)
 * @note   Call this function when you need to report record data.
 */
unsigned char dp_record_combine_update(unsigned char time[],
                                       unsigned char dp_bool,unsigned char v_bool,
                                       unsigned char dp_enum,unsigned char v_enum,
                                       unsigned char dp_value,unsigned char v_value,
                                       unsigned char dp_string,unsigned char v_string[],unsigned char len);

/**
 * @brief  dp delivery processing function
 * @param[in] {dpid} DP number
 * @param[in] {value} dp data buffer address
 * @param[in] {length} dp data length
 * @return Dp processing results
 * -           0(ERROR): failure
 * -           1(SUCCESS): success
 * @note   The function user cannot modify
 */
unsigned char dp_download_handle(unsigned char dpid,const unsigned char value[], unsigned short length);

/**
 * @brief  Get the sum of all dp commands
 * @param[in] Null
 * @return Sent the sum of the commands
 * @note   The function user cannot modify
 */
unsigned char get_download_cmd_total(void);

#ifdef SUPPORT_MCU_RTC_CHECK
/**
 * @brief  MCU proofreads local RTC clock
 * @param[in] {time} Get the time data
 * @return Null
 * @note   MCU needs to implement this function by itself
 */
void mcu_write_rtctime(unsigned char time[]);
#endif

#ifdef WIFI_TEST_ENABLE
/**
 * @brief  Wifi function test feedback
 * @param[in] {result} Wifi function test
 * @ref       0: failure
 * @ref       1: success
 * @param[in] {rssi} Test success indicates wifi signal strength / test failure indicates error type
 * @return Null
 * @note   MCU needs to implement this function by itself
 */
void wifi_test_result(unsigned char result,unsigned char rssi);
#endif

#ifdef WIFI_MODULE_UPDATE
/**
 * @brief  Request WIFI module firmware upgrade result return
 * @param[in] {result} result return
 * @return Null
 * @note   MCU needs to implement this function by itself
 */
void qur_module_ug_result(unsigned char result);
#endif

#ifdef WIFI_QUERY_ROUTE_RSSI
/**
 * @brief  Query the signal strength of the currently connected router and return the result
 * @param[in] {result} wifi function test result; 0:failed / 1:successful
 * @param[in] {rssi} a successful test indicates wifi signal strength / a failed test indicates an error type
 * @return Null
 * @note   MCU needs to implement this function by itself
 */
void router_strenth_result(unsigned char result,unsigned char rssi);
#endif

#ifdef SUPPORT_MCU_FIRM_UPDATE
/**
 * @brief  Request MCU firmware upgrade result return
 * @param[in] {result} result return
 * @return Null
 * @note   MCU needs to implement this function by itself
 */
void qur_ug_result(unsigned char result);

/**
 * @brief  Upgrade package size selection
 * @param[in] {package_sz} Upgrade package size
 * @ref           0x00: 256byte (default)
 * @ref           0x01: 512byte
 * @ref           0x02: 1024byte
 * @return Null
 * @note   MCU needs to implement this function by itself
 */
void upgrade_package_choose(unsigned char package_sz);

/**
 * @brief  MCU enters firmware upgrade mode
 * @param[in] {value} Firmware buffer
 * @param[in] {position} The current data packet is in the firmware location
 * @param[in] {length} Current firmware package length (when the firmware package length is 0, it indicates that the firmware package is sent)
 * @return Null
 * @note   MCU needs to implement this function by itself
 */
unsigned char mcu_firm_update_handle(const unsigned char value[],unsigned long position,unsigned short length);
#endif



#endif

