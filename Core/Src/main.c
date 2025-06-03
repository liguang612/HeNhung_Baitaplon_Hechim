/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stdio.h"
#include "stdbool.h"
#include "string.h"

#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_uart.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct {
  /*
   * 1: SET BAUDRATE FOR A AND B
   * -1: Unknown
  */
  int type;
  int baudrate;
} command;

typedef struct {
  uint32_t baud_A;
  uint32_t baud_B;
  uint32_t magic_number; // Preset number for calculate checksum
  uint32_t checksum;
} UART_BaudRates_Config;
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define RX_BUFFER_SIZE 64

#define CONFIG_FLASH_SECTOR        FLASH_SECTOR_23
#define CONFIG_FLASH_BASE_ADDRESS  0x081E0000
#define CONFIG_MAGIC_NUMBER        0xCAFEFECA
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart3;
UART_HandleTypeDef huart6;
DMA_HandleTypeDef hdma_usart1_rx;
DMA_HandleTypeDef hdma_usart3_rx;
DMA_HandleTypeDef hdma_usart6_rx;

/* USER CODE BEGIN PV */
uint8_t rx_buffer_it1[RX_BUFFER_SIZE];
uint8_t rx_buffer_it3[RX_BUFFER_SIZE];
uint8_t rx_buffer_it6[RX_BUFFER_SIZE];

uint16_t buffer_size_it = 1;

uint16_t received_data_len;

char instructions[] = "----------\nValid commands:\n1. Set baud rate: BAUD <baud_rate> (valid baud rate: 600, 1200, 2400, 4800, 9600, 14400, 19200, 38400, 56000, 57600, 115200)\n----------\n";
int VALID_RATE[11] = { 600, 1200, 2400, 4800, 9600, 14400, 19200, 38400, 56000, 57600, 115200 };
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_USART6_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/*
 * Get the number from a string
 * 	  valid string: return the converted number
 * 	  invalid string: return -1
 * */
int extractNumber(char* str) {
  int res = 0;

  for (int i = 0; i < strlen(str); i++) {
    if (str[i] > '9' || str[i] < '0') return -1;
    res = res * 10 + str[i] - '0';
  }

  return res;
}

/*
 * Check the number in an array or not.
 * */
bool includes(int* arr, int mem, int len) {
  for (int i = 0; i < len; i++) {
    if (arr[i] == mem) return true;
  }
  return false;
}

/*
 * Extract the command name from a string and parameters, return an [command]
 * 		Valid command: BAUD <baud_rate>
 * */
command extractCommand(char* cmd) {
  command res;
  res.type = -1;

  int sz = strlen(cmd);
  for (int i = 0; i < sz; i++) {
    if (cmd[i] == ' ') {
      cmd[i] = '\0'; // Put a latch

      if (!strcmp(cmd, "BAUD")) {
        int baudrate = extractNumber(cmd + i + 1);
        if (includes(VALID_RATE, baudrate, 11)) {
          res.baudrate = baudrate;
          res.type = 1;
        }
      }
      else {
        break;
      }

      cmd[i] = ' ';
    }
  }

  return res;
}


/*
 * Calculate the checksum base on data from Flash memory.
 * */
uint32_t Calculate_Checksum(UART_BaudRates_Config* config) {
  return config->baud_A ^ config->baud_B ^ config->magic_number;
}

/*
 * Save the configuration (baud rate) to the flash memorys
 * */
