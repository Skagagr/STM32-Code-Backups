/**
 * @file    epd.c
 * @brief   2.13寸墨水屏驱动实现（SSD1680Z8，250x122，黑白）
 *
 * @details
 *
 *  设计目标：
 *   - 实现SSD1680Z8完整初始化流程
 *   - 提供SPI驱动的图像刷新能力
 *   - 保证BUSY同步机制可靠性
 *   - 支持全屏刷新模式
 *
 *  依赖：
 *   - STM32 HAL SPI驱动
 *   - GPIO控制层
 *   - 外部供电与时序控制
 *
 *  约束：
 *   - 所有操作必须遵循芯片datasheet时序
 *   - SPI传输期间DC必须保持稳定
 *   - 刷新期间禁止修改RAM
 *
 * @version 1.0
 * @date    2025-04-18
 */

#include "epd.h"

/* -----------------------------------------------------------------------
 * GPIO 控制宏
 * ----------------------------------------------------------------------- */

#define EPD_RST_LOW()    HAL_GPIO_WritePin(EPD_GPIO_PORT, EPD_RST_PIN, GPIO_PIN_RESET)
#define EPD_RST_HIGH()   HAL_GPIO_WritePin(EPD_GPIO_PORT, EPD_RST_PIN, GPIO_PIN_SET)
#define EPD_DC_LOW()     HAL_GPIO_WritePin(EPD_GPIO_PORT, EPD_DC_PIN, GPIO_PIN_RESET)
#define EPD_DC_HIGH()    HAL_GPIO_WritePin(EPD_GPIO_PORT, EPD_DC_PIN, GPIO_PIN_SET)
#define EPD_CS_LOW()     HAL_GPIO_WritePin(EPD_GPIO_PORT, EPD_CS_PIN, GPIO_PIN_RESET)
#define EPD_CS_HIGH()    HAL_GPIO_WritePin(EPD_GPIO_PORT, EPD_CS_PIN, GPIO_PIN_SET)
#define EPD_IS_BUSY()    (HAL_GPIO_ReadPin(EPD_GPIO_PORT, EPD_BUSY_PIN) == GPIO_PIN_SET)

/* -----------------------------------------------------------------------
 * 底层通信
 * ----------------------------------------------------------------------- */

/**
 * @brief 等待BUSY释放
 */
static void epd_wait_busy(EPD_Ctx_t *ctx)
{
    // BUSY=1 表示内部刷新未完成，禁止任何SPI访问
    while (EPD_IS_BUSY())
    {
        HAL_Delay(1);
    }
}

/**
 * @brief 发送命令
 */
static void epd_send_cmd(EPD_Ctx_t *ctx, uint8_t cmd)
{
    EPD_DC_LOW();
    EPD_CS_LOW();

    HAL_SPI_Transmit(&ctx->hspi, &cmd, 1, 100);

    EPD_CS_HIGH();
}

/**
 * @brief 发送数据
 */
static void epd_send_data(EPD_Ctx_t *ctx, uint8_t data)
{
    EPD_DC_HIGH();
    EPD_CS_LOW();

    HAL_SPI_Transmit(&ctx->hspi, &data, 1, 100);

    EPD_CS_HIGH();
}

/**
 * @brief 批量发送数据
 */
static void epd_send_data_buf(EPD_Ctx_t *ctx, const uint8_t *buf, uint16_t len)
{
    // SPI必须保持DC=1连续传输，否则RAM写入会错位
    EPD_DC_HIGH();
    EPD_CS_LOW();

    HAL_SPI_Transmit(&ctx->hspi, (uint8_t *)buf, len, 5000);

    EPD_CS_HIGH();
}

/* -----------------------------------------------------------------------
 * 硬件初始化
 * ----------------------------------------------------------------------- */

/**
 * @brief SPI1初始化
 */
static void epd_spi_init(EPD_Ctx_t *ctx)
{
    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};

    gpio.Pin   = GPIO_PIN_5 | GPIO_PIN_7;
    gpio.Mode  = GPIO_MODE_AF_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &gpio);

    ctx->hspi.Instance = SPI1;
    ctx->hspi.Init.Mode = SPI_MODE_MASTER;

    // SSD1680 SPI最大频率约20MHz，72/8=9MHz安全
    ctx->hspi.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;

    ctx->hspi.Init.Direction         = SPI_DIRECTION_2LINES;
    ctx->hspi.Init.DataSize          = SPI_DATASIZE_8BIT;
    ctx->hspi.Init.CLKPolarity       = SPI_POLARITY_LOW;
    ctx->hspi.Init.CLKPhase          = SPI_PHASE_1EDGE;
    ctx->hspi.Init.NSS               = SPI_NSS_SOFT;
    ctx->hspi.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    ctx->hspi.Init.TIMode            = SPI_TIMODE_DISABLE;
    ctx->hspi.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;

    HAL_SPI_Init(&ctx->hspi);
}

/**
 * @brief GPIO初始化
 */
