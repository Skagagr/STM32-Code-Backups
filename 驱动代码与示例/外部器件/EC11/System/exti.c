#include "exti.h"
#include "stm32f103xb.h"
#include <stdint.h>

/**
 * @brief   初始化外部中断
 * @note    自动配置GPIO输入上拉 + EXTI触发，并使能NVIC
 *
 * @param ctx       句柄指针
 * @param port      GPIO端口
 * @param pin       GPIO引脚
 * @param irqn      中断号
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
    ctx->port       = port;
    ctx->pin        = pin;
    ctx->callback   = callback;

    if      (port == GPIOA) { __HAL_RCC_GPIOA_CLK_ENABLE(); }
    else if (port == GPIOB) { __HAL_RCC_GPIOB_CLK_ENABLE(); }
    else if (port == GPIOC) { __HAL_RCC_GPIOC_CLK_ENABLE(); }

    GPIO_InitTypeDef gpio_cfg =
    {
        .Pin    = pin,
        .Mode   = trigger,
        .Pull   = GPIO_PULLUP
    };
    HAL_GPIO_Init(port, &gpio_cfg);

    HAL_NVIC_SetPriority(irqn, 2, 0);
    HAL_NVIC_EnableIRQ(irqn);
}

/**
 * @brief 在 HAL_GPIO_EXTI_Callback 中调用，分发到对应句柄
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
/*  STM32CubeMX 配置说明                                                */
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