/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : IOT 环境监测系统 —— 主程序
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
 *
 *   系统架构
 *   ────────
 *   STM32F103C8T6 通过 UART1 外接 ESP8266 WiFi 模块，基于 MQTT 3.1.1 协议
 *   与云端 broker (broker.emqx.io:1883) 通信。每 500ms 采集一次 SHT30 温湿度
 *   传感器数据，发布到主题 stm32/sensor；同时订阅主题 stm32/control 接收
 *   云端 LED_ON / LED_OFF 指令控制板载 LED (PC13)。
 *
 *   数据流
 *   ──────
 *   SHT30 ─I2C─> STM32 ─UART1─> ESP8266 ─WiFi─> MQTT Broker ──> 云端
 *   OLED  <──I2C─── STM32 <─UART1── ESP8266 <─WiFi── MQTT Broker <── 云端
 *
 *   外设引脚
 *   ────────
 *   PA9  = USART1_TX → ESP8266 RX
 *   PA10 = USART1_RX → ESP8266 TX
 *   PB6  = I2C1_SCL → OLED + SHT30
 *   PB7  = I2C1_SDA → OLED + SHT30
 *   PC13 = 板载 LED（低电平点亮）
 *
 *   通信协议栈
 *   ──────────
 *   MQTT 3.1.1 (QoS 0)
 *     └── ESP8266 AT 指令固件 v1.2.0.0
 *           └── UART 115200-8N1，中断接收（逐字节）
 *
 *   切换云端服务器
 *   ──────────────
 *   只需修改 main() 中的一行，格式为：
 *   ESP8266_TCPConnect(&esp8266_ctx, "服务器地址", 端口号);
 *
 *   当前配置：broker.emqx.io:1883（EMQX 公共测试服务器，无认证无加密）
 *
 *   常见服务器示例
 *   ├─ EMQX 公共测试    : broker.emqx.io          1883
 *   ├─ HiveMQ 公共测试  : broker.hivemq.com        1883
 *   ├─ 阿里云 IoT 平台  : <ProductKey>.iot-as-mqtt.cn-shanghai.aliyuncs.com  1883
 *   ├─ 华为云 IoTDA     : <DeviceId>.iot-mqtts.cn-north-4.myhuaweicloud.com  1883
 *   ├─ 腾讯云 IoT Hub   : <ProductId>.iotcloud.tencentdevices.com            1883
 *   ├─ 私有部署         : 自建 IP/域名            自定端口
 *   └─ 本地测试         : Mosquitto 等本地 broker  1883
 *
 *   注意事项
 *   ├─ 云平台（阿里/华为/腾讯）通常需要 TLS + 密钥认证，当前 ESP8266 固件
 *   │   v1.2.0.0 不支持 AT+CIPSSL 等 SSL 指令，需升级固件或换方案。
 *   ├─ 换服务器后如 broker 要求用户名密码，需修改 mqtt.c 的 CONNECT 报文。
 *   └─ Client ID（"STM32IoT"）在公共服务器上可能冲突，换服务器建议同步修改。
 *
 * @version 1.1.0
 * @date    2026-05-31
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "i2c_bus.h"
#include "oled.h"
#include "sht3x.h"
#include "esp8266.h"
#include "mqtt.h"
#include "stm32f1xx_hal.h"
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
I2C_HandleTypeDef hi2c1;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
static I2C_Bus_Ctx     i2c1_ctx;     /**< I2C 总线上下文 */
static OLED_Ctx        oled_ctx;     /**< SSD1315 OLED 上下文 */
static SHT30_Ctx       sht30_ctx;    /**< SHT30 温湿度传感器上下文 */
static ESP8266_Bus_Ctx esp8266_ctx;  /**< ESP8266 AT 驱动上下文 */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/**
 * @brief  在二进制缓冲区中搜索子串（可处理嵌入的 0x00 字节）
 *
 * C 标准库的 strstr() 以 '\0' 作为字符串终止符，无法搜索包含 0x00 字节的
 * 二进制数据。MQTT PUBLISH 报文的话题长度字段含 0x00（如 0x00 0x0D），
 * 因此需要此函数以显式长度进行搜索。
 *
 * @param haystack  待搜索的二进制缓冲区
 * @param hay_len   缓冲区有效字节数
 * @param needle    目标子串（以 '\0' 结尾的 C 字符串）
 * @return          找到返回指向匹配位置的指针，否则返回 NULL
 */