static void epd_gpio_init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef gpio = {0};

    gpio.Pin   = EPD_RST_PIN | EPD_DC_PIN | EPD_CS_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    gpio.Pull  = GPIO_NOPULL;

    HAL_GPIO_Init(EPD_GPIO_PORT, &gpio);

    gpio.Pin  = EPD_BUSY_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;

    HAL_GPIO_Init(EPD_GPIO_PORT, &gpio);

    // 上电默认CS不选中，避免误写指令
    EPD_CS_HIGH();
    EPD_RST_HIGH();
}

/* -----------------------------------------------------------------------
 * 控制流程
 * ----------------------------------------------------------------------- */

/**
 * @brief 硬件复位
 */
static void epd_hw_reset(EPD_Ctx_t *ctx)
{
    // RESET低电平必须保持>=10ms（SSD1680要求）
    EPD_RST_HIGH();
    HAL_Delay(10);

    EPD_RST_LOW();
    HAL_Delay(10);

    EPD_RST_HIGH();
    HAL_Delay(10);
}

/**
 * @brief 设置RAM窗口
 */
static void epd_set_window(EPD_Ctx_t *ctx)
{
    epd_send_cmd(ctx, EPD_CMD_SET_RAM_X);
    epd_send_data(ctx, 0x00);
    epd_send_data(ctx, 0x0F);

    epd_send_cmd(ctx, EPD_CMD_SET_RAM_Y);
    epd_send_data(ctx, 0x00);
    epd_send_data(ctx, 0x00);
    epd_send_data(ctx, 0xF9);
    epd_send_data(ctx, 0x00);
}

/**
 * @brief 设置RAM地址计数器
 */
static void epd_set_cursor(EPD_Ctx_t *ctx)
{
    epd_send_cmd(ctx, EPD_CMD_SET_RAM_X_COUNTER);
    epd_send_data(ctx, 0x00);

    epd_send_cmd(ctx, EPD_CMD_SET_RAM_Y_COUNTER);
    epd_send_data(ctx, 0x00);
    epd_send_data(ctx, 0x00);
}

/**
 * @brief 初始化序列（必须严格按手册顺序）
 */
static void epd_init_sequence(EPD_Ctx_t *ctx)
{
    epd_send_cmd(ctx, EPD_CMD_SW_RESET);
    epd_wait_busy(ctx);

    epd_send_cmd(ctx, EPD_CMD_DRIVER_OUTPUT);
    epd_send_data(ctx, 0xF9);
    epd_send_data(ctx, 0x00);
    epd_send_data(ctx, 0x00);

    epd_send_cmd(ctx, EPD_CMD_DATA_ENTRY_MODE);
    epd_send_data(ctx, 0x03);

    epd_set_window(ctx);

    epd_send_cmd(ctx, EPD_CMD_TEMP_SENSOR_CTRL);
    epd_send_data(ctx, 0x80);

    epd_send_cmd(ctx, EPD_CMD_BORDER_WAVEFORM);
    epd_send_data(ctx, 0x05);

    epd_send_cmd(ctx, EPD_CMD_DISPLAY_UPDATE_CTRL2);
    epd_send_data(ctx, 0xF8);

    epd_wait_busy(ctx);
}

/**
 * @brief 全屏刷新
 */
static void epd_refresh_full(EPD_Ctx_t *ctx)
{
    epd_send_cmd(ctx, EPD_CMD_DISPLAY_UPDATE_CTRL2);
    epd_send_data(ctx, 0xF7);

    epd_send_cmd(ctx, EPD_CMD_ACTIVATE_DISPLAY);

    epd_wait_busy(ctx);
}

/* -----------------------------------------------------------------------
 * API
 * ----------------------------------------------------------------------- */

void EPD_Init(EPD_Ctx_t *ctx)
{
    epd_gpio_init();
    epd_spi_init(ctx);

    HAL_Delay(10);

    epd_hw_reset(ctx);
    epd_init_sequence(ctx);
}

void EPD_Display(EPD_Ctx_t *ctx, const uint8_t *image)
{
    epd_set_cursor(ctx);

    epd_send_cmd(ctx, EPD_CMD_WRITE_BW_RAM);
    epd_send_data_buf(ctx, image, (EPD_WIDTH / 8) * EPD_HEIGHT);

    epd_refresh_full(ctx);
}

void EPD_Clear(EPD_Ctx_t *ctx)
{
    uint16_t i;
    uint16_t total = (EPD_WIDTH / 8) * EPD_HEIGHT;

    epd_set_cursor(ctx);

    epd_send_cmd(ctx, EPD_CMD_WRITE_BW_RAM);

    EPD_DC_HIGH();
    EPD_CS_LOW();

    for (i = 0; i < total; i++)
    {
        uint8_t v = 0xFF;
        HAL_SPI_Transmit(&ctx->hspi, &v, 1, 100);
    }

    EPD_CS_HIGH();

    epd_refresh_full(ctx);
}

void EPD_Sleep(EPD_Ctx_t *ctx)
{
    epd_send_cmd(ctx, EPD_CMD_DEEP_SLEEP);
    epd_send_data(ctx, 0x01);
}

void EPD_Wakeup(EPD_Ctx_t *ctx)
{
    epd_hw_reset(ctx);
    epd_init_sequence(ctx);
}