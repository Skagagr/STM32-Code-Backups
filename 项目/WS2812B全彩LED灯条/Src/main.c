/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : WS2812B 花样流水灯主程序
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
#include <math.h> /* 为正弦波效果添加 */

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "stm32f1xx_hal.h"
#include "ws2812b.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim3;
DMA_HandleTypeDef hdma_tim3_ch1_trig;

/* USER CODE BEGIN PV */
WS2812B_Ctx_t ws2812b;

/* 颜色表 —— 修改这里可自定义所有效果的颜色 */
static const WS2812B_Color_t color_table[] = {
    {255,   0,   0},  /* 红 */
    {255, 128,   0},  /* 橙 */
    {255, 255,   0},  /* 黄 */
    {  0, 255,   0},  /* 绿 */
    {  0, 255, 128},  /* 青绿 */
    {  0, 255, 255},  /* 青 */
    {  0, 128, 255},  /* 天蓝 */
    {  0,   0, 255},  /* 蓝 */
    {128,   0, 255},  /* 紫 */
    {255,   0, 255},  /* 品红 */
    {255,   0, 128},  /* 玫红 */
    {255, 255, 255},  /* 白 */
};
#define COLOR_TABLE_SIZE (sizeof(color_table) / sizeof(color_table[0]))

static uint16_t flow_offset = 0;  /* 当前流水偏移量 */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_TIM3_Init(void);

/* USER CODE BEGIN PFP */
/* --- 效果函数声明 --- */
void Effect_ClassicFlow(WS2812B_Ctx_t *ctx, uint16_t delay_ms, uint16_t loops);
void Effect_YoYo(WS2812B_Ctx_t *ctx, uint16_t delay_ms, uint8_t bar_len);
void Effect_CometTail(WS2812B_Ctx_t *ctx, uint16_t delay_ms, uint8_t tail_len);
void Effect_SinWave(WS2812B_Ctx_t *ctx, uint16_t delay_ms, uint16_t loops);
void Effect_DualChase(WS2812B_Ctx_t *ctx, uint16_t delay_ms, uint16_t loops);
void Effect_BreatheAll(WS2812B_Ctx_t *ctx, uint16_t duration_ms);
void Effect_KnightRider(WS2812B_Ctx_t *ctx, uint16_t delay_ms);
void Effect_ExpandFromCenter(WS2812B_Ctx_t *ctx, uint16_t delay_ms);
void Effect_FixedColorScroll(WS2812B_Ctx_t *ctx, uint16_t delay_ms);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/**
 * @brief  HAL 回调：DMA 传输完成中断转发
 */
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    WS2812B_IRQHandler(&ws2812b, htim);
}


/**
 * @brief  效果 1：经典彩色流水
 * @param  delay_ms: 流动速度（ms，越小越快）
 * @param  loops: 循环次数
 */
void Effect_ClassicFlow(WS2812B_Ctx_t *ctx, uint16_t delay_ms, uint16_t loops)
{
    static uint16_t offset = 0;
    for (uint16_t k = 0; k < loops; k++) {
        for (uint16_t i = 0; i < WS2812B_NUM_LEDS; i++) {
            uint16_t idx = (i + offset) % COLOR_TABLE_SIZE;
            WS2812B_SetPixel(ctx, i, color_table[idx].r, color_table[idx].g, color_table[idx].b);
        }
        WS2812B_Show(ctx);
        offset = (offset + 1) % COLOR_TABLE_SIZE;
        HAL_Delay(delay_ms);
    }
}

/**
 * @brief  效果 2：双向溜溜球跑马
 * @param  delay_ms: 移动速度
 * @param  bar_len: 跑马条长度（灯珠数）
 */
