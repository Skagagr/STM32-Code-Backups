#ifndef __WS2812BMODE_H
#define __WS2812BMODE_H

#include "main.h"
#include <math.h>
#include "ws2812b.h"

// 颜色表 —— 修改这里可自定义所有效果的颜色
static const WS2812B_Color_t color_table[] = {
    {255,   0,   0},  // 红
    {255, 128,   0},  // 橙
    {255, 255,   0},  // 黄
    {  0, 255,   0},  // 绿
    {  0, 255, 128},  // 青绿
    {  0, 255, 255},  // 青
    {  0, 128, 255},  // 天蓝
    {  0,   0, 255},  // 蓝
    {128,   0, 255},  // 紫
    {255,   0, 255},  // 品红
    {255,   0, 128},  // 玫红
    {255, 255, 255},  // 白
};
#define COLOR_TABLE_SIZE (sizeof(color_table) / sizeof(color_table[0]))

/**
 * @brief  效果 1：经典彩色流水
 * @param  delay_ms   速度
 * @param  brightness 亮度 0~255
 */
void Effect_ClassicFlow(WS2812B_Ctx_t *ctx, uint16_t delay_ms, uint8_t brightness);

/**
 * @brief  效果 2：双向溜溜球跑马
 * @param  delay_ms   速度
 * @param  brightness 亮度 0~255
 */
void Effect_YoYo(WS2812B_Ctx_t *ctx, uint16_t delay_ms, uint8_t brightness);

/**
 * @brief  效果 3：渐变拖尾流星
 * @param  delay_ms   速度
 * @param  brightness 亮度 0~255
 */
void Effect_CometTail(WS2812B_Ctx_t *ctx, uint16_t delay_ms, uint8_t brightness);

/**
 * @brief  效果 4：正弦波浪
 * @param  delay_ms   速度
 * @param  brightness 亮度 0~255
 */
void Effect_SinWave(WS2812B_Ctx_t *ctx, uint16_t delay_ms, uint8_t brightness);

/**
 * @brief  效果 5：双色交替追逐
 * @param  delay_ms   速度
 * @param  brightness 亮度 0~255
 */
void Effect_DualChase(WS2812B_Ctx_t *ctx, uint16_t delay_ms, uint8_t brightness);

/**
 * @brief  效果 6：整体呼吸渐变
 * @param  delay_ms   速度
 * @param  brightness 最大亮度 0~255
 */
void Effect_BreatheAll(WS2812B_Ctx_t *ctx, uint16_t delay_ms, uint8_t brightness);

/**
 * @brief  效果 7：Knight Rider 扫描
 * @param  delay_ms   速度
 * @param  brightness 亮度 0~255
 */
void Effect_KnightRider(WS2812B_Ctx_t *ctx, uint16_t delay_ms, uint8_t brightness);

/**
 * @brief  效果 8：中间向两边扩散
 * @param  delay_ms   速度
 * @param  brightness 亮度 0~255
 */
void Effect_ExpandFromCenter(WS2812B_Ctx_t *ctx, uint16_t delay_ms, uint8_t brightness);

/**
 * @brief  效果 9：固定颜色滚动
 * @param  delay_ms   速度
 * @param  brightness 亮度 0~255
 */
void Effect_FixedColorScroll(WS2812B_Ctx_t *ctx, uint16_t delay_ms, uint8_t brightness);

#endif