void Save_Config_To_Flash(UART_BaudRates_Config* config_to_save) {
  HAL_StatusTypeDef status;
  FLASH_EraseInitTypeDef EraseInitStruct;
  uint32_t SectorError = 0;
  uint32_t address = CONFIG_FLASH_BASE_ADDRESS;

  // 1. Mở khóa Flash
  HAL_FLASH_Unlock();

  // 2. Xóa Sector
  EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
  EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3; // Cho VCC từ 2.7V đến 3.6V
  EraseInitStruct.Sector = CONFIG_FLASH_SECTOR;
  EraseInitStruct.NbSectors = 1;

  status = HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError);

  // 2.1. Xoá không thành công | địa chỉ bộ nhớ không khớp với quy ước [CONFIG_FLASH_SECTOR] thì báo lỗi và khoá Flash
  if (status != HAL_OK || SectorError != 0xFFFFFFFFU) {
    char err_msg[50];
    sprintf(err_msg, "Flash Erase Error: 0x%lX, SE: 0x%lX\r\n", (unsigned long)status, (unsigned long)SectorError);
    HAL_UART_Transmit(&huart1, (uint8_t*)err_msg, strlen(err_msg), HAL_MAX_DELAY);

    HAL_FLASH_Lock();
    return;
  }

  // 3. Ghi dữ liệu (ghi từng word 32-bit)
  config_to_save->checksum = Calculate_Checksum(config_to_save);

  // 3.1. Thứ tự lưu giá trị: <baudA><baudB><magicNumber><Checksum>
  if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, config_to_save->baud_A) != HAL_OK) goto flash_program_error;
  address += 4;
  if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, config_to_save->baud_B) != HAL_OK) goto flash_program_error;
  address += 4;
  if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, config_to_save->magic_number) != HAL_OK) goto flash_program_error;
  address += 4;
  if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address, config_to_save->checksum) != HAL_OK) goto flash_program_error;

  // 4. Khóa Flash
  HAL_FLASH_Lock();

  char ok_msg[] = "Config saved to Flash successfully.\r\n";
  HAL_UART_Transmit(&huart1, (uint8_t*)ok_msg, strlen(ok_msg), HAL_MAX_DELAY);
  return;

  // 3.2. Nếu có lỗi trong việc ghi thì báo lỗi lên UART_H
flash_program_error:
  HAL_UART_Transmit(&huart1, (uint8_t*)"Flash Program Error!\r\n", strlen("Flash Program Error!\r\n"), HAL_MAX_DELAY);
  HAL_FLASH_Lock();
}

