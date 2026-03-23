/**
 * @file    ws2812b.h
 * @brief   WS2812B 彩灯驱动 — TIM3 CH1 + DMA1 Channel6，支持 30 颗灯珠
 *
 * @details
 *  基于单线 PWM 协议，通过 TIM3_CH1/TRIG 触发 DMA1 Channel6 传输，实现零 CPU 占用。
 *  每个 bit 对应一个 PWM 周期（72MHz 下 ARR=89，周期 1.25µs）。
 *  CCR=29 → bit0（高电平 ~0.4µs），CCR=58 → bit1（高电平 ~0.8µs）。
 *  复位信号以零 CCR 槽追加在缓冲区末尾，无需传输后额外 HAL_Delay()。
 *  DMA 传输完成后通过中断回调置位 _tx_done，WS2812B_Show() 下次调用时
 *  阻塞等待上一帧完成后再启动新一帧，防止缓冲区覆盖。
 *
 *  典型使用流程：
 *  @code
 *      // 1. 在 main.c 中声明句柄
 *      WS2812B_Ctx_t ws2812b;
 *
 *      // 2. 初始化（CubeMX 硬件初始化完成后调用）
 *      WS2812B_Init(&ws2812b, &htim3, TIM_CHANNEL_1);
 *
 *      // 3. 设置颜色并推送
 *      WS2812B_Fill(&ws2812b, 255, 0, 0);         // 全红
 *      WS2812B_Show(&ws2812b);
 *
 *      WS2812B_SetPixel(&ws2812b, 0, 0, 255, 0);  // 第 0 颗灯设为绿
 *      WS2812B_Show(&ws2812b);
 *
 *      WS2812B_Clear(&ws2812b);                   // 熄灭所有灯
 *      WS2812B_Show(&ws2812b);
 *
 *      // 4. 在 HAL 回调中转发（main.c）
 *      void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim) {
 *          WS2812B_IRQHandler(&ws2812b, htim);
 *      }
 *  @endcode
 *
 * @version 1.0.0
 * @date    2026-03-18
 */

#ifndef __WS2812B_H
#define __WS2812B_H

#include "main.h"
#include <stdint.h>

/* ─────────────────────────────────────────────────────────────────────────────
 * 配置区 —— 按工程需求修改
 * ─────────────────────────────────────────────────────────────────────────────*/

#define WS2812B_NUM_LEDS     30U   /**< 灯珠总数，修改此处适配不同长度灯带 */
#define WS2812B_BIT0_CCR     29U   /**< bit0 对应 CCR 值，高电平 ~0.40µs（ARR=89 时勿改） */
#define WS2812B_BIT1_CCR     58U   /**< bit1 对应 CCR 值，高电平 ~0.81µs（ARR=89 时勿改） */
#define WS2812B_RESET_SLOTS  40U   /**< 复位低电平槽数，40 × 1.25µs = 50µs，满足 WS2812B >50µs 要求 */

/* ─────────────────────────────────────────────────────────────────────────────
 * 类型定义
 * ─────────────────────────────────────────────────────────────────────────────*/

/**
 * @brief 单颗灯珠 RGB 颜色
 */
typedef struct {
    uint8_t r;  /**< 红色分量 0~255 */
    uint8_t g;  /**< 绿色分量 0~255 */
    uint8_t b;  /**< 蓝色分量 0~255 */
} WS2812B_Color_t;

/**
 * @brief WS2812B 驱动上下文句柄，每路灯带独立一个实例
 */
typedef struct {
    TIM_HandleTypeDef *htim;    /**< 关联的定时器句柄，需在 CubeMX 中配置 ARR=89, PSC=0 */
    uint32_t           channel; /**< 定时器 PWM 通道，例如 TIM_CHANNEL_1 */
    WS2812B_Color_t    leds[WS2812B_NUM_LEDS]; /**< 像素颜色缓冲区，调用 WS2812B_Show() 前修改此数组 */
    /* 以下为私有成员，外部不可直接访问 */
    uint16_t           _dma_buf[WS2812B_NUM_LEDS * 24 + WS2812B_RESET_SLOTS] __attribute__((aligned(4))); /**< DMA 传输缓冲区，存放每个 bit 对应的 CCR 值，4 字节对齐 */
    volatile uint8_t   _tx_done;  /**< DMA 传输完成标志，1=空闲，0=传输中，由中断置位 */
    uint8_t            _first_tx; /**< 首次传输标志，1=尚未传输过，跳过 Stop_DMA 直接启动 */
} WS2812B_Ctx_t;

