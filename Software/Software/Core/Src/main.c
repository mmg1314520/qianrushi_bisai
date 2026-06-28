/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "sensors.h"
#include "actuators.h"
#include "protocol.h"
#include "esp8266.h"
#include "voice_module.h"
#include "bluetooth_link.h"
#include "display_ui.h"
#include "lvgl.h"
#include "rtthread.h"
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define WIFI_LOCATION_PERIOD_MS   60000U
#define AI_ANALYSIS_PERIOD_MS     120000U

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static SensorData_t sensor_data;
static Esp8266Info_t wifi_info;
static rt_mutex_t data_mutex = RT_NULL;
static rt_mutex_t ui_mutex = RT_NULL;
static rt_mutex_t uart1_mutex = RT_NULL;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void Uart1_SendString(const char *str)
{
    if (uart1_mutex != RT_NULL) {
        rt_mutex_take(uart1_mutex, RT_WAITING_FOREVER);
    }
    HAL_UART_Transmit(&huart1, (uint8_t *)str, (uint16_t)strlen(str), 1000);
    if (uart1_mutex != RT_NULL) {
        rt_mutex_release(uart1_mutex);
    }
}

static void PrintWifiInfo(const Esp8266Info_t *info)
{
    if (info->wifi_ok) {
        Uart1_SendString("WIFI:OK\r\n");
    } else {
        Uart1_SendString("WIFI:FAIL\r\n");
    }

    if (info->time_ok) {
        Uart1_SendString("TIME:");
        Uart1_SendString(info->time);
        Uart1_SendString("\r\n");
    } else {
        Uart1_SendString("TIME:FAIL\r\n");
    }

    if (info->location_ok) {
        Uart1_SendString("LOC:");
        Uart1_SendString(info->location);
        if (info->longitude[0] != '\0' && info->latitude[0] != '\0') {
            Uart1_SendString(" LNG:");
            Uart1_SendString(info->longitude);
            Uart1_SendString(" LAT:");
            Uart1_SendString(info->latitude);
        }
        Uart1_SendString("\r\n");
    } else {
        Uart1_SendString("LOC:FAIL\r\n");
    }

    if (info->weather_ok) {
        Uart1_SendString("WEATHER:");
        Uart1_SendString(info->weather_text);
        if (info->weather_temp[0] != '\0') {
            Uart1_SendString(" TEMP:");
            Uart1_SendString(info->weather_temp);
            Uart1_SendString("C");
        }
        if (info->weather_humidity[0] != '\0') {
            Uart1_SendString(" RH:");
            Uart1_SendString(info->weather_humidity);
            Uart1_SendString("%");
        }
        if (info->weather_wind[0] != '\0') {
            Uart1_SendString(" WIND:");
            Uart1_SendString(info->weather_wind);
        }
        Uart1_SendString("\r\n");
    } else {
        Uart1_SendString("WEATHER:FAIL\r\n");
    }
}

static void PrintClockAndSensors(void)
{
    SensorData_t data_snapshot;
    Esp8266Info_t wifi_snapshot;

    if (data_mutex != RT_NULL) {
        rt_mutex_take(data_mutex, RT_WAITING_FOREVER);
    }
    data_snapshot = sensor_data;
    wifi_snapshot = wifi_info;
    if (data_mutex != RT_NULL) {
        rt_mutex_release(data_mutex);
    }

    if (uart1_mutex != RT_NULL) {
        rt_mutex_take(uart1_mutex, RT_WAITING_FOREVER);
    }
    BluetoothLink_SendTelemetry(&data_snapshot, &wifi_snapshot);
    if (uart1_mutex != RT_NULL) {
        rt_mutex_release(uart1_mutex);
    }
}

static uint8_t WifiInfoIsComplete(const Esp8266Info_t *info)
{
    return (info != NULL &&
            info->wifi_ok &&
            info->time_ok &&
            info->location_ok &&
            info->weather_ok) ? 1U : 0U;
}

