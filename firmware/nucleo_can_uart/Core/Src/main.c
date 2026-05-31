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
//#include <stddef.h>

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "can_if.h"
#include "bxcan_if.h"
#include "air_quality_can_protocol.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define MSG_SIZE 160

typedef struct {
	char msg[MSG_SIZE];
	char buffer[MSG_SIZE];
	volatile int8_t ready_to_send;
	volatile int8_t data_available;
	volatile int counter;
} uart_msg_t;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
CAN_HandleTypeDef hcan1;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
uart_msg_t my_msg;
bxcan_if_t bxcan_ctx;
can_if_t can_bus;
can_if_backend_t can_backend;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_CAN1_Init(void);
/* USER CODE BEGIN PFP */
void uart2_write_data(uart_msg_t* my_msg, int msg_len);
void CAN_Filter_Config(void);

static void format_can_frame(char *out, size_t out_size, const can_frame_t *frame);
static void format_air_quality_frame(char *out, size_t out_size, const aq_can_measurement_t *measurement);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

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
  MX_USART2_UART_Init();
  MX_CAN1_Init();
  /* USER CODE BEGIN 2 */
  memset(my_msg.msg,'\0',MSG_SIZE*sizeof(my_msg.msg[0]));

  my_msg.data_available = 0;
  my_msg.ready_to_send = 1;
  my_msg.counter = 0;

  CAN_Filter_Config();

  if (BXCAN_IF_Init(&bxcan_ctx, &hcan1) != CAN_IF_OK) {
      Error_Handler();
  }

  if (BXCAN_IF_Get_CAN_IF_Backend(&bxcan_ctx, &can_backend) != CAN_IF_OK) {
      Error_Handler();
  }

  if (CAN_IF_Init(&can_bus, &can_backend) != CAN_IF_OK) {
      Error_Handler();
  }

  if(HAL_CAN_ActivateNotification(&hcan1, CAN_IT_TX_MAILBOX_EMPTY | CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_BUSOFF) != HAL_OK)
  {
	  Error_Handler();
  }

  if(HAL_CAN_Start(&hcan1)!= HAL_OK)
  {
	  Error_Handler();
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  while(1)
  {
  /* USER CODE BEGIN WHILE */

	  can_frame_t rx_msg;

	  if (CAN_IF_Read(&can_bus, &rx_msg)) {
	      if (my_msg.ready_to_send == 1) {
	          aq_can_measurement_t measurement = {0};

	          if (AQ_CAN_UnpackMeasurementFrame(&rx_msg, &measurement) == CAN_IF_OK) {
	              format_air_quality_frame(my_msg.msg, MSG_SIZE, &measurement);
	          } else {
	              format_can_frame(my_msg.msg, MSG_SIZE, &rx_msg);
	          }

	          my_msg.data_available = 1;
	      }
	  }

	  if (HAL_UART_GetState(&huart2) == HAL_UART_STATE_READY)
	  {
	    snprintf(my_msg.buffer, MSG_SIZE, "%s", my_msg.msg);
		uart2_write_data(&my_msg, MSG_SIZE);
	  }
	  HAL_Delay(100);
    /* USER CODE END WHILE */
  }
    /* USER CODE BEGIN 3 */
  /* USER CODE END 3 */
  return(0);
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief CAN1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_CAN1_Init(void)
{

  /* USER CODE BEGIN CAN1_Init 0 */

  /* USER CODE END CAN1_Init 0 */

  /* USER CODE BEGIN CAN1_Init 1 */

  /* USER CODE END CAN1_Init 1 */
  hcan1.Instance = CAN1;
  hcan1.Init.Prescaler = 21;
  hcan1.Init.Mode = CAN_MODE_NORMAL;
  hcan1.Init.SyncJumpWidth = CAN_SJW_1TQ;
  hcan1.Init.TimeSeg1 = CAN_BS1_13TQ;
  hcan1.Init.TimeSeg2 = CAN_BS2_2TQ;
  hcan1.Init.TimeTriggeredMode = DISABLE;
  hcan1.Init.AutoBusOff = DISABLE;
  hcan1.Init.AutoWakeUp = DISABLE;
  hcan1.Init.AutoRetransmission = ENABLE;
  hcan1.Init.ReceiveFifoLocked = DISABLE;
  hcan1.Init.TransmitFifoPriority = DISABLE;
  if (HAL_CAN_Init(&hcan1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN CAN1_Init 2 */

  /* USER CODE END CAN1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void uart2_write_data(uart_msg_t* my_msg, int msg_len)
{
	if(my_msg->data_available == 1)
	{
		HAL_UART_Transmit_IT(&huart2, (uint8_t *)my_msg->buffer, strlen(my_msg->buffer));
		my_msg->data_available = 0;
		my_msg->counter++;
		my_msg->ready_to_send = 0;
	}
}

static void format_can_frame(char *out, size_t out_size, const can_frame_t *frame)
{
    if ((out == NULL) || (frame == NULL) || (out_size == 0U)) {
        return;
    }

    uint8_t dlc = frame->dlc;

    if (dlc > CAN_MAX_DLC) {
        dlc = CAN_MAX_DLC;
    }

    int written = snprintf(out, out_size, "CAN RX id=0x%03lX dlc=%u data=[", (unsigned long)frame->id, (unsigned int)dlc);

    if ((written < 0) || ((size_t)written >= out_size)) {
        out[out_size - 1U] = '\0';
        return;
    }

    size_t used = (size_t)written;

    for (uint8_t i = 0U; i < dlc; i++) {
        written = snprintf(&out[used], out_size - used, "%02X%s", (unsigned int)frame->data[i], (i + 1U < dlc) ? " " : "");

        if ((written < 0) || ((size_t)written >= (out_size - used))) {
            out[out_size - 1U] = '\0';
            return;
        }

        used += (size_t)written;
    }

    snprintf(&out[used], out_size - used, "]\r\n");
}

static void format_air_quality_frame(char *out, size_t out_size, const aq_can_measurement_t *measurement)
{
    if ((out == NULL) || (measurement == NULL) || (out_size == 0U))
    {
        return;
    }

    int32_t temp_x100 = (int32_t)measurement->temperature_c_x100;
    int32_t temp_abs = temp_x100;

    if (temp_abs < 0)
    {
        temp_abs = -temp_abs;
    }

    uint32_t humidity_x100 = (uint32_t)measurement->humidity_rh_x100;

    snprintf(out,
             out_size,
             "AQ RX T=%s%ld.%02ld C RH=%lu.%02lu %% VOC=%lu cnt=%u flags=0x%02X\r\n",
             (temp_x100 < 0) ? "-" : "",
             (long)(temp_abs / 100),
             (long)(temp_abs % 100),
             (unsigned long)(humidity_x100 / 100U),
             (unsigned long)(humidity_x100 % 100U),
             (unsigned long)measurement->voc_index,
             (unsigned int)measurement->sample_counter,
             (unsigned int)measurement->status_flags);
}

void CAN_Filter_Config(void)
{
	CAN_FilterTypeDef CanFilter;

	CanFilter.FilterActivation = CAN_FILTER_ENABLE;
	CanFilter.FilterBank = 0u;
	CanFilter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
	CanFilter.FilterIdHigh = 0x0000u;
	CanFilter.FilterIdLow = 0x0000u;
	CanFilter.FilterMaskIdHigh = 0x0000U;
	CanFilter.FilterMaskIdLow = 0x0000u;
	CanFilter.FilterMode = CAN_FILTERMODE_IDMASK;
	CanFilter.FilterScale = CAN_FILTERSCALE_32BIT;
	CanFilter.SlaveStartFilterBank = 14u;

	if(HAL_CAN_ConfigFilter(&hcan1, &CanFilter) != HAL_OK)
	{
		Error_Handler();
	}
}


void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  /* Prevent unused argument(s) compilation warning */
  UNUSED(huart);
  my_msg.ready_to_send = 1;
}

void HAL_CAN_TxMailbox0CompleteCallback(CAN_HandleTypeDef *hcan)
{
	BXCAN_IF_On_Tx_Complete(&bxcan_ctx, hcan);
}
void HAL_CAN_TxMailbox1CompleteCallback(CAN_HandleTypeDef *hcan)
{
	BXCAN_IF_On_Tx_Complete(&bxcan_ctx, hcan);
}
void HAL_CAN_TxMailbox2CompleteCallback(CAN_HandleTypeDef *hcan)
{
	BXCAN_IF_On_Tx_Complete(&bxcan_ctx, hcan);
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
	BXCAN_IF_On_Rx_Fifo0_Message_Pending(&bxcan_ctx, hcan);
	HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
}
void HAL_CAN_ErrorCallback(CAN_HandleTypeDef *hcan)
{

	if(my_msg.ready_to_send == 1)
	{
		sprintf(my_msg.msg,"Error happened!  \r\n");
		my_msg.data_available = 1;
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
