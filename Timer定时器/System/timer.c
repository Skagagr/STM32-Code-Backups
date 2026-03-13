/**
 * @file    timer.c
 * @brief   定时器模块实现（基于 TIM2 定时中断）
 *
 * @details
 *  硬件 TIM2 每 1ms 产生一次中断，驱动所有软件定时器计数。
 *  软件定时器到期后在中断上下文执行回调，回调函数应尽量简短。
 *
 * @version 1.0.0
 * @date    2026-03-12
 */

#include "timer.h"

/* ─────────────────────────────────────────────────────────────────────────────
 * 内部变量
 * ───────────────────────────────────────────────────────────────────────────*/

static TIM_HandleTypeDef *_htim;                   // 硬件定时器句柄
static SoftTimer _timers[TIMER_MAX_COUNT];         // 软件定时器数组

/* ─────────────────────────────────────────────────────────────────────────────
 * 初始化
 * ───────────────────────────────────────────────────────────────────────────*/

void TIMER_Init(void)
{
    for (uint8_t i = 0; i < TIMER_MAX_COUNT; i++)
    {
        _timers[i].period   = 0;
        _timers[i].counter  = 0;
        _timers[i].enabled  = 0;
        _timers[i].callback = NULL;
    }
}

void TIMER_Start(TIM_HandleTypeDef *htim)
{
    _htim = htim;
    HAL_TIM_Base_Start_IT(_htim);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * 软件定时器管理
 * ───────────────────────────────────────────────────────────────────────────*/

void TIMER_Register(uint8_t id, uint32_t period_ms, TIMER_Callback cb)
{
    if (id >= TIMER_MAX_COUNT || cb == NULL) return;

    _timers[id].period   = period_ms / TIMER_TICK_MS;
    _timers[id].counter  = 0;
    _timers[id].enabled  = 1;
    _timers[id].callback = cb;
}

void TIMER_Stop(uint8_t id)
{
    if (id >= TIMER_MAX_COUNT) return;
    _timers[id].enabled = 0;
}

void TIMER_Resume(uint8_t id)
{
    if (id >= TIMER_MAX_COUNT) return;
    _timers[id].enabled = 1;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * 中断 Tick（每 1ms 调用一次）
 * ───────────────────────────────────────────────────────────────────────────*/

void TIMER_Tick(void)
{
    for (uint8_t i = 0; i < TIMER_MAX_COUNT; i++)
    {
        if (!_timers[i].enabled) continue;

        _timers[i].counter++;

        if (_timers[i].counter >= _timers[i].period)
        {
            _timers[i].counter = 0;
            _timers[i].callback();   // 执行回调
        }
    }
}


/* ─────────────────────────────────────────────────────────────────────────────
 * CubeMX 配置
 *
 * TIM2 的参数设置为 1ms 中断一次：
 *
 * 时钟源： Internal Clock
 * PSC：    71
 * ARR：    999
 * 开启：   TIM2 global interrupt
* ───────────────────────────────────────────────────────────────────────────*/