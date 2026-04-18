#include "ws2812b_mode.h"

/**
 * @brief  效果 1：经典彩色流水
 * @param  delay_ms   速度
 * @param  brightness 亮度 0~255
 */
void Effect_ClassicFlow(WS2812B_Ctx_t *ctx, uint16_t delay_ms, uint8_t brightness)
{
    static uint16_t offset = 0;

    for (uint16_t i = 0; i < WS2812B_NUM_LEDS; i++) {
        uint16_t idx = (i + offset) % COLOR_TABLE_SIZE;
        uint8_t r = (color_table[idx].r * brightness) / 255;
        uint8_t g = (color_table[idx].g * brightness) / 255;
        uint8_t b = (color_table[idx].b * brightness) / 255;
        WS2812B_SetPixel(ctx, i, r, g, b);
    }

    WS2812B_Show(ctx);
    offset++;
    HAL_Delay(delay_ms);
}

/**
 * @brief  效果 2：双向溜溜球跑马
 * @param  delay_ms   速度
 * @param  brightness 亮度 0~255
 */
void Effect_YoYo(WS2812B_Ctx_t *ctx, uint16_t delay_ms, uint8_t brightness)
{
    static int16_t pos = 0;
    static int8_t dir = 1;
    static uint8_t color_idx = 0;

    WS2812B_Clear(ctx);

    for (uint8_t i = 0; i < 5; i++) {
        int16_t p = pos - i;
        if (p >= 0 && p < WS2812B_NUM_LEDS) {
            uint8_t r = (color_table[color_idx].r * brightness) / 255;
            uint8_t g = (color_table[color_idx].g * brightness) / 255;
            uint8_t b = (color_table[color_idx].b * brightness) / 255;
            WS2812B_SetPixel(ctx, p, r, g, b);
        }
    }

    WS2812B_Show(ctx);
    pos += dir;
    if (pos >= WS2812B_NUM_LEDS + 4) dir = -1;
    if (pos <= 0) dir = 1;
    HAL_Delay(delay_ms);
}

/**
 * @brief  效果 3：渐变拖尾流星
 * @param  delay_ms   速度
 * @param  brightness 亮度 0~255
 */
void Effect_CometTail(WS2812B_Ctx_t *ctx, uint16_t delay_ms, uint8_t brightness)
{
    static int pos = 0;
    static uint8_t color_idx = 0;
    const uint8_t tail_len = 8;

    WS2812B_Clear(ctx);

    if (pos >= 0 && pos < WS2812B_NUM_LEDS) {
        uint8_t r = (color_table[color_idx].r * brightness) / 255;
        uint8_t g = (color_table[color_idx].g * brightness) / 255;
        uint8_t b = (color_table[color_idx].b * brightness) / 255;
        WS2812B_SetPixel(ctx, pos, r, g, b);
    }

    for (uint8_t i = 1; i <= tail_len; i++) {
        int tp = pos - i;
        if (tp >= 0 && tp < WS2812B_NUM_LEDS) {
            uint8_t br = brightness * (255 - i*255/tail_len) / 255;
            uint8_t r = (color_table[color_idx].r * br) / 255;
            uint8_t g = (color_table[color_idx].g * br) / 255;
            uint8_t b = (color_table[color_idx].b * br) / 255;
            WS2812B_SetPixel(ctx, tp, r, g, b);
        }
    }

    WS2812B_Show(ctx);
    pos++;
    if (pos >= WS2812B_NUM_LEDS) {
        pos = 0;
        color_idx = (color_idx + 1) % COLOR_TABLE_SIZE;
    }
    HAL_Delay(delay_ms);
}

/**
 * @brief  效果 4：正弦波浪
 * @param  delay_ms   速度
 * @param  brightness 亮度 0~255
 */
void Effect_SinWave(WS2812B_Ctx_t *ctx, uint16_t delay_ms, uint8_t brightness)
{
    static float phase = 0;
    static uint8_t color_idx = 0;

    for (uint16_t i = 0; i < WS2812B_NUM_LEDS; i++) {
        float br = (sinf(3.14159f*2*i/WS2812B_NUM_LEDS + phase) + 1) * 0.5f;
        uint8_t r = color_table[color_idx].r * br * brightness / 255;
        uint8_t g = color_table[color_idx].g * br * brightness / 255;
        uint8_t b = color_table[color_idx].b * br * brightness / 255;
        WS2812B_SetPixel(ctx, i, r, g, b);
    }

    WS2812B_Show(ctx);
    phase += 0.15f;
    if (phase > 6.28f) {
        phase = 0;
        color_idx = (color_idx + 1) % COLOR_TABLE_SIZE;
    }
    HAL_Delay(delay_ms);
}

/**
 * @brief  效果 5：双色交替追逐
 * @param  delay_ms   速度
 * @param  brightness 亮度 0~255
 */
