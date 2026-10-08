/**********************************Copyright (c)**********************************
**                       Copyright (C), 2015-2026, Tuya Technology
**
**                             http://www.tuya.com
**
*********************************************************************************/
/**
 * @file    wifi.h
 * @author  Tuya Integrated Protocol Development Group
 * @version v2.6.2
 * @date    2026.02.04
 * @brief   Users do not need to care about the implementation content of this file
 */

/****************************** Disclaimer ！！！ *******************************
Due to the variety of MCU types and compilation environments, this code is for reference only.
Users should ensure the final code quality themselves.
Tuya is not responsible for MCU functional results.
******************************************************************************/

#ifndef __WIFI_H_
#define __WIFI_H_

#include "stdio.h"
#include "string.h"
#include "protocol.h"
#include "system.h"
#include "mcu_api.h"
#include "tuya_type.h"

//=============================================================================
//Define constants
//=============================================================================
#ifndef TRUE
#define      TRUE                1
#endif

#ifndef FALSE
#define         FALSE            0
#endif

#ifndef NULL
#define         NULL             ((void *) 0)
#endif

#ifndef SUCCESS
#define         SUCCESS          1
#endif

#ifndef ERROR
#define         ERROR            0
#endif

#ifndef INVALID
#define         INVALID          0xFF
#endif

#ifndef ENABLE
#define         ENABLE           1
#endif

#ifndef DISABLE
#define         DISABLE          0
#endif
//=============================================================================
//DP (Data Point) types
//=============================================================================
#define         DP_TYPE_RAW                     0x00        //RAW type
#define         DP_TYPE_BOOL                    0x01        //bool type
#define         DP_TYPE_VALUE                   0x02        //value type
#define         DP_TYPE_STRING                  0x03        //string type
#define         DP_TYPE_ENUM                    0x04        //enum type
#define         DP_TYPE_BITMAP                  0x05        //fault type
#define         DP_TYPE_STRUCT                  0x06        //struct type
#define         DP_TYPE_ARRAY                   0x07        //array type

//=============================================================================
//WIFI working status
//=============================================================================
#define         SMART_CONFIG_STATE              0x00
#define         AP_STATE                        0x01
#define         WIFI_NOT_CONNECTED              0x02
#define         WIFI_CONNECTED                  0x03
#define         WIFI_CONN_CLOUD                 0x04
#define         WIFI_LOW_POWER                  0x05
#define         SMART_AND_AP_STATE              0x06
#define         WIFI_SATE_UNKNOW                0xff
//=============================================================================
//WIFI network configuration methods
//=============================================================================
#define         SMART_CONFIG                    0x0  
#define         AP_CONFIG                       0x1   

//=============================================================================
//WIFI reset status
//=============================================================================
#define         RESET_WIFI_ERROR                0
#define         RESET_WIFI_SUCCESS              1

//=============================================================================
//WIFI configuration reset status
//=============================================================================
#define         SET_WIFICONFIG_ERROR            0
#define         SET_WIFICONFIG_SUCCESS          1

//=============================================================================
//MCU firmware upgrade status
//=============================================================================
#define         FIRM_STATE_UN_SUPPORT           0x00                            //MCU upgrade not supported
#define         FIRM_STATE_WIFI_UN_READY        0x01                            //Module not ready
#define         FIRM_STATE_GET_ERROR            0x02                            //Failed to query cloud upgrade information
#define         FIRM_STATE_NO                   0x03                            //No upgrade needed (no newer version on cloud)
#define         FIRM_STATE_START                0x04                            //Upgrade needed, waiting for module to initiate upgrade operation

//=============================================================================
//Working mode between WIFI and MCU 
//=============================================================================
#define         UNION_WORK                      0x0                             //MCU module works with WIFI
#define         WIFI_ALONE                      0x1                             //WIFI module handles independently

//=============================================================================
//System working mode
//=============================================================================
#define         NORMAL_MODE                     0x00                            //Normal working status
#define         FACTORY_MODE                    0x01                            //Factory mode	
#define         UPDATE_MODE                     0x02                            //Upgrade mode	 

//=============================================================================
//Network configuration mode selection
//=============================================================================
#define         CONFIG_MODE_DEFAULT             "0"                             //Default configuration mode
#define         CONFIG_MODE_LOWPOWER            "1"                             //Low power configuration mode
#define         CONFIG_MODE_SPECIAL             "2"                             //Special configuration mode  

//=============================================================================
//Bluetooth connection status
//=============================================================================
#define         BT_STATE_UNBINDED_DISCONNECTED  0x00
#define         BT_STATE_UNBINDED_CONNECTED     0x01
#define         BT_STATE_BINDED_DISCONNECTED    0x02
#define         BT_STATE_BINDED_CONNECTED       0x03
#define         BT_STATE_UNKNOWN                0x04



#define UINT32_NTOH(buf) ((u32)((u32) * ((buf) + 0) << 24 | (u32) * ((buf) + 1) << 16 | (u32) * ((buf) + 2) << 8 | (u32) * ((buf) + 3) << 0))
#define UINT32_HTON(buf, val) ((buf)[0] = ((val) >> 24) & 0xFF, (buf)[1] = ((val) >> 16) & 0xFF, (buf)[2] = ((val) >> 8) & 0xFF, (buf)[3] = ((val) >> 0) & 0xFF)

#define UINT16_NTOH(buf) ((u16) * ((buf) + 0) << 8 | ((u16) * ((buf) + 1) << 0))
#define UINT16_HTON(buf, val) ((buf)[0] = ((val) >> 8) & 0xFF, (buf)[1] = ((val) >> 0) & 0xFF)

#if defined(CONFIG_TUYA_ENABLE_TEST)
#define TUYA_PRINT(...)      \
    do {                     \
        printf(__VA_ARGS__); \
        printf("\n");        \
    } while (0)
#else
void _tuya_debug_print(const char* fmt, ...);
#define TUYA_PRINT _tuya_debug_print
#endif

#if defined(CONFIG_TUYA_ENABLE_TEST)
#define TUYA_DBG_EXEC(expr) expr
#else
#define TUYA_DBG_EXEC(expr)
#endif

#if defined(__C51__) || defined(__CX51__)
// idk why a c complier will warn (void)unused;
#define TUYA_UNUSED(var) (var = (var))
#else
#define TUYA_UNUSED(var) ((void)(var))
#endif

#define TUYA_MAX(x, y) ((x) > (y) ? (x) : (y))
#define TUYA_MIN(x, y) ((x) < (y) ? (x) : (y))

//=============================================================================
//Download command
//=============================================================================
typedef struct {
  u8 dp_id;                              //DP index
  u8 dp_type;                            //DP type
} DOWNLOAD_CMD_S;

#endif


#if defined (CONFIG_TUYA_ENABLE_TEST)
#include <stdio.h>
#endif