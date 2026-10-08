/**********************************Copyright (c)**********************************
**                       Copyright (C), 2015-2026, Tuya Technology
**
**                             http://www.tuya.com
**
*********************************************************************************/
/**
 * @file    mcu_api.h
 * @author  Tuya Integrated Protocol Development Group
 * @version v2.6.2
 * @date    2026.02.04
 * @brief   Functions that users need to actively call are included in this file.
 */

/****************************** Disclaimer !!! *******************************
Due to the variety of MCU types and compilation environments, this code is for reference only.
Users are responsible for the final code quality, and Tuya is not responsible for MCU functional results.
******************************************************************************/

#ifndef __MCU_API_H_
#define __MCU_API_H_

#include "tuya_type.h"

#ifdef MCU_API_GLOBAL
  #define MCU_API_EXTERN
#else
  #define MCU_API_EXTERN   extern
#endif

/**
 * @brief  Hex to BCD
 * @param[in] {Value_H} High Byte
 * @param[in] {Value_L} Low Byte
 * @return Converted Data
 */
u8 hex_to_bcd(u8 Value_H,u8 Value_L);

/**
 * @brief  Get String Length
 * @param[in] {str} String Address
 * @return Data Length
 */
u32 tuya_strlen(const char *str);

/**
 * @brief  Set the first count bytes of memory area pointed by src to character c
 * @param[out] {src} Starting address of memory to set
 * @param[in] {ch} Character to set
 * @param[in] {count} Length of memory to set
 * @return Starting address of memory to set
 */
void *tuya_memset(void *src,u8 ch,u16 count);

/**
 * @brief  Memory Copy
 * @param[out] {dest} Destination Address
 * @param[in] {src} Source Address
 * @param[in] {count} Number of data to copy
 * @return Source address after processing
 */
void *tuya_memcpy(void *dest, const void *src, u16 count);

/**
 * @brief  String Copy
 * @param[in] {dest} Destination Address
 * @param[in] {src} Source Address
 * @return Source address after processing
 */
char *tuya_strcpy(char *dest, const char *src);

char* tuya_strncpy(char* dest, const char* src, u16 count);

    /**
     * @brief  String Compare
     * @param[in] {s1} String 1
     * @param[in] {s2} String 2
     * @return Comparison result
     * -         0:s1=s2
     * -         <0:s1<s2
     * -         >0:s1>s2
     */
i32 tuya_strcmp(char* s1, char* s2);

/**
 * @brief  Split int type into four bytes
 * @param[in] {number} Original 4-byte data
 * @param[out] {value} Processed 4-byte data
 * @return Null
 */
void int_to_byte(u32 number,u8 value[4]);

/**
 * @brief  Merge 4 bytes into one 32-bit variable
 * @param[in] {value} 4-byte array
 * @return Merged 32-bit variable
 */
u32 byte_to_int(const u8 value[4]);

/**
 * @brief  Raw type DP data upload
 * @param[in] {dpid} DP ID number
 * @param[in] {value} Pointer to current DP value
 * @param[in] {len} Data length
 * @return Null
 * @note   Null
 */
u8 mcu_dp_raw_update(u8 dpid,const u8 value[],u16 len);

/**
 * @brief  Bool type DP data upload
 * @param[in] {dpid} DP ID number
 * @param[in] {value} Current DP value
 * @return Null
 * @note   Null
 */
u8 mcu_dp_bool_update(u8 dpid,u8 value);

/**
 * @brief  Value type DP data upload
 * @param[in] {dpid} DP ID number
 * @param[in] {value} Current DP value
 * @return Null
 * @note   Null
 */
u8 mcu_dp_value_update(u8 dpid,u32 value);

/**
 * @brief  String type DP data upload
 * @param[in] {dpid} DP ID number
 * @param[in] {value} Pointer to current DP value
 * @param[in] {len} Data length
 * @return Null
 * @note   Null
 */
