#ifndef __SHT3X_H
#define __SHT3X_H

#include "i2c_bus.h"

#define SHT30_I2C_ADDR   0x88U

typedef struct
{
    I2C_Bus_Ctx *bus;                          // I2C 总线句柄指针
    uint8_t buf[6];         // 读取的6字节原始数据
} SHT30_Ctx;

/**
 * @brief SHT30初始化
 * 
 * @param ctx SHT30_Ctx 句柄指针
 * @param bus I2C_Bus_Ctx 句柄指针
 * @return uint8_t 1：成功， 0：失败
 */
uint8_t SHT30_Init(SHT30_Ctx *ctx, I2C_Bus_Ctx *bus);

/**
 * @brief 读取温度与湿度
 * 
 * @param ctx SHT30_Ctx 句柄指针
 * @param temp 温度
 * @param humi 湿度
 * @return uint8_t 1：成功， 0：失败
 */
uint8_t SHT30_Read(SHT30_Ctx *ctx, float *temp, float *humi);

#endif