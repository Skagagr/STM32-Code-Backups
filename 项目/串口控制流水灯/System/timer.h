/**
 * @file    timer.h
 * @brief   定时器模块接口（基于 TIM2 定时中断）
 *
 * @details
 *  提供基于 STM32 HAL 库的定时器中断封装。
 *  支持多个软件定时器，每个定时器独立计时、独立回调。
 *
 *  移植要求：
 *   - STM32 HAL 库
 *   - 在 CubeMX 中使能 TIM2 并开启全局中断
 *
 *  典型使用流程：
 *  @code
 *      // 1. 硬件初始化（CubeMX 生成）
 *      MX_TIM2_Init();
 *
 *      // 2. 软件模块初始化
 *      TIMER_Init();
 *
 *      // 3. 注册软件定时器
 *      TIMER_Register(0, 1000, LED_Toggle);   // 每 1000ms 执行一次
 *      TIMER_Register(1, 500,  Task_B);       // 每 500ms 执行一次
 *
 *      // 4. 在 HAL 回调中转发
 *      void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
 *          if (htim->Instance == TIM2)
 *              TIMER_Tick();
 *      }
 *  @endcode
 *
 * @version 1.0.0
 * @date    2026-03-13
 */

#ifndef __TIMER_H
#define __TIMER_H

#include "main.h"
#include <stdint.h>

/* ─────────────────────────────────────────────────────────────────────────────
 * 配置区 —— 按工程需求修改
 * ───────────────────────────────────────────────────────────────────────────*/

/** @brief 软件定时器最大数量 */
#define TIMER_MAX_COUNT    8

/** @brief 硬件定时器中断周期（ms），与 CubeMX 里 ARR 配置保持一致 */
#define TIMER_TICK_MS      1

/* ─────────────────────────────────────────────────────────────────────────────
 * 类型定义
 * ───────────────────────────────────────────────────────────────────────────*/

/** @brief 定时器回调函数类型 */
typedef void (*TIMER_Callback)(void);

/**
 * @brief 软件定时器结构体
 */
typedef struct {
    uint32_t       period;      /**< 定时周期（ms） */
    uint32_t       counter;     /**< 当前计数值 */
    uint8_t        enabled;     /**< 使能标志：1 运行，0 停止 */
    TIMER_Callback callback;    /**< 到期回调函数 */
} SoftTimer;

/* ─────────────────────────────────────────────────────────────────────────────
 * 公开 API
 * ───────────────────────────────────────────────────────────────────────────*/

/**
 * @brief  初始化定时器模块，清空所有软件定时器
 */
void TIMER_Init(void);

/**
 * @brief  启动硬件定时器
 * @param  htim  已完成硬件初始化的 HAL TIM 句柄指针
 */
void TIMER_Start(TIM_HandleTypeDef *htim);

/**
 * @brief  注册一个软件定时器
 *
 * @param  id        定时器编号（0 ~ TIMER_MAX_COUNT-1）
 * @param  period_ms 定时周期（毫秒）
 * @param  cb        到期时执行的回调函数
 */
void TIMER_Register(uint8_t id, uint32_t period_ms, TIMER_Callback cb);

/**
 * @brief  停止一个软件定时器
 * @param  id  定时器编号
 */
void TIMER_Stop(uint8_t id);

/**
 * @brief  恢复一个软件定时器
 * @param  id  定时器编号
 */
void TIMER_Resume(uint8_t id);

/**
 * @brief  硬件定时器中断 Tick——须在 HAL_TIM_PeriodElapsedCallback 中转发
 *
 * @details 每次硬件中断触发时调用，遍历所有软件定时器并执行到期回调。
 *
 * @code
 *     void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
 *         if (htim->Instance == TIM2)
 *             TIMER_Tick();
 *     }
 * @endcode
 */
void TIMER_Tick(void);

#endif /* __TIMER_H */