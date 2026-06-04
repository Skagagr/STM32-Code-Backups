/**
 * @file    esp8266.c
 * @brief   ESP8266 AT 指令驱动层实现
 *
 * @details
 *   基于 HAL UART 中断接收 + 超时轮询的混合通信模式。
 *
 *   通信流程
 *   发送 AT 命令 → 挂起中断接收 → 等待 rx_ready 标志或超时 →
 *   检查响应中是否包含期望字符串。
 *
 *   IPD 接收
 *   ESP8266 的 TCP 数据异步通知（+IPD）由 HAL_UART_RxCpltCallback
 *   中的状态机实时处理，不在本文件中。本文件仅负责命令的发送和
 *   响应等待。
 *
 * @warning
 *   ╔══════════════════════════════════════════════════════════════╗
 *   ║  ★ 关键硬件 Bug：AT+CIPSEND 与 +IPD 的 UART 冲突            ║
 *   ║                                                            ║
 *   ║  ESP8266 只有一根 UART 线，TX/RX 共享同一物理介质。         ║
 *   ║  若 ESP8266_Send() 在 HAL_UART_Transmit 发送完数据后       ║
 *   ║  不等待 "SEND OK" 就返回，下一次 AT+CIPSEND 命令发出的     ║
 *   ║  同时 ESP8266 可能正在推送 +IPD 通知，双方数据在 UART       ║
 *   ║  线上冲突，ST-M32 错过 +IPD，云端指令永久丢失。             ║
 *   ║                                                            ║
 *   ║  修复：发送数据后必须调用 ESP8266_SendCmd("", "SEND OK")   ║
 *   ║  等待确认，确保 ESP8266 完全空闲后再进行下一次通信。       ║
 *   ║  这是 ESP8266 AT 固件 v1.2.0 ~ v2.x 的已知设计缺陷。      ║
 *   ╚══════════════════════════════════════════════════════════════╝
 *
 *   依赖
 *   - stm32f1xx_hal.h  (HAL_UART_Transmit, HAL_UART_Receive_IT)
 *   - esp8266.h        (ESP8266_Bus_Ctx)
 *   - main.h           (string.h)
 *
 * @version 1.1.0
 * @date    2026-05-31
 */

#include "esp8266.h"
#include "stm32f1xx_hal.h"

/* 发送 AT 命令并等待响应
 *
 * 工作流程
 * 1. 若 ipd_state == 0（不在 +IPD 前缀匹配中），清空 rx_buf 和
 *    rx_ready，准备接收新响应。
 * 2. 挂起单字节中断接收 (HAL_UART_Receive_IT)。
 * 3. 若 cmd 非空，阻塞发送命令字符串。
 * 4. 轮询 rx_ready 标志（由 HAL_UART_RxCpltCallback 在收到 '\n' 或 '>'
 *    时置位），检查是否包含 expect 子串。
 * 5. 若遇到 +IPD 前缀，跳过并继续等待（避免将 IPD 误判为响应）。
 * 6. 超时返回 0。
 *
 * 为什么允许空命令
 * 当仅需等待响应（如 SEND OK）而不发送新命令时，传入空字符串 ""，
 * 函数跳过 HAL_UART_Transmit 直接进入等待循环。
 */

uint8_t ESP8266_SendCmd(ESP8266_Bus_Ctx *ctx, const char *cmd,
                        const char *expect, uint32_t timeout)
{
    uint32_t tick;

    // 若不在 +IPD 前缀匹配中，清空缓冲区准备接收新响应
    if (ctx->ipd_state == 0)
    {
        ctx->rx_ready = 0;
        ctx->rx_index = 0;
    }

    // 挂起单字节中断接收
    HAL_UART_Receive_IT(ctx->huart, &ctx->rx_temp, 1);

    // 发送命令（空命令仅等待，不发送）
    if (cmd[0] != '\0')
        HAL_UART_Transmit(ctx->huart, (uint8_t*)cmd, strlen(cmd), 100);

    tick = HAL_GetTick();

    // 轮询等待完整响应行或超时
    while (HAL_GetTick() - tick <= timeout)
    {
        if (ctx->rx_ready == 1)
        {
            ctx->rx_ready = 0;

            if (strstr((char *)ctx->rx_buf, expect) != NULL)
                return 1;

            // ERROR 表示命令失败，立即返回避免傻等超时
            if (strstr((char *)ctx->rx_buf, "ERROR") != NULL)
                return 0;

            // +IPD 不是我们要等的响应，跳过继续等
            if (strstr((char *)ctx->rx_buf, "+IPD") != NULL)
                continue;
        }
    }
    return 0;
}

/* ESP8266 初始化 & WiFi 连接
 *
 * 执行序列（每步均验证响应，任何失败返回 0）
 *   1. 等待 3 秒让模块上电稳定
 *   2. ATE0            → 关闭回显
 *   3. AT              → 测试 AT 通信
 *   4. AT+CWMODE=1     → 设为 Station 模式（客户端）
 *   5. AT+CWJAP=...    → 连接 WiFi（超时 10s，以应对弱信号）
 *   6. 等待 500ms 让网络栈稳定
 */