/* ─────────────────────────────────────────────────────────────────────────────
 * 公开 API
 * ─────────────────────────────────────────────────────────────────────────────*/

/**
 * @brief  初始化 WS2812B 上下文
 *
 * @details
 *  清零像素缓冲区和 DMA 缓冲区，绑定定时器句柄和通道。
 *  不启动 PWM，PWM 由第一次 WS2812B_Show() 内部的
 *  HAL_TIM_PWM_Start_DMA() 启动。
 *  需在 CubeMX 生成的硬件初始化（MX_TIM3_Init）之后调用。
 *
 * @param  ctx      指向 WS2812B_Ctx_t 句柄的指针
 * @param  htim     指向 TIM 句柄的指针（需配置 ARR=89, PSC=0）
 * @param  channel  TIM PWM 通道，例如 TIM_CHANNEL_1
 * @retval 1        初始化成功
 * @retval 0        参数为空
 */
uint8_t WS2812B_Init(WS2812B_Ctx_t *ctx,
                     TIM_HandleTypeDef *htim,
                     uint32_t channel);

/**
 * @brief  设置单颗灯珠的颜色（仅写入缓冲区，不立即推送）
 *
 * @details
 *  修改 ctx->leds[index] 的 RGB 值，需调用 WS2812B_Show() 后才会生效。
 *  index 越界时静默返回，不做任何操作。
 *
 * @param  ctx    指向上下文的指针
 * @param  index  灯珠索引，范围 [0, WS2812B_NUM_LEDS-1]
 * @param  r      红色分量 0~255
 * @param  g      绿色分量 0~255
 * @param  b      蓝色分量 0~255
 * @retval 1      设置成功
 * @retval 0      参数为空或索引越界
 */
uint8_t WS2812B_SetPixel(WS2812B_Ctx_t *ctx,
                          uint16_t index,
                          uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief  将所有灯珠设置为同一颜色（仅写入缓冲区，不立即推送）
 *
 * @param  ctx  指向上下文的指针
 * @param  r    红色分量 0~255
 * @param  g    绿色分量 0~255
 * @param  b    蓝色分量 0~255
 * @retval 1    设置成功
 * @retval 0    ctx 为空
 */
uint8_t WS2812B_Fill(WS2812B_Ctx_t *ctx,
                     uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief  熄灭所有灯珠（等同于 Fill 0, 0, 0，不立即推送）
 *
 * @param  ctx  指向上下文的指针
 * @retval 1    成功
 * @retval 0    ctx 为空
 */
uint8_t WS2812B_Clear(WS2812B_Ctx_t *ctx);

/**
 * @brief  通过 DMA 将像素缓冲区推送到灯带
 *
 * @details
 *  阻塞等待上一次 DMA 传输完成，重建 DMA 缓冲区后启动新一次传输。
 *  传输期间 CPU 可自由运行，传输完成由中断回调置位 _tx_done。
 *  首次调用直接启动 DMA，后续调用先 Stop 再 Start 确保从头传输。
 *
 * @param  ctx  指向上下文的指针
 * @retval 1    DMA 启动成功
 * @retval 0    ctx 为空
 */
uint8_t WS2812B_Show(WS2812B_Ctx_t *ctx);

/**
 * @brief  DMA 传输完成中断处理，需在 HAL 回调中调用
 *
 * @details
 *  停止 DMA 传输并置位 _tx_done 标志，使下一次 WS2812B_Show() 解除阻塞。
 *  通过 htim->Instance 匹配实例，支持多路灯带共存。
 *
 *  调用方示例：
 *  @code
 *      void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
 *      {
 *          WS2812B_IRQHandler(&ws2812b, htim);
 *      }
 *  @endcode
 *
 * @param  ctx   指向上下文的指针
 * @param  htim  HAL 回调传入的 htim，用于匹配当前实例
 */
void WS2812B_IRQHandler(WS2812B_Ctx_t *ctx, TIM_HandleTypeDef *htim);

#endif /* __WS2812B_H */