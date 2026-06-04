/**
 * @file    esp8266.h
 * @brief   ESP8266 AT 指令驱动层接口
 *
 * @details
 *   封装 ESP8266 的 AT 指令通信，提供 WiFi 连接、TCP 连接和任意数据收发接口。
 *   基于 HAL UART 中断接收 + 超时轮询的混合模式。
 *
 *   依赖
 *   - STM32 HAL 库 (UART)
 *   - main.h (string.h, stdio.h)
 *
 *   使用示例
 *   @code
 *   ESP8266_Bus_Ctx ctx;
 *   ESP8266_Init(&ctx, &huart1);
 *   ESP8266_TCPConnect(&ctx, "broker.emqx.io", 1883);
 *   ESP8266_Send(&ctx, mqtt_packet, len);
 *   @endcode
 *
 *   注意事项
 *   - ESP8266 供电须使用 3.3V，不可直接接 5V
 *   - 本驱动基于固件版本 1.2.0.0（不支持 AT+MQTT 指令）
 *   - 发送前须确保 TCP 连接已建立
 *   - 接线：PA9→ESP8266 RX, PA10→ESP8266 TX
 *
 * @version 1.1.0
 * @date    2026-05-31
 */

#ifndef __ESP8266_H
#define __ESP8266_H

#include "main.h"

/**
 * @brief ESP8266 总线上下文结构体
 *
 * 维护 UART 通信的全部状态，包括 AT 响应缓冲、云端指令缓冲、
 * 以及 IPD 状态机的运行时变量。所有字段由 HAL_UART_RxCpltCallback
 * 和 ESP8266_SendCmd 协同维护。
 *
 * 双缓冲区 + 状态机
 * - rx_buf[]   : AT 命令响应缓冲区（以 '\n' 为帧结束标志）
 * - ipd_buf[]  : 云端 IPD 数据缓冲区（以字节计数为结束标志）
 * - ipd_state  : 统一状态机（0=空闲 1-5=匹配"+IPD," 6=吸收数据）
 * 两者完全独立，避免二进制 MQTT 报文干扰 AT 响应解析。
 */
typedef struct
{
    UART_HandleTypeDef *huart;       /**< HAL UART 句柄指针 */
    uint8_t  rx_buf[256];            /**< AT 命令响应行缓冲区 */
    uint8_t  ipd_buf[256];           /**< 云端 IPD 数据缓冲区 */

    volatile uint16_t rx_index;      /**< rx_buf 写入位置 */
    volatile uint16_t ipd_index;     /**< ipd_buf 写入位置 */

    volatile uint8_t rx_ready;       /**< 收到完整 AT 响应行标志 */
    volatile uint8_t ipd_ready;      /**< 收到完整 IPD 数据标志 */

    uint8_t  rx_temp;                /**< 单字节中断接收暂存 */

    uint16_t ipd_expect_len;         /**< +IPD,N: 中的 N（期望接收字节数） */
    uint8_t  ipd_state;              /**< IPD 状态机：0=空闲 1-5=匹配前缀 6=吸收数据 */
} ESP8266_Bus_Ctx;

/* 公开接口 */

/**
 * @brief  发送 AT 命令并等待期望的响应
 *
 * @param ctx     ESP8266 上下文指针
 * @param cmd     要发送的 AT 命令字符串（含 \r\n），空串表示仅等待不发送
 * @param expect  期望在响应中出现的子串（如 "OK", ">", "SEND OK"）
 * @param timeout 超时时间（ms）
 * @return        1=收到期望响应, 0=超时
 */
uint8_t ESP8266_SendCmd(ESP8266_Bus_Ctx *ctx, const char *cmd,
                        const char *expect, uint32_t timeout);

/**
 * @brief  初始化 ESP8266 并连接 WiFi
 *
 * 执行序列：ATE0 → AT → AT+CWMODE=1 → AT+CWJAP=...
 * 包含 3 秒上电等待、WiFi 连接等待（最长 10 秒）。
 *
 * @param ctx   ESP8266 上下文指针
 * @param huart HAL UART 句柄指针
 * @return      1=成功, 0=失败
 */
uint8_t ESP8266_Init(ESP8266_Bus_Ctx *ctx, UART_HandleTypeDef *huart);

/**
 * @brief  建立 TCP 连接到指定主机
 *
 * 发送 AT+CIPSTART="TCP","host",port，等待 CONNECT 响应。
 *
 * @param ctx  ESP8266 上下文指针
 * @param host 目标主机名或 IP
 * @param port 目标端口
 * @return     1=成功, 0=失败
 */
uint8_t ESP8266_TCPConnect(ESP8266_Bus_Ctx *ctx, const char* host,
                           uint16_t port);

/**
 * @brief  通过当前 TCP 连接发送任意数据
 *
 * 执行序列：AT+CIPSEND=len → 等待 '>' → 发送数据 → 等待 SEND OK。
 * 必须在 TCP 连接建立后调用。
 *
 * @param ctx    ESP8266 上下文指针
 * @param data   待发送数据指针
 * @param length 数据字节数
 * @return       1=成功, 0=失败
 */
uint8_t ESP8266_Send(ESP8266_Bus_Ctx *ctx, const uint8_t* data,
                     uint16_t length);

#endif /* __ESP8266_H */
