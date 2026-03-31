#include "main.h"
#include "i2c_bus.h"
#include "oled.h"
#include "mpu6050.h"

/* ── HAL 句柄 ── */
I2C_HandleTypeDef hi2c1;

/* ── 模块句柄 ── */
static I2C_Bus_Ctx i2c1_ctx;
static OLED_Ctx    oled_ctx;
static MPU6050_Ctx  mpu_ctx;

/* ── 函数声明 ── */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();

    /* ── 模块初始化 ── */
    I2C_Bus_Init(&i2c1_ctx, &hi2c1);
    OLED_Init(&oled_ctx, &i2c1_ctx);

    if (!MPU6050_Init(&mpu_ctx, &i2c1_ctx))
    {
        OLED_ShowString(&oled_ctx, 0, 0, "MPU6050 FAIL");
        OLED_Refresh(&oled_ctx);
        Error_Handler();
    }

    /* ── 主循环 ── */
    while (1)
    {
        MPU6050_Update(&mpu_ctx);

        OLED_Clear(&oled_ctx);

        char buf[32];

        /* X 轴加速度（g），范围 -2.0~+2.0，水平静止时接近 0 */
        sprintf(buf, "AX:%.2f", mpu_ctx.accel_x);
        OLED_ShowString(&oled_ctx, 0, 0, buf);

        /* Y 轴加速度（g），范围 -2.0~+2.0，水平静止时接近 0 */
        sprintf(buf, "AY:%.2f", mpu_ctx.accel_y);
        OLED_ShowString(&oled_ctx, 0, 8, buf);

        /* Z 轴加速度（g），范围 -2.0~+2.0，水平静止时接近 1.0（重力方向）*/
        sprintf(buf, "AZ:%.2f", mpu_ctx.accel_z);
        OLED_ShowString(&oled_ctx, 0, 16, buf);

        /* X 轴角速度（°/s），范围 -500~+500，静止时接近 0 */
        sprintf(buf, "GX:%.1f", mpu_ctx.gyro_x);
        OLED_ShowString(&oled_ctx, 0, 24, buf);

        /* Y 轴角速度（°/s），范围 -500~+500，静止时接近 0 */
        sprintf(buf, "GY:%.1f", mpu_ctx.gyro_y);
        OLED_ShowString(&oled_ctx, 0, 32, buf);

        /* Z 轴角速度（°/s），范围 -500~+500，静止时接近 0 */
        sprintf(buf, "GZ:%.1f", mpu_ctx.gyro_z);
        OLED_ShowString(&oled_ctx, 0, 40, buf);

        /* 卡尔曼滤波后角度：AnX=Roll 范围 -180~+180°，AnY=Pitch 范围 -90~+90° */
        sprintf(buf, "AnX:%.1f AnY:%.1f", mpu_ctx.angle_x, mpu_ctx.angle_y);
        OLED_ShowString(&oled_ctx, 0, 48, buf);

        /* 芯片内部温度（°C），范围 -40~+85，比室温偏高几度属正常 */
        sprintf(buf, "T:%.1f", mpu_ctx.temp);
        OLED_ShowString(&oled_ctx, 0, 56, buf);

        OLED_Refresh(&oled_ctx);

        // HAL_Delay(100);
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
    // /* ── I2C 总线复位（解决 STM32F103 I2C 上电 bug）── */
    __HAL_RCC_I2C1_FORCE_RESET();
    HAL_Delay(10);
    __HAL_RCC_I2C1_RELEASE_RESET();
    HAL_Delay(10);
    // /* ─────────────────────────────────────────────────── */

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
  __HAL_RCC_GPIOB_CLK_ENABLE();

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
