/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dac.h"
#include "dma.h"
#include "hrtim.h"
#include "hrtim.h"
#include "i2c.h"
#include "usart.h"
#include "rtc.h"
#include "spi.h"
#include "tim.h"
#include "usb_otg.h"
#include "gpio.h"
#include "fmc.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define HTS221_ADDR                 (0x5F << 1)   // 7-bit 0x5F, HAL expects left-shifted address
#define HTS221_AUTOINC              0x80

#define HTS221_CTRL_REG1            0x20
#define HTS221_HUMIDITY_OUT_L       0x28
#define HTS221_H0_rH_x2             0x30
#define HTS221_H1_rH_x2             0x31
#define HTS221_H0_T0_OUT_L          0x36
#define HTS221_H1_T0_OUT_L          0x3A
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void PeriphCommonClock_Config(void);
/* USER CODE BEGIN PFP */
static void UART4_Print(const char *text);
static void UART4_PrintHumidity_x10(int32_t rh_x10);
static HAL_StatusTypeDef HTS221_WriteReg(uint8_t reg, uint8_t value);
static HAL_StatusTypeDef HTS221_ReadReg(uint8_t reg, uint8_t *value);
static HAL_StatusTypeDef HTS221_ReadRegs(uint8_t startReg, uint8_t *buffer, uint16_t len);
static int16_t HTS221_CombineInt16(uint8_t lowByte, uint8_t highByte);
static HAL_StatusTypeDef HTS221_Init(void);
static HAL_StatusTypeDef HTS221_ReadHumidity_x10(int32_t *rh_x10);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void UART4_Print(const char *text)
{
    HAL_UART_Transmit(&huart4, (uint8_t *)text, strlen(text), HAL_MAX_DELAY);
}

static void UART4_PrintHumidity_x10(int32_t rh_x10)
{
    char msg[32];
    int32_t integerPart = rh_x10 / 10;
    int32_t decimalPart = rh_x10 % 10;

    snprintf(msg, sizeof(msg), "%ld.%ld %%RH\r\n", (long)integerPart, (long)decimalPart);
    UART4_Print(msg);
}

static HAL_StatusTypeDef HTS221_WriteReg(uint8_t reg, uint8_t value)
{
    return HAL_I2C_Mem_Write(&hi2c2,
                             HTS221_ADDR,
                             reg,
                             I2C_MEMADD_SIZE_8BIT,
                             &value,
                             1,
                             HAL_MAX_DELAY);
}

static HAL_StatusTypeDef HTS221_ReadReg(uint8_t reg, uint8_t *value)
{
    return HAL_I2C_Mem_Read(&hi2c2,
                            HTS221_ADDR,
                            reg,
                            I2C_MEMADD_SIZE_8BIT,
                            value,
                            1,
                            HAL_MAX_DELAY);
}

static HAL_StatusTypeDef HTS221_ReadRegs(uint8_t startReg, uint8_t *buffer, uint16_t len)
{
    return HAL_I2C_Mem_Read(&hi2c2,
                            HTS221_ADDR,
                            (uint16_t)(startReg | HTS221_AUTOINC),
                            I2C_MEMADD_SIZE_8BIT,
                            buffer,
                            len,
                            HAL_MAX_DELAY);
}

static int16_t HTS221_CombineInt16(uint8_t lowByte, uint8_t highByte)
{
    return (int16_t)(((uint16_t)highByte << 8) | lowByte);
}

static HAL_StatusTypeDef HTS221_Init(void)
{
    uint8_t check = 0;

    if (HTS221_WriteReg(HTS221_CTRL_REG1, 0x81) != HAL_OK)
    {
        UART4_Print("ERROR: CTRL_REG1 write failed\r\n");
        return HAL_ERROR;
    }

    if (HTS221_ReadReg(HTS221_CTRL_REG1, &check) != HAL_OK)
    {
        UART4_Print("ERROR: CTRL_REG1 readback failed\r\n");
        return HAL_ERROR;
    }

    if (check != 0x81)
    {
        UART4_Print("ERROR: CTRL_REG1 wrong value\r\n");
        return HAL_ERROR;
    }

    HAL_Delay(100);
    return HAL_OK;
}

