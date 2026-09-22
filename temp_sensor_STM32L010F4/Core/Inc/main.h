/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l0xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

void Wifi_RailOn(void);
void Wifi_RailOff(void);

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define RF_UART hlpuart1
#define LPUART_WIFI_TX_Pin GPIO_PIN_2
#define LPUART_WIFI_TX_GPIO_Port GPIOA
#define LPUART_WIFI_RX_Pin GPIO_PIN_3
#define LPUART_WIFI_RX_GPIO_Port GPIOA
#define OUT_WIFI_ON_Pin GPIO_PIN_4
#define OUT_WIFI_ON_GPIO_Port GPIOA
#define OUT_SPARE_Pin GPIO_PIN_5
#define OUT_SPARE_GPIO_Port GPIOA
#define ADC_BATT_Pin GPIO_PIN_6
#define ADC_BATT_GPIO_Port GPIOA
#define IN_WIFI_BUTTON_Pin GPIO_PIN_7
#define IN_WIFI_BUTTON_GPIO_Port GPIOA
#define SCL_TEMP_SENS_Pin GPIO_PIN_9
#define SCL_TEMP_SENS_GPIO_Port GPIOA
#define SDA_TEMP_SENS_Pin GPIO_PIN_10
#define SDA_TEMP_SENS_GPIO_Port GPIOA

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
