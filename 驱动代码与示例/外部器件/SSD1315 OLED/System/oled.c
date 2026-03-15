/**
 * @file    oled.c
 * @brief   SSD1315 OLED 驱动实现（128×64，I2C 阻塞模式）
 *
 * @details
 *  所有绘图操作写入内部 GRAM 缓冲区，调用 OLED_Refresh() 后批量上屏。
 *  底层每字节独立一帧发送，与 SSD1315 I2C 时序完全兼容。
 *
 * @version 1.2.0
 * @date    2026-03-15
 */

#include "oled.h"
#include "font_chinese.h"
#include <string.h>
#include <stdio.h>

/* ─────────────────────────────────────────────────────────────────────────────
 * 私有：底层发送
 * ───────────────────────────────────────────────────────────────────────────*/

/**
 * @brief  发送单条命令（独立 I2C 帧）
 * @details
 *  帧格式：START | addr | 0x00（Co=0, D/C#=0）| cmd | STOP
 *  每条命令独立一帧，防止 SSD1315 把参数字节误判为新命令。
 */
static uint8_t oled_cmd(OLED_Ctx *ctx, uint8_t cmd)
{
    uint8_t buf[2] = {0x00, cmd};
    return I2C_Bus_Write(ctx->bus, OLED_I2C_ADDR, buf, 2);
}

/**
 * @brief  发送单字节数据（独立 I2C 帧）
 * @details
 *  帧格式：START | addr | 0x40（Co=0, D/C#=1）| data | STOP
 */
