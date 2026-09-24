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
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ACK_TIMEOUT_MS 200
#define MAX_RETRIES    3
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
#define RB_SIZE 64
static volatile uint8_t  rb[RB_SIZE];
static volatile uint16_t rb_head = 0;
static volatile uint16_t rb_tail = 0;
static uint8_t           rx_byte;

static volatile uint32_t link_errors = 0;
static volatile uint32_t rb_overflow = 0;

static char    line[32];
static uint8_t line_len = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void log_printf(const char *fmt, ...)
{
  char buf[96];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(buf, sizeof buf, fmt, ap);
  va_end(ap);
  if (n <= 0) return;
  if (n > (int)sizeof buf - 1) n = sizeof buf - 1;
  HAL_UART_Transmit(&huart1, (uint8_t *)buf, (uint16_t)n, 50);
}

static void link_send(const char *s)
{
  HAL_UART_Transmit(&huart2, (uint8_t *)s, (uint16_t)strlen(s), 50);
}

static void led_set(int on)
{
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, on ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2) {
    uint16_t next = (rb_head + 1) % RB_SIZE;
    if (next != rb_tail) {
      rb[rb_head] = rx_byte;
      rb_head = next;
    } else {
      rb_overflow++;
    }
    HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
  }
}


void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART2) {
    link_errors++;
    __HAL_UART_CLEAR_OREFLAG(huart);
    HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
  }
}

static bool rb_get(uint8_t *c)
{
  if (rb_tail == rb_head) return false;
  *c = rb[rb_tail];
  rb_tail = (rb_tail + 1) % RB_SIZE;
  return true;
}

static void rb_flush(void)
{
  rb_tail = rb_head;
}

static bool read_line(char *buf, size_t len, uint32_t timeout_ms)
{
  size_t n = 0;
  uint32_t start = HAL_GetTick();
  while (HAL_GetTick() - start < timeout_ms) {
    uint8_t c;
    if (!rb_get(&c)) continue;
    if (c == '\r') continue;
    if (c == '\n') { buf[n] = '\0'; return true; }
    if (n < len - 1) buf[n++] = (char)c;
  }
  buf[n] = '\0';
  return false;
}

static bool send_cmd(const char *cmd)
{
  char tx[16], expected[16], rx[32];
  snprintf(tx, sizeof tx, "%s\n", cmd);
  snprintf(expected, sizeof expected, "ACK %s", cmd);

  for (int attempt = 1; attempt <= MAX_RETRIES; attempt++) {
    rb_flush();
    link_send(tx);
    log_printf("[STM] TX -> %s (attempt %d)\r\n", cmd, attempt);

    if (read_line(rx, sizeof rx, ACK_TIMEOUT_MS) && strcmp(rx, expected) == 0) {
      log_printf("[STM] RX <- %s\r\n", rx);
      return true;
    }
    log_printf("[STM] NO ACK (got: '%s')\r\n", rx);
  }
  log_printf("[STM] ESP32 not responding after %d attempts\r\n", MAX_RETRIES);
  return false;
}

static bool button_pressed(void)
{
  return HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET;
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

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  	led_set(0);
    log_printf("\r\n[STM] boot. button PA0 -> ESP32 LED\r\n");
    HAL_UART_Receive_IT(&huart2, &rx_byte, 1);
    bool remote_led = false;
    uint32_t last_errors = 0;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
    while (1)
      {
        if (button_pressed()) {
          HAL_Delay(20);                       // debounce
          if (button_pressed()) {
            remote_led = !remote_led;
            if (send_cmd(remote_led ? "ON" : "OFF")) {
              led_set(remote_led);             // PC13 дублює стан LED на ESP
            } else {
              remote_led = !remote_led;        // відкат, ESP не підтвердив
            }
            while (button_pressed()) HAL_Delay(10);  // чекаємо відпускання
          }
        }

        if (link_errors != last_errors) {
          last_errors = link_errors;
          log_printf("[STM] UART error #%lu\r\n", (unsigned long)last_errors);
        }

        HAL_Delay(10);
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 12;
  RCC_OscInitStruct.PLL.PLLN = 96;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
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
