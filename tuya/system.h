/**********************************Copyright (c)**********************************
**                     All rights reserved (C), 2015-2020, Tuya
**
**                             http://www.tuya.com
**
*********************************************************************************/
/**
 * @file    system.h
 * @author  Tuya Team
 * @version v1.0.4
 * @date    2020.5.26
 * @brief   UART data processing.The user does not need to care about what the file implements.
 */
 
 
#ifndef __SYSTEM_H_
#define __SYSTEM_H_

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
#define         PRODUCT_INFO_CMD                1                               //Product information
#define         WIFI_STATE_CMD                  2                               //Wifi working status
#define         WIFI_RESET_CMD                  3                               //Reset wifi
#define         WIFI_MODE_CMD                   4                               //Select smartconfig/AP mode
#define         STATE_UPLOAD_CMD                5                               //Real-time status report
#define         GET_LOCAL_TIME_CMD              6                               //Get local time
#define         WIFI_TEST_CMD                   7                               //Wifi function test
#define         STATE_RC_UPLOAD_CMD             8                               //Report status report
#define         DATA_QUERT_CMD                  9                               //Command issued
#define         WIFI_UPDATE_CMD                 0x0a                            //Request WIFI module firmware upgrade
#define         ROUTE_RSSI_CMD                  0x0b                            //Query the signal strength of the currently connected router
#define         REQUEST_MCU_UG_CMD              0x0c                            //Request MCU firmware upgrade
#define         UPDATE_START_CMD                0x0d                            //MCU upgrade package size notification (start of upgrade)
#define         UPDATE_TRANS_CMD                0x0e                            //Upgrade transmission
#define         GET_DP_CACHE_CMD                0x10                            //Get dp cache instruction

//=============================================================================
#define VERSION                 0x00                                            //MCU receive frame protocol version number
#define PROTOCOL_HEAD           0x07                                            //Fixed protocol header length
#define FIRM_UPDATA_SIZE        256                                             //Upgrade package size
#define FRAME_FIRST             0x55
#define FRAME_SECOND            0xaa
//============================================================================= 
SYSTEM_EXTERN unsigned char wifi_data_process_buf[PROTOCOL_HEAD + WIFI_DATA_PROCESS_LMT];         //Serial data processing buffer
SYSTEM_EXTERN unsigned char volatile wifi_uart_rx_buf[PROTOCOL_HEAD + WIFI_UART_RECV_BUF_LMT];    //Serial receive buffer
SYSTEM_EXTERN unsigned char wifi_uart_tx_buf[PROTOCOL_HEAD + WIFIR_UART_SEND_BUF_LMT];            //Serial port send buffer
SYSTEM_EXTERN volatile unsigned char *queue_in;
SYSTEM_EXTERN volatile unsigned char *queue_out;

SYSTEM_EXTERN unsigned char stop_update_flag;                                   //ENABLE:Stop all data uploads   DISABLE:Restore all data uploads

#ifndef WIFI_CONTROL_SELF_MODE
SYSTEM_EXTERN unsigned char reset_wifi_flag;                                    //Reset wifi flag (TRUE: successful / FALSE: failed)
SYSTEM_EXTERN unsigned char set_wifimode_flag;                                  //Set the WIFI working mode flag (TRUE: Success / FALSE: Failed)
SYSTEM_EXTERN unsigned char wifi_work_state;                                    //Wifi module current working status
#endif


/**
 * @brief  Write wifi uart bytes
 * @param[in] {dest} UART send buffer starts writing the address
 * @param[in] {byte} Write byte value
 * @return UART send buffer ends write address
 */
unsigned short set_wifi_uart_byte(unsigned short dest, unsigned char byte);

/**
 * @brief  Write wifi uart buffer
 * @param[in] {dest} UART send buffer starts writing the address
 * @param[in] {src} source address
 * @param[in] {len} Data length
 * @return UART send buffer ends write address
 */
unsigned short set_wifi_uart_buffer(unsigned short dest, unsigned char *src, unsigned short len);

/**
 * @brief  Send a frame of data to the wifi serial port
 * @param[in] {fr_type} Frame type
 * @param[in] {len} Data length
 * @return Null
 */
void wifi_uart_write_frame(unsigned char fr_type, unsigned short len);

/**
 * @brief  Calculate checksum
 * @param[in] {pack} Data source pointer
 * @param[in] {pack_len} Need to calculate the length of the checksum data
 * @return checksum
 */
unsigned char get_check_sum(unsigned char *pack, unsigned short pack_len);

/**
 * @brief  Data frame processing
 * @param[in] {offset} Data start position
 * @return Null
 */
void data_handle(unsigned short offset);

/**
 * @brief  Determines whether there is data in the uart receive buffer
 * @param  Null
 * @return Is there data
 */
unsigned char get_queue_total_data(void);

/**
 * @brief  Read uart receive buffer 1 byte data
 * @param  Null
 * @return Read the data
 */
unsigned char Queue_Read_Byte(void);

#endif
  
  