static uint8_t oled_data(OLED_Ctx *ctx, uint8_t data)
{
    uint8_t buf[2] = {0x40, data};
    return I2C_Bus_Write(ctx->bus, OLED_I2C_ADDR, buf, 2);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * 初始化
 * ───────────────────────────────────────────────────────────────────────────*/

uint8_t OLED_Init(OLED_Ctx *ctx, I2C_Bus_Ctx *bus)
{
    if (ctx == NULL || bus == NULL) return 0;

    ctx->bus    = bus;
    ctx->inited = 0;
    memset(ctx->gram, 0, sizeof(ctx->gram));

    HAL_Delay(100); /* 上电稳定延时 */

/* 逐条发送初始化命令，每条独立一帧 */
#define CMD(c)  oled_cmd(ctx, (c))

    CMD(0xAE);        /* 关闭显示 */
    CMD(0xD5);        /* 时钟分频 */
    CMD(0x80);
    CMD(0xA8);        /* 多路复用率 */
    CMD(0x3F);        /*   64 行 */
    CMD(0xD3);        /* 显示偏移 */
    CMD(0x00);
    CMD(0x40);        /* 起始行 = 0 */
    CMD(0x8D);        /* 电荷泵 */
    CMD(0x14);        /*   开启 */
    CMD(0x20);        /* 页寻址模式 */
    CMD(0x02);
    CMD(0xA1);        /* 列地址重映射（正向）*/
    CMD(0xC8);        /* COM 扫描方向 */
    CMD(0xDA);        /* COM 引脚配置 */
    CMD(0x12);
    CMD(0x81);        /* 对比度 */
    CMD(0xCF);
    CMD(0xD9);        /* 预充电周期 */
    CMD(0xF1);
    CMD(0xDB);        /* VCOMH 电平 */
    CMD(0x30);
    CMD(0xA4);        /* 显示跟随 GRAM */
    CMD(0xA6);        /* 正常显示（不反色）*/
    CMD(0xAF);        /* 开启显示 */

#undef CMD

    ctx->inited = 1;

    OLED_Clear(ctx);
    OLED_Refresh(ctx);
    return 1;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * 刷屏 / 清屏
 * ───────────────────────────────────────────────────────────────────────────*/

uint8_t OLED_Refresh(OLED_Ctx *ctx)
{
    if (ctx == NULL || !ctx->inited) return 0;

    for (uint8_t page = 0; page < OLED_PAGES; page++)
    {
        oled_cmd(ctx, (uint8_t)(0xB0 + page)); /* 页地址 */
        oled_cmd(ctx, 0x10);                   /* 列高 4 位 = 0 */
        oled_cmd(ctx, 0x00);                   /* 列低 4 位 = 0 */
        for (uint8_t col = 0; col < OLED_WIDTH; col++)
            oled_data(ctx, ctx->gram[page][col]);
    }
    return 1;
}

uint8_t OLED_Clear(OLED_Ctx *ctx)
{
    if (ctx == NULL) return 0;
    memset(ctx->gram, 0, sizeof(ctx->gram));
    return 1;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * 屏幕控制
 * ───────────────────────────────────────────────────────────────────────────*/

uint8_t OLED_SetContrast(OLED_Ctx *ctx, uint8_t contrast)
{
    if (ctx == NULL || !ctx->inited) return 0;
    oled_cmd(ctx, 0x81);
    return oled_cmd(ctx, contrast);
}

uint8_t OLED_SetDisplay(OLED_Ctx *ctx, uint8_t enable)
{
    if (ctx == NULL || !ctx->inited) return 0;
    return oled_cmd(ctx, enable ? 0xAF : 0xAE);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * 绘图
 * ───────────────────────────────────────────────────────────────────────────*/

uint8_t OLED_DrawPixel(OLED_Ctx *ctx, uint8_t x, uint8_t y, uint8_t color)
{
    if (ctx == NULL || x >= OLED_WIDTH || y >= OLED_HEIGHT) return 0;
    uint8_t page = y / 8;
    uint8_t bit  = y % 8;
    if (color) ctx->gram[page][x] |=  (uint8_t)(1u << bit);
    else       ctx->gram[page][x] &= ~(uint8_t)(1u << bit);
    return 1;
}

uint8_t OLED_DrawHLine(OLED_Ctx *ctx, uint8_t x, uint8_t y, uint8_t len, uint8_t color)
{
    if (ctx == NULL || x >= OLED_WIDTH || y >= OLED_HEIGHT) return 0;
    for (uint8_t i = 0; i < len && (x + i) < OLED_WIDTH; i++)
        OLED_DrawPixel(ctx, (uint8_t)(x + i), y, color);
    return 1;
}

uint8_t OLED_DrawVLine(OLED_Ctx *ctx, uint8_t x, uint8_t y, uint8_t len, uint8_t color)
{
    if (ctx == NULL || x >= OLED_WIDTH || y >= OLED_HEIGHT) return 0;
    for (uint8_t i = 0; i < len && (y + i) < OLED_HEIGHT; i++)
        OLED_DrawPixel(ctx, x, (uint8_t)(y + i), color);
    return 1;
}

uint8_t OLED_DrawRect(OLED_Ctx *ctx, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color)
{
    if (ctx == NULL) return 0;
    OLED_DrawHLine(ctx, x,                    y,                    w, color);
    OLED_DrawHLine(ctx, x,                    (uint8_t)(y + h - 1), w, color);
    OLED_DrawVLine(ctx, x,                    y,                    h, color);
    OLED_DrawVLine(ctx, (uint8_t)(x + w - 1), y,                    h, color);
    return 1;
}

uint8_t OLED_FillRect(OLED_Ctx *ctx, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color)
{
    if (ctx == NULL) return 0;
    for (uint8_t row = 0; row < h && (y + row) < OLED_HEIGHT; row++)
        OLED_DrawHLine(ctx, x, (uint8_t)(y + row), w, color);
    return 1;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * ASCII 字符显示（8×16）
 * ───────────────────────────────────────────────────────────────────────────*/

uint8_t OLED_ShowChar(OLED_Ctx *ctx, uint8_t x, uint8_t y, char ch)
{
    if (ctx == NULL || !ctx->inited) return 0;
    if (ch < 0x20 || ch > 0x7E)     return 0;
    if (x >= OLED_WIDTH)             return 0;

    uint8_t idx  = (uint8_t)(ch - 0x20);
    uint8_t page = y / 8;
    uint8_t boff = y % 8;
    const uint8_t *font = g_font_ascii_8x16[idx];

    /* 字模格式：行优先，高位在左
     * font[0..7]  = 上半部分每行，font[8..15] = 下半部分每行
     * 转换为列优先写入 GRAM                                   */
    for (uint8_t col = 0; col < 8 && (x + col) < OLED_WIDTH; col++)
    {
        uint16_t col_data = 0;
        for (uint8_t row = 0; row < 8; row++)
        {
            if (font[row]     & (0x80 >> col)) col_data |= (uint16_t)(1u << row);
            if (font[row + 8] & (0x80 >> col)) col_data |= (uint16_t)(1u << (row + 8));
        }
        col_data <<= boff;
        if (page     < OLED_PAGES) ctx->gram[page    ][x + col] |= (uint8_t)(col_data & 0xFF);
        if ((page+1) < OLED_PAGES) ctx->gram[page + 1][x + col] |= (uint8_t)(col_data >> 8);
    }
    return 1;
}

uint8_t OLED_ShowString(OLED_Ctx *ctx, uint8_t x, uint8_t y, const char *str)
{
    if (ctx == NULL || str == NULL) return 0;
    uint8_t cx = x;
    while (*str)
    {
        if (cx + 8 > OLED_WIDTH) break;
        OLED_ShowChar(ctx, cx, y, *str++);
        cx += 8;
    }
    return 1;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * 数值显示
 * ───────────────────────────────────────────────────────────────────────────*/

uint8_t OLED_ShowNum(OLED_Ctx *ctx, uint8_t x, uint8_t y, uint32_t num, uint8_t width)
{
    if (ctx == NULL) return 0;
    char buf[12];
    if (width == 0) snprintf(buf, sizeof(buf), "%lu",  (unsigned long)num);
    else            snprintf(buf, sizeof(buf), "%*lu", width, (unsigned long)num);
    return OLED_ShowString(ctx, x, y, buf);
}

uint8_t OLED_ShowInt(OLED_Ctx *ctx, uint8_t x, uint8_t y, int32_t num, uint8_t width)
{
    if (ctx == NULL) return 0;
    char buf[13];
    if (width == 0) snprintf(buf, sizeof(buf), "%ld",  (long)num);
    else            snprintf(buf, sizeof(buf), "%*ld", width, (long)num);
    return OLED_ShowString(ctx, x, y, buf);
}

uint8_t OLED_ShowFloat(OLED_Ctx *ctx, uint8_t x, uint8_t y, float num, uint8_t decimal)
{
    if (ctx == NULL || decimal > 4) return 0;
    char buf[20];
    snprintf(buf, sizeof(buf), "%.*f", decimal, (double)num);
    return OLED_ShowString(ctx, x, y, buf);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * 中文显示（UTF-8，16×16）
 * ───────────────────────────────────────────────────────────────────────────*/

/**
 * @brief  将单个汉字字模绘制到 GRAM
 * @details
 *  字模格式（PCtoLCD2002 列行式逆向）：
 *  bitmap[0..15]  = 左半部分，每字节对应一列的低8行
 *  bitmap[16..31] = 右半部分，每字节对应一列的高8行
 *  直接按列拼成 16bit 写入 GRAM，无需位变换。
 */
static void draw_chinese_char(OLED_Ctx *ctx, uint8_t x, uint8_t y,
                              const uint8_t *bitmap)
{
    uint8_t page = y / 8;
    uint8_t boff = y % 8;

    for (uint8_t col = 0; col < 16 && (x + col) < OLED_WIDTH; col++)
    {
        /* 直接拼接：低字节=上8行，高字节=下8行 */
        uint16_t col_data = (uint16_t)bitmap[col] |
                            ((uint16_t)bitmap[col + 16] << 8);
        col_data <<= boff;

        if (page     < OLED_PAGES) ctx->gram[page    ][x+col] |= (uint8_t)(col_data & 0xFF);
        if ((page+1) < OLED_PAGES) ctx->gram[page + 1][x+col] |= (uint8_t)((col_data >> 8) & 0xFF);
        if ((page+2) < OLED_PAGES) ctx->gram[page + 2][x+col] |= (uint8_t)((col_data >> 16) & 0xFF);
    }
}

uint8_t OLED_ShowChinese(OLED_Ctx *ctx, uint8_t x, uint8_t y, const char *str)
{
    if (ctx == NULL || str == NULL || !ctx->inited) return 0;

    const uint8_t *p  = (const uint8_t *)str;
    uint8_t        cx = x;

    while (*p)
    {
        if (cx + 16 > OLED_WIDTH) break;

        if ((*p & 0xE0) == 0xE0)
        {
            /* UTF-8 三字节汉字 */
            if (!*(p+1) || !*(p+2)) break;

            for (uint8_t i = 0; i < CHINESE_FONT_COUNT; i++)
            {
                if (g_chinese_font[i].utf8[0] == p[0] &&
                    g_chinese_font[i].utf8[1] == p[1] &&
                    g_chinese_font[i].utf8[2] == p[2])
                {
                    draw_chinese_char(ctx, cx, y, g_chinese_font[i].bitmap);
                    break;
                }
            }
            p += 3; cx += 16;
        }
        else if ((*p & 0x80) == 0)
        {
            /* ASCII，跳过（用 OLED_ShowString 处理）*/
            p++; cx += 8;
        }
        else
        {
            /* 双字节 UTF-8，跳过 */
            p += ((*p & 0xC0) == 0xC0) ? 2 : 1;
            cx += 8;
        }
    }
    return 1;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * CubeMX 配置说明
 * ───────────────────────────────────────────────────────────────────────────
 *
 * 【Connectivity → I2C1】
 *   ├─ I2C Speed Mode : Standard Mode（100 kHz）
 *   └─ 其余保持默认
 *
 * 【NVIC Settings】
 *   └─ 阻塞模式无需开启 I2C 中断
 *
 * 【接线说明】
 *   ├─ PB6 → SCL（需接 4.7kΩ 上拉至 3.3V）
 *   ├─ PB7 → SDA（需接 4.7kΩ 上拉至 3.3V）
 *   ├─ VCC → 3.3V
 *   └─ GND → GND
 *
 * 【SSD1315 I2C 地址】
 *   ├─ SA0 接 GND → 0x3C（8位格式 0x78）← 默认
 *   └─ SA0 接 VCC → 0x3D（8位格式 0x7A），修改 oled.h 中 OLED_I2C_ADDR 为 0x7A
 *
 * 【MX_I2C1_Init 需加复位代码（STM32F103 I2C bug）】
 *   __HAL_RCC_I2C1_FORCE_RESET();
 *   HAL_Delay(10);
 *   __HAL_RCC_I2C1_RELEASE_RESET();
 *   HAL_Delay(10);
 *   // 加在 HAL_I2C_Init() 之前
 *
 * 【CMakeLists.txt（浮点 printf）】
 *   target_link_options(${PROJECT_NAME}.elf PRIVATE -u _printf_float)
 *
 * 【常见显示异常排查】
 *   ├─ 全屏雪花    → 初始化命令未送达，检查 I2C 地址和接线
 *   ├─ 全黑无显示  → I2C 通信失败，用 I2C_Bus_IsDeviceReady 确认设备在线
 *   ├─ 上下颠倒    → 将 0xA1 改 0xA0，0xC8 改 0xC0
 *   ├─ 字符旋转    → OLED_ShowChar 中行列转换方向有误
 *   └─ 汉字上下反  → draw_chinese_char 中字节高低位顺序有误
 *
 * ───────────────────────────────────────────────────────────────────────────*/