static uint8_t* bin_search(const uint8_t* haystack, uint16_t hay_len,
                           const char* needle)
{
    uint16_t n = strlen(needle);
    if (n > hay_len) return NULL;
    for (uint16_t i = 0; i <= hay_len - n; i++)
    {
        if (memcmp(&haystack[i], needle, n) == 0)
            return (uint8_t*)&haystack[i];
    }
    return NULL;
}

/**
 * @brief  UART 接收中断回调 —— +IPD 实时检测状态机
 *
 * ESP8266 的异步数据到达通知格式为 "\r\n+IPD,<len>:<data>"，其中 <data>
 * 是 <len> 字节的原始 TCP 数据（MQTT 报文），紧随冒号后且不含换行符。
 * 本回调实现了一个逐字符状态机来实时检测并吸收 IPD 数据，避免依赖 '\n'
 * 终止符（因 MQTT 二进制报文通常不含换行）。
 *
 * 状态机转换图
 * ────────────
 *
 *   ┌──────────┐   匹配 "+IPD," 逐字符   ┌──────────────┐
 *   │  IDLE    │ ──────────────────────> │  读取长度     │
 *   │ (普通AT) │  ipd_prefix_idx: 0→5   │ (等冒号)      │
 *   └──────────┘ <────────────────────── └──────┬───────┘
 *        ↑               非预期字符 / 换行        │ 收到 ':'
 *        │                                        ▼
 *        │                               ┌──────────────┐
 *        └─────────────────────────────── │  吸收数据     │
 *                收满 ipd_expect_len 字节  │ (ipd_receiving=1)
 *                                         └──────────────┘
 *
 * 关键设计决策
 * ────────────
 * 1. 逐字符前缀匹配 "+IPD,"，不依赖 strstr()，避免因 buffer 中存在
 *    0x00 字节导致的误判。
 * 2. 长度字段逐数字累加（ipd_expect_len = ipd_expect_len * 10 + digit），
 *    无需事后从缓冲区解析。
 * 3. IPD 数据直接存入独立的 ipd_buf[]，与 AT 响应缓冲区 rx_buf[] 分离，
 *    避免二进制数据干扰 AT 命令响应解析。
 * 4. '>' 和 '\n' 都会复位 ipd_prefix_idx，确保 AT 命令响应正确终止
 *    任何进行中的 IPD 前缀匹配。
 *
 * @param huart 触发中断的 UART 句柄
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    ESP8266_Bus_Ctx *ctx = &esp8266_ctx;
    if (huart != ctx->huart) return;
    uint8_t byte = ctx->rx_temp;

    /* ── 状态 1：IPD 数据吸收模式 ─────────────────────────────── */
    if (ctx->ipd_receiving)
    {
        ctx->ipd_buf[ctx->ipd_index++] = byte;
        if (ctx->ipd_index >= ctx->ipd_expect_len)
        {
            ctx->ipd_buf[ctx->ipd_index] = '\0';
            ctx->ipd_recv_len = ctx->ipd_expect_len;
            ctx->ipd_receiving = 0;
            ctx->ipd_index     = 0;

            // 发送期间：延迟通知，避免ipd_ready被主循环提前消费
            if (ctx->sending)
                ctx->ipd_pending = 1;
            else
                ctx->ipd_ready = 1;
        }
        HAL_UART_Receive_IT(ctx->huart, &ctx->rx_temp, 1);
        return;
    }

    /* ── 状态 2 / 3：+"IPD," 前缀匹配 → 读取长度 ────────────── */
    static const char ipd_prefix[] = "+IPD,";

    if (ctx->ipd_prefix_idx < 5)
    {
        /* 逐字符比对 "+IPD,"（共 5 个字符） */
        if (byte == ipd_prefix[ctx->ipd_prefix_idx])
        {
            ctx->ipd_prefix_idx++;
            if (ctx->ipd_prefix_idx == 5)
                ctx->ipd_expect_len = 0;   // 前缀匹配完毕，准备读取长度数字
        }
        else
        {
            ctx->ipd_prefix_idx = (byte == '+') ? 1 : 0;  // 失配回退
        }
    }
    else  // ipd_prefix_idx == 5：已匹配 "+IPD,"，逐数字累加长度
    {
        if (byte >= '0' && byte <= '9')
        {
            ctx->ipd_expect_len = ctx->ipd_expect_len * 10 + (byte - '0');
        }
        else if (byte == ':')
        {
            // 冒号触发数据吸收模式
            ctx->ipd_index      = 0;
            ctx->ipd_receiving  = 1;
            ctx->ipd_prefix_idx = 0;
            ctx->rx_index       = 0;
            HAL_UART_Receive_IT(ctx->huart, &ctx->rx_temp, 1);
            return;
        }
        else
        {
            ctx->ipd_prefix_idx = 0;  // 非数字非冒号：重置
        }
    }

    /* ── 普通 AT 响应逐行处理 ───────────────────────────────── */
    if (byte == '>')
    {
        // AT+CIPSEND 发送就绪提示符：直接置位，不依赖换行
        ctx->rx_buf[0] = '>'; ctx->rx_buf[1] = '\0';
        ctx->rx_ready    = 1;
        ctx->rx_index    = 0;
        ctx->ipd_prefix_idx = 0;
    }
    else if (byte == '\n')
    {
        // 换行符：结束当前 AT 响应行，通知等待者
        if (ctx->rx_index > 0 && ctx->rx_buf[ctx->rx_index - 1] == '\r')
            ctx->rx_index--;                     // 去掉行尾 \r
        ctx->rx_buf[ctx->rx_index] = '\0';
        ctx->rx_ready = 1;
        ctx->rx_index = 0;
        ctx->ipd_prefix_idx = 0;
    }
    else
    {
        // 普通字符：累积到 AT 响应缓冲区
        if (ctx->rx_index < 255)
            ctx->rx_buf[ctx->rx_index++] = byte;
        else
            ctx->rx_index = 0;                   // 缓冲区满则回绕
    }

    HAL_UART_Receive_IT(ctx->huart, &ctx->rx_temp, 1);
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
    uint8_t init_ok = 0;    
    float temp = 0, humi = 0;                           // 温度 湿度
    uint8_t temp_threshold = 25, humi_threshold = 60;   // 温度阈值 湿度阈值，范围0 - 255
    static uint32_t last_sensor_tick = 0;               // 用于定时刷新 OLED 和上报
    static uint32_t last_blink_tick = 0;                // 用于 超阈值报警 LED 闪烁
    uint8_t oled_need_refresh = 0;                      // OLED 主动刷新标志位
    static uint32_t last_key_tick = 0;                  // 按键消抖
    static uint32_t last_upload_tick = 0;               // 10秒上报一次

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
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
    I2C_Bus_Init(&i2c1_ctx, &hi2c1);
    OLED_Init(&oled_ctx, &i2c1_ctx);
    SHT30_Init(&sht30_ctx, &i2c1_ctx);

    // 确保 UART 中断接收可正常挂起
    huart1.RxState = HAL_UART_STATE_READY;

    // 初始化 ESP8266 并建立 MQTT 连接
    if (ESP8266_Init(&esp8266_ctx, &huart1))
    {
        if (ESP8266_TCPConnect(&esp8266_ctx, "broker.emqx.io", 1883))
        {
            if (MQTT_Connect(&esp8266_ctx))
            {
                MQTT_Subscribe(&esp8266_ctx);
                init_ok = 1;
            }
            else
                OLED_ShowString(&oled_ctx, 0, 0, "MQTT fail");
        }
        else
            OLED_ShowString(&oled_ctx, 0, 0, "TCP fail");
    }
    else
        OLED_ShowString(&oled_ctx, 0, 0, "ESP fail");

    OLED_Refresh(&oled_ctx);
    HAL_Delay(3000);   // 短暂展示初始化结果

    // 初始化失败时点亮 LED 告警（PC13 低电平有效）
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, init_ok ? GPIO_PIN_SET : GPIO_PIN_RESET);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
    while (1)
    {
        // 处理云端指令（ipd_ready 由 UART 回调中的 IPD 状态机置位）
        if (esp8266_ctx.ipd_ready == 1)
        {
            uint16_t len = esp8266_ctx.ipd_recv_len;
            esp8266_ctx.ipd_ready = 0;

            // MQTT 报文含 0x00 字节，必须用 bin_search 而非 strstr
            if (bin_search(esp8266_ctx.ipd_buf, len, "LED_ON"))
                HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET); // 低电平点亮
            else if (bin_search(esp8266_ctx.ipd_buf, len, "LED_OFF"))
                HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);   // 高电平熄灭
        }

        // 处理按键，100ms轮询一次，兼顾消抖和长按速度
        if (HAL_GetTick() - last_key_tick >= 100)
        {
            last_key_tick = HAL_GetTick();

            if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_4) == GPIO_PIN_RESET)
            {
                temp_threshold++;
                oled_need_refresh = 1;
            }
            else if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_5) == GPIO_PIN_RESET)
            {
                temp_threshold--;
                oled_need_refresh = 1;
            }
            else if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_6) == GPIO_PIN_RESET)
            {
                humi_threshold++;
                oled_need_refresh = 1;
            }
            else if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_7) == GPIO_PIN_RESET)
            {
                humi_threshold--;
                oled_need_refresh = 1;
            }
    }

        // 超阈值报警闪烁
        if (temp >= temp_threshold || humi >= humi_threshold) 
        {
            if (HAL_GetTick() - last_blink_tick >= 100)
            {
                last_blink_tick = HAL_GetTick();
                HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_14);
            }
        }
        else
        {
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_14, GPIO_PIN_SET);
            last_blink_tick = HAL_GetTick();
        }

        // 每1秒读取温湿度
        if (HAL_GetTick() - last_sensor_tick >= 1000)
        {
            last_sensor_tick = HAL_GetTick();
            // 读取 SHT30 温湿度传感器（I2C，地址 0x44）
            SHT30_Read(&sht30_ctx, &temp, &humi);
            oled_need_refresh = 1;
        }

        // 刷新 OLED
        if (oled_need_refresh)
        {
            oled_need_refresh = 0;

            // 更新 OLED 显示：系统运行时长
            OLED_Clear(&oled_ctx);
            OLED_ShowString(&oled_ctx, 0, 16, "Second:");
            OLED_ShowNum(&oled_ctx, 60, 16, HAL_GetTick() / 1000, 0);

            // OLED 显示温湿度
            OLED_ShowFloat(&oled_ctx, 0, 32, temp, 2);
            OLED_ShowFloat(&oled_ctx, 0, 48, humi, 2);
            // OLED 显示温湿度阈值
            OLED_ShowString(&oled_ctx, 68, 32, "Max:");
            OLED_ShowNum(&oled_ctx, 104, 32, temp_threshold, 0);
            OLED_ShowString(&oled_ctx, 68, 48, "Max:");
            OLED_ShowNum(&oled_ctx, 104, 48, humi_threshold, 0);

            // OLED_ShowString(&oled_ctx, 0, 0, (char*)esp8266_ctx.rx_buf);

            OLED_Refresh(&oled_ctx);
        }

         // 每10秒上报，温湿度和温湿度阈值
        if (HAL_GetTick() - last_upload_tick >= 10000)
        {
            last_upload_tick = HAL_GetTick();

            if (init_ok)
            {
                MQTT_Publish_Sensor(&esp8266_ctx, temp, humi);
                MQTT_Publish_Threshold(&esp8266_ctx, temp_threshold, humi_threshold);
            }
        }

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
  /* USER CODE BEGIN MX_GPIO_Init_1 */
    // PC 13 14 15 LED
    // PA 4 5 6 7 按键
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15, GPIO_PIN_SET);

  /*Configure GPIO pins : PC13 PC14 PC15 */
  GPIO_InitStruct.Pin = GPIO_PIN_13|GPIO_PIN_14|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PA4 PA5 PA6 PA7 */
  GPIO_InitStruct.Pin = GPIO_PIN_4|GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

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
