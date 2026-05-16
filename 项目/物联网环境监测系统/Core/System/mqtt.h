/**
 * @file    mqtt.h
 * @brief   MQTT 3.1.1 协议层接口
 *
 * @details
 *  定义 MQTT CONNECT 和 PUBLISH 公开接口函数声明。
 *  依赖 esp8266.h，调用前须完成 ESP8266 初始化和 TCP 连接。
 *
 * @version 1.0.0
 * @date    2026-05-04
 */
#ifndef __MQTT_H
#define __MQTT_H

#include "main.h"
#include "esp8266.h"

/**
 * @brief MQTT 连接
 * 
 * @param ctx ESP8266_Bus_Ctx 句柄指针
 * @return uint8_t 1
 */
uint8_t MQTT_Connect(ESP8266_Bus_Ctx *ctx);

/**
 * @brief 
 * 
 * @param ctx ESP8266_Bus_Ctx 句柄指针
 * @param temp 温度
 * @param humi 湿度
 * @return uint8_t 1：成功，0：失败
 */
uint8_t MQTT_Publish(ESP8266_Bus_Ctx *ctx, float temp, float humi);

#endif