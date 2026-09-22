/**********************************Copyright (c)**********************************
**                     All rights reserved (C), 2015-2020, Tuya
**
**                             http://www.tuya.com
**
*********************************************************************************/
/**
 * @file    protocol.c
 * @author  Tuya Team
 * @version v1.0.4
 * @date    2020.5.26
 * @brief                *******Very important, be sure to watch!!!********
 *          1. The user implements the data delivery/reporting function in this file.
 *          2. DP ID / TYPE and data processing functions require the user to implement according to the actual definition
 *          3. There are #err hints inside the function that needs the user to implement the code after starting some macro definitions. 
 *             Please delete the #err after completing the function.
 */

/******************************************************************************
                                Transplant instructions:
1:The MCU must directly call the wifi_uart_service() function in mcu_api.c in the while.
2:After the normal initialization of the program is completed, 
  it is recommended not to turn off the serial port interrupt. 
  If the interrupt must be turned off, the off interrupt time must be short, 
  and the interrupt will cause the serial port packet to be lost.
3:Do not call the escalation function in the interrupt/timer interrupt
******************************************************************************/

#include "wifi.h"

/* PORTED: the two function bodies below are the only ones in this vendor file
   that reach out of it -- one to the board, one to the application's cached DP
   values.  Everything else here is Tuya's. */
#include "bsp.h"
#include "tuya_link.h"
         
/******************************************************************************
                              The first step: initialization
1:Include "wifi.h" in files that need to use wifi related files
2:Call the wifi_protocol_init() function in the mcu_api.c file in the MCU initialization
3:Fill the MCU serial single-byte send function into the uart_transmit_output 
   function in the protocol.c file, and delete #error
4:Call the uart_receive_input function in the mcu_api.c file in the MCU serial receive 
   function and pass the received byte as a parameter.
5:The wifi_uart_service() function in the mcu_api.c file is called after the MCU enters the while loop.
******************************************************************************/

/******************************************************************************
                        1:dp data point sequence type comparison table
          **This is the automatic generation of code, such as the relevant changes in
              the development platform, please re-download MCU_SDK**
******************************************************************************/
const DOWNLOAD_CMD_S download_cmd[] =
{
  {DPID_TEMP_CURRENT, DP_TYPE_VALUE},
  {DPID_HUMIDITY_VALUE, DP_TYPE_VALUE},
  {DPID_BATTERY_STATE, DP_TYPE_ENUM},
  {DPID_BATTERY_PERCENTAGE, DP_TYPE_VALUE},
};



/******************************************************************************
                        2:Serial single-byte send function
Please fill in the MCU serial port send function into the function,
and pass the received data as a parameter to the serial port send function.
******************************************************************************/

/**
 * @brief  Send data processing
 * @param[in] {value} Serial port receives byte data
 * @return Null
 * @note   Please fill the MCU serial port sending function into this function 
 *            and pass the received data into the serial port sending function as parameters
 */
void uart_transmit_output(unsigned char value)
{
  /* PORTED: blocking single-byte send on the module's UART.  Blocking is what
     the SDK assumes -- it walks a frame byte by byte through here -- and at
     9600 baud a whole frame holds the caller for 7-16 ms.  Affordable only
     because nothing else needs the CPU while the module is powered
     (temp_sensor_main.c). */
  BSP_Wifi_TransmitByte(value);
}

/******************************************************************************
                            1:All data upload processing
The current function handles all data upload (including deliverable/reportable and report only)

  Users need to implement according to the actual situation:
  1:Need to implement the reportable/reportable data point report
  2:Need to report only reported data points
This function must be called internally by the MCU.
Users can also call this function to achieve all data upload.
******************************************************************************/

//Automated generation of data reporting functions

/**
 * @brief  All dp point information of the system is uploaded to realize APP and muc data synchronization
 * @param  Null
 * @return Null
 * @note   This function SDK needs to be called internally;
 *         The MCU must implement the data upload function in the function;
 *         including only reporting and reportable hair style data.
 */