void Load_Config_From_Flash(void) {
  UART_BaudRates_Config loaded_config;
  uint32_t address = CONFIG_FLASH_BASE_ADDRESS;
  char msg[100];

  // Thứ tự lưu giá trị: <baudA><baudB><magicNumber><Checksum>
  loaded_config.baud_A = *(volatile uint32_t*)address;
  loaded_config.baud_B = *(volatile uint32_t*)(address + 4);
  loaded_config.magic_number = *(volatile uint32_t*)(address + 8);
  loaded_config.checksum = *(volatile uint32_t*)(address + 12);

  bool status = true;
  // 1. Kiểm tra dữ liệu được load từ flash là dữ liệu hợp lệ
  if (loaded_config.magic_number == CONFIG_MAGIC_NUMBER &&
    loaded_config.checksum == Calculate_Checksum(&loaded_config)) {

    sprintf(msg, "Valid config found in Flash. A=%lu, B=%lu. Applying...\r\n", (unsigned long)loaded_config.baud_A, (unsigned long)loaded_config.baud_B);
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);

    // 1.1. Re-init UART_A & UART_B với baud rate load được từ flash
    if (status && huart3.Init.BaudRate != loaded_config.baud_A) {
      if (status && HAL_UART_DeInit(&huart3) != HAL_OK) status = false;
      huart3.Init.BaudRate = loaded_config.baud_A;
      if (status && HAL_UART_Init(&huart3) != HAL_OK) status = false;
    }

    if (status && huart6.Init.BaudRate != loaded_config.baud_B) {
      if (status && HAL_UART_DeInit(&huart6) != HAL_OK) status = false;
      huart6.Init.BaudRate = loaded_config.baud_B;
      if (status && HAL_UART_Init(&huart6) != HAL_OK) status = false;
    }
  }
  else {
    // 1.2 Dữ liệu không hợp lệ || Flash chưa được ghi lần nào
    sprintf(msg, "No valid config in Flash or data corrupted. Using default baud rates (A=%lu, B=%lu).\r\n", (unsigned long)huart3.Init.BaudRate, (unsigned long)huart6.Init.BaudRate);
    HAL_UART_Transmit(&huart1, (uint8_t*)msg, strlen(msg), HAL_MAX_DELAY);

    UART_BaudRates_Config default_config;
    default_config.baud_A = huart3.Init.BaudRate;
    default_config.baud_B = huart6.Init.BaudRate;
    default_config.magic_number = CONFIG_MAGIC_NUMBER;

    Save_Config_To_Flash(&default_config);
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
  MX_DMA_Init();
  MX_USART1_UART_Init();
  MX_USART3_UART_Init();
  MX_USART6_UART_Init();
  /* USER CODE BEGIN 2 */
  Load_Config_From_Flash();

  // Start DMA lắng nghe các sự kiện nhận dữ liệu
  if (HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_buffer_it1, RX_BUFFER_SIZE) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_ReceiveToIdle_DMA(&huart3, rx_buffer_it3, RX_BUFFER_SIZE) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_ReceiveToIdle_DMA(&huart6, rx_buffer_it6, RX_BUFFER_SIZE) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    //	  HAL_UART_Transmit(&huart3, (const uint8_t*)buf, strlen(buf), 2);
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
  RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
  RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

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
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 180;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
    | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV4;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief USART6 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART6_UART_Init(void)
{

  /* USER CODE BEGIN USART6_Init 0 */

  /* USER CODE END USART6_Init 0 */

  /* USER CODE BEGIN USART6_Init 1 */

  /* USER CODE END USART6_Init 1 */
  huart6.Instance = USART6;
  huart6.Init.BaudRate = 115200;
  huart6.Init.WordLength = UART_WORDLENGTH_8B;
  huart6.Init.StopBits = UART_STOPBITS_1;
  huart6.Init.Parity = UART_PARITY_NONE;
  huart6.Init.Mode = UART_MODE_TX_RX;
  huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart6.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart6) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART6_Init 2 */

  /* USER CODE END USART6_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);
  /* DMA2_Stream1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream1_IRQn);
  /* DMA2_Stream2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream2_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream2_IRQn);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef* huart, uint16_t Size)
{
  // Ký tự xuống dòng (chỉ nhằm mục đích trình bày đẹp hơn
  char end_char[] = "\r\n";

  if (huart->Instance == USART1) {
    // 1. Nhận dữ liệu
    HAL_UART_Transmit(&huart1, (uint8_t*)end_char, strlen(end_char), HAL_MAX_DELAY);

    // 2. Ngắt chuỗi và giải mã command từ xâu người dùng nhập
    rx_buffer_it1[Size] = '\0';
    command cmd = extractCommand((char*)rx_buffer_it1);

    if (cmd.type == -1) {
      // 3.1. Nếu lỗi thì báo lên UART_H
      HAL_UART_Transmit(&huart1, (uint8_t*)instructions, strlen(instructions), HAL_MAX_DELAY);
    }
    else if (cmd.type == 1) {
      // 3.2. Xử lý set baud rate cho UART_A & UART_B
      bool status = true;

      char info[] = "CMD: Setting Baud rate for UART_A & UART_B at baud rate: ";
      HAL_UART_Transmit(&huart1, (uint8_t*)info, strlen(info), HAL_MAX_DELAY);

      sprintf(info, "%d\r\n", cmd.baudrate);
      HAL_UART_Transmit(&huart1, (uint8_t*)info, strlen(info), HAL_MAX_DELAY);

      // 4. Stop DMA
      if (status && HAL_UART_DMAStop(&huart3) != HAL_OK) {
        status = false;
      }
      if (status && HAL_UART_DMAStop(&huart6) != HAL_OK) {
        status = false;
      }

      // 5. De-init UART
      if (status && HAL_UART_DeInit(&huart3) != HAL_OK) {
        status = false;
      }
      if (status && HAL_UART_DeInit(&huart6) != HAL_OK) {
        status = false;
      }

      // 6. Re-init UART_A & UART_B
      if (status) (&huart3)->Init.BaudRate = cmd.baudrate;
      if (status) (&huart6)->Init.BaudRate = cmd.baudrate;

      // Re-init UART
      if (status && HAL_UART_Init(&huart3) != HAL_OK) {
        status = false;
      }
      if (status && HAL_UART_Init(&huart6) != HAL_OK) {
        status = false;
      }

      // 7. Restart DMA
      if (status && HAL_UARTEx_ReceiveToIdle_DMA(&huart3, rx_buffer_it3, RX_BUFFER_SIZE) != HAL_OK) {
        status = false;
      }
      if (status && HAL_UARTEx_ReceiveToIdle_DMA(&huart6, rx_buffer_it6, RX_BUFFER_SIZE) != HAL_OK) {
        status = false;
      }

      // Thông báo lên UART_H
      if (status) {
        sprintf(info, "Set baud rate at %d successfully!\r\n", cmd.baudrate);
        HAL_UART_Transmit(&huart1, (uint8_t*)info, strlen(info), HAL_MAX_DELAY);

        UART_BaudRates_Config new_config;
        new_config.baud_A = cmd.baudrate;
        new_config.baud_B = cmd.baudrate;
        new_config.magic_number = CONFIG_MAGIC_NUMBER;

        Save_Config_To_Flash(&new_config);
      }
      else {
        sprintf(info, "Error occur!\r\n");
        HAL_UART_Transmit(&huart1, (uint8_t*)info, strlen(info), HAL_MAX_DELAY);
      }
    }

    // IMPORTANT: Restart DMA để tiếp tục lắng nghe lệnh mới
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_buffer_it1, RX_BUFFER_SIZE);
  }
  else if (huart->Instance == USART3) {
    received_data_len = Size;

    // 1. Ngắt dòng
    HAL_UART_Transmit(&huart3, (uint8_t*)end_char, strlen(end_char), HAL_MAX_DELAY);

    // 2. Gửi tin nhắn cho UART_B
    HAL_UART_Transmit(&huart6, rx_buffer_it3, received_data_len, HAL_MAX_DELAY);
    HAL_UART_Transmit(&huart6, (uint8_t*)end_char, strlen(end_char), HAL_MAX_DELAY);

    // 3. Gửi thông tin giám sát lên UART_H
    char info[] = "A -> B: ";
    HAL_UART_Transmit(&huart1, (uint8_t*)info, strlen(info), HAL_MAX_DELAY);

    HAL_UART_Transmit(&huart1, rx_buffer_it3, received_data_len, HAL_MAX_DELAY);
    HAL_UART_Transmit(&huart1, (uint8_t*)end_char, strlen(end_char), HAL_MAX_DELAY);


    HAL_UARTEx_ReceiveToIdle_DMA(&huart3, rx_buffer_it3, RX_BUFFER_SIZE);
  }
  else if (huart->Instance == USART6)
  {
    received_data_len = Size;

    // 1. Ngắt dòng
    HAL_UART_Transmit(&huart6, (uint8_t*)end_char, strlen(end_char), HAL_MAX_DELAY);

    // 2. Gửi tin nhắn cho UART_A
    HAL_UART_Transmit(&huart3, rx_buffer_it6, received_data_len, HAL_MAX_DELAY);
    HAL_UART_Transmit(&huart3, (uint8_t*)end_char, strlen(end_char), HAL_MAX_DELAY);

    // 3. Gửi thông tin giám sát lên UART_H
    char info[] = "B -> A: ";
    HAL_UART_Transmit(&huart1, (uint8_t*)info, strlen(info), HAL_MAX_DELAY);

    HAL_UART_Transmit(&huart1, rx_buffer_it6, received_data_len, HAL_MAX_DELAY);
    HAL_UART_Transmit(&huart1, (uint8_t*)end_char, strlen(end_char), HAL_MAX_DELAY);

    HAL_UARTEx_ReceiveToIdle_DMA(&huart6, rx_buffer_it6, RX_BUFFER_SIZE);
  }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef* huart)
{
  char error_log_msg[64];
  uint32_t error_code = HAL_UART_GetError(huart);

  if (huart->Instance == USART1)
  {
    sprintf(error_log_msg, "Error on UART_H, Code: 0x%lX. Restarting...\r\n", error_code);
    //    HAL_UART_Transmit(&huart1, (uint8_t*)error_log_msg, strlen(error_log_msg), 100);

    if (HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_buffer_it1, RX_BUFFER_SIZE) != HAL_OK)
    {
      //      Error_Handler();
    }
  }
  else if (huart->Instance == USART3)
  {
    sprintf(error_log_msg, "Error on UART_A, Code: 0x%lX. Restarting...\r\n", error_code);
    //    HAL_UART_Transmit(&huart1, (uint8_t*)error_log_msg, strlen(error_log_msg), HAL_MAX_DELAY);

    if (HAL_UARTEx_ReceiveToIdle_DMA(&huart3, rx_buffer_it3, RX_BUFFER_SIZE) != HAL_OK)
    {
      //      Error_Handler();
    }
  }
  else if (huart->Instance == USART6)
  {
    sprintf(error_log_msg, "Error on UART_B, Code: 0x%lX. Restarting...\r\n", error_code);
    //    HAL_UART_Transmit(&huart1, (uint8_t*)error_log_msg, strlen(error_log_msg), HAL_MAX_DELAY);

    if (HAL_UARTEx_ReceiveToIdle_DMA(&huart6, rx_buffer_it6, RX_BUFFER_SIZE) != HAL_OK)
    {
      //      Error_Handler();
    }
  }
}
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
void assert_failed(uint8_t* file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
     /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
