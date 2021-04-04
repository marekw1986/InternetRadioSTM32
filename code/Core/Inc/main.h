/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
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
#include "stm32f1xx_hal.h"

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

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define VS_XRST_Pin GPIO_PIN_13
#define VS_XRST_GPIO_Port GPIOC
#define USB_EN_Pin GPIO_PIN_0
#define USB_EN_GPIO_Port GPIOC
#define USB_OVERCURRENT_Pin GPIO_PIN_5
#define USB_OVERCURRENT_GPIO_Port GPIOA
#define TST_Pin GPIO_PIN_10
#define TST_GPIO_Port GPIOB
#define SPIRAM_CS_Pin GPIO_PIN_15
#define SPIRAM_CS_GPIO_Port GPIOA
#define SD_PRESENT_Pin GPIO_PIN_2
#define SD_PRESENT_GPIO_Port GPIOD
#define VS_XDCS_Pin GPIO_PIN_6
#define VS_XDCS_GPIO_Port GPIOB
#define VS_XCS_Pin GPIO_PIN_7
#define VS_XCS_GPIO_Port GPIOB
#define VS_DREQ_Pin GPIO_PIN_8
#define VS_DREQ_GPIO_Port GPIOB
#define SD_CS_Pin GPIO_PIN_9
#define SD_CS_GPIO_Port GPIOB
/* USER CODE BEGIN Private defines */
#define SD_SPI_HANDLE hspi3
/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
