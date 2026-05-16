/**
 * @file    esp8266.c
 * @brief   ESP8266 AT 指令驱动层实现（阻塞模式）
 *
 * @details
 *  封装 HAL_UART_Transmit / Receive，提供 AT 指令收发、WiFi 连接、TCP 连接接口。
 *  支持多实例（ESP8266_Bus_Ctx），可同时驱动多个 ESP8266 模块。
 *  使用阻塞模式接收，通过 timeout 控制等待时间。
 *
 * @version 1.0.0
 * @date    2026-05-03
 */
#include "esp8266.h"

/**
 * @brief 发送命令
 * 
 * @param ctx ESP8266_Bus_Ctx 句柄指针
 * @param cmd 命令
 * @param expect 期望的回复
 * @param timeout 接收超时
 * @return uint8_t 
 */
uint8_t ESP8266_SendCmd(ESP8266_Bus_Ctx *ctx, const char *cmd, const char *expect, uint32_t timeout)
{
    uint8_t data[255] = {0};

    HAL_UART_Transmit(ctx->huart, (uint8_t*)cmd, strlen(cmd), 100);
    HAL_UART_Receive (ctx->huart, data, 255, timeout);

    return strstr((char *)data, expect) != NULL ? 1 : 0;
}

/**
 * @brief 初始化 ESP8266
 * 
 * @param ctx ESP8266_Bus_Ctx 句柄指针
 * @param huart HAL 库初始化 UART 句柄指针
 * @return uint8_t 1：成功，0：失败
 */
uint8_t ESP8266_Init(ESP8266_Bus_Ctx *ctx, UART_HandleTypeDef *huart)
{
    if (ctx == NULL || huart == NULL) return 0;
    ctx->huart = huart;

    HAL_Delay(1000);

    // 1. 测试通信：发"AT\r\n"，期望收到"OK"
    if (!ESP8266_SendCmd(ctx, "AT\r\n", "OK", 100))
        return 0;
    // 2. 设置Station模式：发"AT+CWMODE=1\r\n"，期望"OK"
    if (!ESP8266_SendCmd(ctx, "AT+CWMODE=1\r\n", "OK", 100))
        return 0;
    // 3. 连接WiFi：发"AT+CWJAP=\"SSID\",\"PASSWORD\"\r\n"，期望"GOT IP"
    // WiFi名称：12345678，密码：66666666
    if (!ESP8266_SendCmd(ctx, "AT+CWJAP=\"12345678\",\"66666666\"\r\n", "GOT IP", 10000))
        return 0;
    // 4. 全部成功返回1，任何一步失败返回0
    return 1;
}

/**
 * @brief 建立 TCP 连接
 * 
 * @param ctx ESP8266_Bus_Ctx 句柄指针
 * @param host 地址
 * @param port 端口
 * @return uint8_t 1：成功，0：失败
 */
uint8_t ESP8266_TCPConnect(ESP8266_Bus_Ctx *ctx, const char* host, uint16_t port)
{
    char cmd[128] = {0};
    // 拼接指令：AT+CIPSTART="TCP","host",port
    sprintf(cmd, "AT+CIPSTART=\"TCP\",\"%s\",%d\r\n", host, port);
    
    if(!ESP8266_SendCmd(ctx, cmd, "CONNECT", 10000))
        return 0;

    return 1;
}

/**
 * @brief 发送任意数据
 * 
 * @param ctx ESP8266_Bus_Ctx 句柄指针
 * @param data 要发送的数据
 * @param length 数据长度
 * @return uint8_t 1：成功，0：失败
 */
uint8_t ESP8266_Send(ESP8266_Bus_Ctx *ctx, const uint8_t* data, uint16_t length)
{
    char cmd[128] = {0};
    sprintf(cmd, "AT+CIPSEND=%d\r\n", length);

    // 发送 AT+CIPSEND=length，告诉 ESP8266 要发多少字节
    if(!ESP8266_SendCmd(ctx, cmd, ">", 1000))
        return 0;

    // 发送原始字节数据
    HAL_UART_Transmit(ctx->huart, data, length, 100);

    return 1;

}

/* ─────────────────────────────────────────────────────────────────────────────
 * CubeMX 配置说明
 * ─────────────────────────────────────────────────────────────────────────────
 *
 * 【Connectivity → USART1】
 *   ├─ Mode         : Asynchronous
 *   ├─ Baud Rate    : 115200
 *   ├─ Word Length  : 8 Bits
 *   ├─ Parity       : None
 *   └─ Stop Bits    : 1
 *
 * 【NVIC Settings】
 *   └─ 阻塞模式无需开启 USART 中断
 *
 * 【接线说明】
 *   ├─ PA9  → USART1_TX → ESP8266 RX
 *   └─ PA10 → USART1_RX → ESP8266 TX
 *
 * 【注意事项】
 *   ├─ ESP8266 供电须使用 3.3V，不可直接接 5V
 *   ├─ ESP8266_Init 必须在 MX_USART1_UART_Init 之后调用
 *   ├─ 本驱动固件版本为 1.2.0.0，不支持 AT+MQTT 指令
 *   └─ ESP8266_Send 发送前须确保 TCP 连接已建立
 *
 * ─────────────────────────────────────────────────────────────────────────────*/