void Effect_YoYo(WS2812B_Ctx_t *ctx, uint16_t delay_ms, uint8_t bar_len)
{
    int16_t pos = 0;
    int8_t dir = 1;

    for (uint16_t k = 0; k < (WS2812B_NUM_LEDS + bar_len) * 2; k++) {
        WS2812B_Clear(ctx);
        for (uint8_t i = 0; i < bar_len; i++) {
            int16_t led_pos = pos - i;
            if (led_pos >= 0 && led_pos < WS2812B_NUM_LEDS) {
                uint16_t color_idx = (led_pos + k) % COLOR_TABLE_SIZE;
                WS2812B_SetPixel(ctx, led_pos, color_table[color_idx].r, color_table[color_idx].g, color_table[color_idx].b);
            }
        }
        WS2812B_Show(ctx);
        pos += dir;
        if (pos >= WS2812B_NUM_LEDS + bar_len - 1) dir = -1;
        if (pos <= 0) dir = 1;
        HAL_Delay(delay_ms);
    }
}

/**
 * @brief  效果 3：渐变拖尾流星
 * @param  delay_ms: 流星速度
 * @param  tail_len: 尾巴长度
 */
void Effect_CometTail(WS2812B_Ctx_t *ctx, uint16_t delay_ms, uint8_t tail_len)
{
    static uint8_t color_idx = 0;
    int16_t pos;

    for (pos = -tail_len; pos < WS2812B_NUM_LEDS + tail_len; pos++) {
        WS2812B_Clear(ctx);
        if (pos >= 0 && pos < WS2812B_NUM_LEDS) {
            WS2812B_SetPixel(ctx, pos, color_table[color_idx].r, color_table[color_idx].g, color_table[color_idx].b);
        }
        for (uint8_t i = 1; i <= tail_len; i++) {
            int16_t tail_pos = pos - i;
            if (tail_pos >= 0 && tail_pos < WS2812B_NUM_LEDS) {
                uint8_t brightness = 255 - (i * 255 / tail_len);
                WS2812B_SetPixel(ctx, tail_pos,
                    (color_table[color_idx].r * brightness) / 255,
                    (color_table[color_idx].g * brightness) / 255,
                    (color_table[color_idx].b * brightness) / 255);
            }
        }
        WS2812B_Show(ctx);
        HAL_Delay(delay_ms);
    }
    color_idx = (color_idx + 1) % COLOR_TABLE_SIZE;
}

/**
 * @brief  效果 4：正弦波浪
 * @param  delay_ms: 波动速度
 * @param  loops: 循环次数
 */
void Effect_SinWave(WS2812B_Ctx_t *ctx, uint16_t delay_ms, uint16_t loops)
{
    static float phase = 0.0f;
    uint16_t color_idx = 0;

    for (uint16_t k = 0; k < loops * 50; k++) {
        for (uint16_t i = 0; i < WS2812B_NUM_LEDS; i++) {
            float brightness = (sinf(2 * 3.14159f * (float)i / WS2812B_NUM_LEDS + phase) + 1) / 2;
            uint16_t idx = (i + color_idx) % COLOR_TABLE_SIZE;
            uint8_t r = (uint8_t)(color_table[idx].r * brightness);
            uint8_t g = (uint8_t)(color_table[idx].g * brightness);
            uint8_t b = (uint8_t)(color_table[idx].b * brightness);
            WS2812B_SetPixel(ctx, i, r, g, b);
        }
        WS2812B_Show(ctx);
        phase += 0.2f;
        if (phase > 2 * 3.14159f) {
            phase = 0;
            color_idx = (color_idx + 1) % COLOR_TABLE_SIZE;
        }
        HAL_Delay(delay_ms);
    }
}

/**
 * @brief  效果 5：双色交替追逐
 * @param  delay_ms: 追逐速度
 * @param  loops: 循环次数
 */
