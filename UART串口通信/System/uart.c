/**
 * @file    uart.c
 * @brief   UART 非阻塞收发模块（中断 TX + 可选 DMA/中断 RX）
 *
 * @details
 *  接收模式通过 uart.h 中的宏在编译期确定：
 *   - UART_RX_MODE_DMA：空闲中断 + DMA，整帧搬运，CPU 占用低
 *   - UART_RX_MODE_IT ：逐字节中断，无需 DMA，资源占用少
 *
 * @version 1.1.0
 * @date    2026-03-12
 */

#include "uart.h"

/* ─────────────────────────────────────────────────────────────────────────────
 * 初始化
 * ───────────────────────────────────────────────────────────────────────────*/

void UART_Init(UART_Ctx *ctx, UART_HandleTypeDef *huart)
{
    ctx->huart    = huart;
    ctx->tx_state = UART_TX_IDLE;
    ctx->rx_len   = 0;
    ctx->rx_ready = 0;
    memset(ctx->tx_buf, 0, sizeof(ctx->tx_buf));
    memset(ctx->rx_buf, 0, sizeof(ctx->rx_buf));

#ifdef UART_RX_MODE_IT
    ctx->rx_index = 0;
#endif
}

/* ─────────────────────────────────────────────────────────────────────────────
 * 发送
 * ───────────────────────────────────────────────────────────────────────────*/

uint8_t UART_IsTxIdle(UART_Ctx *ctx)
{
    return (ctx->tx_state == UART_TX_IDLE);
}

uint8_t UART_SendData(UART_Ctx *ctx, const uint8_t *data, uint16_t len)
{
    if (ctx->tx_state == UART_TX_BUSY)  return 0;
    if (len == 0 || len > UART_TX_BUF_SIZE) return 0;

    memcpy(ctx->tx_buf, data, len);
    ctx->tx_state = UART_TX_BUSY;
    HAL_UART_Transmit_IT(ctx->huart, ctx->tx_buf, len);

    return 1;
}

uint8_t UART_SendString(UART_Ctx *ctx, const char *str)
{
    return UART_SendData(ctx, (const uint8_t *)str, (uint16_t)strlen(str));
}

/* ─────────────────────────────────────────────────────────────────────────────
 * 接收
 * ───────────────────────────────────────────────────────────────────────────*/

void UART_StartReceive(UART_Ctx *ctx)
{
#ifdef UART_RX_MODE_DMA
    /* DMA 模式：整帧空闲触发，一次中断取走所有数据 */
    HAL_UARTEx_ReceiveToIdle_DMA(ctx->huart, ctx->rx_buf, UART_RX_BUF_SIZE);

#elif defined(UART_RX_MODE_IT)
    /* 中断模式：逐字节接收，存入缓冲区，超时或换行判断帧结束 */
    ctx->rx_index = 0;
    HAL_UART_Receive_IT(ctx->huart, ctx->rx_buf, 1);
#endif
}

/* ─────────────────────────────────────────────────────────────────────────────
 * 回调
 * ───────────────────────────────────────────────────────────────────────────*/

void UART_TxCpltCallback(UART_Ctx *ctx)
{
    ctx->tx_state = UART_TX_IDLE;
}

#ifdef UART_RX_MODE_DMA
/**
 * @brief DMA 模式：空闲中断触发，整帧就绪
 */
void UART_RxEventCallback(UART_Ctx *ctx, uint16_t size)
{
    ctx->rx_len   = size;
    ctx->rx_ready = 1;
    HAL_UARTEx_ReceiveToIdle_DMA(ctx->huart, ctx->rx_buf, UART_RX_BUF_SIZE);
}
#endif

#ifdef UART_RX_MODE_IT
/**
 * @brief 中断模式：每收到一字节触发，遇 '\n' 或缓冲区满则置帧就绪
 */
void UART_RxCpltCallback(UART_Ctx *ctx)
{
    uint8_t byte = ctx->rx_buf[ctx->rx_index];
    ctx->rx_index++;

    /* 遇到换行符或缓冲区即将溢出，视为一帧结束 */
    if (byte == '\n' || ctx->rx_index >= UART_RX_BUF_SIZE)
    {
        ctx->rx_len   = ctx->rx_index;
        ctx->rx_ready = 1;
        ctx->rx_index = 0;
    }

    /* 继续挂起下一字节接收 */
    HAL_UART_Receive_IT(ctx->huart,
                        ctx->rx_buf + ctx->rx_index, 1);
}
#endif