u8 mcu_dp_string_update(u8 dpid,const u8 value[],u16 len);

/**
 * @brief  Enum type DP data upload
 * @param[in] {dpid} DP ID number
 * @param[in] {value} Current DP value
 * @return Null
 * @note   Null
 */
u8 mcu_dp_enum_update(u8 dpid,u8 value);

/**
 * @brief  Fault type DP data upload
 * @param[in] {dpid} DP ID number
 * @param[in] {value} Current DP value
 * @return Null
 * @note   Null
 */
u8 mcu_dp_fault_update(u8 dpid,u32 value);


/**
 * @brief  Struct type DP data upload
 * @param[in] {dpid} DP ID number
 * @param[in] {ptr_to_struct}  Pointer to the struct
 * @param[in] {struct_size}  Struct size
 * @return Null
 * @note   u8 0 indicates success, others indicate failure
 */
u8 mcu_dp_struct_update(u8 dpid, const void *ptr_to_struct, u16 struct_size);

/**
 * @brief  Array type DP data upload
 * @param[in] {dpid} DP ID number
 * @param[in] {ptr_to_array}  Pointer to the array
 * @param[in] {array_total_size}  Total array size
 * @return Null
 * @note   u8 0 indicates success, others indicate failure
 */
u8 mcu_dp_array_update(u8 dpid, const void *ptr_to_array, u16 array_total_size);


#ifdef MCU_DP_UPLOAD_SYN
/**
 * @brief  Raw type DP data synchronous upload
 * @param[in] {dpid} DP ID number
 * @param[in] {value} Pointer to current DP value
 * @param[in] {len} Data length
 * @return Null
 * @note   Null
 */
u8 mcu_dp_raw_update_syn(u8 dpid,const u8 value[],u16 len);

/**
 * @brief  Bool type DP data synchronous upload
 * @param[in] {dpid} DP ID number
 * @param[in] {value} Pointer to current DP value
 * @return Null
 * @note   Null
 */
u8 mcu_dp_bool_update_syn(u8 dpid,u8 value);

/**
 * @brief  Value type DP data synchronous upload
 * @param[in] {dpid} DP ID number
 * @param[in] {value} Pointer to current DP value
 * @return Null
 * @note   Null
 */
u8 mcu_dp_value_update_syn(u8 dpid,u32 value);

/**
 * @brief  String type DP data synchronous upload
 * @param[in] {dpid} DP ID number
 * @param[in] {value} Pointer to current DP value
 * @param[in] {len} Data length
 * @return Null
 * @note   Null
 */
u8 mcu_dp_string_update_syn(u8 dpid,const u8 value[],u16 len);

/**
 * @brief  Enum type DP data synchronous upload
 * @param[in] {dpid} DP ID number
 * @param[in] {value} Pointer to current DP value
 * @return Null
 * @note   Null
 */
u8 mcu_dp_enum_update_syn(u8 dpid,u8 value);

/**
 * @brief  Fault type DP data synchronous upload
 * @param[in] {dpid} DP ID number
 * @param[in] {value} Pointer to current DP value
 * @return Null
 * @note   Null
 */
u8 mcu_dp_fault_update_syn(u8 dpid,u32 value);


/**
 * @brief  Struct type DP data synchronous upload
 * @param[in] {dpid} DP ID number
 * @param[in] {ptr_to_struct}  Pointer to the struct
 * @param[in] {struct_size}  Struct size
 * @return Null
 * @note   u8 0 indicates success, others indicate failure
 */
u8 mcu_dp_struct_update_syn(u8 dpid, const void *ptr_to_struct, u16 struct_size);

/**
 * @brief  Array type DP data synchronous upload
 * @param[in] {dpid} DP ID number
 * @param[in] {ptr_to_array}  Pointer to the array
 * @param[in] {array_total_size}  Total array size
 * @return Null
 * @note   u8 0 indicates success, others indicate failure
 */
