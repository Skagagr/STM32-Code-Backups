/**
 * @file    mpu6050.h
 * @brief   MPU6050 六轴传感器驱动（I2C，卡尔曼滤波输出角度）
 *
 * @details
 *  基于 I2C_Bus 抽象层驱动 MPU6050，提供：
 *    - 加速度计原始数据读取（±2g）
 *    - 陀螺仪原始数据读取（±500°/s）
 *    - 卡尔曼滤波融合输出 X/Y 轴倾斜角度
 *    - 温度读取
 *
 *  【坐标系说明】
 *    MPU6050 芯片正面朝上水平放置时：
 *    X 轴：向右为正（Roll，横滚角）
 *    Y 轴：向前为正（Pitch，俯仰角）
 *    Z 轴：向上为正
 *
 *  典型使用流程：
 *  @code
 *      // 1. 声明句柄
 *      MPU6050_Ctx mpu_ctx;
 *
 *      // 2. 初始化（I2C_Bus_Init 之后调用）
 *      MPU6050_Init(&mpu_ctx, &i2c1_ctx);
 *
 *      // 3. 主循环中定时更新（建议 5ms 调用一次，即 200Hz）
 *      MPU6050_Update(&mpu_ctx);
 *
 *      // 4. 读取角度
 *      float pitch = mpu_ctx.angle_x;   // X 轴倾斜角（°）
 *      float roll  = mpu_ctx.angle_y;   // Y 轴倾斜角（°）
 *  @endcode
 *
 * @version 1.0.0
 * @date    2026-03-15
 */

#ifndef __MPU6050_H
#define __MPU6050_H

#include "main.h"
#include "i2c_bus.h"
#include "kalman.h"
#include <stdint.h>

/* ── 配置区 —— 按工程需求修改 ── */
#define MPU6050_I2C_ADDR     0xD0U   /**< AD0 接 GND → 0x68（8位格式 0xD0）
                                          AD0 接 VCC → 0x69（8位格式 0xD2）*/
#define MPU6050_SAMPLE_MS    5U      /**< 采样间隔（ms），对应 200Hz */

/* ── MPU6050 寄存器地址 ── */
#define MPU6050_REG_SMPLRT_DIV    0x19U  /**< 采样率分频 */
#define MPU6050_REG_CONFIG        0x1AU  /**< 低通滤波配置 */
#define MPU6050_REG_GYRO_CONFIG   0x1BU  /**< 陀螺仪量程配置 */
#define MPU6050_REG_ACCEL_CONFIG  0x1CU  /**< 加速度计量程配置 */
#define MPU6050_REG_ACCEL_XOUT_H  0x3BU  /**< 加速度计数据起始寄存器 */
#define MPU6050_REG_TEMP_OUT_H    0x41U  /**< 温度数据寄存器 */
#define MPU6050_REG_GYRO_XOUT_H   0x43U  /**< 陀螺仪数据起始寄存器 */
#define MPU6050_REG_PWR_MGMT_1    0x6BU  /**< 电源管理寄存器 */
#define MPU6050_REG_WHO_AM_I      0x75U  /**< 设备 ID，固定返回 0x68 */

/* ── 量程换算系数 ── */
#define MPU6050_ACCEL_SCALE  16384.0f  /**< ±2g：1g = 16384 LSB */
#define MPU6050_GYRO_SCALE   65.5f     /**< ±500°/s：1°/s = 65.5 LSB */
#define MPU6050_TEMP_SCALE   340.0f    /**< 温度换算系数 */
#define MPU6050_TEMP_OFFSET  36.53f    /**< 温度换算偏移 */

/* ── 类型定义 ── */

/**
 * @brief  MPU6050 原始数据（ADC 值）
 */
typedef struct
{
    int16_t accel_x;  /**< 加速度计 X 轴原始值 */
    int16_t accel_y;  /**< 加速度计 Y 轴原始值 */
    int16_t accel_z;  /**< 加速度计 Z 轴原始值 */
    int16_t gyro_x;   /**< 陀螺仪 X 轴原始值 */
    int16_t gyro_y;   /**< 陀螺仪 Y 轴原始值 */
    int16_t gyro_z;   /**< 陀螺仪 Z 轴原始值 */
    int16_t temp_raw; /**< 温度原始值 */
} MPU6050_Raw_t;

/**
 * @brief  MPU6050 模块上下文句柄
 */
typedef struct
{
    I2C_Bus_Ctx   *bus;       /**< I2C 总线句柄指针 */
    MPU6050_Raw_t  raw;       /**< 最新原始数据 */
    float          accel_x;   /**< 加速度计 X 轴（g）*/
    float          accel_y;   /**< 加速度计 Y 轴（g）*/
    float          accel_z;   /**< 加速度计 Z 轴（g）*/
    float          gyro_x;    /**< 陀螺仪 X 轴角速度（°/s）*/
    float          gyro_y;    /**< 陀螺仪 Y 轴角速度（°/s）*/
    float          gyro_z;    /**< 陀螺仪 Z 轴角速度（°/s）*/
    float          temp;      /**< 温度（°C）*/
    float          angle_x;   /**< X 轴卡尔曼滤波角度（°），自平衡小车主要使用此值 */
    float          angle_y;   /**< Y 轴卡尔曼滤波角度（°）*/
    Kalman_t       kf_x;      /**< X 轴卡尔曼滤波器 */
    Kalman_t       kf_y;      /**< Y 轴卡尔曼滤波器 */
    uint32_t       last_tick; /**< 上次更新时间戳（ms），用于计算 dt */
    uint8_t        inited;    /**< 初始化标志：1=已初始化 */
} MPU6050_Ctx;

/* ── 公开 API ── */

/**
 * @brief  初始化 MPU6050
 *
 * @details
 *  配置采样率、低通滤波、量程，唤醒芯片。
 *  必须在 I2C_Bus_Init() 之后调用，且上电需等待 100ms。
 *
 * @param  ctx  MPU6050 上下文指针
 * @param  bus  已初始化的 I2C 总线上下文指针
 * @retval 1    成功
 * @retval 0    失败（设备未响应或 WHO_AM_I 校验失败）
 */
uint8_t MPU6050_Init(MPU6050_Ctx *ctx, I2C_Bus_Ctx *bus);

/**
 * @brief  更新一次 MPU6050 数据（读取 + 卡尔曼滤波）
 *
 * @details
 *  读取加速度计和陀螺仪原始数据，换算为物理量，
 *  用卡尔曼滤波融合输出 angle_x / angle_y。
 *  建议在 TIM 中断或主循环中每 5ms 调用一次（200Hz）。
 *
 * @param  ctx  MPU6050 上下文指针
 * @retval 1    成功
 * @retval 0    读取失败
 */
uint8_t MPU6050_Update(MPU6050_Ctx *ctx);

/**
 * @brief  读取 MPU6050 温度
 *
 * @param  ctx   MPU6050 上下文指针
 * @param  temp  输出温度值（°C）
 * @retval 1     成功
 * @retval 0     失败
 */
uint8_t MPU6050_GetTemp(MPU6050_Ctx *ctx, float *temp);

/**
 * @brief  校验设备是否在线（读取 WHO_AM_I 寄存器）
 *
 * @param  ctx  MPU6050 上下文指针
 * @retval 1    设备在线且 ID 正确
 * @retval 0    设备不在线或 ID 错误
 */
uint8_t MPU6050_Check(MPU6050_Ctx *ctx);

#endif /* __MPU6050_H */
