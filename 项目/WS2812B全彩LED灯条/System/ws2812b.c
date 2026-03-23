/**
 * @file    ws2812b.c
 * @brief   WS2812B 彩灯驱动实现 — TIM3 CH1 + DMA1 Channel6
 *
 * @details
 *  将 GRB 像素数据编码为 PWM DMA 缓冲区，每次调用 WS2812B_Show()
 *  触发一次 DMA 传输。复位信号（>50µs 低电平）以零 CCR 槽的形式
 *  追加在缓冲区末尾，传输结束后无需额外 HAL_Delay()。
 *  首次调用 WS2812B_Show() 直接启动 DMA，后续调用先 Stop 再 Start，
 *  避免 DMA 未运行时调用 Stop 导致 HardFault。
 *
 * @version 1.0.0
 * @date    2026-03-18
 */

#include "ws2812b.h"
#include <string.h>

/* ─────────────────────────────────────────────────────────────────────────────
 * 私有函数
 * ─────────────────────────────────────────────────────────────────────────────*/

/**
 * @brief  将一个字节编码为 8 个 PWM CCR 值写入 DMA 缓冲区（高位在前）
 *
 * @details
 *  从 bit7 到 bit0 依次判断，为 1 则填入 WS2812B_BIT1_CCR，
 *  为 0 则填入 WS2812B_BIT0_CCR，共写入 8 个 uint16_t。
 *
 * @param  buf   目标缓冲区指针，需保证有连续 8 个 uint16_t 的空间
 * @param  byte  待编码的数据字节
 */
static void _encode_byte(uint16_t *buf, uint8_t byte)
{
    for (int i = 7; i >= 0; i--)
    {
        buf[7 - i] = (byte & (1u << i)) ? WS2812B_BIT1_CCR : WS2812B_BIT0_CCR;
    }
}

/**
 * @brief  根据像素数组重建完整的 DMA 传输缓冲区
 *
 * @details
 *  WS2812B 线序为 G → R → B，每颗灯共 24 bit。
 *  缓冲区末尾追加 WS2812B_RESET_SLOTS 个零值作为复位信号。
 *
 * @param  ctx  指向上下文的指针
 */
static void _build_dma_buffer(WS2812B_Ctx_t *ctx)
{
    uint16_t *p = ctx->_dma_buf;

    for (uint16_t i = 0; i < WS2812B_NUM_LEDS; i++)
    {
        _encode_byte(p, ctx->leds[i].g);  p += 8;  /* 绿色先发 */
        _encode_byte(p, ctx->leds[i].r);  p += 8;  /* 红色其次 */
        _encode_byte(p, ctx->leds[i].b);  p += 8;  /* 蓝色最后 */
    }

    /* 复位段：填零使输出持续低电平 ≥50µs */
    memset(p, 0, WS2812B_RESET_SLOTS * sizeof(uint16_t));
}

/* ─────────────────────────────────────────────────────────────────────────────
 * 初始化
 * ─────────────────────────────────────────────────────────────────────────────*/