static void SyncWifiInfo(void)
{
    Esp8266Info_t new_info;

    Uart1_SendString("WIFI AUTO SYNC START\r\n");
    if (ui_mutex != RT_NULL) {
        rt_mutex_take(ui_mutex, RT_WAITING_FOREVER);
    }
    DisplayUI_ShowStatus("WiFi sync...");
    if (ui_mutex != RT_NULL) {
        rt_mutex_release(ui_mutex);
    }

    if (Esp8266_Sync(&new_info)) {
        if (data_mutex != RT_NULL) {
            rt_mutex_take(data_mutex, RT_WAITING_FOREVER);
        }
        new_info.ai_ok = wifi_info.ai_ok;
        memcpy(new_info.ai_advice, wifi_info.ai_advice, sizeof(new_info.ai_advice));
        memcpy(new_info.ai_error, wifi_info.ai_error, sizeof(new_info.ai_error));
        memcpy(&wifi_info, &new_info, sizeof(wifi_info));
        if (data_mutex != RT_NULL) {
            rt_mutex_release(data_mutex);
        }

        PrintWifiInfo(&new_info);
        Uart1_SendString("SYNC:PRINT DONE\r\n");
        if (ui_mutex != RT_NULL) {
            rt_mutex_take(ui_mutex, RT_WAITING_FOREVER);
        }
        DisplayUI_ShowStatus(WifiInfoIsComplete(&new_info) ? "WiFi data OK" : "WiFi partial");
        if (ui_mutex != RT_NULL) {
            rt_mutex_release(ui_mutex);
        }
        Uart1_SendString("SYNC:STATUS DONE\r\n");
    } else {
        Uart1_SendString("WIFI AUTO SYNC FAIL\r\nERR:");
        Uart1_SendString(Esp8266_GetLastError());
        Uart1_SendString("\r\n");
        if (ui_mutex != RT_NULL) {
            rt_mutex_take(ui_mutex, RT_WAITING_FOREVER);
        }
        DisplayUI_ShowStatus("WiFi sync FAIL");
        if (ui_mutex != RT_NULL) {
            rt_mutex_release(ui_mutex);
        }
    }
}

#if 0
static void AnalyzeCropWithDoubao(void)
{
    SensorData_t data_snapshot;
    Esp8266Info_t wifi_snapshot;
    char advice[96] = {0};
    uint8_t ok;

    if (data_mutex != RT_NULL) {
        rt_mutex_take(data_mutex, RT_WAITING_FOREVER);
    }
    data_snapshot = sensor_data;
    wifi_snapshot = wifi_info;
    if (data_mutex != RT_NULL) {
        rt_mutex_release(data_mutex);
    }

    if (!wifi_snapshot.wifi_ok) {
        if (data_mutex != RT_NULL) {
            rt_mutex_take(data_mutex, RT_WAITING_FOREVER);
        }
        wifi_info.ai_ok = 0U;
        wifi_info.ai_advice[0] = '\0';
        strncpy(wifi_info.ai_error, "WIFI_FAIL", sizeof(wifi_info.ai_error) - 1U);
        wifi_info.ai_error[sizeof(wifi_info.ai_error) - 1U] = '\0';
        if (data_mutex != RT_NULL) {
            rt_mutex_release(data_mutex);
        }
        Uart1_SendString("DOUBAO:SKIP WIFI_FAIL\r\n");
        return;
    }

    Uart1_SendString("DOUBAO:ANALYZE START\r\n");
    ok = Esp8266_AnalyzeCrop(&data_snapshot, &wifi_snapshot, advice, sizeof(advice));

    if (data_mutex != RT_NULL) {
        rt_mutex_take(data_mutex, RT_WAITING_FOREVER);
    }
    wifi_info.ai_ok = ok;
    if (ok) {
        strncpy(wifi_info.ai_advice, advice, sizeof(wifi_info.ai_advice) - 1U);
        wifi_info.ai_advice[sizeof(wifi_info.ai_advice) - 1U] = '\0';
        wifi_info.ai_error[0] = '\0';
    } else {
        wifi_info.ai_advice[0] = '\0';
        strncpy(wifi_info.ai_error, Esp8266_GetLastError(), sizeof(wifi_info.ai_error) - 1U);
        wifi_info.ai_error[sizeof(wifi_info.ai_error) - 1U] = '\0';
    }
    if (data_mutex != RT_NULL) {
        rt_mutex_release(data_mutex);
    }

    if (ok) {
        Uart1_SendString("DOUBAO:OK ");
        Uart1_SendString(advice);
        Uart1_SendString("\r\n");
    } else {
        Uart1_SendString("DOUBAO:FAIL ");
        Uart1_SendString(Esp8266_GetLastError());
        Uart1_SendString("\r\n");
    }
}
#endif

static void SensorThreadEntry(void *parameter)
{
    SensorData_t data;

    RT_UNUSED(parameter);
    while (1) {
        Sensors_ReadAll(&data);
        if (data_mutex != RT_NULL) {
            rt_mutex_take(data_mutex, RT_WAITING_FOREVER);
        }
        sensor_data = data;
        if (data_mutex != RT_NULL) {
            rt_mutex_release(data_mutex);
        }

        PrintClockAndSensors();
        rt_thread_mdelay(1000);
    }
}

static void WifiThreadEntry(void *parameter)
{
    uint32_t elapsed_ms = 0U;

    RT_UNUSED(parameter);
    rt_thread_mdelay(1500);
    SyncWifiInfo();

    while (1) {
        rt_thread_mdelay(1000);
        elapsed_ms += 1000U;

        if (elapsed_ms % WIFI_LOCATION_PERIOD_MS == 0U) {
            SyncWifiInfo();
        }
    }
}

