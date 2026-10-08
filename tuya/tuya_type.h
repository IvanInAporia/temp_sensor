/**********************************Copyright (c)**********************************
**                       Copyright (C), 2015-2026, Tuya Technology
**
**                             http://www.tuya.com
**
*********************************************************************************/
/**
 * @file    tuya_type.h
 * @author  Tuya Comprehensive Protocol Development Group
 * @version v2.6.2
 * @date    2026.02.04
 * @brief   Users do not need to care about the implementation content of this file
 */

/****************************** Disclaimer !!! *******************************
Due to the variety of MCU types and compilation environments, this code is for reference only.
Users are responsible for the final code quality.
Tuya is not responsible for the functional results of the MCU.
******************************************************************************/

#ifndef  __TUYA_TYPE_H__
#define  __TUYA_TYPE_H__

#ifdef __cplusplus
extern "C" {
#endif

#if defined (__IAR_SYSTEMS_ICC__) 
    #define VIRTUAL_FUNC __weak
#else 
    #define VIRTUAL_FUNC __attribute__((weak))
#endif

#ifndef IN
#define IN
#endif

#ifndef OUT
#define OUT
#endif

#ifndef INOUT
#define INOUT
#endif

#ifndef STATIC
#define STATIC static
#endif

#ifndef CONST
#define CONST const
#endif

#ifndef SIZEOF
#define SIZEOF sizeof
#endif

#ifndef INLINE
#define INLINE inline
#endif


#ifndef NULL
    #ifdef __cplusplus
    #define NULL 0
    #else
    #define NULL ((void *)0)
    #endif
#endif


#ifndef bool_t
typedef unsigned char   bool_t;
#endif

#ifndef i8
typedef signed char   i8;
#endif

#ifndef u8
typedef unsigned char   u8;
#endif

#ifndef i16
typedef signed short   i16;
#endif

#ifndef u16
typedef unsigned short   u16;
#endif

#ifndef i32
typedef signed int  i32;
#endif

#ifndef u32
typedef unsigned int u32;
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#if defined(CONFIG_MCU_SDK_TEST_ONLY_USE_NEW_DISPATCHER)

typedef u8 PROCESS_BUFFER_OVERFLOW_HANDLE_METHOD_E;
#define PROCESS_BUFFER_OVERFLOW_HANDLE_METHOD_DISCARD_WHOLE_COMMAND          0x00   // Discard data of the claimed command length without processing
#define PROCESS_BUFFER_OVERFLOW_HANDLE_METHOD_DISCARD_CURRENT_AND_RX_BUFFER  0x01   // Discard currently received data and data in the receive buffer
#define PROCESS_BUFFER_OVERFLOW_HANDLE_METHOD_DISCARD_CURRENT                0x02   // Discard only currently received data

#endif

#ifdef __cplusplus
}
#endif 

#endif