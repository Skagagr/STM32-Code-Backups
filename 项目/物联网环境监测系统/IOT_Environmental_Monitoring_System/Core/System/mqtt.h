/**
 * @file    mqtt.h
 * @brief   MQTT 3.1.1 协议层接口
 *
 * @details
 *   手动构造 MQTT 3.1.1 二进制报文（CONNECT / PUBLISH / SUBSCRIBE），
 *   不依赖第三方 MQTT 库。适用于不支持 AT+MQTT 指令的旧版 ESP8266 固件。
 *
 *   报文格式均基于 MQTT 3.1.1 规范，QoS 0（至多一次）。
 *
 *   依赖
 *   ────
 *   - esp8266.h (ESP8266_Send)
 *   - main.h    (string.h, stdio.h)
 *
 * @version 1.1.0
 * @date    2026-05-31
 */

#ifndef __MQTT_H
#define __MQTT_H

#include "main.h"
#include "esp8266.h"

/**
 * @brief  发送 MQTT CONNECT 报文
 *
 * 连接参数：Client ID = "STM32IoT", Keep Alive = 60s, Clean Session = 1。
 * 注意：此函数不等待 CONNACK，直接返回。
 *
 * @param ctx  ESP8266 上下文指针
 * @return     始终返回 1
 */
uint8_t MQTT_Connect(ESP8266_Bus_Ctx *ctx);

/**
 * @brief  发送温湿度传感器数据
 *
 * 构造 MQTT PUBLISH 报文，发布 JSON 格式温湿度到主题 stm32/sensor。
 * 负载格式：{"temp":XX.XX,"humi":XX.XX}
 *
 * @param ctx  ESP8266 上下文指针
 * @param temp 温度值（℃）
 * @param humi 湿度值（%RH）
 * @return     1=成功, 0=发送失败
 */
uint8_t MQTT_Publish(ESP8266_Bus_Ctx *ctx, float temp, float humi);

/**
 * @brief  订阅云端控制主题
 *
 * 订阅主题 stm32/control (QoS 0)，用于接收 LED_ON / LED_OFF 指令。
 * 注意：此函数不等待 SUBACK，直接返回。
 *
 * @param ctx  ESP8266 上下文指针
 * @return     始终返回 1
 */
uint8_t MQTT_Subscribe(ESP8266_Bus_Ctx *ctx);

#endif /* __MQTT_H */
