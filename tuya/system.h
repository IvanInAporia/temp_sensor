/**********************************Copyright (c)**********************************
**                       Copyright (C), 2015-2026, Tuya Technology
**
**                             http://www.tuya.com
**
*********************************************************************************/
/**
 * @file    system.h
 * @author  Tuya Comprehensive Protocol Development Team
 * @version v2.6.2
 * @date    2026.02.04
 * @brief   Serial port data processing. Users do not need to care about the implementation of this file.
 */

/****************************** Disclaimer !!! *******************************
Due to the variety of MCU types and compilation environments, this code is for reference only.
Users are responsible for the final code quality. Tuya is not responsible for MCU function results.
******************************************************************************/

#ifndef __SYSTEM_H_
#define __SYSTEM_H_

#include "tuya_type.h"

#ifdef SYSTEM_GLOBAL
  #define SYSTEM_EXTERN
#else
  #define SYSTEM_EXTERN   extern
#endif

//=============================================================================
//Byte order of the frame
//=============================================================================
#define         HEAD_FIRST                      0
#define         HEAD_SECOND                     1        
#define         PROTOCOL_VERSION                2
#define         FRAME_TYPE                      3
#define         LENGTH_HIGH                     4
#define         LENGTH_LOW                      5
#define         DATA_START                      6

//=============================================================================
//Data frame type
//=============================================================================
#define         HEAT_BEAT_CMD                   0                               //Heartbeat packet
#define         PRODUCT_INFO_CMD                1                               //Product information
#define         WORK_MODE_CMD                   2                               //Query the module working mode set by the MCU
#define         WIFI_STATE_CMD                  3                               //Wi-Fi working state
#define         WIFI_RESET_CMD                  4                               //Reset Wi-Fi
#define         WIFI_MODE_CMD                   5                               //Select smartconfig/AP mode
#define         DATA_QUERT_CMD                  6                               //Command delivery
#define         STATE_UPLOAD_CMD                7                               //Status upload
#define         STATE_QUERY_CMD                 8                               //Status query
#define         UPDATE_START_CMD                0x0a                            //Update start
#define         UPDATE_TRANS_CMD                0x0b                            //Update transmission
#define         GET_ONLINE_TIME_CMD             0x0c                            //Get system time (GMT)
#define         FACTORY_MODE_CMD                0x0d                            //Enter production test mode
#define         WIFI_TEST_CMD                   0x0e                            //Wi-Fi function test
#define         GET_LOCAL_TIME_CMD              0x1c                            //Get local time
#define         WEATHER_OPEN_CMD                0x20                            //Open weather service
#define         WEATHER_DATA_CMD                0x21                            //Weather data
#define         STATE_UPLOAD_SYN_CMD            0x22                            //Status upload (synchronous)
#define         STATE_UPLOAD_SYN_RECV_CMD       0x23                            //Status upload result notification (synchronous)
#define         HEAT_BEAT_STOP                  0x25                            //Stop Wi-Fi module heartbeat
#define         STREAM_TRANS_CMD                0x28                            //Stream data transmission
#define         GET_WIFI_STATUS_CMD             0x2b                            //Get current Wi-Fi connection status
#define         WIFI_CONNECT_TEST_CMD           0x2c                            //Wi-Fi function test (connect to specified router)
#define         GET_MAC_CMD                     0x2d                            //Get module MAC
#define         GET_IR_STATUS_CMD               0x2e                            //Infrared status notification
#define         IR_TX_RX_TEST_CMD               0x2f                            //Infrared enter transceiver production test
#define         MAPS_STREAM_TRANS_CMD           0x30                            //Stream data transmission (supports multiple maps)
#define         FILE_DOWNLOAD_START_CMD         0x31                            //File download start
#define         FILE_DOWNLOAD_TRANS_CMD         0x32                            //File download data transmission
#define         BLE_RELATIVE_CMD                0x35                            //Bluetooth functional test (scan specified Bluetooth beacon)
#define           BLE_RELATIVE_SUB_CMD_SCAN_TEST                      0x01
#define           BLE_RELATIVE_SUB_CMD_DEV_STATE_REPORT               0x04
#define           BLE_RELATIVE_SUB_CMD_DEV_STATE_QUERY                0x05
#define           BLE_RELATIVE_SUB_CMD_BEACON_RMT_DATA_NOTIFY         0x06
#define           BLE_RELATIVE_SUB_CMD_BEACON_RMT_BIND_UNBIND_NOTIFY  0x07
#define           BLE_RELATIVE_SUB_CMD_SUB_DEV_DATA_NOTIFY            0x08
#define         GET_VOICE_STATE_CMD             0x60                            //Get voice status code
#define         MIC_SILENCE_CMD                 0x61                            //MIC mute setting
#define         SET_SPEAKER_VOLUME_CMD          0x62                            //Speaker volume setting
#define         VOICE_TEST_CMD                  0x63                            //Voice module audio production test
#define         VOICE_AWAKEN_TEST_CMD           0x64                            //Voice module wake-up production test
#define         VOICE_EXTEND_FUN_CMD            0x65                            //Voice module extended function