static void ProtocolThreadEntry(void *parameter)
{
    RT_UNUSED(parameter);
    while (1) {
        Protocol_Process();
        rt_thread_mdelay(10);
    }
}

static void VoiceThreadEntry(void *parameter)
{
    SensorData_t data_snapshot;
    Esp8266Info_t wifi_snapshot;

    RT_UNUSED(parameter);
    while (1) {
        if (data_mutex != RT_NULL) {
            rt_mutex_take(data_mutex, RT_WAITING_FOREVER);
        }
        data_snapshot = sensor_data;
        wifi_snapshot = wifi_info;
        if (data_mutex != RT_NULL) {
            rt_mutex_release(data_mutex);
        }

        VoiceModule_Process(&data_snapshot, &wifi_snapshot);
        rt_thread_mdelay(10);
    }
}

static void DisplayThreadEntry(void *parameter)
{
    SensorData_t data_snapshot;
    Esp8266Info_t wifi_snapshot;
    uint32_t last_update_tick = 0U;

    RT_UNUSED(parameter);
    while (1) {
        if (HAL_GetTick() - last_update_tick >= 1000U) {
            last_update_tick = HAL_GetTick();
            if (data_mutex != RT_NULL) {
                rt_mutex_take(data_mutex, RT_WAITING_FOREVER);
            }
            data_snapshot = sensor_data;
            wifi_snapshot = wifi_info;
            if (data_mutex != RT_NULL) {
                rt_mutex_release(data_mutex);
            }

            if (ui_mutex != RT_NULL) {
                rt_mutex_take(ui_mutex, RT_WAITING_FOREVER);
            }
            DisplayUI_Update(&data_snapshot, &wifi_snapshot);
            if (ui_mutex != RT_NULL) {
                rt_mutex_release(ui_mutex);
            }
        }

        if (ui_mutex != RT_NULL) {
            rt_mutex_take(ui_mutex, RT_WAITING_FOREVER);
        }
        DisplayUI_Process();
        if (ui_mutex != RT_NULL) {
            rt_mutex_release(ui_mutex);
        }
        rt_thread_mdelay(5);
    }
}

static void ActuatorThreadEntry(void *parameter)
{
    RT_UNUSED(parameter);
    while (1) {
        Buzzer_Process();
        rt_thread_mdelay(10);
    }
}

static void CreateAppThread(const char *name,
                            void (*entry)(void *parameter),
                            rt_uint32_t stack_size,
                            rt_uint8_t priority,
                            rt_uint32_t tick)
{
    rt_thread_t tid;

    tid = rt_thread_create(name, entry, RT_NULL, stack_size, priority, tick);
    if (tid != RT_NULL) {
        rt_thread_startup(tid);
    } else {
        Uart1_SendString("THREAD CREATE FAIL:");
        Uart1_SendString(name);
        Uart1_SendString("\r\n");
    }
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

  /* MCU Configuration--------------------------------------------------------*/

  /* HAL and system clock are initialized in rt_hw_board_init(). */

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* System clock is already configured before the RT-Thread scheduler starts. */

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_SPI2_Init();
  MX_TIM4_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  MX_USART6_UART_Init();
  /* USER CODE BEGIN 2 */
  data_mutex = rt_mutex_create("datam", RT_IPC_FLAG_PRIO);
  ui_mutex = rt_mutex_create("uim", RT_IPC_FLAG_PRIO);
  uart1_mutex = rt_mutex_create("u1m", RT_IPC_FLAG_PRIO);
  Actuators_Init();
  Sensors_Init();
  DisplayUI_Init();
  Esp8266_Init();
  Protocol_Init();
  VoiceModule_Init();
  HAL_UART_Transmit(&huart1, (uint8_t *)"START\r\n", 7, 1000);
  Uart1_SendString("RT-Thread Nano start\r\n");
  Uart1_SendString("VOICE:USART6 PC6 TX PC7 RX\r\n");
  CreateAppThread("sensor", SensorThreadEntry, 2048, 12, 10);
  CreateAppThread("wifi", WifiThreadEntry, 4096, 16, 10);
  CreateAppThread("proto", ProtocolThreadEntry, 1536, 11, 10);
  CreateAppThread("voice", VoiceThreadEntry, 2048, 13, 10);
  CreateAppThread("disp", DisplayThreadEntry, 4096, 14, 10);
  CreateAppThread("act", ActuatorThreadEntry, 1024, 10, 10);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    rt_thread_mdelay(1000);
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
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
#ifdef USE_FULL_ASSERT
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
