#include "main.h"
#include "stm32f1xx_hal_gpio.h"
#include "uart.h"
#include "timer.h"

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart1;
UART_Ctx uart1_ctx;

static volatile uint16_t LED_Count = 0;  // LED 延时流水计数
static uint16_t LED_ms = 0;     // LED 每隔多长时间流水一次
static uint8_t LED_Index = 0;  // 加到全局变量区
static uint8_t LED_Pin[5] =
{
    GPIO_PIN_1, GPIO_PIN_2, GPIO_PIN_3, GPIO_PIN_4, GPIO_PIN_5
};

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
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

/* ── HAL 定时器回调转发 ─────────────────────── */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2)
        TIMER_Tick();
}

/* ── 回调函数（简短，只做标志位或 Toggle）──── */
void Task_LED_Count(void)
{
    LED_Count++;
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
    MX_TIM2_Init();
    MX_USART1_UART_Init();

    UART_Init(&uart1_ctx, &huart1);
    UART_StartReceive(&uart1_ctx);
    UART_SendString(&uart1_ctx, "系统已准备就绪\r\n");

    TIMER_Init();
    TIMER_Register(0, 1, Task_LED_Count);  // 每 1ms 次数 +1
    TIMER_Start(&htim2);


  while (1)
    {
        /* ── UART 接收处理：收到数据后原样回显 ──────────────────────────────── */
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
            if (strcmp(cmd, "start") == 0)  // 开始流水灯
            {
                LED_ms = 200;
                TIMER_Resume(0);  // ← 加上这行，stop 之后才能重新启动
            }
            else if (strcmp(cmd, "stop") == 0)  // 停止流水灯
            {
                // 归零
                LED_ms = 0;

                // 停止定时器
                TIMER_Stop(0);

                // 熄灭 LED
                HAL_GPIO_WritePin(GPIOA,
                    GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5,
                    GPIO_PIN_SET);
            }
            else if (strcmp(cmd, "speed:100") == 0 && LED_ms != 0)
            {
                LED_ms = 100;
            }
            else if (strcmp(cmd, "speed:500") == 0 && LED_ms != 0)
            {
                LED_ms = 500;
            }
            else if (strcmp(cmd, "speed:1000") == 0 && LED_ms != 0)
            {
                LED_ms = 1000;
            }
        }

        /* LED 流水灯处理：根据 LED_ms 进行每次流水的延时 ─────────-──────────── */
        if (LED_Count >= LED_ms && LED_ms != 0)
        {
            LED_Count = 0;      // 重置次数

            // 流水逻辑：先全灭，再点亮当前索引
            // 全灭
            HAL_GPIO_WritePin(GPIOA,
                GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5,
                GPIO_PIN_SET);

            // 点亮当前
            HAL_GPIO_WritePin(GPIOA, LED_Pin[LED_Index], GPIO_PIN_RESET);

            // 移到下一颗
            LED_Index++;
            if (LED_Index >= 5) LED_Index = 0;
        }
    }
}

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
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 71;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

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
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    // 初始全灭
    HAL_GPIO_WritePin(GPIOA,
        GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5,
        GPIO_PIN_SET);

    // 配置为推挽输出
    GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4|GPIO_PIN_5;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
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
