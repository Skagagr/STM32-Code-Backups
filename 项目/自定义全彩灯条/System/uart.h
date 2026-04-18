/**
 * @file    uart.h
 * @brief   UART 非阻塞收发模块接口（中断 TX + 可选 DMA/中断 RX）
 *
 * @details
 *  提供基于 STM32 HAL 库的 UART 中断发送与可配置接收封装。
 *  支持多路 UART 共存，每路串口独立维护一个 UART_Ctx 实例。
 *
 *  移植要求：
 *   - STM32 HAL 库（F1 / F4 / F7 / H7 / G0 / G4 / L4 等系列均可）
 *   - DMA 模式需在 CubeMX 中使能对应 UART 的 DMA RX 通道
 *
 * @version 1.1.0
 * @date    2026-03-12
 */

#ifndef __UART_H
#define __UART_H

#include "main.h"
#include <stdint.h>
#include <string.h>

/* ─────────────────────────────────────────────────────────────────────────────
 * 配置区 —— 按工程需求修改
 * ───────────────────────────────────────────────────────────────────────────*/

/** @brief 发送缓冲区大小（字节） */
#define UART_TX_BUF_SIZE   256

/** @brief 接收缓冲区大小（字节） */
#define UART_RX_BUF_SIZE   256

/**
 * @brief 接收模式选择
 *
 * @details 取消注释其中一行来选择接收模式：
 *  - UART_RX_MODE_DMA：空闲中断 + DMA，整帧触发一次，CPU 占用低
 *                       需在 CubeMX 使能 UART DMA RX 通道
 *  - UART_RX_MODE_IT ：逐字节中断，无需 DMA，资源占用少
 *                       适合低频指令收发
 */
// #define UART_RX_MODE_DMA       // ← 使用 DMA 接收
#define UART_RX_MODE_IT     // ← 使用中断接收

/* 模式互斥检查 */
#if defined(UART_RX_MODE_DMA) && defined(UART_RX_MODE_IT)
    #error "UART_RX_MODE_DMA 和 UART_RX_MODE_IT 不能同时开启，请只保留一个"
#endif
#if !defined(UART_RX_MODE_DMA) && !defined(UART_RX_MODE_IT)
    #error "必须选择一种接收模式：UART_RX_MODE_DMA 或 UART_RX_MODE_IT"
#endif

/* ─────────────────────────────────────────────────────────────────────────────
 * 类型定义
 * ───────────────────────────────────────────────────────────────────────────*/

/**
 * @brief 发送状态枚举
 */
typedef enum {
    UART_TX_IDLE = 0,   /**< 发送通道空闲 */
    UART_TX_BUSY = 1    /**< 发送通道忙碌 */
} UART_TxState;

/**
 * @brief UART 模块句柄
 */
typedef struct {
    UART_HandleTypeDef *huart;              /**< HAL UART 句柄指针 */

    /* TX */
    volatile UART_TxState tx_state;         /**< 发送状态，中断回调中修改 */
    uint8_t tx_buf[UART_TX_BUF_SIZE];       /**< 发送缓冲区 */

    /* RX */
    uint8_t  rx_buf[UART_RX_BUF_SIZE];      /**< 接收缓冲区 */
    volatile uint16_t rx_len;               /**< 最新一帧实际接收字节数 */
    volatile uint8_t  rx_ready;            /**< 新帧就绪标志：1 有数据，处理后置 0 */

#ifdef UART_RX_MODE_IT
    uint16_t rx_index;                      /**< 中断模式下的缓冲区写入索引 */
#endif

} UART_Ctx;

/* ─────────────────────────────────────────────────────────────────────────────
 * 公开 API
 * ───────────────────────────────────────────────────────────────────────────*/

/**
 * @brief  初始化 UART 软件模块
 * @param  ctx    UART 模块句柄指针
 * @param  huart  已完成硬件初始化的 HAL UART 句柄指针
 */
void UART_Init(UART_Ctx *ctx, UART_HandleTypeDef *huart);

/**
 * @brief  中断方式发送原始字节数据（非阻塞）
 * @param  ctx   UART 模块句柄指针
 * @param  data  待发送数据指针
 * @param  len   数据长度，须 ≤ UART_TX_BUF_SIZE
 * @retval 1     成功启动发送
 * @retval 0     忙碌或参数非法
 */
uint8_t UART_SendData(UART_Ctx *ctx, const uint8_t *data, uint16_t len);

/**
 * @brief  中断方式发送字符串（非阻塞）
 * @param  ctx  UART 模块句柄指针
 * @param  str  待发送的 C 字符串
 * @retval 1    成功启动发送
 * @retval 0    忙碌或参数非法
 */
uint8_t UART_SendString(UART_Ctx *ctx, const char *str);

/**
 * @brief  查询发送通道是否空闲
 * @retval 1  空闲
 * @retval 0  忙碌
 */
uint8_t UART_IsTxIdle(UART_Ctx *ctx);

/**
 * @brief  启动接收（根据配置自动选择 DMA 或中断模式）
 * @param  ctx  UART 模块句柄指针
 */
void UART_StartReceive(UART_Ctx *ctx);

/**
 * @brief  发送完成回调——须在 HAL_UART_TxCpltCallback 中转发
 * @param  ctx  UART 模块句柄指针
 */
void UART_TxCpltCallback(UART_Ctx *ctx);

/**
 * @brief  DMA 模式接收完成回调——须在 HAL_UARTEx_RxEventCallback 中转发
 * @note   仅 UART_RX_MODE_DMA 模式下使用
 * @param  ctx   UART 模块句柄指针
 * @param  size  本帧实际接收字节数
 */
void UART_RxEventCallback(UART_Ctx *ctx, uint16_t size);

/**
 * @brief  中断模式接收完成回调——须在 HAL_UART_RxCpltCallback 中转发
 * @note   仅 UART_RX_MODE_IT 模式下使用
 * @param  ctx  UART 模块句柄指针
 */
void UART_RxCpltCallback(UART_Ctx *ctx);

#endif /* __UART_H */