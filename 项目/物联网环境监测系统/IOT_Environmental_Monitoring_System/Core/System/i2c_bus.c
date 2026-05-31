/**
 * @file    i2c_bus.c
 * @brief   通用 I2C 总线抽象层实现（阻塞模式）
 *
 * @details
 *  封装 HAL_I2C_Master_Transmit / Receive，提供统一收发接口。
 *  支持多总线实例（I2C1 / I2C2）同时工作。
 *  I2C_Bus_Write 内含总线忙超时复位，解决 STM32F103 I2C 连续传输死锁问题。
 *
 * @version 1.1.0
 * @date    2026-03-14
 */

#include "i2c_bus.h"
#include <string.h>

/* ─────────────────────────────────────────────────────────────────────────────
 * 初始化
 * ───────────────────────────────────────────────────────────────────────────*/

uint8_t I2C_Bus_Init(I2C_Bus_Ctx *ctx, I2C_HandleTypeDef *hi2c)
{
    if (ctx == NULL || hi2c == NULL) return 0;
    ctx->hi2c   = hi2c;
    ctx->inited = 1;
    return 1;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * 发送
 * ───────────────────────────────────────────────────────────────────────────*/

uint8_t I2C_Bus_Write(I2C_Bus_Ctx *ctx, uint8_t dev_addr,
                      const uint8_t *pData, uint16_t size)
{
    if (ctx == NULL || !ctx->inited || pData == NULL || size == 0)
        return 0;

    /* STM32F103 I2C 已知 bug：连续多次传输后总线可能进入忙状态
     * 检测 BUSY 标志，超时则强制复位，避免后续传输全部失败       */
    uint32_t t = HAL_GetTick();
    while (__HAL_I2C_GET_FLAG(ctx->hi2c, I2C_FLAG_BUSY))
    {
        if (HAL_GetTick() - t > 10)
        {
            HAL_I2C_DeInit(ctx->hi2c);
            HAL_Delay(5);
            HAL_I2C_Init(ctx->hi2c);
            break;
        }
    }

    // 总线忙检测块
    HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(
        ctx->hi2c,
        dev_addr,
        (uint8_t *)pData,
        size,
        I2C_BUS_TIMEOUT_MS);

    return (ret == HAL_OK) ? 1 : 0;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * 接收
 * ───────────────────────────────────────────────────────────────────────────*/

uint8_t I2C_Bus_Read(I2C_Bus_Ctx *ctx, uint8_t dev_addr,
                     uint8_t *pData, uint16_t size)
{
    if (ctx == NULL || !ctx->inited || pData == NULL || size == 0)
        return 0;

    HAL_StatusTypeDef ret = HAL_I2C_Master_Receive(
        ctx->hi2c,
        dev_addr,
        pData,
        size,
        I2C_BUS_TIMEOUT_MS);

    return (ret == HAL_OK) ? 1 : 0;
}

uint8_t I2C_Bus_ReadReg(I2C_Bus_Ctx *ctx, uint8_t dev_addr, uint8_t reg_addr,
                        uint8_t *pData, uint16_t size)
{
    if (ctx == NULL || !ctx->inited || pData == NULL || size == 0)
        return 0;

    /* 先发送寄存器地址 */
    HAL_StatusTypeDef ret = HAL_I2C_Master_Transmit(
        ctx->hi2c,
        dev_addr,
        &reg_addr,
        1,
        I2C_BUS_TIMEOUT_MS);

    if (ret != HAL_OK) return 0;

    /* 再读取数据 */
    ret = HAL_I2C_Master_Receive(
        ctx->hi2c,
        dev_addr,
        pData,
        size,
        I2C_BUS_TIMEOUT_MS);

    return (ret == HAL_OK) ? 1 : 0;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * 设备检测
 * ───────────────────────────────────────────────────────────────────────────*/

uint8_t I2C_Bus_IsDeviceReady(I2C_Bus_Ctx *ctx, uint8_t dev_addr)
{
    if (ctx == NULL || !ctx->inited) return 0;

    HAL_StatusTypeDef ret = HAL_I2C_IsDeviceReady(
        ctx->hi2c,
        dev_addr,
        3,
        I2C_BUS_TIMEOUT_MS);

    return (ret == HAL_OK) ? 1 : 0;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * CubeMX 配置说明
 * ───────────────────────────────────────────────────────────────────────────
 *
 * 【Connectivity → I2C1】
 *   ├─ I2C Speed Mode : Standard Mode（100 kHz）
 *   │    可改 Fast Mode（400 kHz），需确认上拉电阻和设备支持
 *   └─ 其余保持默认
 *
 * 【NVIC Settings】
 *   └─ 阻塞模式无需开启 I2C 中断
 *
 * 【接线说明】
 *   ├─ PB6 → I2C1_SCL（需接 4.7kΩ 上拉至 3.3V）
 *   └─ PB7 → I2C1_SDA（需接 4.7kΩ 上拉至 3.3V）
 *
 * 【MX_I2C1_Init 必须加复位代码（STM32F103 I2C 上电 bug）】
 *   在 HAL_I2C_Init() 之前加入：
 *   __HAL_RCC_I2C1_FORCE_RESET();
 *   HAL_Delay(10);
 *   __HAL_RCC_I2C1_RELEASE_RESET();
 *   HAL_Delay(10);
 *
 * 【注意事项】
 *   ├─ dev_addr 使用 8 位格式：7 位地址左移 1 位
 *   │   例：7位地址 0x3C → 传入 0x78；0x3D → 传入 0x7A
 *   ├─ I2C_Bus_Init 必须在 MX_I2C1_Init 之后调用
 *   └─ I2C_Bus_IsDeviceReady 必须在 I2C_Bus_Init 之后调用
 *
 * ───────────────────────────────────────────────────────────────────────────*/