#define         MODULE_EXTEND_FUN_CMD           0x34                            //Module extended service
#define           MODULE_EXTEND_FUN_SUB_CMD_START_TIME_NOTIFY   0x01
#define           MODULE_EXTEND_FUN_SUB_CMD_SYNC_TIME           0x02
#define           MODULE_EXTEND_FUN_SUB_CMD_REQUIRE_WEATHER     0x03
#define           MODULE_EXTEND_FUN_SUB_CMD_START_RESET_NOTIFY  0x04
#define           MODULE_EXTEND_FUN_SUB_CMD_RESET_NOTIFY        0x05
#define           MODULE_EXTEND_FUN_SUB_CMD_WIFI_REMOTE         0x20
#define           MODULE_EXTEND_FUN_SUB_CMD_GET_MODULE_INFO     0x07
#define           MODULE_EXTEND_FUN_SUB_CMD_SET_LOG_LEVEL    0x08
#define           MODULE_EXTEND_FUN_SUB_CMD_RECORD_DP_REPORT 0x0B
#define           MODULE_EXTEND_FUN_SUB_CMD_SBUS                0x0C
#define             MODULE_EXTEND_FUN_SUB_CMD_SBUS_CMD_STATE_NOTIFY 0x01
#define             MODULE_EXTEND_FUN_SUB_CMD_SUBS_CMD_DATA_SEND    0x02


#define         DP_WITH_TYPE_EXTEND_CMD                       0x36                            // Command with DP
#define           DP_WITH_TYPE_EXTEND_SUB_CMD_START           0x01
#define           DP_WITH_TYPE_EXTEND_SUB_CMD_DOWNLOAD        0x02
#define           DP_WITH_TYPE_EXTEND_SUB_CMD_UPLOAD          0x03

#define         PRODUCT_TEST_CMD                                0x39                            // Finished product production test
#define           PRODUCT_TEST_SUB_CMD_STATE_NOTIFY             0x00
#define           PRODUCT_TEST_SUB_CMD_KEY_TEST                 0x03
#define           PRODUCT_TEST_SUB_CMD_LED_TEST                 0x04
#define           PRODUCT_TEST_SUB_CMD_TRANSPARENT_TRANS_TEST   0x05
#define           PRODUCT_TEST_SUB_CMD_MODULE_TEST_STATE_NOTIFY 0x06
#define           PRODUCT_TEST_SUB_CMD_TEST_RESULT_REPORT       0x07

#define         CN_IOT_EXTEND_CMD               0x70                            // CN-IOT extension command

#define         FAN_PRODUCT_SERVICE_CMD                 0x72                            // Fan product service
#define           FAN_PRODUCT_SERVICE_SUB_CMD_TEST      0x01
#define           FAN_PRODUCT_SERVICE_SUB_CMD_SET_DUTY  0x02

#define         USER_DEFINED_CMD                0xEF                            // User-defined command

#define         MATTER_COMMON_CMD               0x66                            // Matter common firmware
#define           MATTER_COMMON_CMD_IDENTIFY_STATE_CHANGE_NOTIFY  0x00
#define           MATTER_COMMON_CMD_EVENT_NOTIFY                  0x01

#define         CLOUD_STRONAGE_TRANS_CMD        0x69                            // Cloud storage data transmission
#define           CLOUD_STONAGE_TRANS_SUB_CMD_UPLOAD_START        0x01
#define           CLOUD_STONAGE_TRANS_SUB_CMD_UPLOAD_DATA         0x02

#define         AP_TRANSPARENT_TRANS_TEST_CMD   0x80                            // AP transparent transmission test command
#define           AP_TRANSPARENT_TRANS_TEST_SUB_CMD_UPSTREAM_DATA   0x01
#define           AP_TRANSPARENT_TRANS_TEST_SUB_CMD_DOWNSTREAM_DATA 0x02

#define         FACTORY_RECOVERY_CMD            0xF4                            // Factory reset

#define         DP_CACHE_GET_CMD                0x90                            // Get DP cache command

#define         HIBERNATE_CMD                   0x91                            // Hibernation function
#define           HIBERNATE_SUB_CMD_HIBERNATE_NOTIFY  0x00
#define           HIBERNATE_SUB_CMD_WAKEUP_NOTIFY     0x01
#define           HIBERNATE_SUB_CMD_HIBERNATE_REQUEST 0x02


#define         OFFLINE_VOICE_CTRL_CMD            0x95




//=============================================================================
#define MCU_RX_VER              0x00                                            //Protocol version number of frame sent by module
#define MCU_TX_VER              0x03                                            //Protocol version number of frame sent by MCU (default)
#define PROTOCOL_HEAD           0x07                                            //Fixed protocol header length
#define FRAME_FIRST             0x55                                            //Frame header first byte
#define FRAME_SECOND            0xaa                                            //Frame header second byte
//=============================================================================



