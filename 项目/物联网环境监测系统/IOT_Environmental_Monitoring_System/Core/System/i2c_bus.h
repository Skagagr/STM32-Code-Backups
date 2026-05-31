/**
 * @file    i2c_bus.h
 * @brief   通用 I2C 总线抽象层（阻塞模式）
 *
 * @details
 *  封装 HAL I2C 阻塞收发接口，支持多设备挂载同一总线。
 *  上层模块（OLED、传感器等）通过此层访问 I2C，与 HAL 解耦。
 *
 *  典型使用流程：
 *  @code
 *      // 1. 在 main.c 完成 CubeMX 硬件初始化后，初始化总线
 *      I2C_Bus_Init(&hi2c1);
 *
 *      // 2. 发送数据（写寄存器）
 *      uint8_t buf[2] = {0x00, 0xAE};
 *      I2C_Bus_Write(0x78, buf, 2);
 *
 *      // 3. 读取数据
 *      uint8_t val;
 *      I2C_Bus_ReadReg(0x68, 0x3B, &val, 1);
 *  @endcode
 *
 * @version 1.0.0
 * @date    2026-03-14
 */

#ifndef __I2C_BUS_H
#define __I2C_BUS_H

#include "main.h"
#include <stdint.h>

/* ── 配置区 —— 按工程需求修改 ── */
#define I2C_BUS_TIMEOUT_MS   10U   /**< I2C 阻塞超时时间(ms)，总线异常时防止死等 */

/* ── 类型定义 ── */

/**
 * @brief I2C 总线上下文句柄
 */
typedef struct
{
    I2C_HandleTypeDef *hi2c;    /**< 指向 CubeMX 生成的 HAL I2C 句柄 */
    uint8_t            inited;  /**< 初始化标志：1=已初始化，0=未初始化 */
} I2C_Bus_Ctx;

/* ── 公开 API ── */

/**
 * @brief  初始化 I2C 总线模块
 *
 * @details
 *  绑定 HAL I2C 句柄，记录初始化状态。
 *  必须在 CubeMX 生成的 MX_I2Cx_Init() 之后调用。
 *
 * @param  ctx   I2C 总线上下文指针
 * @param  hi2c  CubeMX 生成的 HAL I2C 句柄指针
 * @retval 1     成功
 * @retval 0     参数非法
 */
uint8_t I2C_Bus_Init(I2C_Bus_Ctx *ctx, I2C_HandleTypeDef *hi2c);

/**
 * @brief  向指定设备地址写入数据
 *
 * @details
 *  发送 dev_addr + pData 内容，适用于指令流、寄存器写入等场景。
 *  dev_addr 使用 8 位格式（左移 1 位后的地址），例如 0x78。
 *
 * @param  ctx       I2C 总线上下文指针
 * @param  dev_addr  设备 I2C 地址（8 位格式）
 * @param  pData     待发送数据缓冲区指针
 * @param  size      发送字节数
 * @retval 1         成功
 * @retval 0         发送失败或参数非法
 */
uint8_t I2C_Bus_Write(I2C_Bus_Ctx *ctx, uint8_t dev_addr,
                      const uint8_t *pData, uint16_t size);

/**
 * @brief  从指定设备地址读取数据
 *
 * @details
 *  从 dev_addr 设备直接读取 size 字节到 pData。
 *
 * @param  ctx       I2C 总线上下文指针
 * @param  dev_addr  设备 I2C 地址（8 位格式）
 * @param  pData     接收缓冲区指针
 * @param  size      读取字节数
 * @retval 1         成功
 * @retval 0         读取失败或参数非法
 */
uint8_t I2C_Bus_Read(I2C_Bus_Ctx *ctx, uint8_t dev_addr,
                     uint8_t *pData, uint16_t size);

/**
 * @brief  写寄存器地址后读取数据（先写后读）
 *
 * @details
 *  先发送 1 字节寄存器地址 reg_addr，再重复起始条件读取数据。
 *  适用于大多数传感器的寄存器读取操作。
 *
 * @param  ctx       I2C 总线上下文指针
 * @param  dev_addr  设备 I2C 地址（8 位格式）
 * @param  reg_addr  寄存器地址
 * @param  pData     接收缓冲区指针
 * @param  size      读取字节数
 * @retval 1         成功
 * @retval 0         失败或参数非法
 */
uint8_t I2C_Bus_ReadReg(I2C_Bus_Ctx *ctx, uint8_t dev_addr, uint8_t reg_addr,
                        uint8_t *pData, uint16_t size);

/**
 * @brief  检测总线上指定地址的设备是否存在
 *
 * @details
 *  发送地址帧后检测 ACK，可用于设备在线检测和地址扫描。
 *
 * @param  ctx       I2C 总线上下文指针
 * @param  dev_addr  设备 I2C 地址（8 位格式）
 * @retval 1         设备存在（收到 ACK）
 * @retval 0         设备不存在或总线错误
 */
uint8_t I2C_Bus_IsDeviceReady(I2C_Bus_Ctx *ctx, uint8_t dev_addr);

#endif /* __I2C_BUS_H */