void Effect_DualChase(WS2812B_Ctx_t *ctx, uint16_t delay_ms, uint16_t loops)
{
    uint16_t pos = 0;
    uint8_t color1_idx = 0;
    uint8_t color2_idx = 5;

    for (uint16_t k = 0; k < loops; k++) {
        WS2812B_Clear(ctx);
        for (uint16_t i = 0; i < 3; i++) {
            uint16_t led_pos = (pos + i) % WS2812B_NUM_LEDS;
            WS2812B_SetPixel(ctx, led_pos, color_table[color1_idx].r, color_table[color1_idx].g, color_table[color1_idx].b);
        }
        for (uint16_t i = 0; i < 3; i++) {
            int16_t led_pos = (WS2812B_NUM_LEDS - 1 - pos - i + WS2812B_NUM_LEDS) % WS2812B_NUM_LEDS;
            WS2812B_SetPixel(ctx, led_pos, color_table[color2_idx].r, color_table[color2_idx].g, color_table[color2_idx].b);
        }
        WS2812B_Show(ctx);
        pos = (pos + 1) % WS2812B_NUM_LEDS;
        if (pos == 0) {
            color1_idx = (color1_idx + 1) % COLOR_TABLE_SIZE;
            color2_idx = (color2_idx + 2) % COLOR_TABLE_SIZE;
        }
        HAL_Delay(delay_ms);
    }
}

/**
 * @brief  效果 6：整体呼吸渐变
 * @param  duration_ms: 单次呼吸时长（ms）
 */
void Effect_BreatheAll(WS2812B_Ctx_t *ctx, uint16_t duration_ms)
{
    static uint8_t color_idx = 0;
    uint16_t steps = duration_ms / 20;

    for (uint16_t step = 0; step < steps; step++) {
        uint8_t brightness = (step < steps / 2) ?
            (step * 255) / (steps / 2) :
            255 - ((step - steps / 2) * 255) / (steps / 2);

        WS2812B_Fill(ctx,
            (color_table[color_idx].r * brightness) / 255,
            (color_table[color_idx].g * brightness) / 255,
            (color_table[color_idx].b * brightness) / 255);
        WS2812B_Show(ctx);
        HAL_Delay(20);
    }
    color_idx = (color_idx + 1) % COLOR_TABLE_SIZE;
}

/**
 * @brief  效果 7： Knight Rider 扫描
 * @param  delay_ms: 扫描速度
 */
void Effect_KnightRider(WS2812B_Ctx_t *ctx, uint16_t delay_ms)
{
    static uint8_t color_idx = 0;

    for (uint16_t i = 0; i < WS2812B_NUM_LEDS; i++) {
        WS2812B_SetPixel(ctx, i, color_table[color_idx].r, color_table[color_idx].g, color_table[color_idx].b);
        WS2812B_Show(ctx); HAL_Delay(delay_ms);
    }
    for (uint16_t i = 0; i < WS2812B_NUM_LEDS; i++) {
        WS2812B_SetPixel(ctx, i, 0, 0, 0);
        WS2812B_Show(ctx); HAL_Delay(delay_ms);
    }
    for (int16_t i = WS2812B_NUM_LEDS - 1; i >= 0; i--) {
        WS2812B_SetPixel(ctx, i, color_table[(color_idx + 1) % COLOR_TABLE_SIZE].r, color_table[(color_idx + 1) % COLOR_TABLE_SIZE].g, color_table[(color_idx + 1) % COLOR_TABLE_SIZE].b);
        WS2812B_Show(ctx); HAL_Delay(delay_ms);
    }
    for (int16_t i = WS2812B_NUM_LEDS - 1; i >= 0; i--) {
        WS2812B_SetPixel(ctx, i, 0, 0, 0);
        WS2812B_Show(ctx); HAL_Delay(delay_ms);
    }
    color_idx = (color_idx + 2) % COLOR_TABLE_SIZE;
}

/**
 * @brief  效果 8：中间向两边扩散
 * @param  delay_ms: 扩散速度
 */
void Effect_ExpandFromCenter(WS2812B_Ctx_t *ctx, uint16_t delay_ms)
{
    static uint8_t color_idx = 0;
    int16_t center = WS2812B_NUM_LEDS / 2;

    WS2812B_Clear(ctx);
    for (int16_t i = 0; i <= center; i++) {
        int16_t left = center - i;
        int16_t right = center + i;
        if (left >= 0) {
            WS2812B_SetPixel(ctx, left, color_table[color_idx].r, color_table[color_idx].g, color_table[color_idx].b);
        }
        if (right < WS2812B_NUM_LEDS && right != left) {
            WS2812B_SetPixel(ctx, right, color_table[(color_idx + 1) % COLOR_TABLE_SIZE].r, color_table[(color_idx + 1) % COLOR_TABLE_SIZE].g, color_table[(color_idx + 1) % COLOR_TABLE_SIZE].b);
        }
        WS2812B_Show(ctx);
        HAL_Delay(delay_ms);
    }
    HAL_Delay(500);
    color_idx = (color_idx + 2) % COLOR_TABLE_SIZE;
}

