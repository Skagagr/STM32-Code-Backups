/**
 * @file    mqtt.c
 * @brief   MQTT 3.1.1 协议层实现（QoS 0，手动构造报文）
 *
 * @details
 *  基于 ESP8266 TCP 连接，手动构造 MQTT 3.1.1 二进制报文。
 *  支持 CONNECT 和 PUBLISH（QoS 0）报文。
 *  不依赖第三方 MQTT 库，适用于不支持 AT+MQTT 的旧版 ESP8266 固件。
 *
 * @version 1.0.0
 * @date    2026-05-04
 */
#include "mqtt.h"

/**
 * @brief MQTT 连接
 * 
 * @param ctx ESP8266_Bus_Ctx 句柄指针
 * @return uint8_t 1
 */
uint8_t MQTT_Connect(ESP8266_Bus_Ctx *ctx)
{
    // 报文结构：
    // [Fixed Header]
    // 0x10                         // CONNECT 报文类型
    // remaining_length             // 剩余字节数

    // [Variable Header]
    // 0x00 0x04 'M' 'Q' 'T' 'T'    // 协议名，6字节
    // 0x04                         // 协议版本 3.1.1
    // 0x02                         // 连接标志：Clean Session=1，其余为0
    // 0x00 0x3C                    // 保持连接 60秒

    // [Payload]
    // 0x00 0x08 'S' 'T' 'M' '3' '2' 'I' 'o' 'T'  // Client ID，2字节长度+字符串
    uint8_t cmd[] = {0x10, 0x14, 0x00, 0x04, 'M', 'Q', 'T', 'T', 0x04, 0x02, 0x00, 0x3c, 0x00, 0x08, 'S', 'T', 'M', '3', '2', 'I', 'o', 'T'};

    ESP8266_Send(ctx, cmd, sizeof(cmd));

    return 1;
}

/**
 * @brief 
 * 
 * @param ctx ESP8266_Bus_Ctx 句柄指针
 * @param temp 温度
 * @param humi 湿度
 * @return uint8_t 1：成功，0：失败
 */
uint8_t MQTT_Publish(ESP8266_Bus_Ctx *ctx, float temp, float humi)
{
    // 报文结构：
    // [Fixed Header]
    // 0x30                         // PUBLISH 报文类型，QoS=0
    // remaining_length             // 剩余字节数

    // [Variable Header]
    // 0x00 0x0C                    // topic 长度，12字节
    // 's' 't' 'm' '3' '2' '/' 's' 'e' 'n' 's' 'o' 'r'  // topic: stm32/sensor

    // [Payload]
    // {"temp":xx.xx,"humi":xx.xx}  // 用 sprintf 动态生成
    uint8_t data[128];
    uint8_t remaining_length;
    char payload[64] = {0};

    // 动态生成温度的 json 字符串
    sprintf(payload, "{\"temp\":%.2f,\"humi\":%.2f}", temp, humi);
    // 计算剩余长度，2 = 0x00 0x0C。12 = stm32/sensor
    remaining_length = strlen(payload) + 2 + 12;

    // 组成要发送的数据
    data[0] = 0x30;
    data[1] = (uint8_t)remaining_length;
    data[2] = 0x00;
    data[3] = 0x0C;
    memcpy(&data[4], "stm32/sensor", 12);
    memcpy(&data[16], payload, strlen(payload));

    return ESP8266_Send(ctx, data, 2 + remaining_length);
}
/* ─────────────────────────────────────────────────────────────────────────────
 * 使用说明
 * ─────────────────────────────────────────────────────────────────────────────
 *
 * 【依赖】
 *   └─ esp8266.c/h：需先完成 ESP8266_Init 和 ESP8266_TCPConnect
 *
 * 【调用顺序】
 *   1. ESP8266_Init        // WiFi 连接
 *   2. ESP8266_TCPConnect  // 建立 TCP 连接到 broker
 *   3. MQTT_Connect        // 发送 MQTT CONNECT 报文
 *   4. MQTT_Publish        // 循环发送数据
 *
 * 【当前配置】
 *   ├─ Broker   : broker.emqx.io:1883
 *   ├─ Client ID: STM32IoT
 *   ├─ Topic    : stm32/sensor
 *   ├─ QoS      : 0
 *   └─ Payload  : {"temp":xx.xx,"humi":xx.xx}
 *
 * 【注意事项】
 *   ├─ MQTT_Connect 返回 1 不代表 broker 已确认连接（未处理 CONNACK 报文）
 *   └─ broker 会在 Keep Alive（60秒）超时后断开连接，需定期发送 PINGREQ
 *
 * ─────────────────────────────────────────────────────────────────────────────*/