u8 mcu_dp_array_update_syn(u8 dpid, const void *ptr_to_array, u16 array_total_size);

#endif

/**
 * @brief  MCU get bool type DP download value
 * @param[in] {value} DP data buffer address
 * @param[in] {len} DP data length
 * @return Current DP value
 * @note   Null
 */
u8 mcu_get_dp_download_bool(const u8 value[],u16 len);

/**
 * @brief  MCU get enum type DP download value
 * @param[in] {value} DP data buffer address
 * @param[in] {len} DP data length
 * @return Current DP value
 * @note   Null
 */
u8 mcu_get_dp_download_enum(const u8 value[],u16 len);

/**
 * @brief  MCU get value type DP download value
 * @param[in] {value} DP data buffer address
 * @param[in] {len} DP data length
 * @return Current DP value
 * @note   Null
 */
u32 mcu_get_dp_download_value(const u8 value[],u16 len);

/**
 * @brief  UART receive data temporary storage processing
 * @param[in] {value} 1-byte data received by UART
 * @return Null
 * @note   Call this function in the MCU UART processing function and pass the received data as a parameter.
 */
void uart_receive_input(u8 value);

/**
 * @brief  UART receive multiple bytes data temporary storage processing
 * @param[in] {value} Source address of data to be received by UART
 * @param[in] {data_len} Data length of data to be received by UART
 * @return Null
 * @note   Call this function if support for one-time multi-byte caching is needed.
 */
void uart_receive_buff_input(u8 value[], u16 data_len);

/**
 * @brief  WiFi UART data processing service
 * @param  Null
 * @return Null
 * @note   Call this function in the MCU main function while loop.
 */
void wifi_uart_service(void);

/**
 * @brief  Protocol UART initialization function
 * @param  Null
 * @return Null
 * @note   Call this function in MCU initialization code.
 */
void wifi_protocol_init(void);

#ifndef WIFI_CONTROL_SELF_MODE
/**
 * @brief  MCU get reset WiFi success flag
 * @param  Null
 * @return Reset flag
 * -           0(RESET_WIFI_ERROR): Failure
 * -           1(RESET_WIFI_SUCCESS): Success
 * @note   1: Call this function after MCU actively calls mcu_reset_wifi() to get reset status
 *         2: If in module self-processing mode, MCU does not need to call this function
 */
u8 mcu_get_reset_wifi_flag(void);

/**
 * @brief  MCU actively reset WiFi working mode
 * @param  Null
 * @return Null
 * @note   1: MCU actively calls, get whether WiFi reset is successful via mcu_get_reset_wifi_flag() function
 *         2: If in module self-processing mode, MCU does not need to call this function
 */
void mcu_reset_wifi(void);

/**
 * @brief  Get set WiFi status success flag
 * @param  Null
 * @return wifimode flag
 * -           0(SET_WIFICONFIG_ERROR): Failure
 * -           1(SET_WIFICONFIG_SUCCESS): Success
 * @note   1: Call this function after MCU actively calls mcu_set_wifi_mode() to get reset status
 *         2: If in module self-processing mode, MCU does not need to call this function
 */
u8 mcu_get_wifimode_flag(void);

/**
 * @brief  MCU set WiFi working mode
 * @param[in] {mode} Mode to enter
 * @ref        0(SMART_CONFIG): Enter smartconfig mode
 * @ref        1(AP_CONFIG): Enter AP mode
 * @return Null
 * @note   1: MCU actively calls
 *         2: After success, you can check if set_wifi_config_state is TRUE; TRUE means setting WiFi working mode successful.
 *         3: If in module self-processing mode, MCU does not need to call this function
 */
void mcu_set_wifi_mode(u8 mode);

