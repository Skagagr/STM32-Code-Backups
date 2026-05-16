/**
 * @file    esp8266.h
 * @brief   ESP8266 AT 指令驱动层接口
 *
 * @details
 *  定义 ESP8266_Bus_Ctx 上下文结构体及所有公开接口函数声明。
 *  包含此头文件即可使用 ESP8266 驱动，无需关心底层实现。
 *
 * @version 1.0.0
 * @date    2026-05-03
 */
#ifndef __ESP8266_H
#define __ESP8266_H

#include "main.h"


typedef struct
{
    UART_HandleTypeDef *huart;    /**< 指向 CubeMX 生成的 HAL UART 句柄 */
} ESP8266_Bus_Ctx;

/**
 * @brief 发送命令
 * 
 * @param ctx ESP8266_Bus_Ctx 句柄指针
 * @param cmd 命令
 * @param expect 期望的回复
 * @param timeout 接收超时
 * @return uint8_t 
 */
uint8_t ESP8266_SendCmd(ESP8266_Bus_Ctx *ctx, const char *cmd, const char *expect, uint32_t timeout);

/**
 * @brief 初始化 ESP8266
 * 
 * @param ctx ESP8266_Bus_Ctx 句柄指针
 * @param huart HAL 库初始化 UART 句柄指针
 * @return uint8_t 1：成功，4：通信失败，5：设置Station模式失败，6：连接WiFi失败
 */
uint8_t ESP8266_Init(ESP8266_Bus_Ctx *ctx, UART_HandleTypeDef *huart);

/**
 * @brief 建立 TCP 连接
 * 
 * @param ctx ESP8266_Bus_Ctx 句柄指针
 * @param host 地址
 * @param port 端口
 * @return uint8_t 1：成功，0：失败
 */
uint8_t ESP8266_TCPConnect(ESP8266_Bus_Ctx *ctx, const char* host, uint16_t port);

/**
 * @brief 发送任意数据
 * 
 * @param ctx ESP8266_Bus_Ctx 句柄指针
 * @param data 要发送的数据
 * @param length 数据长度
 * @return uint8_t 1：成功，0：失败
 */
uint8_t ESP8266_Send(ESP8266_Bus_Ctx *ctx, const uint8_t* data, uint16_t length);


#endif