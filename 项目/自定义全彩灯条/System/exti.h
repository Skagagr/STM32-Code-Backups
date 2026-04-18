/**
 * @file    exti.h
 * @brief   通用外部中断模块接口，支持STM32F103C8T6（GPIOA/B/C）
 * @version 1.1.0
 * @date    2026-04-17
 */

#ifndef __EXTI_H
#define __EXTI_H

#include "main.h"
#include <stdint.h>

/* ------------------------------------------------------------------ */
/*  类型定义                                                            */
/* ------------------------------------------------------------------ */

/**
 * @brief 外部中断回调函数类型
 */
typedef void (*EXTI_CallbackFunc)(void);

/**
 * @brief 外部中断句柄结构体
 */
typedef struct
{
    GPIO_TypeDef      *port;      // GPIO 端口
    uint16_t           pin;       // GPIO 引脚
    EXTI_CallbackFunc  callback;  // 触发回调函数
} EXTICtx_t;

/* ------------------------------------------------------------------ */
/*  公开接口声明                                                        */
/* ------------------------------------------------------------------ */

/**
 * @brief   初始化外部中断
 * @note    自动配置GPIO输入上拉 + EXTI触发，并使能NVIC。
 *          STM32F103C8T6 支持 GPIOA / GPIOB / GPIOC。
 *
 * @param ctx       句柄指针
 * @param port      GPIO端口（GPIOA / GPIOB / GPIOC）
 * @param pin       GPIO引脚（如 GPIO_PIN_1）
 * @param irqn      中断号（如 EXTI1_IRQn）
 * @param trigger   触发方式：GPIO_MODE_IT_RISING / FALLING / RISING_FALLING
 * @param callback  触发回调函数
 */
void EXTI_Init(EXTICtx_t *ctx,
               GPIO_TypeDef *port,
               uint16_t pin,
               IRQn_Type irqn,
               uint32_t trigger,
               EXTI_CallbackFunc callback);

/**
 * @brief   在 HAL_GPIO_EXTI_Callback 中调用，分发到对应句柄
 *
 * @param ctx       句柄指针
 * @param gpio_pin  HAL回调传入的引脚号
 */
void EXTI_Handler(EXTICtx_t *ctx, uint16_t gpio_pin);

#endif /* __EXTI_H */