/**
 * @brief  MCU actively get current WiFi working state
 * @param  Null
 * @return wifi work state
 * -          SMART_CONFIG_STATE: smartconfig configuration state
 * -          AP_STATE: AP configuration state
 * -          WIFI_NOT_CONNECTED: WiFi configured successfully but not connected to router
 * -          WIFI_CONNECTED: WiFi configured successfully and connected to router
 * -          WIFI_CONN_CLOUD: WiFi connected to cloud server
 * -          WIFI_LOW_POWER: WiFi in low power mode
 * -          SMART_AND_AP_STATE: WiFi smartconfig&AP mode
 * @note   If in module self-processing mode, MCU does not need to call this function
 */
u8 mcu_get_wifi_work_state(void);
#endif

#ifdef SUPPORT_GREEN_TIME
/**
 * @brief  MCU get Greenwich time
 * @param  Null
 * @return Null
 * @note   MCU needs to call this function itself.
 */
void mcu_get_green_time(void);
#endif

#ifdef SUPPORT_MCU_RTC_CHECK
/**
 * @brief  MCU get system time for calibrating local clock
 * @param  Null
 * @return Null
 * @note   MCU actively calls, then calibrates RTC clock within the mcu_write_rtctime function.
 */
void mcu_get_system_time(void);
#endif

#ifdef WIFI_TEST_ENABLE
/**
 * @brief  MCU initiate WiFi function test
 * @param  Null
 * @return Null
 * @note   MCU needs to call this function itself.
 */
void mcu_start_wifitest(void);
#endif

#ifdef WIFI_HEARTSTOP_ENABLE
/**
 * @brief  Notify WiFi module to stop heartbeat
 * @param  Null
 * @return Null
 * @note   MCU needs to call this function itself.
 */
void wifi_heart_stop(void);
#endif

#ifdef GET_WIFI_STATUS_ENABLE
/**
 * @brief  Get current WiFi connection status
 * @param  Null
 * @return Null
 * @note   MCU needs to call this function itself.
 */
void mcu_get_wifi_connect_status(void);
#endif

#ifdef WIFI_STREAM_ENABLE
/**
 * @brief  Stream service send
 * @param[in] {id} ID number
 * @param[in] {buffer} Address of packet to send
 * @param[in] {buf_len} Length of packet to send
 * @return Stream service transmission result
 * -           0(ERROR): Failure
 * -           1(SUCCESS): Success
 * @note   MCU needs to implement this function itself.
 */
u8 stream_trans_send(u32 id, u8 *buffer, u32 buf_len);

/**
 * @brief  Multi-map stream service send
 * @param[in] {id} Map stream service session ID
 * @param[in] {sub_id} Sub-map ID
 * @param[in] {sub_id_pro_mode} Sub-map ID data processing mode
 * @ref       0x00: Continue accumulation
 * @ref       0x01: Clear uploaded data
 * @param[in] {buffer} Address of data packet to send
 * @param[in] {buf_len} Length of data packet to send
 * @return Stream service transmission result
 * -           0(ERROR): Failure
 * -           1(SUCCESS): Success
 * @note   MCU needs to implement this function itself.
 */
u8 maps_stream_trans_send(u32 id, u8 sub_id, u8 sub_id_pro_mode, u8 *buffer, u32 buf_len);
#endif

#ifdef WIFI_CONNECT_TEST_ENABLE
/**
 * @brief  MCU initiate WiFi function test (connect to specified router)
 * @param[in] {ssid_buf} Address storing router name string data (ssid max length 32 bytes)
 * @param[in] {passwd_buffer} Address storing router password string data (passwd max length 64 bytes)
 * @return Null
 * @note   MCU needs to call this function itself.
 */
void mcu_start_connect_wifitest(u8 *ssid_buf,u8 *passwd_buffer);
#endif

#ifdef GET_MODULE_MAC_ENABLE
/**
 * @brief  Get module MAC
 * @param  Null
 * @return Null
 * @note   MCU needs to call this function itself.
 */
void mcu_get_module_mac(void);
#endif

