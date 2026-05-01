/**
 * @file    gfx.c
 * @brief   墨水屏2D绘图基础图形库实现（Framebuffer方式）
 *
 * @details
 *
 *  设计目标：
 *   - 提供像素级绘图能力（DrawPixel）
 *   - 使用独立Framebuffer进行图像缓存
 *   - 解耦EPD驱动，实现上层图形抽象
 *
 *  依赖：
 *   - gfx.h
 *   - epd.h（EPD_Display接口）
 *   - STM32 HAL SPI/GPIO间接依赖
 *
 *  约束：
 *   - framebuffer必须与屏幕分辨率严格匹配
 *   - 采用1bit黑白像素格式
 *   - 坐标原点固定为左上角(0,0)
 *
 * @version 1.0
 * @date    2026-04-18
 */

#include "gfx.h"
#include "font_8x8.h"
#include <stdint.h>

/* -----------------------------------------------------------------------
 * 私有：显存数组
 * ----------------------------------------------------------------------- */

static uint8_t framebuffer[GFX_BUF_SIZE]; // Framebuffer存储整屏像素数据

/* -----------------------------------------------------------------------
 * 公开 API 实现
 * ----------------------------------------------------------------------- */

/**
 * @brief 初始化图形系统
 *
 * 初始化Framebuffer为白色背景
 */
void GFX_Init(void)
{
    GFX_Clear(GFX_WHITE); // 清屏为白色
}

/**
 * @brief 清空Framebuffer
 *
 * @param color 填充颜色（GFX_WHITE / GFX_BLACK）
 */
void GFX_Clear(uint8_t color)
{
    memset(framebuffer, color, sizeof(framebuffer)); // 批量填充显存
}

/**
 * @brief 像素绘制函数（核心函数）
 *
 * @details
 * 坐标系统：
 *  - 原点：左上角
 *  - X：向右递增
 *  - Y：向下递增（逻辑坐标）
 *
 * @param x     X坐标
 * @param y     Y坐标
 * @param color 像素颜色
 */
void GFX_DrawPixel(int16_t x, int16_t y, uint8_t color)
{
    /* 越界检查 */
    if (x < 0 || x >= GFX_PIXEL_WIDTH || y < 0 || y >= GFX_PIXEL_HEIGHT)
    {
        return;
    }

    /* 坐标转换（Y轴翻转匹配物理RAM） */
    int16_t ram_x = x;
    int16_t ram_y = (GFX_PIXEL_HEIGHT - 1) - y;

    /* 计算字节索引 */
    uint16_t byte_idx = (ram_y * GFX_BYTE_WIDTH) + (ram_x / 8);
    uint8_t bit_idx = 7 - (ram_x % 8);

    /* 写入像素 */
    if (color == GFX_BLACK)
    {
        framebuffer[byte_idx] &= ~(1 << bit_idx); // 置黑
    }
    else
    {
        framebuffer[byte_idx] |= (1 << bit_idx);  // 置白
    }
}

/**
 * @brief 像素绘制函数（核心函数修改版）
 *
 * @details
 * 坐标系统：
 *  - 原点：左上角
 *  - X：向右递增
 *  - Y：向下递增（逻辑坐标）
 * 因为我将屏幕横置了，x和y就要反着写，所有封装了一下
 * @param x     X坐标
 * @param y     Y坐标
 * @param color 像素颜色
 */
void GFX_DrawPixel_True(int16_t x, int16_t y, uint8_t color)
{
    GFX_DrawPixel(y, x, color);
}

/**
 * @brief 绘制单个字符
 * 
 * @param x x 轴所在的位置
 * @param y y 轴所在的位置
 * @param font 字符
 */
void GFX_DrawChar(uint8_t x, uint8_t y, const uint8_t font)
{
    x--;
    y--;
    for (uint8_t Dy = 0; Dy < 8; Dy++)
    {
        for (uint8_t Dx = 0; Dx < 8; Dx++)
        {
            uint8_t byte = font_8x8[font][Dy];

            if ((byte >> Dx) & 1)
                GFX_DrawPixel_True(Dx + x * 8, Dy + y * 8, GFX_BLACK);
            else
                GFX_DrawPixel_True(Dx + x * 8, Dy + y * 8, GFX_WHITE);
        }
    }
}

/**
 * @brief 绘制字符串
 * 
 * @param x x 轴所在的位置
 * @param y y 轴所在的位置
 * @param str 字符串
 */
void GFX_DrawString(uint8_t x, uint8_t y, const char *str)
{
    while (*str)
    {
        GFX_DrawChar(x, y, (uint8_t)*str);
        x++;
        str++;
    }
}

/**
 * @brief 刷新显示到墨水屏
 *
 * @param epd EPD设备句柄
 */
void GFX_Refresh(EPD_Ctx_t *epd)
{
    EPD_Display(epd, framebuffer); // 提交Framebuffer到屏幕
}