uint8_t WS2812B_Init(WS2812B_Ctx_t *ctx,
                     TIM_HandleTypeDef *htim,
                     uint32_t channel)
{
    if (ctx == NULL || htim == NULL) return 0;

    ctx->htim      = htim;
    ctx->channel   = channel;
    ctx->_tx_done  = 1;  /* 初始状态视为空闲 */
    ctx->_first_tx = 1;  /* 标记尚未传输过 */

    memset(ctx->leds,     0, sizeof(ctx->leds));
    memset(ctx->_dma_buf, 0, sizeof(ctx->_dma_buf));

    return 1;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * 像素控制
 * ─────────────────────────────────────────────────────────────────────────────*/

uint8_t WS2812B_SetPixel(WS2812B_Ctx_t *ctx,
                          uint16_t index,
                          uint8_t r, uint8_t g, uint8_t b)
{
    if (ctx == NULL || index >= WS2812B_NUM_LEDS) return 0;

    ctx->leds[index].r = r;
    ctx->leds[index].g = g;
    ctx->leds[index].b = b;

    return 1;
}

uint8_t WS2812B_Fill(WS2812B_Ctx_t *ctx,
                     uint8_t r, uint8_t g, uint8_t b)
{
    if (ctx == NULL) return 0;

    for (uint16_t i = 0; i < WS2812B_NUM_LEDS; i++)
    {
        ctx->leds[i].r = r;
        ctx->leds[i].g = g;
        ctx->leds[i].b = b;
    }

    return 1;
}

uint8_t WS2812B_Clear(WS2812B_Ctx_t *ctx)
{
    return WS2812B_Fill(ctx, 0, 0, 0);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * DMA 推送
 * ─────────────────────────────────────────────────────────────────────────────*/

uint8_t WS2812B_Show(WS2812B_Ctx_t *ctx)
{
    if (ctx == NULL) return 0;

    /* 等待上一次 DMA 传输结束，防止缓冲区被覆盖 */
    while (!ctx->_tx_done) {}
    ctx->_tx_done = 0;

    _build_dma_buffer(ctx);

    uint16_t total = WS2812B_NUM_LEDS * 24 + WS2812B_RESET_SLOTS;

    /* 首次传输直接启动，后续先 Stop 再 Start
     * 避免 DMA 未运行时调用 Stop_DMA 导致 HardFault */
    if (!ctx->_first_tx)
    {
        HAL_TIM_PWM_Stop_DMA(ctx->htim, ctx->channel);
    }
    ctx->_first_tx = 0;

    HAL_TIM_PWM_Start_DMA(ctx->htim, ctx->channel,
                           (uint32_t *)ctx->_dma_buf, total);

    return 1;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * 回调处理
 * ─────────────────────────────────────────────────────────────────────────────*/

void WS2812B_IRQHandler(WS2812B_Ctx_t *ctx, TIM_HandleTypeDef *htim)
{
    if (ctx == NULL) return;

    /* 仅响应本实例对应的定时器 */
    if (htim->Instance == ctx->htim->Instance)
    {
        HAL_TIM_PWM_Stop_DMA(ctx->htim, ctx->channel);
        ctx->_tx_done = 1;  /* 通知 WS2812B_Show() 传输已完成 */
    }
}

/* ─────────────────────────────────────────────────────────────────────────────
 * CubeMX 配置说明
 * ─────────────────────────────────────────────────────────────────────────────
 *
 * 【Timers → TIM3】
 *   ├─ Clock Source      : Internal Clock
 *   ├─ Channel1          : PWM Generation CH1
 *   ├─ Prescaler (PSC)   : 0
 *   ├─ Counter Period    : 89        → 72MHz 下周期 1.25µs
 *   ├─ PWM Mode          : PWM Mode 1
 *   └─ 其余保持默认
 *
 * 【DMA Settings（TIM3 → DMA1 Channel6）】
 *   ├─ DMA Request       : TIM3_CH1/TRIG
 *   ├─ Channel           : DMA1 Channel 6
 *   ├─ Direction         : Memory To Peripheral
 *   ├─ Data Width        : Half Word（外设侧）/ Half Word（内存侧）
 *   └─ Mode              : Normal（非 Circular）
 *
 * 【NVIC Settings】
 *   └─ DMA1 Channel6 global interrupt 灰色强制使能 ✅
 *
 * 【接线说明】
 *   ├─ PA6（TIM3_CH1）→ WS2812B DIN（3.3V 直驱，短线 <30cm 可用）
 *   ├─ WS2812B VDD  → 5V 电源（30 颗全亮满载约 1.8A，需独立供电）
 *   └─ WS2812B GND  → 电源 GND，与 STM32 GND 共地
 *
 * 【注意事项】
 *   ├─ WS2812B DIN/DOUT 方向不可接反，数据从 DIN 输入
 *   ├─ 3.3V 直驱能否工作取决于灯珠批次，长线或信号不稳时需加电平转换
 *   ├─ 电平转换可用 NPN 三极管（S9013 等）+ 上拉至 5V，注意反相需对调 BIT0/BIT1 CCR
 *   ├─ DMA 必须选 TIM3_CH1/TRIG（对应 DMA1 Channel6），选 TIM3_CH4/UP 会导致 HardFault
 *   └─ WS2812B_Init() 不启动 PWM，首次 WS2812B_Show() 时才启动，避免重复 Stop 崩溃
 *
 * 【main.c 回调转发】
 *   void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
 *   {
 *       WS2812B_IRQHandler(&ws2812b, htim);
 *   }
 *
 * 【DMA 缓冲区大小】
 *   ├─ 30 LEDs × 24 bit + 40 reset slots = 760 个 uint16_t = 1520 字节
 *   └─ STM32F103C8T6 RAM 20KB，占用约 7.4%，余量充足
 *
 * 【常见问题排查】
 *   ├─ HardFault 在 Stop_DMA  → DMA 未运行时调用 Stop，检查 _first_tx 逻辑
 *   ├─ HardFault 在 Start_DMA → DMA 仍在 BUSY，检查上一次传输是否正常结束
 *   ├─ Show() 永久阻塞         → _tx_done 未置位，检查回调是否正确挂载
 *   ├─ 灯不亮 / 乱色           → 检查 DIN/DOUT 方向，确认 DIN 接输入端
 *   ├─ 颜色与设置不符          → WS2812B 线序为 G→R→B，驱动已处理，外部勿再调换
 *   └─ 3.3V 直驱不稳定         → 换用电平转换或将灯带 VDD 改为 3.3V 供电
 *
 * ─────────────────────────────────────────────────────────────────────────────*/