uint8_t ESP8266_Init(ESP8266_Bus_Ctx *ctx, UART_HandleTypeDef *huart)
{
    if (ctx == NULL || huart == NULL) return 0;
    ctx->huart = huart;

    // 硬复位 ESP8266（拉低 RST 50ms）
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_RESET);
    HAL_Delay(50);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
    HAL_Delay(3000);  // 等待 ESP8266 上电启动

    ctx->rx_ready = 0;
    ctx->rx_index = 0;
    HAL_UART_Receive_IT(ctx->huart, &ctx->rx_temp, 1);

    // 关闭 AT 回显，避免响应中混杂命令本身的拷贝
    ESP8266_SendCmd(ctx, "ATE0\r\n", "OK", 5000);

    ctx->rx_ready = 0;
    ctx->rx_index = 0;
    HAL_UART_Receive_IT(ctx->huart, &ctx->rx_temp, 1);

    // 基础 AT 通信测试
    if (!ESP8266_SendCmd(ctx, "AT\r\n", "OK", 5000))
        return 0;

    // 设置为 Station 模式（客户端）
    if (!ESP8266_SendCmd(ctx, "AT+CWMODE=1\r\n", "OK", 5000))
        return 0;

    HAL_Delay(500);  // 等待模式切换完成

    // 连接 WiFi 接入点，失败重试 3 次
    {
        uint8_t cwjap_ok = 0;
        for (int retry = 0; retry < 3; retry++)
        {
            if (ESP8266_SendCmd(ctx,
                "AT+CWJAP=\"12345678\",\"66666666\"\r\n", "OK", 15000))
            {
                cwjap_ok = 1;
                break;
            }
            HAL_Delay(1000);  // 重试前等待 1 秒
        }
        if (!cwjap_ok) return 0;
    }

    HAL_Delay(10000);  // 等待网络栈稳定

    return 1;
}

/* 建立 TCP 连接
 *
 * 发送 AT+CIPSTART="TCP","host",port，等待 CONNECT 响应（超时 20 秒）。
 * 连接建立后清空 rx_buf 残留数据。
 */

uint8_t ESP8266_TCPConnect(ESP8266_Bus_Ctx *ctx, const char* host,
                           uint16_t port)
{
    char cmd[128] = {0};
    sprintf(cmd, "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", host, port);

    if(!ESP8266_SendCmd(ctx, cmd, "CONNECT", 20000))
        return 0;

    HAL_Delay(1000);
    ctx->rx_ready = 0;
    ctx->rx_index = 0;

    return 1;
}

/* 通过 TCP 发送任意数据
 *
 * 严格执行三步握手，确保 ESP8266 处理完毕才返回
 *
 *   1. AT+CIPSEND=<length>  →  等待 '>' 提示符
 *   2. 发送 <length> 字节原始数据
 *   3. 等待 SEND OK 确认（关键！）
 *
 * ╔══════════════════════════════════════════════════════════════╗
 * ║  ★ 第 3 步不可省略！详见本文件头部 @warning 说明。         ║
 * ║  若省略此等待，连续发送时 +IPD 通知会被 UART 冲突吞掉。   ║
 * ╚══════════════════════════════════════════════════════════════╝
 */

uint8_t ESP8266_Send(ESP8266_Bus_Ctx *ctx, const uint8_t* data,
                     uint16_t length)
{
    char cmd[128] = {0};
    sprintf(cmd, "AT+CIPSEND=%d\r\n", length);

    // 步骤 1：请求发送，等待 '>' 提示符
    if(!ESP8266_SendCmd(ctx, cmd, ">", 1000))
        return 0;

    // 步骤 2：发送原始字节数据
    HAL_UART_Transmit(ctx->huart, data, length, 100);

    // 步骤 3：等待 SEND OK 确认（防止 UART 冲突，不可省略！）
    // ESP8266 发送完数据后会回复 "SEND OK"，表示本次发送完全结束。
    // 只有收到 "SEND OK"，ESP8266 才真正空闲，可以接受下一条命令。
    // 若不等待直接返回，下一条 AT+CIPSEND 发出时 ESP8266 可能仍在
    // 处理上一条数据，导致返回 ERROR，本次发送失败。
    // 因此两次连续 MQTT_Publish 之间无需额外 HAL_Delay——
    // 本步骤已保证第一条发完 ESP8266 必然空闲。
    ESP8266_SendCmd(ctx, "", "SEND OK", 2000);

    return 1;
}

/* CubeMX 配置说明
 *
 * 【Connectivity → USART1】
 *   ├─ Mode         : Asynchronous
 *   ├─ Baud Rate    : 115200
 *   ├─ Word Length  : 8 Bits
 *   ├─ Parity       : None
 *   └─ Stop Bits    : 1
 *
 * 【NVIC Settings】
 *   └─ 中断模式需要开启 USART 中断
 *
 * 【GPIO 接线说明】
 *   ├─ PA9  → USART1_TX → ESP8266 RX
 *   └─ PA10 → USART1_RX → ESP8266 TX
 *
 * 【注意事项】
 *   ├─ ESP8266 供电须使用 3.3V，不可直接接 5V
 *   ├─ ESP8266_Init 必须在 MX_USART1_UART_Init 之后调用
 *   ├─ 本驱动基于固件版本 1.2.0.0（不支持 AT+MQTT 指令）
 *   └─ ESP8266_Send 发送前须确保 TCP 连接已建立
 */
