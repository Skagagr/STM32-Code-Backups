/**
 * @file    gfx.h
 * @brief   墨水屏2D绘图基础图形库接口（Framebuffer方式）
 *
 * @details
 *
 *  设计目标：
 *   - 提供像素级绘图API
 *   - 使用Framebuffer作为中间缓存
 *   - 屏蔽EPD底层驱动复杂性
 *
 *  依赖：
 *   - epd.h（显示驱动）
 *   - string.h（memset）
 *
 *  约束：
 *   - framebuffer必须与EPD分辨率一致
 *   - 1bit黑白像素存储
 *   - 坐标系固定左上角为原点
 *
 * @version 1.0
 * @date    2026-04-18
 */

#ifndef __GFX_H
#define __GFX_H

#include "epd.h"
#include <stdint.h>
#include <string.h>

/* -----------------------------------------------------------------------
 * 屏幕参数（与 epd.h v1.0 完全一致）
 * ----------------------------------------------------------------------- */

#define GFX_PIXEL_WIDTH   EPD_PIXEL_WIDTH
#define GFX_PIXEL_HEIGHT  EPD_PIXEL_HEIGHT
#define GFX_BYTE_WIDTH    (EPD_WIDTH / 8)
#define GFX_BUF_SIZE      (EPD_WIDTH / 8 * EPD_HEIGHT)

/* -----------------------------------------------------------------------
 * 颜色定义
 * ----------------------------------------------------------------------- */

#define GFX_WHITE   0xFF
#define GFX_BLACK   0x00

/* -----------------------------------------------------------------------
 * 公开 API
 * ----------------------------------------------------------------------- */

/**
 * @brief 初始化图形模块
 */
void GFX_Init(void);

/**
 * @brief 清屏
 *
 * @param color 填充颜色
 */
void GFX_Clear(uint8_t color);

/**
 * @brief 绘制像素点
 *
 * @param x     X坐标
 * @param y     Y坐标
 * @param color 像素颜色
 */
void GFX_DrawPixel(int16_t x, int16_t y, uint8_t color);

/**
 * @brief 像素绘制函数（核心函数修改版）
 *
 * @details
 * 因为我将屏幕横置了，x和y就要反着写，所有封装了一下
 * 
 * @param x     X坐标
 * @param y     Y坐标
 * @param color 像素颜色
 */
void GFX_DrawPixel_True(int16_t x, int16_t y, uint8_t color);

/**
 * @brief 绘制单个字符
 * 
 * @param x x 轴所在的位置
 * @param y y 轴所在的位置
 * @param glyph 字模数据，行扫描，从右往左取模
 */
void GFX_DrawChar(uint8_t x, uint8_t y, const uint8_t font);

/**
 * @brief 绘制字符串
 * 
 * @param x x 轴所在的位置
 * @param y y 轴所在的位置
 * @param str 字符串
 */
void GFX_DrawString(uint8_t x, uint8_t y, const char *str);

/**
 * @brief 刷新显示
 *
 * @param epd EPD句柄
 */
void GFX_Refresh(EPD_Ctx_t *epd);

#endif