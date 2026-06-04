/**
 * @file    mqtt.c
 * @brief   MQTT 3.1.1 协议层实现（手动构造二进制报文）
 *
 * @details
 *   全部报文均以硬编码 + sprintf 方式手动构造，QoS 0。
 *
 *   配置一览
 *   Broker    : broker.emqx.io:1883 (EMQX 公共测试服务器)
 *   Client ID : STM32IoT
 *   Keep Alive: 60 秒（实际由每 500ms 的 PUBLISH 维持）
 *   发布主题  : stm32/sensor (JSON: {"temp":XX.XX,"humi":XX.XX})
 *   订阅主题  : stm32/control (接收 LED_ON / LED_OFF)
 *
 *   协议限制
 *   - 未处理 CONNACK / SUBACK（无确认机制）
 *   - 未发送 PINGREQ（由高频 PUBLISH 替代保活）
 *   - 剩余长度仅支持单字节编码（≤127 字节）
 *
 *   调用顺序
 *   1. ESP8266_Init         → WiFi 连接
 *   2. ESP8266_TCPConnect   → TCP 连接 broker
 *   3. MQTT_Connect         → MQTT 握手
 *   4. MQTT_Subscribe       → 订阅控制主题
 *   5. MQTT_Publish (循环)  → 上报传感器数据
 *
 * @version 1.1.0
 * @date    2026-05-31
 */

#include "mqtt.h"

/* MQTT CONNECT 报文
 *
 * 固定报头   : 0x10 (CONNECT)
 * 剩余长度   : 0x14 (20 字节)
 * 协议名     : "MQTT" (0x00 0x04 'M' 'Q' 'T' 'T')
 * 协议版本   : 0x04 (3.1.1)
 * 连接标志   : 0x02 (Clean Session = 1)
 * 保活时间   : 0x00 0x3C (60 秒)
 * Client ID  : "STM32IoT" (0x00 0x08 'S' 'T' 'M' '3' '2' 'I' 'o' 'T')
 */

uint8_t MQTT_Connect(ESP8266_Bus_Ctx *ctx)
{
    uint8_t cmd[] = {
        0x10, 0x14,                         // 固定报头 + 剩余长度
        0x00, 0x04, 'M', 'Q', 'T', 'T',     // 协议名
        0x04,                               // 协议版本 3.1.1
        0x02,                               // 连接标志（Clean Session）
        0x00, 0x3c,                         // Keep Alive 60s
        0x00, 0x08,                         // Client ID 长度
        'S', 'T', 'M', '3', '2', 'I', 'o', 'T'
    };

    ESP8266_Send(ctx, cmd, sizeof(cmd));
    return 1;
}

/* MQTT PUBLISH 报文（温湿度上报）
 *
 * 固定报头   : 0x30 (PUBLISH, QoS 0, 不保留)
 * 剩余长度   : 2 + 12 + strlen(payload)  （动态计算）
 * 主题长度   : 0x00 0x0C (12)
 * 主题名     : "stm32/sensor"
 * 负载       : sprintf 生成的 JSON 字符串
 *
 * JSON 格式示例: {"temp":25.60,"humi":60.20}
 */

uint8_t MQTT_Publish_Sensor(ESP8266_Bus_Ctx *ctx, float temp, float humi)
{
    uint8_t data[128];
    uint8_t remaining_length;
    char payload[64] = {0};

    // 构造 JSON 负载
    sprintf(payload, "{\"temp\":%.2f,\"humi\":%.2f}", temp, humi);

    // 剩余长度 = 主题长度前缀(2) + 主题名(12) + JSON负载
    remaining_length = strlen(payload) + 2 + 12;

    // 拼装 MQTT PUBLISH 报文
    data[0] = 0x30;                          // 固定报头
    data[1] = (uint8_t)remaining_length;     // 剩余长度（<128 字节，单字节编码）
    data[2] = 0x00;                          // 主题长度 MSB
    data[3] = 0x0C;                          // 主题长度 LSB = 12
    memcpy(&data[4], "stm32/sensor", 12);    // 主题名
    memcpy(&data[16], payload, strlen(payload));  // JSON 负载

    return ESP8266_Send(ctx, data, 2 + remaining_length);
}


