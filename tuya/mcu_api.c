/**********************************Copyright (c)**********************************
**                       Copyright (C), 2015-2026, Tuya Technology
**
**                             http://www.tuya.com
**
*********************************************************************************/
/**
 * @file    mcu_api.c
 * @author  Tuya Comprehensive Protocol Development Group
 * @version v2.6.2
 * @date    2026.02.04
 * @brief   Functions that users need to actively call are all in this file
 */

/****************************** Disclaimer !!! *******************************
Due to the variety of MCU types and compilation environments, this code is for reference only. Users should judge the final code quality by themselves.
Tuya is not responsible for the MCU functional results.
******************************************************************************/

#define MCU_API_GLOBAL

#include "mcu_api.h"

#include "protocol.h"
#include "wifi.h"

/**
 * @brief  hex to bcd
 * @param[in] {Value_H} High byte
 * @param[in] {Value_L} Low byte
 * @return Converted data
 */
u8 hex_to_bcd(u8 Value_H, u8 Value_L) {
    u8 bcd_value;

    if ((Value_H >= '0') && (Value_H <= '9'))
        Value_H -= '0';
    else if ((Value_H >= 'A') && (Value_H <= 'F'))
        Value_H = Value_H - 'A' + 10;
    else if ((Value_H >= 'a') && (Value_H <= 'f'))
        Value_H = Value_H - 'a' + 10;

    bcd_value = Value_H & 0x0f;

    bcd_value <<= 4;
    if ((Value_L >= '0') && (Value_L <= '9'))
        Value_L -= '0';
    else if ((Value_L >= 'A') && (Value_L <= 'F'))
        Value_L = Value_L - 'a' + 10;
    else if ((Value_L >= 'a') && (Value_L <= 'f'))
        Value_L = Value_L - 'a' + 10;

    bcd_value |= Value_L & 0x0f;

    return bcd_value;
}

/**
 * @brief  Get string length
 * @param[in] {str} String address
 * @return Data length
 */
u32 tuya_strlen(const char* str) {
    u32 len = 0;
    if (str == NULL) {
        return 0;
    }

    for (len = 0; *str++ != '\0';) {
        len++;
    }

    return len;
}

/**
 * @brief  Set the first count bytes of the memory area pointed to by src to character c
 * @param[out] {src} Destination memory address
 * @param[in] {ch} Character to set
 * @param[in] {count} Length of memory to set
 * @return Destination memory address
 */
void* tuya_memset(void* src, u8 ch, u16 count) {
    u8* tmp = (u8*)src;

    if (src == NULL) {
        return NULL;
    }

    while (count--) {
        *tmp++ = ch;
    }

    return src;
}

/**
 * @brief  Memory copy
 * @param[out] {dest} Destination address
 * @param[in] {src} Source address
 * @param[in] {count} Number of bytes to copy
 * @return Source address after processing
 */
void* tuya_memcpy(void* dest, const void* src, u16 count) {
    u8* pdest = (u8*)dest;
    const u8* psrc = (const u8*)src;
    u16 i;

    if (dest == NULL || src == NULL) {
        return NULL;
    }

    if ((pdest <= psrc) || (pdest > psrc + count)) {
        for (i = 0; i < count; i++) {
            pdest[i] = psrc[i];
        }
    } else {
        for (i = count; i > 0; i--) {
            pdest[i - 1] = psrc[i - 1];
        }
    }

    return dest;
}

/**
 * @brief  String copy
 * @param[in] {dest} Destination address
 * @param[in] {src} Source address
 * @return Source address after processing
 */
char* tuya_strcpy(char* dest, const char* src) {
    char* p = dest;
    if ((NULL == dest) || (NULL == src)) {
        return NULL;
    }

    while (*src != '\0') {
        *dest++ = *src++;
    }
    *dest = '\0';
    return p;
}

char* tuya_strncpy(char* dest, const char* src, u16 count) {
    char* tmp = dest;
        while (count) {
        if ((*tmp = *src) != 0)
            src++;
            tmp++;
            count--;
    }
    return dest;
}

/**
 * @brief  String comparison
 * @param[in] {s1} String 1
 * @param[in] {s2} String 2
 * @return Comparison result
 * -         0:s1=s2
 * -         <0:s1<s2
 * -         >0:s1>s2
 */