/**
 * @brief  效果 9：固定颜色滚动
 * @param  delay_ms: 滚动速度
 */
void Effect_FixedColorScroll(WS2812B_Ctx_t *ctx, uint16_t delay_ms)
{
    static uint16_t offset = 0;
    uint8_t segment_len = WS2812B_NUM_LEDS / 3;

    for (uint16_t k = 0; k < WS2812B_NUM_LEDS; k++) {
        for (uint16_t i = 0; i < WS2812B_NUM_LEDS; i++) {
            uint16_t pos = (i + offset) % WS2812B_NUM_LEDS;
            uint8_t color_idx = (pos < segment_len) ? 0 : (pos < segment_len * 2) ? 3 : 7;
            WS2812B_SetPixel(ctx, i, color_table[color_idx].r, color_table[color_idx].g, color_table[color_idx].b);
        }
        WS2812B_Show(ctx);
        offset = (offset + 1) % WS2812B_NUM_LEDS;
        HAL_Delay(delay_ms);
    }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_TIM3_Init();

  /* USER CODE BEGIN 2 */
  WS2812B_Init(&ws2812b, &htim3, TIM_CHANNEL_1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* --- 效果播放列表：在此调整顺序或注释掉不喜欢的效果 --- */
    Effect_ClassicFlow(&ws2812b, 80, 15);    // 1. 经典流水
    Effect_YoYo(&ws2812b, 60, 5);             // 2. 溜溜球
    Effect_CometTail(&ws2812b, 50, 8);        // 3. 流星
    Effect_SinWave(&ws2812b, 30, 5);          // 4. 正弦波浪
    Effect_DualChase(&ws2812b, 50, 30);       // 5. 双色追逐
    Effect_BreatheAll(&ws2812b, 2000);         // 6. 整体呼吸
    Effect_KnightRider(&ws2812b, 60);          // 7. 扫描
    Effect_ExpandFromCenter(&ws2812b, 80);     // 8. 中间扩散
    Effect_FixedColorScroll(&ws2812b, 40);     // 9. 固定色滚动

    // /* 根据偏移量把颜色表映射到每颗灯珠 */
    // for (uint16_t i = 0; i < WS2812B_NUM_LEDS; i++)
    // {
    //     uint16_t idx = (i + flow_offset) % COLOR_TABLE_SIZE;
    //     WS2812B_SetPixel(&ws2812b, i,
    //                       color_table[idx].r,
    //                       color_table[idx].g,
    //                       color_table[idx].b);
    // }

    // WS2812B_Show(&ws2812b);

    // /* 偏移量递增，超出颜色表长度后回绕 */
    // flow_offset = (flow_offset + 1) % COLOR_TABLE_SIZE;

    // HAL_Delay(50);  /* 调整此值控制流速，越小越快 */
  }
  /* USER CODE END WHILE */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

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
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

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
  HAL_TIM_MspPostInit(&htim3);
}

/**
  * @brief DMA Initialization Function
  * @param None
  * @retval None
  */
static void MX_DMA_Init(void)
{
  __HAL_RCC_DMA1_CLK_ENABLE();
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
  /* USER CODE BEGIN MX_GPIO_Init_1 */
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOA_CLK_ENABLE();  // 使能 GPIOC 时钟

    GPIO_InitStruct.Pin   = GPIO_PIN_1;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1, GPIO_PIN_SET);  // 初始熄灭（低电平亮）
  /* USER CODE END MX_GPIO_Init_2 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  __disable_irq();
  while (1)
  {
  }
}

#ifdef USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif /* USE_FULL_ASSERT */