#ifdef IR_TX_RX_TEST_ENABLE
/**
 * @brief  MCU initiate infrared entry into transceiver production test
 * @param  Null
 * @return Null
 * @note   MCU needs to call this function itself.
 */
void mcu_start_ir_test(void);
#endif

#ifdef MODULE_EXPANDING_SERVICE_ENABLE
/**
 * @brief  Open module time service notification
 * @param[in] {time_type} Time type
 * @ref       0x00: Greenwich time
 * @ref       0x01: Local time
 * @return Null
 * @note   MCU needs to call this function itself.
 */
void open_module_time_serve(u8 time_type);

/**
 * @brief  Actively request weather service data
 * @param  Null
 * @return Null
 * @note   MCU needs to call this function itself.
 */
void request_weather_serve(void);

/**
 * @brief  Open module reset state notification
 * @param  Null
 * @return Null
 * @note   MCU needs to call this function itself.
 */
void open_module_reset_state_serve(void);
#endif

#ifdef BLE_RELATED_FUNCTION_ENABLE
/**
 * @brief  MCU initiate Bluetooth functional test (scan specified Bluetooth beacon)
 * @param  Null
 * @return Null
 * @note   MCU needs to call this function itself.
 */
void mcu_start_BLE_test(void);
#endif

#ifdef VOICE_MODULE_PROTOCOL_ENABLE
/**
 * @brief  Get voice status code
 * @param  Null
 * @return Null
 * @note   MCU needs to call this function itself.
 */
void get_voice_state(void);

/**
 * @brief  MIC mute setting
 * @param[in] {set_val} Mute setting value
 * @ref       0x00: mic on
 * @ref       0x01: mic mute
 * @ref       0xA0: query mute status
 * @return Null
 * @note   MCU needs to call this function itself.
 */
void set_voice_MIC_silence(u8 set_val);

/**
 * @brief  Speaker volume setting
 * @param[in] {set_val} Volume setting value
 * @ref       0~10: Volume range
 * @ref       0xA0: Query volume value
 * @return Null
 * @note   MCU needs to call this function itself.
 */
void set_speaker_voice(u8 set_val);

/**
 * @brief  Audio production test
 * @param[in] {set_val} Audio production test value
 * @ref       0x00: Turn off audio production test
 * @ref       0x01: mic1 audio loop test
 * @ref       0x02: mic2 audio loop test
 * @ref       0xA0: Query current production test status
 * @return Null
 * @note   MCU needs to call this function itself.
 */
void voice_test(u8 test_val);

/**
 * @brief  Wake-up production test
 * @param  Null
 * @return Null
 * @note   MCU needs to call this function itself.
 */
void voice_awaken_test(void);

/**
 * @brief  Voice module MCU function setting
 * @param[in] {play} Play/Pause function 1(play) / 0(pause)
 * @param[in] {bt_play} Bluetooth switch function 1(on) / 0(off)
 * @return Null
 * @note   MCU needs to call this function itself.
 */
void voice_mcu_fun_set(u8 play, u8 bt_play);
#endif

#if defined(DP_WITH_TYPE_ENABLE)
void dp_with_type_enable(u8 status);
#endif


#if defined(CLOUD_STORAGE_FUNCTION_ENABLE)
void cloud_storage_start_upload(u8 type, const char* name, u32 len, u16 pkg_size);
void cloud_storage_data_upload(u32 offset, const u8* buf_ptr, u16 length);
#endif


#if defined(AP_TRANSPARENT_TRANS_TEST_ENABLE)
void ap_transparent_trans_upstream(const u8* buf_ptr, u16 length);
#endif

#if defined(FACTORY_RECOVERY_FUNCTION_ENABLE)
void request_factory_recovery();
#endif

#if defined(HIBERNATE_ENABLE)
void hibernate_enter_request(void);
#endif

#if defined(SBUS_SERVICE_ENABLE)
void sbus_status_query(void);
void sbus_send_data(const u8* value, u8 value_length);
#endif



#endif