i32 tuya_strcmp(char* s1, char* s2) {
    while (*s1 && *s2 && *s1 == *s2) {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

/**
 * @brief  Split int type into four bytes
 * @param[in] {number} Original 4-byte data
 * @param[out] {value} Processed 4-byte data
 * @return Null
 */
void int_to_byte(u32 number, u8 value[4]) {
    value[0] = number >> 24;
    value[1] = number >> 16;
    value[2] = number >> 8;
    value[3] = number & 0xff;
}

/**
 * @brief  Merge 4 bytes into one 32bit variable
 * @param[in] {value} 4-byte array
 * @return Merged 32bit variable
 */
u32 byte_to_int(const u8 value[4]) {
    u32 nubmer = 0;

    nubmer = (u32)value[0];
    nubmer <<= 8;
    nubmer |= (u32)value[1];
    nubmer <<= 8;
    nubmer |= (u32)value[2];
    nubmer <<= 8;
    nubmer |= (u32)value[3];

    return nubmer;
}

/**
 * @brief  Raw type dp data upload
 * @param[in] {dpid} dpid number
 * @param[in] {value} Pointer to current dp value
 * @param[in] {len} Data length
 * @return Null
 * @note   Null
 */
u8 mcu_dp_raw_update(u8 dpid, const u8 value[], u16 len) {
    u16 send_len = 0;

    if (stop_update_flag == ENABLE)
        return SUCCESS;
    //
    send_len = set_wifi_uart_byte(send_len, dpid);
    send_len = set_wifi_uart_byte(send_len, DP_TYPE_RAW);
    //
    send_len = set_wifi_uart_byte(send_len, len / 0x100);
    send_len = set_wifi_uart_byte(send_len, len % 0x100);
    //
    send_len = set_wifi_uart_buffer(send_len, (u8*)value, len);

    wifi_uart_write_frame(STATE_UPLOAD_CMD, MCU_TX_VER, send_len);

    return SUCCESS;
}

/**
 * @brief  Bool type dp data upload
 * @param[in] {dpid} dpid number
 * @param[in] {value} Current dp value
 * @return Null
 * @note   Null
 */
u8 mcu_dp_bool_update(u8 dpid, u8 value) {
    u16 send_len = 0;

    if (stop_update_flag == ENABLE)
        return SUCCESS;

    send_len = set_wifi_uart_byte(send_len, dpid);
    send_len = set_wifi_uart_byte(send_len, DP_TYPE_BOOL);
    //
    send_len = set_wifi_uart_byte(send_len, 0);
    send_len = set_wifi_uart_byte(send_len, 1);
    //
    if (value == FALSE) {
        send_len = set_wifi_uart_byte(send_len, FALSE);
    } else {
        send_len = set_wifi_uart_byte(send_len, 1);
    }

    wifi_uart_write_frame(STATE_UPLOAD_CMD, MCU_TX_VER, send_len);

    return SUCCESS;
}

/**
 * @brief  Value type dp data upload
 * @param[in] {dpid} dpid number
 * @param[in] {value} Current dp value
 * @return Null
 * @note   Null
 */
u8 mcu_dp_value_update(u8 dpid, u32 value) {
    u16 send_len = 0;

    if (stop_update_flag == ENABLE)
        return SUCCESS;

    send_len = set_wifi_uart_byte(send_len, dpid);
    send_len = set_wifi_uart_byte(send_len, DP_TYPE_VALUE);
    //
    send_len = set_wifi_uart_byte(send_len, 0);
    send_len = set_wifi_uart_byte(send_len, 4);
    //
    send_len = set_wifi_uart_byte(send_len, value >> 24);
    send_len = set_wifi_uart_byte(send_len, value >> 16);
    send_len = set_wifi_uart_byte(send_len, value >> 8);
    send_len = set_wifi_uart_byte(send_len, value & 0xff);

    wifi_uart_write_frame(STATE_UPLOAD_CMD, MCU_TX_VER, send_len);

    return SUCCESS;
}

/**
 * @brief  String type dp data upload
 * @param[in] {dpid} dpid number
 * @param[in] {value} Pointer to current dp value
 * @param[in] {len} Data length
 * @return Null
 * @note   Null
 */
u8 mcu_dp_string_update(u8 dpid, const u8 value[], u16 len) {
    u16 send_len = 0;

    if (stop_update_flag == ENABLE)
        return SUCCESS;
    //
    send_len = set_wifi_uart_byte(send_len, dpid);
    send_len = set_wifi_uart_byte(send_len, DP_TYPE_STRING);
    //
    send_len = set_wifi_uart_byte(send_len, len / 0x100);
    send_len = set_wifi_uart_byte(send_len, len % 0x100);
    //
    send_len = set_wifi_uart_buffer(send_len, (u8*)value, len);

    wifi_uart_write_frame(STATE_UPLOAD_CMD, MCU_TX_VER, send_len);

    return SUCCESS;
}

/**
 * @brief  Enum type dp data upload
 * @param[in] {dpid} dpid number
 * @param[in] {value} Current dp value
 * @return Null
 * @note   Null
 */
u8 mcu_dp_enum_update(u8 dpid, u8 value) {
    u16 send_len = 0;

    if (stop_update_flag == ENABLE)
        return SUCCESS;

    send_len = set_wifi_uart_byte(send_len, dpid);
    send_len = set_wifi_uart_byte(send_len, DP_TYPE_ENUM);
    //
    send_len = set_wifi_uart_byte(send_len, 0);
    send_len = set_wifi_uart_byte(send_len, 1);
    //
    send_len = set_wifi_uart_byte(send_len, value);

    wifi_uart_write_frame(STATE_UPLOAD_CMD, MCU_TX_VER, send_len);

    return SUCCESS;
}

/**
 * @brief  Fault type dp data upload
 * @param[in] {dpid} dpid number
 * @param[in] {value} Current dp value
 * @return Null
 * @note   Null
 */
u8 mcu_dp_fault_update(u8 dpid, u32 value) {
    u16 send_len = 0;

    if (stop_update_flag == ENABLE)
        return SUCCESS;

    send_len = set_wifi_uart_byte(send_len, dpid);
    send_len = set_wifi_uart_byte(send_len, DP_TYPE_BITMAP);
    //
    send_len = set_wifi_uart_byte(send_len, 0);

    if ((value | 0xff) == 0xff) {
        send_len = set_wifi_uart_byte(send_len, 1);
        send_len = set_wifi_uart_byte(send_len, value);
    } else if ((value | 0xffff) == 0xffff) {
        send_len = set_wifi_uart_byte(send_len, 2);
        send_len = set_wifi_uart_byte(send_len, value >> 8);
        send_len = set_wifi_uart_byte(send_len, value & 0xff);
    } else {
        send_len = set_wifi_uart_byte(send_len, 4);
        send_len = set_wifi_uart_byte(send_len, value >> 24);
        send_len = set_wifi_uart_byte(send_len, value >> 16);
        send_len = set_wifi_uart_byte(send_len, value >> 8);
        send_len = set_wifi_uart_byte(send_len, value & 0xff);
    }

    wifi_uart_write_frame(STATE_UPLOAD_CMD, MCU_TX_VER, send_len);

    return SUCCESS;
}

u8 mcu_dp_struct_update(u8 dpid, const void* ptr_to_struct, u16 struct_size) {
    u16 send_len = 0;
    u8 dp_data_sz[2];
    UINT16_HTON(dp_data_sz, struct_size);

    send_len = set_wifi_uart_byte(send_len, dpid);
    send_len = set_wifi_uart_byte(send_len, DP_TYPE_STRUCT);
    //
    send_len = set_wifi_uart_byte(send_len, dp_data_sz[0]);
    send_len = set_wifi_uart_byte(send_len, dp_data_sz[1]);
    //
    send_len = set_wifi_uart_buffer(send_len, (u8*)ptr_to_struct, struct_size);

    wifi_uart_write_frame(STATE_UPLOAD_CMD, MCU_TX_VER, send_len);

    return SUCCESS;
}

u8 mcu_dp_array_update(u8 dpid, const void* ptr_to_array, u16 array_total_size) {
    u16 send_len = 0;
    u8 dp_data_sz[2];
    UINT16_HTON(dp_data_sz, array_total_size);

    send_len = set_wifi_uart_byte(send_len, dpid);
    send_len = set_wifi_uart_byte(send_len, DP_TYPE_ARRAY);
    //
    send_len = set_wifi_uart_byte(send_len, dp_data_sz[0]);
    send_len = set_wifi_uart_byte(send_len, dp_data_sz[1]);
    //
    send_len = set_wifi_uart_buffer(send_len, (u8*)ptr_to_array, array_total_size);

    wifi_uart_write_frame(STATE_UPLOAD_CMD, MCU_TX_VER, send_len);

    return SUCCESS;
}

#ifdef MCU_DP_UPLOAD_SYN
/**
 * @brief  Raw type dp data sync upload
 * @param[in] {dpid} dpid number
 * @param[in] {value} Pointer to current dp value
 * @param[in] {len} Data length
 * @return Null
 * @note   Null
 */
u8 mcu_dp_raw_update_syn(u8 dpid, const u8 value[], u16 len) {
    u16 send_len = 0;

    if (stop_update_flag == ENABLE)
        return SUCCESS;
    //
    send_len = set_wifi_uart_byte(send_len, dpid);
    send_len = set_wifi_uart_byte(send_len, DP_TYPE_RAW);
    //
    send_len = set_wifi_uart_byte(send_len, len / 0x100);
    send_len = set_wifi_uart_byte(send_len, len % 0x100);
    //
    send_len = set_wifi_uart_buffer(send_len, (u8*)value, len);

    wifi_uart_write_frame(STATE_UPLOAD_SYN_CMD, MCU_TX_VER, send_len);

    return SUCCESS;
}

/**
 * @brief  Bool type dp data sync upload
 * @param[in] {dpid} dpid number
 * @param[in] {value} Pointer to current dp value
 * @return Null
 * @note   Null
 */
u8 mcu_dp_bool_update_syn(u8 dpid, u8 value) {
    u16 send_len = 0;

    if (stop_update_flag == ENABLE)
        return SUCCESS;

    send_len = set_wifi_uart_byte(send_len, dpid);
    send_len = set_wifi_uart_byte(send_len, DP_TYPE_BOOL);
    //
    send_len = set_wifi_uart_byte(send_len, 0);
    send_len = set_wifi_uart_byte(send_len, 1);
    //
    if (value == FALSE) {
        send_len = set_wifi_uart_byte(send_len, FALSE);
    } else {
        send_len = set_wifi_uart_byte(send_len, 1);
    }

    wifi_uart_write_frame(STATE_UPLOAD_SYN_CMD, MCU_TX_VER, send_len);

    return SUCCESS;
}

/**
 * @brief  Value type dp data sync upload
 * @param[in] {dpid} dpid number
 * @param[in] {value} Pointer to current dp value
 * @return Null
 * @note   Null
 */
u8 mcu_dp_value_update_syn(u8 dpid, u32 value) {
    u16 send_len = 0;

    if (stop_update_flag == ENABLE)
        return SUCCESS;

    send_len = set_wifi_uart_byte(send_len, dpid);
    send_len = set_wifi_uart_byte(send_len, DP_TYPE_VALUE);
    //
    send_len = set_wifi_uart_byte(send_len, 0);
    send_len = set_wifi_uart_byte(send_len, 4);
    //
    send_len = set_wifi_uart_byte(send_len, value >> 24);
    send_len = set_wifi_uart_byte(send_len, value >> 16);
    send_len = set_wifi_uart_byte(send_len, value >> 8);
    send_len = set_wifi_uart_byte(send_len, value & 0xff);

    wifi_uart_write_frame(STATE_UPLOAD_SYN_CMD, MCU_TX_VER, send_len);

    return SUCCESS;
}

/**
 * @brief  String type dp data sync upload
 * @param[in] {dpid} dpid number
 * @param[in] {value} Pointer to current dp value
 * @param[in] {len} Data length
 * @return Null
 * @note   Null
 */
u8 mcu_dp_string_update_syn(u8 dpid, const u8 value[], u16 len) {
    u16 send_len = 0;

    if (stop_update_flag == ENABLE)
        return SUCCESS;
    //
    send_len = set_wifi_uart_byte(send_len, dpid);
    send_len = set_wifi_uart_byte(send_len, DP_TYPE_STRING);
    //
    send_len = set_wifi_uart_byte(send_len, len / 0x100);
    send_len = set_wifi_uart_byte(send_len, len % 0x100);
    //
    send_len = set_wifi_uart_buffer(send_len, (u8*)value, len);

    wifi_uart_write_frame(STATE_UPLOAD_SYN_CMD, MCU_TX_VER, send_len);

    return SUCCESS;
}

/**
 * @brief  Enum type dp data sync upload
 * @param[in] {dpid} dpid number
 * @param[in] {value} Pointer to current dp value
 * @return Null
 * @note   Null
 */
u8 mcu_dp_enum_update_syn(u8 dpid, u8 value) {
    u16 send_len = 0;

    if (stop_update_flag == ENABLE)
        return SUCCESS;

    send_len = set_wifi_uart_byte(send_len, dpid);
    send_len = set_wifi_uart_byte(send_len, DP_TYPE_ENUM);
    //
    send_len = set_wifi_uart_byte(send_len, 0);
    send_len = set_wifi_uart_byte(send_len, 1);

    send_len = set_wifi_uart_byte(send_len, value);

    wifi_uart_write_frame(STATE_UPLOAD_SYN_CMD, MCU_TX_VER, send_len);

    return SUCCESS;
}

/**
 * @brief  Fault type dp data sync upload
 * @param[in] {dpid} dpid number
 * @param[in] {value} Pointer to current dp value
 * @return Null
 * @note   Null
 */
u8 mcu_dp_fault_update_syn(u8 dpid, u32 value) {
    u16 send_len = 0;

    if (stop_update_flag == ENABLE)
        return SUCCESS;

    send_len = set_wifi_uart_byte(send_len, dpid);
    send_len = set_wifi_uart_byte(send_len, DP_TYPE_BITMAP);
    //
    send_len = set_wifi_uart_byte(send_len, 0);

    if ((value | 0xff) == 0xff) {
        send_len = set_wifi_uart_byte(send_len, 1);
        send_len = set_wifi_uart_byte(send_len, value);
    } else if ((value | 0xffff) == 0xffff) {
        send_len = set_wifi_uart_byte(send_len, 2);
        send_len = set_wifi_uart_byte(send_len, value >> 8);
        send_len = set_wifi_uart_byte(send_len, value & 0xff);
    } else {
        send_len = set_wifi_uart_byte(send_len, 4);
        send_len = set_wifi_uart_byte(send_len, value >> 24);
        send_len = set_wifi_uart_byte(send_len, value >> 16);
        send_len = set_wifi_uart_byte(send_len, value >> 8);
        send_len = set_wifi_uart_byte(send_len, value & 0xff);
    }

    wifi_uart_write_frame(STATE_UPLOAD_SYN_CMD, MCU_TX_VER, send_len);

    return SUCCESS;
}

u8 mcu_dp_struct_update_syn(u8 dpid, const void* ptr_to_struct, u16 struct_size) {
    u16 send_len = 0;
    u8 dp_data_sz[2];
    UINT16_HTON(dp_data_sz, struct_size);

    send_len = set_wifi_uart_byte(send_len, dpid);
    send_len = set_wifi_uart_byte(send_len, DP_TYPE_STRUCT);
    //
    send_len = set_wifi_uart_byte(send_len, dp_data_sz[0]);
    send_len = set_wifi_uart_byte(send_len, dp_data_sz[1]);
    //
    send_len = set_wifi_uart_buffer(send_len, (u8*)ptr_to_struct, struct_size);

    wifi_uart_write_frame(STATE_UPLOAD_SYN_CMD, MCU_TX_VER, send_len);

    return SUCCESS;
}

u8 mcu_dp_array_update_syn(u8 dpid, const void* ptr_to_array, u16 array_total_size) {
    u16 send_len = 0;
    u8 dp_data_sz[2];
    UINT16_HTON(dp_data_sz, array_total_size);

    send_len = set_wifi_uart_byte(send_len, dpid);
    send_len = set_wifi_uart_byte(send_len, DP_TYPE_ARRAY);
    //
    send_len = set_wifi_uart_byte(send_len, dp_data_sz[0]);
    send_len = set_wifi_uart_byte(send_len, dp_data_sz[1]);
    //
    send_len = set_wifi_uart_buffer(send_len, (u8*)ptr_to_array, array_total_size);

    wifi_uart_write_frame(STATE_UPLOAD_SYN_CMD, MCU_TX_VER, send_len);

    return SUCCESS;
}
#endif

/**
 * @brief  MCU gets bool type download dp value
 * @param[in] {value} dp data buffer address
 * @param[in] {len} dp data length
 * @return Current dp value
 * @note   Null
 */
u8 mcu_get_dp_download_bool(const u8 value[], u16 len) {
    TUYA_UNUSED(len);
    return (value[0]);
}

/**
 * @brief  MCU gets enum type download dp value
 * @param[in] {value} dp data buffer address
 * @param[in] {len} dp data length
 * @return Current dp value
 * @note   Null
 */
u8 mcu_get_dp_download_enum(const u8 value[], u16 len) {
    TUYA_UNUSED(len);
    return (value[0]);
}

/**
 * @brief  MCU gets value type download dp value
 * @param[in] {value} dp data buffer address
 * @param[in] {len} dp data length
 * @return Current dp value
 * @note   Null
 */
u32 mcu_get_dp_download_value(const u8 value[], u16 len) {
    TUYA_UNUSED(len);
    return (byte_to_int(value));
}

/**
 * @brief  UART receive data temporary storage processing
 * @param[in] {value} One byte data received by UART
 * @return Null
 * @note   Call this function in the MCU UART processing function, and pass the received data as a parameter
 */
void uart_receive_input(u8 value) {
    /* PORTED: called from the LPUART receive interrupt, via the one-byte re-arm
       loop in libs/src/tuya_link/tuya_link.c (OnByteReceived). */

    if (1 == rx_buf_out - rx_buf_in) {
        // UART receive buffer is full
    } else if ((rx_buf_in > rx_buf_out) && ((unsigned long int)(rx_buf_in - rx_buf_out) >= sizeof(wifi_uart_rx_buf))) {
        // UART receive buffer is full
    } else {
        // UART receive buffer is not full
        if (rx_buf_in >= (u8*)(wifi_uart_rx_buf + sizeof(wifi_uart_rx_buf))) {
            rx_buf_in = (u8*)(wifi_uart_rx_buf);
        }

        *rx_buf_in++ = value;
    }
}

/**
 * @brief  UART receive multiple bytes data temporary storage processing
 * @param[in] {value} Source address of the data to be received by UART
 * @param[in] {data_len} Data length of the data to be received by UART
 * @return Null
 * @note   If you need to support one-time multi-byte buffering, you can call this function
 */
void uart_receive_buff_input(u8 value[], u16 data_len) {
    /* PORTED: #error removed; unused here -- the receive path is one byte at a
       time (uart_receive_input). */

    u16 i = 0;
    for (i = 0; i < data_len; i++) {
        uart_receive_input(value[i]);
    }
}

#ifdef CONFIG_MCU_SDK_TEST_ONLY_USE_NEW_DISPATCHER

typedef u8 DISPATCHER_STATUS_E;
#define DISPATCHER_STATUS_WAIT_SYNC_1 0x00
#define DISPATCHER_STATUS_WAIT_SYNC_2 0x01
#define DISPATCHER_STATUS_RECV_HEADER 0x02
#define DISPATCHER_STATUS_RECV_DATA 0x03
#define DISPATCHER_STATUS_RECV_CHECKSUM 0x04
#define DISPATCHER_STATUS_DUMMY_RECV 0x05

#endif

#ifndef CONFIG_MCU_SDK_TEST_ONLY_USE_NEW_DISPATCHER
/* PORTED: fill level of wifi_data_process_buf.  Tuya keeps it as a static
   local of wifi_uart_service(); hoisted here so wifi_protocol_init() can
   clear it, otherwise a partial frame left over from the previous window
   sits in front of the first frame of the next one. */
static u16 rx_in = 0;
#endif

/**
 * @brief  Wifi UART data processing service
 * @param  Null
 * @return Null
 * @note   Call this function in the MCU main function while loop
 */
void wifi_uart_service(void) {
    /* PORTED: called from Tuya_Service(), which every wait in the report window
       runs on every pass (libs/src/tuya_link/tuya_link.c, Tuya_WaitUntil).  Not
       from the main loop unconditionally as Tuya assumes, because outside a
       window the module is unpowered and there is nothing to service. */

#ifndef CONFIG_MCU_SDK_TEST_ONLY_USE_NEW_DISPATCHER

    u16 offset = 0;
    u16 rx_value_len = 0;

    while ((rx_in < sizeof(wifi_data_process_buf)) && with_data_rxbuff() > 0) {
        wifi_data_process_buf[rx_in++] = take_byte_rxbuff();
    }

    if (rx_in < PROTOCOL_HEAD)
        return;

    while ((rx_in - offset) >= PROTOCOL_HEAD) {
        if (wifi_data_process_buf[offset + HEAD_FIRST] != FRAME_FIRST) {
            offset++;
            TUYA_DBG_EXEC(TUYA_PRINT("header 1 error, got %02X", wifi_data_process_buf[offset + HEAD_FIRST]));
            continue;
        }

        if (wifi_data_process_buf[offset + HEAD_SECOND] != FRAME_SECOND) {
            TUYA_DBG_EXEC(TUYA_PRINT("header 2 error, got %02X", wifi_data_process_buf[offset + HEAD_SECOND]));
            offset++;
            continue;
        }

        if (wifi_data_process_buf[offset + PROTOCOL_VERSION] != MCU_RX_VER) {
            offset += 2;
            continue;
        }

        rx_value_len = wifi_data_process_buf[offset + LENGTH_HIGH] * 0x100;
        rx_value_len += (wifi_data_process_buf[offset + LENGTH_LOW] + PROTOCOL_HEAD);

        /* PORTED: two fixes to the oversize-frame branch, which as Tuya wrote it
           could wedge the parser for the rest of the window.

           The bound: rx_value_len already counts the header, so a frame fits
           only up to sizeof(wifi_data_process_buf).  Tuya allowed another
           PROTOCOL_HEAD on top, and a frame in that gap waited forever for
           bytes there was no room for.

           The discard: Tuya emptied the ring but left the oversize header at
           the front of wifi_data_process_buf, where the next pass found it
           again, discarded the ring again, and never moved past it.  Setting
           offset to rx_in empties the process buffer too, as the trace message
           below always said it did. */
        if (rx_value_len > sizeof(wifi_data_process_buf)) {
            TUYA_DBG_EXEC(TUYA_PRINT("length overflow, got %d.", rx_value_len));
            TUYA_DBG_EXEC(TUYA_PRINT("whole process buffer and rx buffer discarded."));
            rx_buf_in = rx_buf_out = wifi_uart_rx_buf;
            offset = rx_in;
            break;
        }

        if ((rx_in - offset) < rx_value_len) {
            break;
        }

        // Data reception completed
        if (get_check_sum((u8*)wifi_data_process_buf + offset, rx_value_len - 1) != wifi_data_process_buf[offset + rx_value_len - 1]) {
            // Checksum error
            TUYA_DBG_EXEC(TUYA_PRINT("crc error (crc:0x%X  but data:0x%X)\r\n", get_check_sum((u8*)wifi_data_process_buf + offset, rx_value_len - 1), wifi_data_process_buf[offset + rx_value_len - 1]));
            offset += 3;
            continue;
        }

        data_handle(offset);
        offset += rx_value_len;
    }  // end while

    rx_in -= offset;
    if (rx_in > 0) {
        tuya_memcpy((i8*)wifi_data_process_buf, (const i8*)wifi_data_process_buf + offset, rx_in);
    }

#else

    STATIC DISPATCHER_STATUS_E dispatcher_status = DISPATCHER_STATUS_WAIT_SYNC_1;
    STATIC u16 proc_buf_next_writeable_index = 0;
    STATIC u16 expected_byte_count = 0;
    STATIC bool_t overflow_detected = FALSE;

    u8 recv_byte = 0;

    while (with_data_rxbuff() > 0) {
        recv_byte = take_byte_rxbuff();
        switch (dispatcher_status) {
            case DISPATCHER_STATUS_WAIT_SYNC_1: {
                if (recv_byte != 0x55) {
                    TUYA_DBG_EXEC(TUYA_PRINT("Sync failed at 0x55, got 0x%02x", recv_byte));
                    break;
                }
                dispatcher_status = DISPATCHER_STATUS_WAIT_SYNC_2;
                wifi_data_process_buf[proc_buf_next_writeable_index++] = recv_byte;
            } break;

            case DISPATCHER_STATUS_WAIT_SYNC_2: {
                if (recv_byte != 0xAA) {
                    TUYA_DBG_EXEC(TUYA_PRINT("Sync failed at 0xAA, got 0x%02x", recv_byte));
                    break;
                }
                dispatcher_status = DISPATCHER_STATUS_RECV_HEADER;
                wifi_data_process_buf[proc_buf_next_writeable_index++] = recv_byte;
                expected_byte_count = 4;
            } break;

            case DISPATCHER_STATUS_RECV_HEADER: {
                wifi_data_process_buf[proc_buf_next_writeable_index++] = recv_byte;
                expected_byte_count--;
                if (expected_byte_count == 0) {
                    if (wifi_data_process_buf[PROTOCOL_VERSION] != MCU_RX_VER) {
                        TUYA_DBG_EXEC(TUYA_PRINT("Unexcepted version: %02x", wifi_data_process_buf[PROTOCOL_VERSION]));
                        goto sync_lost;
                    }
                    expected_byte_count = (u16)(wifi_data_process_buf[LENGTH_HIGH] << 8) | wifi_data_process_buf[LENGTH_LOW];
                    if (expected_byte_count > sizeof(wifi_data_process_buf) - PROTOCOL_HEAD) {
                        TUYA_DBG_EXEC(TUYA_PRINT("Overflow detected, len: %d", expected_byte_count));
                        overflow_detected = TRUE;
                        switch (on_process_buffer_overflow(wifi_data_process_buf[FRAME_TYPE], expected_byte_count)) {
                            case PROCESS_BUFFER_OVERFLOW_HANDLE_METHOD_DISCARD_WHOLE_COMMAND:  // Discard the whole command
                            default:
                                expected_byte_count += 1;  // Add checksum
                                dispatcher_status = DISPATCHER_STATUS_DUMMY_RECV;
                                TUYA_DBG_EXEC(TUYA_PRINT("Whole command discarded, do dummy read for %d bytes", expected_byte_count));
                                break;
                            case PROCESS_BUFFER_OVERFLOW_HANDLE_METHOD_DISCARD_CURRENT_AND_RX_BUFFER:  // Discard current and receive buffer data
                                rx_buf_in = rx_buf_out = wifi_uart_rx_buf;
                                TUYA_DBG_EXEC(TUYA_PRINT("Current command discarded, rx buffer cleared."));
                                goto sync_lost;
                            case PROCESS_BUFFER_OVERFLOW_HANDLE_METHOD_DISCARD_CURRENT:  // Only discard current command
                                TUYA_DBG_EXEC(TUYA_PRINT("Current command discarded."));
                                goto sync_lost;
                        }
                    }
                    TUYA_DBG_EXEC(TUYA_PRINT("Got full head, cmd is 0x%02x, len is %d", wifi_data_process_buf[FRAME_TYPE], expected_byte_count));
                    if (expected_byte_count == 0) {
                        dispatcher_status = DISPATCHER_STATUS_RECV_CHECKSUM;
                    } else {
                        dispatcher_status = DISPATCHER_STATUS_RECV_DATA;
                    }
                }
            } break;

            case DISPATCHER_STATUS_RECV_DATA: {
                wifi_data_process_buf[proc_buf_next_writeable_index++] = recv_byte;
                expected_byte_count--;
                if (expected_byte_count == 0) {
                    TUYA_DBG_EXEC(TUYA_PRINT("Got full data"));
                    dispatcher_status = DISPATCHER_STATUS_RECV_CHECKSUM;
                }
            } break;

            case DISPATCHER_STATUS_RECV_CHECKSUM: {
                if (get_check_sum((u8*)wifi_data_process_buf, proc_buf_next_writeable_index) != recv_byte) {
                    TUYA_DBG_EXEC(TUYA_PRINT("crc error (crc:0x%X  but data:0x%X)\r\n", get_check_sum((u8*)wifi_data_process_buf, proc_buf_next_writeable_index - 1), wifi_data_process_buf[proc_buf_next_writeable_index - 1]));
                    goto sync_lost;
                }
                wifi_data_process_buf[proc_buf_next_writeable_index++] = recv_byte;

                TUYA_DBG_EXEC(TUYA_PRINT("Got full command, processing..."));
                if (!overflow_detected || on_first_command_after_overflow((const u8*)wifi_data_process_buf, proc_buf_next_writeable_index)) {
                    data_handle(0);
                    overflow_detected = FALSE;
                } else {
                    TUYA_DBG_EXEC(TUYA_PRINT("command rejected after overflow"));
                }

                // Prepare to receive next data packet
                dispatcher_status = DISPATCHER_STATUS_WAIT_SYNC_1;
                proc_buf_next_writeable_index = 0;
                expected_byte_count = 0;
            } break;

            case DISPATCHER_STATUS_DUMMY_RECV: {
                expected_byte_count--;
                if (expected_byte_count == 0) {
                    dispatcher_status = DISPATCHER_STATUS_WAIT_SYNC_1;
                    proc_buf_next_writeable_index = 0;
                    expected_byte_count = 0;
                }
            } break;
        }

        continue;
    sync_lost:
        TUYA_DBG_EXEC(TUYA_PRINT("Sync lost, resetting dispatcher state"));
        dispatcher_status = DISPATCHER_STATUS_WAIT_SYNC_1;
        proc_buf_next_writeable_index = 0;
        expected_byte_count = 0;
    }

#endif

    return;
}

/**
 * @brief  Protocol UART initialization function
 * @param  Null
 * @return Null
 * @note   Call this function in MCU initialization code
 */
void wifi_protocol_init(void) {
    /* PORTED: called from Tuya_PowerOn(), not once at startup.  The module is
       power-gated and cold-boots on every report window, so the parser state and
       the cached Wi-Fi state have to be reset with it. */
    rx_buf_in = (u8*)wifi_uart_rx_buf;
    rx_buf_out = (u8*)wifi_uart_rx_buf;
#ifndef CONFIG_MCU_SDK_TEST_ONLY_USE_NEW_DISPATCHER
    rx_in = 0;
#endif

    stop_update_flag = DISABLE;

#ifndef WIFI_CONTROL_SELF_MODE
    wifi_work_state = WIFI_SATE_UNKNOW;
#endif
}

#ifndef WIFI_CONTROL_SELF_MODE
/**
 * @brief  MCU gets reset wifi success flag
 * @param  Null
 * @return Reset flag
 * -           0(RESET_WIFI_ERROR): Failure
 * -           1(RESET_WIFI_SUCCESS): Success
 * @note   1: MCU actively calls mcu_reset_wifi() and then calls this function to get reset status
 *         2: If it is module self-processing mode, MCU does not need to call this function
 */
u8 mcu_get_reset_wifi_flag(void) {
    return reset_wifi_flag;
}

/**
 * @brief  MCU actively resets wifi working mode
 * @param  Null
 * @return Null
 * @note   1: MCU actively calls, get whether reset wifi is successful through mcu_get_reset_wifi_flag() function
 *         2: If it is module self-processing mode, MCU does not need to call this function
 */
void mcu_reset_wifi(void) {
    reset_wifi_flag = RESET_WIFI_ERROR;

    /* PORTED: the module restarts on this command and re-reports its state
       once it is back, in pairing mode.  Until then the cached state describes
       the session the reset has just ended -- possibly WIFI_CONN_CLOUD, which
       would have the pairing window (temp_sensor_main.c) take the old
       connection for the new one and close at once. */
    wifi_work_state = WIFI_SATE_UNKNOW;

    wifi_uart_write_frame(WIFI_RESET_CMD, MCU_TX_VER, 0);
}

/**
 * @brief  Get set wifi state success flag
 * @param  Null
 * @return wifimode flag
 * -           0(SET_WIFICONFIG_ERROR): Failure
 * -           1(SET_WIFICONFIG_SUCCESS): Success
 * @note   1: MCU actively calls mcu_set_wifi_mode() and then calls this function to get reset status
 *         2: If it is module self-processing mode, MCU does not need to call this function
 */
u8 mcu_get_wifimode_flag(void) {
    return set_wifimode_flag;
}

/**
 * @brief  MCU sets wifi working mode
 * @param[in] {mode} Mode to enter
 * @ref        0(SMART_CONFIG): Enter smartconfig mode
 * @ref        1(AP_CONFIG): Enter AP mode
 * @return Null
 * @note   1: MCU actively calls
 *         2: After success, you can judge whether set_wifi_config_state is TRUE; TRUE means setting wifi working mode successfully
 *         3: If it is module self-processing mode, MCU does not need to call this function
 */
void mcu_set_wifi_mode(u8 mode) {
    u8 length = 0;

    set_wifimode_flag = SET_WIFICONFIG_ERROR;

    length = set_wifi_uart_byte(length, mode);

    wifi_uart_write_frame(WIFI_MODE_CMD, MCU_TX_VER, length);
}

/**
 * @brief  MCU actively gets current wifi working state
 * @param  Null
 * @return wifi work state
 * -          SMART_CONFIG_STATE: smartconfig configuration state
 * -          AP_STATE: AP configuration state
 * -          WIFI_NOT_CONNECTED: WIFI configuration succeeded but not connected to router
 * -          WIFI_CONNECTED: WIFI configuration succeeded and connected to router
 * -          WIFI_CONN_CLOUD: WIFI has connected to cloud server
 * -          WIFI_LOW_POWER: WIFI is in low power mode
 * -          SMART_AND_AP_STATE: WIFI smartconfig&AP mode
 * @note   If it is module self-processing mode, MCU does not need to call this function
 */
u8 mcu_get_wifi_work_state(void) {
    return wifi_work_state;
}
#endif

#ifdef SUPPORT_GREEN_TIME
/**
 * @brief  MCU gets Greenwich time
 * @param  Null
 * @return Null
 * @note   MCU needs to call this function by itself
 */
void mcu_get_green_time(void) {
    wifi_uart_write_frame(GET_ONLINE_TIME_CMD, MCU_TX_VER, 0);
}
#endif

#ifdef SUPPORT_MCU_RTC_CHECK
/**
 * @brief  MCU gets system time for calibrating local clock
 * @param  Null
 * @return Null
 * @note   MCU actively calls and calibrates rtc clock in mcu_write_rtctime function after completion
 */
void mcu_get_system_time(void) {
    wifi_uart_write_frame(GET_LOCAL_TIME_CMD, MCU_TX_VER, 0);
}
#endif

#ifdef WIFI_TEST_ENABLE
/**
 * @brief  MCU initiates wifi function test
 * @param  Null
 * @return Null
 * @note   MCU needs to call this function by itself
 */
void mcu_start_wifitest(void) {
    wifi_uart_write_frame(WIFI_TEST_CMD, MCU_TX_VER, 0);
}
#endif

#ifdef WIFI_HEARTSTOP_ENABLE
/**
 * @brief  Notify WIFI module to stop heartbeat
 * @param  Null
 * @return Null
 * @note   MCU needs to call this function by itself
 */
void wifi_heart_stop(void) {
    wifi_uart_write_frame(HEAT_BEAT_STOP, MCU_TX_VER, 0);
}
#endif

#ifdef GET_WIFI_STATUS_ENABLE
/**
 * @brief  Get current wifi connection status
 * @param  Null
 * @return Null
 * @note   MCU needs to call this function by itself
 */
void mcu_get_wifi_connect_status(void) {
    wifi_uart_write_frame(GET_WIFI_STATUS_CMD, MCU_TX_VER, 0);
}
#endif

#ifdef WIFI_STREAM_ENABLE
/**
 * @brief  Stream service send
 * @param[in] {id} ID number
 * @param[in] {buffer} Address of send packet
 * @param[in] {buf_len} Send packet length
 * @return Stream service transmission result
 * -           0(ERROR): Failure
 * -           1(SUCCESS): Success
 * @note   MCU needs to implement this function by itself
 */
u8 stream_trans_send(u32 id, u8* buffer, u32 buf_len) {
    u32 map_offset = 0;
    u32 pack_num = 0;
    u32 rest_length = 0;
    i32 this_len = STREM_PACK_LEN;
    i32 cnt;

        if (stop_update_flag == ENABLE) return ERROR;

    pack_num = buf_len / STREM_PACK_LEN;
    rest_length = buf_len - pack_num * STREM_PACK_LEN;
    if (rest_length > 0) {
        pack_num++;
    }

    for (cnt = 0; cnt < pack_num; cnt++, map_offset += this_len) {
        if (cnt == pack_num - 1 && rest_length > 0) {
            this_len = rest_length;
        } else {
            this_len = STREM_PACK_LEN;
        }

        if (ERROR == stream_trans(id, map_offset, buffer + map_offset, this_len)) {
            // mcu is upgrading, stream service transmission is not allowed
            // printf("is upgrade\n");
            return ERROR;
        }

#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error "Please complete the stream service sending code by yourself according to the prompt, and delete this line after completion"
#endif

        /*  If MCU can use multi-process or multi-thread, you can put this function in a separate process or thread, and open the code shown below  */
        /*
        while(0xff == stream_status); //Wait for module reply

        if(0x00 != stream_status) {
            //Failure   Check error code in stream_trans_send_result function
            return ERROR;
        }
        */

        /*  If MCU does not support multi-process or multi-thread, use this function to send only one packet each time (data length not greater than STREM_PACK_LEN),
        After sending, process the module's return result in stream_trans_send_result function, and call this function again to send next packet when sending succeeds, note packet offset should be increased  */
    }

    return SUCCESS;
}

/**
 * @brief  Multi-map stream service send
 * @param[in] {id} Map stream service session ID
 * @param[in] {sub_id} Sub-map ID
 * @param[in] {sub_id_pro_mode} Sub-map ID data processing mode
 * @ref       0x00: Continue to accumulate
 * @ref       0x01: Clear uploaded data
 * @param[in] {buffer} Address of data packet to send
 * @param[in] {buf_len} Length of data packet to send
 * @return Stream service transmission result
 * -           0(ERROR): Failure
 * -           1(SUCCESS): Success
 * @note   MCU needs to implement this function by itself
 */
u8 maps_stream_trans_send(u32 id, u8 sub_id, u8 sub_id_pro_mode, u8* buffer, u32 buf_len) {
    u32 map_offset = 0;
    u32 pack_num = 0;
    u32 rest_length = 0;
    u8 pro_ver = 0;  // Map service protocol version defaults to 0
    i32 this_len = STREM_PACK_LEN;
    i32 cnt;

        if (stop_update_flag == ENABLE) return SUCCESS;

    pack_num = buf_len / STREM_PACK_LEN;
    rest_length = buf_len - pack_num * STREM_PACK_LEN;
    if (rest_length > 0) {
        pack_num++;
    }

    for (cnt = 0; cnt < pack_num; cnt++, map_offset += this_len) {
        if (cnt == pack_num - 1 && rest_length > 0) {
            this_len = rest_length;
        } else {
            this_len = STREM_PACK_LEN;
        }

        if (ERROR == maps_stream_trans(pro_ver, id, sub_id, sub_id_pro_mode, map_offset, buffer + map_offset, this_len)) {
            // mcu is upgrading, stream service transmission is not allowed
            // printf("is upgrade\n");
            return ERROR;
        }

#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error "Please complete the stream service sending code by yourself according to the prompt, and delete this line after completion"
#endif
        /*  If MCU can use multi-process or multi-thread, you can put this function in a separate process or thread, and open the code shown below  */
        /*
        while(0xff == maps_stream_status); //Wait for module reply

        if(0x00 != maps_stream_status) {
            //Failure   Check error code in maps_stream_trans_send_result function
            return ERROR;
        }
        */

        /*  If MCU does not support multi-process or multi-thread, use this function to send only one packet each time (data length not greater than STREM_PACK_LEN),
        After sending, process the module's return result in maps_stream_trans_send_result function, and call this function again to send next packet when sending succeeds, note packet offset should be increased  */
    }

    return SUCCESS;
}
#endif

#ifdef WIFI_CONNECT_TEST_ENABLE
/**
 * @brief  MCU initiates wifi function test (connect to specified router)
 * @param[in] {ssid_buf} Address storing router name string data (ssid length supports up to 32 bytes)
 * @param[in] {passwd_buffer} Address storing router password string data (passwd length supports up to 64 bytes)
 * @return Null
 * @note   MCU needs to call this function by itself
 */
void mcu_start_connect_wifitest(u8* ssid_buf, u8* passwd_buffer) {
    u16 send_len = 0;

    if (tuya_strlen(ssid_buf) > 32 || tuya_strlen(passwd_buffer) > 64) {
        // printf("ssid_buf or passwd_buffer is too long!");
        return;
    }

    send_len = set_wifi_uart_buffer(send_len, "{\"ssid\":\"", tuya_strlen("{\"ssid\":\""));
    send_len = set_wifi_uart_buffer(send_len, ssid_buf, tuya_strlen(ssid_buf));
    send_len = set_wifi_uart_buffer(send_len, "\",\"password\":\"", tuya_strlen("\",\"password\":\""));
    send_len = set_wifi_uart_buffer(send_len, passwd_buffer, tuya_strlen(passwd_buffer));
    send_len = set_wifi_uart_buffer(send_len, "\"}", tuya_strlen("\"}"));

    wifi_uart_write_frame(WIFI_CONNECT_TEST_CMD, MCU_TX_VER, send_len);
}
#endif

#ifdef GET_MODULE_MAC_ENABLE
/**
 * @brief  Get module MAC
 * @param  Null
 * @return Null
 * @note   MCU needs to call this function by itself
 */
void mcu_get_module_mac(void) {
    wifi_uart_write_frame(GET_MAC_CMD, MCU_TX_VER, 0);
}
#endif

#ifdef IR_TX_RX_TEST_ENABLE
/**
 * @brief  MCU initiates infrared enter transceiver production test
 * @param  Null
 * @return Null
 * @note   MCU needs to call this function by itself
 */
void mcu_start_ir_test(void) {
    wifi_uart_write_frame(IR_TX_RX_TEST_CMD, MCU_TX_VER, 0);
}
#endif

#ifdef MODULE_EXPANDING_SERVICE_ENABLE
/**
 * @brief  Open module time service notification
 * @param[in] {time_type} Time type
 * @ref       0x00: Greenwich time
 * @ref       0x01: Local time
 * @return Null
 * @note   MCU needs to call this function by itself
 */
void open_module_time_serve(u8 time_type) {
    u16 send_len = 0;
    send_len = set_wifi_uart_byte(send_len, 0x01);
    send_len = set_wifi_uart_byte(send_len, time_type);
    wifi_uart_write_frame(MODULE_EXTEND_FUN_CMD, MCU_TX_VER, send_len);
}

/**
 * @brief  Actively request weather service data
 * @param  Null
 * @return Null
 * @note   MCU needs to call this function by itself
 */
void request_weather_serve(void) {
    u16 send_len = 0;
    send_len = set_wifi_uart_byte(send_len, 0x03);
    wifi_uart_write_frame(MODULE_EXTEND_FUN_CMD, MCU_TX_VER, send_len);
}

/**
 * @brief  Open module reset state notification
 * @param  Null
 * @return Null
 * @note   MCU needs to call this function by itself
 */
void open_module_reset_state_serve(void) {
    u16 send_len = 0;
    send_len = set_wifi_uart_byte(send_len, 0x04);
    wifi_uart_write_frame(MODULE_EXTEND_FUN_CMD, MCU_TX_VER, send_len);
}
#endif

#ifdef BLE_RELATED_FUNCTION_ENABLE
/**
 * @brief  MCU initiates bluetooth function test (scan specified bluetooth beacon)
 * @param  Null
 * @return Null
 * @note   MCU needs to call this function by itself
 */
void mcu_start_BLE_test(void) {
    u16 send_len = 0;
    send_len = set_wifi_uart_byte(send_len, 0x01);
    wifi_uart_write_frame(BLE_RELATIVE_CMD, MCU_TX_VER, send_len);
}
#endif

#ifdef VOICE_MODULE_PROTOCOL_ENABLE
/**
 * @brief  Get voice status code
 * @param  Null
 * @return Null
 * @note   MCU needs to call this function by itself
 */
void get_voice_state(void) {
    wifi_uart_write_frame(GET_VOICE_STATE_CMD, MCU_TX_VER, 0);
}

/**
 * @brief  MIC mute setting
 * @param[in] {set_val} Mute setting value
 * @ref       0x00: mic on
 * @ref       0x01: mic mute
 * @ref       0xA0: query mute status
 * @return Null
 * @note   MCU needs to call this function by itself
 */
void set_voice_MIC_silence(u8 set_val) {
    u16 send_len = 0;
    send_len = set_wifi_uart_byte(send_len, set_val);
    wifi_uart_write_frame(MIC_SILENCE_CMD, MCU_TX_VER, send_len);
}

/**
 * @brief  speaker volume setting
 * @param[in] {set_val} Volume setting value
 * @ref       0~10: Volume range
 * @ref       0xA0: Query volume value
 * @return Null
 * @note   MCU needs to call this function by itself
 */
void set_speaker_voice(u8 set_val) {
    u16 send_len = 0;
    send_len = set_wifi_uart_byte(send_len, set_val);
    wifi_uart_write_frame(SET_SPEAKER_VOLUME_CMD, MCU_TX_VER, send_len);
}

/**
 * @brief  Audio production test
 * @param[in] {set_val} Audio production test value
 * @ref       0x00: Close audio production test
 * @ref       0x01: mic1 audio loop test
 * @ref       0x02: mic2 audio loop test
 * @ref       0xA0: Query current production test status
 * @return Null
 * @note   MCU needs to call this function by itself
 */
void voice_test(u8 test_val) {
    u16 send_len = 0;
    send_len = set_wifi_uart_byte(send_len, test_val);
    wifi_uart_write_frame(VOICE_TEST_CMD, MCU_TX_VER, send_len);
}

/**
 * @brief  Awakening production test
 * @param  Null
 * @return Null
 * @note   MCU needs to call this function by itself
 */
void voice_awaken_test(void) {
    wifi_uart_write_frame(VOICE_AWAKEN_TEST_CMD, MCU_TX_VER, 0);
}

/**
 * @brief  Voice module MCU function setting
 * @param[in] {play} Play/pause function 1(play) / 0(pause)
 * @param[in] {bt_play} Bluetooth switch function 1(on) / 0(off)
 * @return Null
 * @note   MCU needs to call this function by itself
 */
void voice_mcu_fun_set(u8 play, u8 bt_play) {
    u16 send_len = 0;
    u8 str[50];

    // MCU setting temporarily only supports "play/pause" "bluetooth switch"

    sprintf((i8*)str, "{\"play\":%s,\"bt_play\":%s}", play ? "true" : "false", bt_play ? "true" : "false");

    send_len = set_wifi_uart_byte(send_len, 0x00);
    send_len = set_wifi_uart_buffer(send_len, str, tuya_strlen(str));
    wifi_uart_write_frame(VOICE_EXTEND_FUN_CMD, MCU_TX_VER, send_len);
}
#endif

#if defined(BLE_RELATED_FUNCTION_ENABLE)
/**
 * @brief Query device bluetooth connection state
 */
void query_bt_connect_state() {
    u16 send_len = 0;
    send_len = set_wifi_uart_byte(send_len, BLE_RELATIVE_SUB_CMD_DEV_STATE_QUERY);
    wifi_uart_write_frame(BLE_RELATIVE_CMD, MCU_TX_VER, send_len);
}
#endif

#if defined(DP_WITH_TYPE_ENABLE)

/**
 * @brief DP function setting with type
 *
 * @param status 0x00: Disable 0x01: Enable
 */
void dp_with_type_enable(u8 status) {
    u16 send_len = 0;
    send_len = set_wifi_uart_byte(send_len, DP_WITH_TYPE_EXTEND_SUB_CMD_START);
    send_len = set_wifi_uart_byte(send_len, status);
    wifi_uart_write_frame(DP_WITH_TYPE_EXTEND_CMD, MCU_TX_VER, send_len);
}

#endif

#if defined(FAN_PRODUCT_SERVICE_ENABLE)
/**
 * @brief Fan class function test
 * For example: wind speed step 20, hold time 5, module controls motor 0%-20%-40%-60%-80%-100% in sequence, interval time of each gear is 5 seconds
 *
 * @param step Wind speed step (%)
 * @param keep_time Hold time (seconds)
 */
void fan_product_test_fun_test(u8 step, u8 keep_time) {
    u16 send_len = 0;
    send_len = set_wifi_uart_byte(send_len, FAN_PRODUCT_SERVICE_SUB_CMD_TEST);
    send_len = set_wifi_uart_byte(send_len, step);
    send_len = set_wifi_uart_byte(send_len, keep_time);
    wifi_uart_write_frame(DP_WITH_TYPE_EXTEND_CMD, MCU_TX_VER, send_len);
}

/**
 * @brief Fan class product duty cycle test
 * When set duty cycle is 70%, PWM1 outputs 70%, PWM2 outputs 0%, after detecting zero-crossing signal, PWM1 outputs 0%, PWM2 outputs 70%.
 *
 * @param duty Duty cycle (%)
 */
void fan_product_test_duty_set(u8 duty) {
    u16 send_len = 0;
    send_len = set_wifi_uart_byte(send_len, FAN_PRODUCT_SERVICE_SUB_CMD_SET_DUTY);
    send_len = set_wifi_uart_byte(send_len, duty);
    wifi_uart_write_frame(DP_WITH_TYPE_EXTEND_CMD, MCU_TX_VER, send_len);
}
#endif

#if defined(CLOUD_STORAGE_FUNCTION_ENABLE)

/**
 * @brief Cloud storage upload start
 *
 *
 * @param type     File type 0x00: LOG 0x01: NILM
 * @param name     File name
 * @param len      File length (Byte)
 * @param pkg_size Single package size (Byte)
 */
void cloud_storage_start_upload(u8 type, const char* name, u32 len, u16 pkg_size) {
#ifndef CONFIG_TUYA_DISABLE_ALL_ERROR_MARCO
#error Ensure buffer can accommodate JSON
#endif
    char json_buf[64];
    u16 json_sz = sprintf(json_buf, "{\"type\":%d,\"name\":\"%s\",\"len\":%u,\"segment\":%d}", type, name, len, pkg_size);
    u16 send_len = 0;
    send_len = set_wifi_uart_byte(send_len, CLOUD_STONAGE_TRANS_SUB_CMD_UPLOAD_START);
    send_len = set_wifi_uart_buffer(send_len, json_buf, json_sz);
    wifi_uart_write_frame(CLOUD_STRONAGE_TRANS_CMD, MCU_TX_VER, send_len);
}

/**
 * @brief Cloud storage file upload
 *
 *
 * @param offset  Package offset (Byte)
 * @param buf_ptr Pointer to data to upload
 * @param length  Data length to upload (Byte)
 */
void cloud_storage_data_upload(u32 offset, const u8* buf_ptr, u16 length) {
    u16 send_len = 0;
    send_len = set_wifi_uart_byte(send_len, CLOUD_STONAGE_TRANS_SUB_CMD_UPLOAD_DATA);
    send_len = set_wifi_uart_buffer(send_len, buf_ptr, length);
    wifi_uart_write_frame(CLOUD_STRONAGE_TRANS_CMD, MCU_TX_VER, send_len);
}

#endif

#if defined(AP_TRANSPARENT_TRANS_TEST_ENABLE)
/**
 * @brief AP transparent transmission upstream data send
 *
 *
 * @param buf_ptr Pointer to data to send
 * @param length  Data length to send (Byte)
 */
void ap_transparent_trans_upstream(const u8* buf_ptr, u16 length) {
    u16 send_len = 0;
    send_len = set_wifi_uart_byte(send_len, AP_TRANSPARENT_TRANS_TEST_SUB_CMD_UPSTREAM_DATA);
    send_len = set_wifi_uart_buffer(send_len, buf_ptr, length);
    wifi_uart_write_frame(CLOUD_STRONAGE_TRANS_CMD, MCU_TX_VER, send_len);
}
#endif

#if defined(FACTORY_RECOVERY_FUNCTION_ENABLE)
void request_factory_recovery() {
    wifi_uart_write_frame(FACTORY_RECOVERY_CMD, MCU_TX_VER, 0);
}
#endif

#if defined(HIBERNATE_ENABLE)
/**
 * @brief Request module to enter hibernate mode
 *
 */
void hibernate_enter_request(void) {
    u16 send_len = 0;
    send_len = set_wifi_uart_byte(send_len, HIBERNATE_SUB_CMD_HIBERNATE_REQUEST);
    wifi_uart_write_frame(HIBERNATE_CMD, MCU_TX_VER, send_len);
}
#endif

#if defined(SBUS_SERVICE_ENABLE)

/**
 * @brief Query soft bus state
 *
 */
void sbus_status_query(void) {
    u16 send_len = 0;
    send_len = set_wifi_uart_byte(send_len, MODULE_EXTEND_FUN_SUB_CMD_SBUS);
    send_len = set_wifi_uart_byte(send_len, 0x01);
    wifi_uart_write_frame(MODULE_EXTEND_FUN_CMD, MCU_TX_VER, send_len);
}

/**
 * @brief Soft bus send data
 *
 *
 * @param value         Pointer to data to send
 * @param value_length  Data length to send
 */
void sbus_send_data(const u8* value, u8 value_length) {
    u16 send_len = 0;
    send_len = set_wifi_uart_byte(send_len, MODULE_EXTEND_FUN_SUB_CMD_SBUS);
    send_len = set_wifi_uart_byte(send_len, 0x02);
    send_len = set_wifi_uart_byte(send_len, value_length);
    send_len = set_wifi_uart_buffer(send_len, value, (u16)value_length);
    wifi_uart_write_frame(MODULE_EXTEND_FUN_CMD, MCU_TX_VER, send_len);
}

#endif