void Effect_DualChase(WS2812B_Ctx_t *ctx, uint16_t delay_ms, uint8_t brightness)
{
    static uint16_t pos = 0;
    static uint8_t c1 = 0, c2 = 5;

    WS2812B_Clear(ctx);

    for (int i = 0; i < 3; i++) {
        int p = (pos + i) % WS2812B_NUM_LEDS;
        uint8_t r = (color_table[c1].r * brightness) / 255;
        uint8_t g = (color_table[c1].g * brightness) / 255;
        uint8_t b = (color_table[c1].b * brightness) / 255;
        WS2812B_SetPixel(ctx, p, r, g, b);
    }
    for (int i = 0; i < 3; i++) {
        int p = (WS2812B_NUM_LEDS-1 - pos -i + WS2812B_NUM_LEDS) % WS2812B_NUM_LEDS;
        uint8_t r = (color_table[c2].r * brightness) / 255;
        uint8_t g = (color_table[c2].g * brightness) / 255;
        uint8_t b = (color_table[c2].b * brightness) / 255;
        WS2812B_SetPixel(ctx, p, r, g, b);
    }

    WS2812B_Show(ctx);
    pos++;
    if (pos >= WS2812B_NUM_LEDS) {
        pos = 0;
        c1 = (c1+1)%COLOR_TABLE_SIZE;
        c2 = (c2+2)%COLOR_TABLE_SIZE;
    }
    HAL_Delay(delay_ms);
}

/**
 * @brief  效果 6：整体呼吸渐变
 * @param  delay_ms   速度
 * @param  brightness 最大亮度 0~255
 */
void Effect_BreatheAll(WS2812B_Ctx_t *ctx, uint16_t delay_ms, uint8_t brightness)
{
    static uint8_t step = 0;
    static int8_t dir = 1;
    static uint8_t color_idx = 0;

    uint8_t br = (step * brightness) / 30;
    uint8_t r = (color_table[color_idx].r * br) / 255;
    uint8_t g = (color_table[color_idx].g * br) / 255;
    uint8_t b = (color_table[color_idx].b * br) / 255;
    WS2812B_Fill(ctx, r, g, b);
    WS2812B_Show(ctx);

    step += dir;
    if (step >= 30) dir = -1;
    if (step <= 0) {
        dir = 1;
        color_idx = (color_idx+1)%COLOR_TABLE_SIZE;
    }
    HAL_Delay(delay_ms);
}

/**
 * @brief  效果 7：Knight Rider 扫描
 * @param  delay_ms   速度
 * @param  brightness 亮度 0~255
 */
void Effect_KnightRider(WS2812B_Ctx_t *ctx, uint16_t delay_ms, uint8_t brightness)
{
    static int pos = 0;
    static int8_t dir = 1;
    static uint8_t color_idx = 0;

    WS2812B_Clear(ctx);
    uint8_t r = (color_table[color_idx].r * brightness) / 255;
    uint8_t g = (color_table[color_idx].g * brightness) / 255;
    uint8_t b = (color_table[color_idx].b * brightness) / 255;
    WS2812B_SetPixel(ctx, pos, r, g, b);
    WS2812B_Show(ctx);

    pos += dir;
    if (pos >= WS2812B_NUM_LEDS-1 || pos <= 0) {
        dir = -dir;
        if (pos <=0) color_idx = (color_idx+1)%COLOR_TABLE_SIZE;
    }
    HAL_Delay(delay_ms);
}

/**
 * @brief  效果 8：中间向两边扩散
 * @param  delay_ms   速度
 * @param  brightness 亮度 0~255
 */
void Effect_ExpandFromCenter(WS2812B_Ctx_t *ctx, uint16_t delay_ms, uint8_t brightness)
{
    static int step = 0;
    static uint8_t color_idx = 0;
    int center = WS2812B_NUM_LEDS / 2;

    WS2812B_Clear(ctx);
    int l = center - step;
    int r = center + step;
    if (l >=0) {
        uint8_t rr = (color_table[color_idx].r * brightness)/255;
        uint8_t gg = (color_table[color_idx].g * brightness)/255;
        uint8_t bb = (color_table[color_idx].b * brightness)/255;
        WS2812B_SetPixel(ctx, l, rr, gg, bb);
    }
    if (r < WS2812B_NUM_LEDS && r!=l) {
        uint8_t rr = (color_table[color_idx+1].r * brightness)/255;
        uint8_t gg = (color_table[color_idx+1].g * brightness)/255;
        uint8_t bb = (color_table[color_idx+1].b * brightness)/255;
        WS2812B_SetPixel(ctx, r, rr, gg, bb);
    }

    WS2812B_Show(ctx);
    step++;
    if (step > center) {
        step = 0;
        color_idx = (color_idx+2)%COLOR_TABLE_SIZE;
    }
    HAL_Delay(delay_ms);
}

/**
 * @brief  效果 9：固定颜色滚动
 * @param  delay_ms   速度
 * @param  brightness 亮度 0~255
 */
void Effect_FixedColorScroll(WS2812B_Ctx_t *ctx, uint16_t delay_ms, uint8_t brightness)
{
    static uint16_t offset = 0;
    uint8_t seg = WS2812B_NUM_LEDS / 3;

    for (uint16_t i = 0; i < WS2812B_NUM_LEDS; i++) {
        uint16_t p = (i + offset) % WS2812B_NUM_LEDS;
        uint8_t c = (p<seg) ? 0 : (p<seg*2) ? 3 : 7;
        uint8_t r = (color_table[c].r * brightness)/255;
        uint8_t g = (color_table[c].g * brightness)/255;
        uint8_t b = (color_table[c].b * brightness)/255;
        WS2812B_SetPixel(ctx, i, r, g, b);
    }

    WS2812B_Show(ctx);
    offset++;
    HAL_Delay(delay_ms);
}