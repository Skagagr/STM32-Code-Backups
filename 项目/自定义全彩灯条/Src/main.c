#include "main.h"
#include "uart.h"
#include "exti.h"
#include "ws2812b.h"
#include "ws2812b_mode.h"

I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim3;
DMA_HandleTypeDef hdma_tim3_ch1_trig;

UART_HandleTypeDef huart1;
UART_Ctx uart1_ctx;

static EXTICtx_t g_exti_pa0;  // PA0 外部中断句柄
static EXTICtx_t g_exti_pa2;  // PA2 外部中断句柄
static volatile uint8_t ec11_spin_count = 0;    // 旋转编码器，正反旋转计数
volatile int8_t ec11_spin_temp_count = 0;       // 旋转编码器，正反转计数暂存
volatile uint8_t ec11_state = 0;    // 旋转编码器状态，不同状态控制ws2812b不同属性

WS2812B_Ctx_t ws2812b;  // ws2812b 句柄
volatile uint8_t ws2812b_speed = 50;     // 速度 15 - 150
volatile uint8_t ws2812b_bright = 150;   // 亮度 10 - 250


void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM3_Init(void);
static void MX_USART1_UART_Init(void);

/**
 * @brief TX 回调
 * 
 * @param huart HAL UART 句柄指针
 */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
        UART_TxCpltCallback(&uart1_ctx);
}

/**
 * @brief RX 回调
 * 
 * @param huart HAL UART 句柄指针
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
        UART_RxCpltCallback(&uart1_ctx);
}

/**
 * @brief PA2 外部中断回调（旋转 ec11）单边沿正交编码计数
 */
static void PA2_Callback(void)
{
    uint8_t A = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_2);
    uint8_t B = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1);

    if (A == B) // 正转
    {
        if (ec11_state == 0)
        {
            // 亮度 +10（上限 250）
            if (ws2812b_bright < 250)
                ws2812b_bright += 10;
        }
        else if (ec11_state == 1)
        {
            // 正转 → 速度变快 → 数值减小
            if (ws2812b_speed > 20)
                ws2812b_speed -= 5;
        }
        else if (ec11_state == 2)
            ec11_spin_temp_count++;
    }
    else    // 反转
    {
        if (ec11_state == 0)
        {
            // 亮度 -10（下限 20，不黑屏）
            if (ws2812b_bright > 20)
                ws2812b_bright -= 10;
        }
        else if (ec11_state == 1)
        {
            // 反转 → 速度变慢 → 数值增大
            if (ws2812b_speed < 195)
                ws2812b_speed += 5;
        }
        else if (ec11_state == 2)
            ec11_spin_temp_count--;
    }
}

/**
 * @brief PA0 外部中断回调（按下 ec11）
 */
static void PA0_Callback(void)
{
    ec11_state = (ec11_state + 1) % 3;  // 只会有 0 1 2 三种状态
}

/**
 * @brief HAL 弱函数重写
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    EXTI_Handler(&g_exti_pa0, GPIO_Pin);
    EXTI_Handler(&g_exti_pa2, GPIO_Pin);
}

/**
 * @brief  HAL 回调：DMA 传输完成中断转发
 */
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    WS2812B_IRQHandler(&ws2812b, htim);
}

int main(void)
{
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_DMA_Init();
    MX_I2C1_Init();
    MX_TIM3_Init();
    MX_USART1_UART_Init();

    EXTI_Init(&g_exti_pa0,
        GPIOA, GPIO_PIN_0,
        EXTI0_IRQn,
        GPIO_MODE_IT_FALLING,
        PA0_Callback);

    EXTI_Init(
        &g_exti_pa2,
        GPIOA, GPIO_PIN_2,
        EXTI2_IRQn,
        GPIO_MODE_IT_FALLING,
        PA2_Callback);

    UART_Init(&uart1_ctx, &huart1);
    UART_StartReceive(&uart1_ctx);
    UART_SendString(&uart1_ctx, "系统已经准备就绪\r\n");

    WS2812B_Init(&ws2812b, &htim3, TIM_CHANNEL_1);

    while (1)
    {
        ec11_spin_count = (ec11_spin_temp_count % 9 + 9) % 9;   // 正模运算（欧几里得模），把结果限制到 [0, N-1] 区间内，结果永远 ≥ 0

        if (ec11_state == 0)
        {
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET);
        }
        else if (ec11_state == 1)
        {
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET);
        }
        else if (ec11_state == 2)
        {
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
            HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET);
        }


        switch (ec11_spin_count)
        {
            case 0: Effect_ClassicFlow(&ws2812b, ws2812b_speed, ws2812b_bright);     // 经典流水
                break;
            case 1: Effect_YoYo(&ws2812b, ws2812b_speed, ws2812b_bright);            // 溜溜球
                break;
            case 2: Effect_CometTail(&ws2812b, ws2812b_speed, ws2812b_bright);       // 流星拖尾
                break;
            case 3: Effect_SinWave(&ws2812b, ws2812b_speed, ws2812b_bright);         // 正弦波浪
                break;
            case 4: Effect_DualChase(&ws2812b, ws2812b_speed, ws2812b_bright);       // 双色追逐
                break;
            case 5: Effect_BreatheAll(&ws2812b, ws2812b_speed, ws2812b_bright);      // 整体呼吸
                break;
            case 6: Effect_KnightRider(&ws2812b, ws2812b_speed, ws2812b_bright);     // 扫描
                break;
            case 7: Effect_ExpandFromCenter(&ws2812b, ws2812b_speed, ws2812b_bright);// 中间扩散
                break;
            case 8: Effect_FixedColorScroll(&ws2812b, ws2812b_speed, ws2812b_bright);// 固定色滚动
                break;
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
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 0;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 89;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */
  HAL_TIM_MspPostInit(&htim3);

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
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Channel6_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Channel6_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Channel6_IRQn);

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
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pins : PA0 PA2 */
//   GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_2;
//   GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
//   GPIO_InitStruct.Pull = GPIO_NOPULL;
//   HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB13 PB14 PB15 */
  GPIO_InitStruct.Pin = GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
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