void all_data_update(void)
{
  /* PORTED: the SDK calls this when the module asks for everything it knows
     (command 0x08), which happens after every reconnect -- and the module
     cold-boots on every report window, so it happens on every window.

     It must answer from a cache, not from the sensor: it can be called from
     inside wifi_uart_service() at any point in the window, including before
     this cycle's measurement would be ready, and Tuya's own porting notes
     forbid doing work of any length in here. */
  Tuya_ReportCachedDps();
}

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
                                       unsigned char dp_string,unsigned char v_string[],unsigned char len)
{
  unsigned short length = 0;
  
  if(stop_update_flag == ENABLE)
    return SUCCESS;
  
  //local_time
  length = set_wifi_uart_buffer(length,(unsigned char *)time,7);
  
  //bool
  length = set_wifi_uart_byte(length,dp_bool);
  length = set_wifi_uart_byte(length,DP_TYPE_BOOL);
  length = set_wifi_uart_byte(length,0);
  length = set_wifi_uart_byte(length,1);
  if(v_bool == FALSE)
  {
    length = set_wifi_uart_byte(length,FALSE);
  }
  else
  {
    length = set_wifi_uart_byte(length,1);
  }
  //enum
  length = set_wifi_uart_byte(length,dp_enum);
  length = set_wifi_uart_byte(length,DP_TYPE_ENUM);
  length = set_wifi_uart_byte(length,0);
  length = set_wifi_uart_byte(length,1);
  length = set_wifi_uart_byte(length,v_enum);
  //value
  length = set_wifi_uart_byte(length,dp_value);
  length = set_wifi_uart_byte(length,DP_TYPE_VALUE);
  length = set_wifi_uart_byte(length,0);
  length = set_wifi_uart_byte(length,4);
  length = set_wifi_uart_byte(length,v_value >> 24);
  length = set_wifi_uart_byte(length,v_value >> 16);
  length = set_wifi_uart_byte(length,v_value >> 8);
  length = set_wifi_uart_byte(length,v_value & 0xff);
  //string
  length = set_wifi_uart_byte(length,dp_string);
  length = set_wifi_uart_byte(length,DP_TYPE_STRING);
  length = set_wifi_uart_byte(length,len / 0x100);
  length = set_wifi_uart_byte(length,len % 0x100);
  length = set_wifi_uart_buffer(length,(unsigned char *)v_string,len);
  
  wifi_uart_write_frame(STATE_RC_UPLOAD_CMD,length);
  
  return SUCCESS;
}

/******************************************************************************
                                WARNING!!!    
                            2:All data upload processing
Automate code template functions, please implement data processing by yourself
******************************************************************************/



/******************************************************************************
                                WARNING!!!                     
Users of this part of the function should not modify!!
******************************************************************************/

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
unsigned char dp_download_handle(unsigned char dpid,const unsigned char value[], unsigned short length)
{
    /*********************************
    Current function processing can issue/report data calls                    
    Need to implement the data processing in the specific function
    The result of the processing needs to be fed back to the APP, otherwise the APP will consider the delivery failure.
    ***********************************/
    /* PORTED: was `unsigned char ret;`, returned uninitialised.  Every DP on
       this product is report-only, so the switch below has no cases and the
       default path is the only path -- it returned whatever was on the stack,
       and a stray SUCCESS would have the SDK acknowledge a delivery that never
       happened. */
    unsigned char ret = ERROR;
    switch(dpid)
    {

        default:
        break;
    }
    return ret;
}

/**
 * @brief  Get the sum of all dp commands
 * @param[in] Null
 * @return Sent the sum of the commands
 * @note   The function user cannot modify
 */
unsigned char get_download_cmd_total(void)
{
    return(sizeof(download_cmd) / sizeof(download_cmd[0]));
}


/******************************************************************************
                                WARNING!!!                     
This code is called internally by the SDK. 
Please implement the internal data of the function according to the actual dp data.
******************************************************************************/
#ifdef SUPPORT_MCU_RTC_CHECK
/**
 * @brief  MCU proofreads local RTC clock
 * @param[in] {time} Get the time data
 * @return Null
 * @note   MCU needs to implement this function by itself
 */
void mcu_write_rtctime(unsigned char time[])
{
  #error "Please complete the RTC clock write code yourself and delete the line"
  /*
  time[0] is the time success flag, 0 is a failure, and 1 is a success.
  time[1] is the year and 0x00 is the year 2000.
  time[2] is the month, starting from 1 to ending at 12
  time[3] is the date, starting from 1 to 31
  time[4] is the clock, starting from 0 to ending at 23
  time[5] is minutes, starting from 0 to ending at 59
  time[6] is seconds, starting from 0 to ending at 59
  time[7] is the week, starting from 1 to 7 and 1 is Monday.
 */
  if(time[0] == 1)
  {
    //Correctly receive the local clock data returned by the wifi module
	 
  }
  else
  {
  	//Error getting local clock data, it may be that the current wifi module is not connected
  }
}
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
void wifi_test_result(unsigned char result,unsigned char rssi)
{
  //#error "Please implement the wifi function test success/failure code by yourself. Please delete the line after completion"
  if(result == 0)
  {
    //Test failed
    if(rssi == 0x00)
    {
      //Can't scan to the router named tuya_mdev_test, please check
    }
    else if(rssi == 0x01)
    {
      //Module not authorized
    }
  }
  else
  {
    //Test success
    //rssiis the signal strength (0-100, 0 signal is the worst, 100 signal is the strongest)
  }
  
}
#endif

