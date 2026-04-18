/**
 * @file    exti.c
 * @brief   通用外部中断模块，支持STM32F103C8T6（GPIOA/B/C）
 * @version 1.1.0
 * @date    2026-04-17
 */

#include "exti.h"

/* ------------------------------------------------------------------ */
/*  内部工具函数                                                        */
/* ------------------------------------------------------------------ */

/**
 * @brief 根据GPIO端口使能对应时钟
 *
 * @param port  GPIO端口指针
 */
static void _enable_gpio_clk(GPIO_TypeDef *port)
{
    if      (port == GPIOA) __HAL_RCC_GPIOA_CLK_ENABLE();
    else if (port == GPIOB) __HAL_RCC_GPIOB_CLK_ENABLE();
    else if (port == GPIOC) __HAL_RCC_GPIOC_CLK_ENABLE();
}

/* ------------------------------------------------------------------ */
/*  初始化                                                              */
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
               EXTI_CallbackFunc callback)
{
    ctx->port     = port;
    ctx->pin      = pin;
    ctx->callback = callback;

    _enable_gpio_clk(port);

    GPIO_InitTypeDef gpio_cfg =
    {
        .Pin   = pin,
        .Mode  = trigger,
        .Pull  = GPIO_PULLUP,
    };
    HAL_GPIO_Init(port, &gpio_cfg);

    HAL_NVIC_SetPriority(irqn, 2, 0);
    HAL_NVIC_EnableIRQ(irqn);
}

/* ------------------------------------------------------------------ */
/*  回调                                                                */
/* ------------------------------------------------------------------ */

/**
 * @brief   在 HAL_GPIO_EXTI_Callback 中调用，分发到对应句柄
 *
 * @param ctx       句柄指针
 * @param gpio_pin  HAL回调传入的引脚号
 */
void EXTI_Handler(EXTICtx_t *ctx, uint16_t gpio_pin)
{
    if (gpio_pin == ctx->pin && ctx->callback != NULL)
        ctx->callback();
}

/* ------------------------------------------------------------------ */
/*  CubeMX 配置说明                                                     */
/* ------------------------------------------------------------------ */
/*
 * 本模块自行初始化GPIO和NVIC，CubeMX只需确认以下两项：
 *
 * 1. RCC
 *    Pinout & Configuration → System Core → RCC
 *    - HSE: Crystal/Ceramic Resonator，时钟配置到72MHz
 *
 * 2. NVIC
 *    Pinout & Configuration → System Core → NVIC
 *    - 勾选对应的 "EXTI line interrupt"（根据实际使用引脚）
 *
 * 注意：CubeMX 里不要对本模块使用的引脚做任何GPIO配置，
 *       否则 MX_GPIO_Init() 生成的代码会覆盖本模块初始化。
 */