static HAL_StatusTypeDef HTS221_ReadHumidity_x10(int32_t *rh_x10)
{
    uint8_t regValue = 0;
    uint8_t buffer[2];

    uint8_t h0_rH_x2;
    uint8_t h1_rH_x2;

    int16_t h0_t0_out;
    int16_t h1_t0_out;
    int16_t h_out;

    int32_t rh0_x10;
    int32_t rh1_x10;
    int32_t denominator;
    int32_t result_x10;

    if (rh_x10 == NULL)
    {
        UART4_Print("ERROR: NULL pointer\r\n");
        return HAL_ERROR;
    }

    if (HTS221_ReadReg(HTS221_H0_rH_x2, &h0_rH_x2) != HAL_OK)
    {
        UART4_Print("ERROR: H0_rH_x2 read failed\r\n");
        return HAL_ERROR;
    }

    if (HTS221_ReadReg(HTS221_H1_rH_x2, &h1_rH_x2) != HAL_OK)
    {
        UART4_Print("ERROR: H1_rH_x2 read failed\r\n");
        return HAL_ERROR;
    }

    if (HTS221_ReadRegs(HTS221_H0_T0_OUT_L, buffer, 2) != HAL_OK)
    {
        UART4_Print("ERROR: H0_T0_OUT read failed\r\n");
        return HAL_ERROR;
    }
    h0_t0_out = HTS221_CombineInt16(buffer[0], buffer[1]);

    if (HTS221_ReadRegs(HTS221_H1_T0_OUT_L, buffer, 2) != HAL_OK)
    {
        UART4_Print("ERROR: H1_T0_OUT read failed\r\n");
        return HAL_ERROR;
    }
    h1_t0_out = HTS221_CombineInt16(buffer[0], buffer[1]);

    if (HTS221_ReadRegs(HTS221_HUMIDITY_OUT_L, buffer, 2) != HAL_OK)
    {
        UART4_Print("ERROR: HUMIDITY_OUT read failed\r\n");
        return HAL_ERROR;
    }
    h_out = HTS221_CombineInt16(buffer[0], buffer[1]);

    denominator = (int32_t)h1_t0_out - (int32_t)h0_t0_out;
    if (denominator == 0)
    {
        UART4_Print("ERROR: division by zero\r\n");
        return HAL_ERROR;
    }

    /* RH0 = H0_rH_x2 / 2, RH1 = H1_rH_x2 / 2
       For one decimal place:
       RH0_x10 = H0_rH_x2 * 5
       RH1_x10 = H1_rH_x2 * 5
    */
    rh0_x10 = (int32_t)h0_rH_x2 * 5;
    rh1_x10 = (int32_t)h1_rH_x2 * 5;

    result_x10 = rh0_x10 +
                 ((rh1_x10 - rh0_x10) * ((int32_t)h_out - (int32_t)h0_t0_out)) / denominator;

    /* minimal error handling / clipping */
    if (result_x10 < 0)
    {
        result_x10 = 0;
    }
    else if (result_x10 > 1000)
    {
        result_x10 = 1000;
    }

    *rh_x10 = result_x10;
    return HAL_OK;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

/* Configure the peripherals common clocks */
  PeriphCommonClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_ADC2_Init();
  MX_ADC3_Init();
  MX_DAC1_Init();
  MX_FMC_Init();
  MX_HRTIM_Init();
  MX_I2C2_Init();
  MX_I2C3_Init();
  MX_I2C4_Init();
  MX_LPUART1_UART_Init();
  MX_UART4_Init();
  MX_UART7_Init();
  MX_UART8_Init();
  MX_USART1_Init();
  MX_USART2_UART_Init();
  MX_RTC_Init();
  MX_SPI1_Init();
  MX_SPI2_Init();
  MX_SPI5_Init();
  MX_SPI6_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_TIM5_Init();
  MX_TIM6_Init();
  MX_TIM7_Init();
  MX_TIM8_Init();
  MX_TIM12_Init();
  MX_TIM13_Init();
  MX_TIM14_Init();
  MX_TIM15_Init();
  MX_TIM16_Init();
  MX_TIM17_Init();
  MX_USB_OTG_FS_USB_Init();
  /* USER CODE BEGIN 2 */
  if (HTS221_Init() != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    int32_t humidity_x10;

    if (HTS221_ReadHumidity_x10(&humidity_x10) == HAL_OK)
    {
      UART4_PrintHumidity_x10(humidity_x10);
    }
    else
    {
      UART4_Print("ERROR: humidity measurement failed\r\n");
    }

    HAL_Delay(1000);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}
  /** Macro to configure the PLL clock source
  */
  __HAL_RCC_PLL_PLLSOURCE_CONFIG(RCC_PLLSOURCE_HSE);
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_LSI
                              |RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 9;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOMEDIUM;
  RCC_OscInitStruct.PLL.PLLFRACN = 3072;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief Peripherals Common Clock Configuration
  * @retval None
  */
void PeriphCommonClock_Config(void)
{
  RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

  /** Initializes the peripherals clock
  */
  PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInitStruct.PLL2.PLL2M = 1;
  PeriphClkInitStruct.PLL2.PLL2N = 9;
  PeriphClkInitStruct.PLL2.PLL2P = 4;
  PeriphClkInitStruct.PLL2.PLL2Q = 2;
  PeriphClkInitStruct.PLL2.PLL2R = 2;
  PeriphClkInitStruct.PLL2.PLL2RGE = RCC_PLL2VCIRANGE_3;
  PeriphClkInitStruct.PLL2.PLL2VCOSEL = RCC_PLL2VCOMEDIUM;
  PeriphClkInitStruct.PLL2.PLL2FRACN = 3072;
  PeriphClkInitStruct.AdcClockSelection = RCC_ADCCLKSOURCE_PLL2;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
