#include "main.h"
#include "uart.h"


UART_HandleTypeDef huart1;
UART_Ctx uart1_ctx;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);

/* ── TX 回调（两种模式都需要）─────────────────── */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
        UART_TxCpltCallback(&uart1_ctx);
}

/* ── 中断模式 RX 回调 ────────────────────────── */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
        UART_RxCpltCallback(&uart1_ctx);
}




/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_USART1_UART_Init();

    // 初始化 UART 模块
    UART_Init(&uart1_ctx, &huart1);

    // 启动持续接收
    UART_StartReceive(&uart1_ctx);

    // 开机提示
    UART_SendString(&uart1_ctx, "系统已准备就绪\r\n");

    uint32_t last_tick = 0;


    while (1)
    {
        /* ── 定时发送：每 1 秒发一条心跳 ────────────────────────────────── */
        if (HAL_GetTick() - last_tick >= 1000)
        {
            last_tick = HAL_GetTick();
            UART_SendString(&uart1_ctx, "心跳\r\n");
        }

        /* ── 接收处理：收到数据后原样回显 ───────────────────────────────── */
        if (uart1_ctx.rx_ready)
        {
            uart1_ctx.rx_ready = 0;   // 先清标志
            char cmd[UART_RX_BUF_SIZE] = {0};   // 在这里声明，仅块内有效

            // 去掉末尾 \r\n
            for (int i = 0; i < uart1_ctx.rx_len; i++)
            {
                if (uart1_ctx.rx_buf[i] == '\r' || uart1_ctx.rx_buf[i] == '\n')
                {
                    uart1_ctx.rx_buf[i] = '\0';
                    break;
                }
            }

            // 赋值给 cmd
            strcpy(cmd, (char *)uart1_ctx.rx_buf);

            // 回显
            UART_SendString(&uart1_ctx, cmd);
            UART_SendString(&uart1_ctx, "\r\n");

            // 根据输入的指令执行相应操作
            if (strcmp(cmd, "PA1") == 0)
                HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_1);
            else if (strcmp(cmd, "PA2") == 0)
                HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_2);
            else if (strcmp(cmd, "PA3") == 0)
                HAL_GPIO_TogglePin(GPIOA,GPIO_PIN_3);
            else if (strcmp(cmd, "PA4") == 0)
                HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_4);
            else if (strcmp(cmd, "PA5") == 0)
                HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
        }

        /* ── 其他任务：发送期间 CPU 正常执行 ────────────────────────────── */
        // do_task_A();
        // do_task_B();

  }
}


/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)   // 硬件初始化（板级）CubeMX生成的
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
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4
                          |GPIO_PIN_5, GPIO_PIN_RESET);

  /*Configure GPIO pins : PA1 PA2 PA3 PA4
                           PA5 */
  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4
                          |GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */


/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
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