/* MQTT PUBLISH 报文（温湿度阈值上报）
 *
 * 固定报头   : 0x30 (PUBLISH, QoS 0, 不保留)
 * 剩余长度   : 2 + 12 + strlen(payload)  （动态计算）
 * 主题长度   : 0x00 0x0C (12)
 * 主题名     : "stm32/threshold"
 * 负载       : sprintf 生成的 JSON 字符串
 *
 * JSON 格式示例: {"temp_threshold":25,"humi_threshold":60}
 */

uint8_t MQTT_Publish_Threshold(ESP8266_Bus_Ctx *ctx, uint8_t temp_threshold, uint8_t humi_threshold)
{
    uint8_t data[128];
    uint8_t remaining_length;
    char payload[64] = {0};

    // 构造 JSON 负载
    sprintf(payload, "{\"temp_threshold\":%d,\"humi_threshold\":%d}", temp_threshold, humi_threshold);

    // 剩余长度 = 主题长度前缀(2) + 主题名(15) + JSON负载
    remaining_length = strlen(payload) + 2 + 15;

    // 拼装 MQTT PUBLISH 报文
    data[0] = 0x30;                                 // 固定报头
    data[1] = (uint8_t)remaining_length;            // 剩余长度（<128 字节，单字节编码）
    data[2] = 0x00;                                 // 主题长度 MSB
    data[3] = 0x0F;                                 // 主题长度 LSB = 15
    memcpy(&data[4], "stm32/threshold", 15);        // 主题名
    memcpy(&data[19], payload, strlen(payload));    // JSON 负载

    return ESP8266_Send(ctx, data, 2 + remaining_length);
}

/* MQTT SUBSCRIBE 报文
 *
 * 固定报头   : 0x82 (SUBSCRIBE, QoS 1)
 * 剩余长度   : 0x12 (18 字节)
 * 报文标识符 : 0x00 0x01 (Packet Identifier = 1)
 * 主题过滤器 : "stm32/control" (0x00 0x0D 's'...'l')
 * 请求 QoS  : 0x00 (QoS 0)
 */

uint8_t MQTT_Subscribe(ESP8266_Bus_Ctx *ctx)
{
    uint8_t cmd[] = {
        0x82, 0x12,                         // 固定报头 + 剩余长度
        0x00, 0x01,                         // 报文标识符
        0x00, 0x0D,                         // 主题长度 = 13
        's', 't', 'm', '3', '2', '/',
        'c', 'o', 'n', 't', 'r', 'o', 'l',  // "stm32/control"
        0x00                                // 请求 QoS = 0
    };

    ESP8266_Send(ctx, cmd, sizeof(cmd));
    return 1;
}

/* 使用说明
 *
 * 【依赖】
 *   └─ esp8266.c/h：需先完成 ESP8266_Init 和 ESP8266_TCPConnect
 *
 * 【调用顺序】
 *   1. ESP8266_Init        // WiFi 连接
 *   2. ESP8266_TCPConnect  // 建立 TCP 连接到 broker
 *   3. MQTT_Connect        // 发送 MQTT CONNECT 报文
 *   4. MQTT_Subscribe      // 订阅控制主题
 *   5. MQTT_Publish        // 循环发送数据
 *
 * 【当前配置】
 *   ├─ Broker   : broker.emqx.io:1883
 *   ├─ Client ID: STM32IoT
 *   ├─ 发布主题 : stm32/sensor
 *   ├─ 订阅主题 : stm32/control
 *   ├─ QoS      : 0
 *   └─ Payload  : {"temp":xx.xx,"humi":xx.xx}
 *
 * 【MQTT 报文手动构造说明】
 *   本文件不使用第三方 MQTT 库，所有报文（CONNECT / PUBLISH / SUBSCRIBE）
 *   均以硬编码数组 + sprintf 方式手动拼接，符合 MQTT 3.1.1 规范。
 *   报文结构详见各函数注释中的逐字节说明。
 *
 * 【注意事项】
 *   ├─ MQTT_Connect 返回 1 不代表 broker 已确认连接（未处理 CONNACK 报文）
 *   ├─ MQTT_Subscribe 返回 1 不代表 broker 已确认订阅（未处理 SUBACK 报文）
 *   ├─ broker 会在 Keep Alive（60 秒）超时后断开连接
 *   └─ 当前由每 500ms 的 PUBLISH 维持心跳，若暂停发布需补充 PINGREQ
 */