#ifdef WIFI_MODULE_UPDATE
/**
 * @brief  Request WIFI module firmware upgrade result return
 * @param[in] {result} result return
 * @return Null
 * @note   MCU needs to implement this function by itself
 */
void qur_module_ug_result(unsigned char result)
{
  #error "Please implement the Request WIFI module firmware upgrade result return code by yourself. Please delete the line after completion"
  switch(result) 
  {
    case 0:
    //Start to detect firmware update (Uninterruptible power supply)
    
      break;
    case 1:
    //Already the latest firmware (Power off)
    
      break;
    case 2:
    //Updating firmware (Uninterruptible power supply)
    
      break;
    case 3:
    //Successful firmware update (Power off)

      break;
    case 4:
    //Firmware update failed (Power off)
    
      break;
    default:
    break;
  }
}
#endif

#ifdef WIFI_QUERY_ROUTE_RSSI
/**
 * @brief  Query the signal strength of the currently connected router and return the result
 * @param[in] {result} wifi function test result; 0:failed / 1:successful
 * @param[in] {rssi} a successful test indicates wifi signal strength / a failed test indicates an error type
 * @return Null
 * @note   MCU needs to implement this function by itself
 */
void router_strenth_result(unsigned char result,unsigned char rssi)
{
  //#error "Please implement the Query the signal strength of the currently connected router and return the result code by yourself. Please delete the line after completion"
  if(result == 0)
  {
    //Test failed
    if(rssi == 0x00)
    {
        //Not connected to the route, please check
    }
    else if(rssi == 0x01)
    {
      //Module not authorized
    }
  }
  else
  {
    //Test success
    //rssiis the signal strength (0-100, 0 signal is the worst, 100 signal is the strongest)
  }
}
#endif

#ifdef SUPPORT_MCU_FIRM_UPDATE
/**
 * @brief  Request MCU firmware upgrade result return
 * @param[in] {result} result return
 * @return Null
 * @note   MCU needs to implement this function by itself
 */
void qur_ug_result(unsigned char result)
{
  #error "Please implement the Request MCU firmware upgrade result return code by yourself. Please delete the line after completion"
  switch(result) 
  {
    case 0:
    //Start to detect firmware update (Uninterruptible power supply)
    
      break;
    case 1:
    //Already the latest firmware (Power off)
    
      break;
    case 2:
    //Updating firmware (Uninterruptible power supply)
    
      break;
    case 3:
    //Successful firmware update (Power off)

      break;
    case 4:
    //Firmware update failed (Power off)
    
      break;
    default:
    break;
  }
}

/**
 * @brief  Upgrade package size selection
 * @param[in] {package_sz} Upgrade package size
 * @ref           0x00: 256byte (default)
 * @ref           0x01: 512byte
 * @ref           0x02: 1024byte
 * @return Null
 * @note   MCU needs to implement this function by itself
 */
void upgrade_package_choose(unsigned char package_sz)
{
  #error "Please implement the upgrade package size selection processing code by yourself. Please delete this line after completion"
  unsigned short length = 0;
  length = set_wifi_uart_byte(length,package_sz);
  wifi_uart_write_frame(UPDATE_START_CMD,length);
}

/**
 * @brief  MCU enters firmware upgrade mode
 * @param[in] {value} Firmware buffer
 * @param[in] {position} The current data packet is in the firmware location
 * @param[in] {length} Current firmware package length (when the firmware package length is 0, it indicates that the firmware package is sent)
 * @return Null
 * @note   MCU needs to implement this function by itself
 */
unsigned char mcu_firm_update_handle(const unsigned char value[],unsigned long position,unsigned short length)
{
  #error "Please complete the MCU firmware upgrade processing code yourself. Please delete the line after completion"
  if(length == 0)
  {
    //Firmware data transmission completed
    
  }
  else
  {
    //Firmware data processing
  }
  
  return SUCCESS;
}
#endif