SYSTEM_EXTERN volatile u8 wifi_data_process_buf[PROTOCOL_HEAD + WIFI_DATA_PROCESS_LMT];     //Serial port data processing buffer
SYSTEM_EXTERN volatile u8 wifi_uart_rx_buf[PROTOCOL_HEAD + WIFI_UART_RECV_BUF_LMT];         //Serial port receive buffer
SYSTEM_EXTERN volatile u8 wifi_uart_tx_buf[PROTOCOL_HEAD + WIFIR_UART_SEND_BUF_LMT];        //Serial port send buffer
//
/* PORTED: the pointers themselves made volatile, not just what they point at.
   rx_buf_in is advanced by the receive interrupt (uart_receive_input) and read
   by wifi_uart_service(); rx_buf_out the other way round.  As Tuya declared
   them, the compiler is free to keep either in a register across a polling
   loop.  Under -flto the SDK inlines into Tuya_WaitUntil's loop, and CMSIS
   __WFI has no memory clobber, so only whichever opaque calls happen to be
   left in that loop would force a reload. */
SYSTEM_EXTERN volatile u8 * volatile rx_buf_in;
SYSTEM_EXTERN volatile u8 * volatile rx_buf_out;

SYSTEM_EXTERN volatile u8 stop_update_flag;                                                 //ENABLE: Stop all data upload  TY_DISABLE: Resume all data upload

#ifndef WIFI_CONTROL_SELF_MODE
SYSTEM_EXTERN volatile u8 reset_wifi_flag;                                                  //Reset Wi-Fi flag (TRUE: success / FALSE: failure)
SYSTEM_EXTERN volatile u8 set_wifimode_flag;                                                //Set Wi-Fi working mode flag (TRUE: success / FALSE: failure)
SYSTEM_EXTERN volatile u8 wifi_work_state;                                                  //Wi-Fi module current working state
#endif

#ifdef WIFI_STREAM_ENABLE
SYSTEM_EXTERN volatile u8 stream_status;                                                             //Stream service packet sending return status
SYSTEM_EXTERN volatile u8 maps_stream_status;                                                        //Multi-map stream service packet sending return status
#endif

/**
 * @brief  Write wifi_uart byte
 * @param[in] {dest} Buffer start address
 * @param[in] {byte} Byte value to write
 * @return Total length after writing
 */
u16 set_wifi_uart_byte(u16 dest, u8 byte);

/**
 * @brief  Write wifi_uart_buffer
 * @param[in] {dest} Destination address
 * @param[in] {src} Source address
 * @param[in] {len} Data length
 * @return Buffer address after writing ends
 */
u16 set_wifi_uart_buffer(u16 dest, const u8 *src, u16 len);

/**
 * @brief  Calculate checksum
 * @param[in] {pack} Data source pointer
 * @param[in] {pack_len} Checksum calculation length
 * @return Checksum
 */
u8 get_check_sum(u8 *pack, u16 pack_len);

/**
 * @brief  Send one frame of data to the Wi-Fi serial port
 * @param[in] {fr_type} Frame type
 * @param[in] {fr_ver} Frame version
 * @param[in] {len} Data length
 * @return Null
 */
void wifi_uart_write_frame(u8 fr_type, u8 fr_ver, u16 len);

#ifdef WIFI_STREAM_ENABLE
/**
 * @brief  Stream data transmission
 * @param[in] {id} Stream service identifier
 * @param[in] {offset} Offset
 * @param[in] {buffer} Data address
 * @param[in] {buf_len} Data length
 * @return Null
 * @note   Null
 */
u8 stream_trans(u16 id, u32 offset, u8 *buffer, u16 buf_len);

/**
 * @brief  Multi-map stream data transmission
 * @param[in] {pro_ver} Map service protocol version
 * @param[in] {id} Map stream service session ID
 * @param[in] {sub_id} Sub-map ID
 * @param[in] {sub_id_pro_mode} Sub-map ID data processing mode
 * @ref           0x00: Continue accumulating
 * @ref           0x00: Clear uploaded data
 * @param[in] {offset} Offset
 * @param[in] {buffer} Data address
 * @param[in] {buf_len} Data length
 * @return Null
 * @note   Null
 */
u8 maps_stream_trans(u8 pro_ver, u16 id, u8 sub_id, u8 sub_id_pro_mode,
                                u32 offset, u8 *buffer, u16 buf_len);
#endif

/**
 * @brief  Data frame processing
 * @param[in] {offset} Data start position
 * @return Null
 */
void data_handle(u16 offset);

/**
 * @brief  Check if there is data in the serial port receive buffer
 * @param  Null
 * @return Whether there is data
 */
u8 with_data_rxbuff(void);

/**
 * @brief  Read 1 byte of data from the queue
 * @param  Null
 * @return Read the data
 */
u8 take_byte_rxbuff(void);
#endif