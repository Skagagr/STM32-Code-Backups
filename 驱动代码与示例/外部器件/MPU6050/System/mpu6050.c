/**
 * @file    mpu6050.c
 * @brief   MPU6050 六轴传感器驱动实现（I2C，卡尔曼滤波输出角度）
 *
 * @details
 *  通过 I2C_Bus 抽象层与 MPU6050 通信，每次 Update 读取 14 字节
 *  原始数据（加速度计 6字节 + 温度 2字节 + 陀螺仪 6字节），
 *  换算后经卡尔曼滤波输出稳定角度。
 *
 * @version 1.0.0
 * @date    2026-03-15
 */

#include "mpu6050.h"
#include <math.h>
#include <string.h>

/* ─────────────────────────────────────────────────────────────────────────────
 * 私有：寄存器读写
 * ───────────────────────────────────────────────────────────────────────────*/

static uint8_t mpu_write_reg(MPU6050_Ctx *ctx, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return I2C_Bus_Write(ctx->bus, MPU6050_I2C_ADDR, buf, 2);
}

static uint8_t mpu_read_reg(MPU6050_Ctx *ctx, uint8_t reg,
                            uint8_t *pData, uint16_t len)
{
    return I2C_Bus_ReadReg(ctx->bus, MPU6050_I2C_ADDR, reg, pData, len);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * 初始化
 * ───────────────────────────────────────────────────────────────────────────*/

uint8_t MPU6050_Init(MPU6050_Ctx *ctx, I2C_Bus_Ctx *bus)
{
    if (ctx == NULL || bus == NULL) return 0;

    ctx->bus      = bus;
    ctx->inited   = 0;
    ctx->angle_x  = 0.0f;
    ctx->angle_y  = 0.0f;
    ctx->last_tick = 0;
    memset(&ctx->raw, 0, sizeof(ctx->raw));

    Kalman_Init(&ctx->kf_x);
    Kalman_Init(&ctx->kf_y);

    /* 上电稳定延时 */
    HAL_Delay(100);

    /* 校验设备 ID */
    if (!MPU6050_Check(ctx)) return 0;

    /* 解除休眠，使用内部 8MHz 振荡器 */
    if (!mpu_write_reg(ctx, MPU6050_REG_PWR_MGMT_1, 0x00)) return 0;
    HAL_Delay(10);

    /* 采样率 = 陀螺仪输出率 / (1 + SMPLRT_DIV)
     * 陀螺仪输出率 1kHz，SMPLRT_DIV=4 → 采样率 200Hz，对应 dt=5ms */
    if (!mpu_write_reg(ctx, MPU6050_REG_SMPLRT_DIV, 0x04)) return 0;

    /* 低通滤波：带宽 44Hz（0x03），滤除高频振动噪声 */
    if (!mpu_write_reg(ctx, MPU6050_REG_CONFIG, 0x03)) return 0;

    /* 陀螺仪量程：±500°/s（0x08）*/
    if (!mpu_write_reg(ctx, MPU6050_REG_GYRO_CONFIG, 0x08)) return 0;

    /* 加速度计量程：±2g（0x00）*/
    if (!mpu_write_reg(ctx, MPU6050_REG_ACCEL_CONFIG, 0x00)) return 0;

    ctx->inited    = 1;
    ctx->last_tick = HAL_GetTick();
    return 1;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * 设备校验
 * ───────────────────────────────────────────────────────────────────────────*/

uint8_t MPU6050_Check(MPU6050_Ctx *ctx)
{
    if (ctx == NULL) return 0;
    uint8_t id = 0;
    if (!mpu_read_reg(ctx, MPU6050_REG_WHO_AM_I, &id, 1)) return 0;
    // return (id == 0x68) ? 1 : 0;  /* MPU6050 固定返回 0x68 */    // 原本的
    return (id == 0x68 || id == 0x70) ? 1 : 0;
/*
`0x70`，不是 `0x68`。

我的模块实际芯片可能是 MPU6500 而不是 MPU6050，MPU6500 的 WHO_AM_I 返回 `0x70`。

两个芯片寄存器基本兼容，只需要把校验值改一下：

return (id == 0x68 || id == 0x70) ? 1 : 0;

改完应该就能通过初始化了
*/
}

/* ─────────────────────────────────────────────────────────────────────────────
 * 数据更新
 * ───────────────────────────────────────────────────────────────────────────*/

uint8_t MPU6050_Update(MPU6050_Ctx *ctx)
{
    if (ctx == NULL || !ctx->inited) return 0;

    /* 一次读取 14 字节：加速度计(6) + 温度(2) + 陀螺仪(6) */
    uint8_t buf[14];
    if (!mpu_read_reg(ctx, MPU6050_REG_ACCEL_XOUT_H, buf, 14)) return 0;

    /* 拼接原始数据（大端序）*/
    ctx->raw.accel_x = (int16_t)((buf[0]  << 8) | buf[1]);
    ctx->raw.accel_y = (int16_t)((buf[2]  << 8) | buf[3]);
    ctx->raw.accel_z = (int16_t)((buf[4]  << 8) | buf[5]);
    ctx->raw.temp_raw= (int16_t)((buf[6]  << 8) | buf[7]);
    ctx->raw.gyro_x  = (int16_t)((buf[8]  << 8) | buf[9]);
    ctx->raw.gyro_y  = (int16_t)((buf[10] << 8) | buf[11]);
    ctx->raw.gyro_z  = (int16_t)((buf[12] << 8) | buf[13]);

    /* 换算为物理量 */
    ctx->accel_x = (float)ctx->raw.accel_x / MPU6050_ACCEL_SCALE;
    ctx->accel_y = (float)ctx->raw.accel_y / MPU6050_ACCEL_SCALE;
    ctx->accel_z = (float)ctx->raw.accel_z / MPU6050_ACCEL_SCALE;
    ctx->gyro_x  = (float)ctx->raw.gyro_x  / MPU6050_GYRO_SCALE;
    ctx->gyro_y  = (float)ctx->raw.gyro_y  / MPU6050_GYRO_SCALE;
    ctx->gyro_z  = (float)ctx->raw.gyro_z  / MPU6050_GYRO_SCALE;
    ctx->temp    = (float)ctx->raw.temp_raw / MPU6050_TEMP_SCALE + MPU6050_TEMP_OFFSET;

    /* 计算 dt（秒）*/
    uint32_t now = HAL_GetTick();
    float dt = (float)(now - ctx->last_tick) / 1000.0f;
    if (dt <= 0.0f || dt > 0.5f) dt = (float)MPU6050_SAMPLE_MS / 1000.0f;
    ctx->last_tick = now;

    /* 加速度计计算角度
     * X 轴角度（Roll）：绕 X 轴转动，用 Y、Z 轴加速度计算
     * Y 轴角度（Pitch）：绕 Y 轴转动，用 X、Z 轴加速度计算 */
    float acc_angle_x = atan2f(ctx->accel_y, ctx->accel_z) * 180.0f / (float)M_PI;
    float acc_angle_y = atan2f(-ctx->accel_x,
                                sqrtf(ctx->accel_y * ctx->accel_y +
                                      ctx->accel_z * ctx->accel_z)) * 180.0f / (float)M_PI;

    /* 卡尔曼滤波融合 */
    ctx->angle_x = Kalman_Update(&ctx->kf_x, acc_angle_x, ctx->gyro_x, dt);
    ctx->angle_y = Kalman_Update(&ctx->kf_y, acc_angle_y, ctx->gyro_y, dt);

    return 1;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * 温度读取
 * ───────────────────────────────────────────────────────────────────────────*/

uint8_t MPU6050_GetTemp(MPU6050_Ctx *ctx, float *temp)
{
    if (ctx == NULL || temp == NULL || !ctx->inited) return 0;
    *temp = ctx->temp;
    return 1;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * CubeMX 配置说明
 * ───────────────────────────────────────────────────────────────────────────
 *
 * 【Connectivity → I2C1】
 *   └─ 与 OLED 共享，已配置，无需改动
 *
 * 【接线说明】
 *   ├─ VCC → 3.3V
 *   ├─ GND → GND
 *   ├─ SCL → PB6（与 OLED 共接）
 *   ├─ SDA → PB7（与 OLED 共接）
 *   ├─ AD0 → GND（通过 4.7kΩ 下拉，I2C 地址 0x68 = 8位 0xD0）
 *   └─ INT → 暂不使用（本驱动使用轮询方式）
 *
 * 【注意事项】
 *   ├─ MPU6050 与 OLED 挂在同一 I2C 总线，地址不冲突（0xD0 vs 0x78）
 *   ├─ MPU6050_Update 建议每 5ms 调用一次，可放在 TIM 中断或主循环
 *   ├─ 小车直立时 angle_x 应接近 0°，安装角度偏差可在软件中加偏移修正
 *   └─ 卡尔曼参数在 kalman.h 配置区调整：
 *        Q_ANGLE 越小越信任陀螺仪（响应快但可能漂移）
 *        R_MEASURE 越大越信任陀螺仪（平滑但响应慢）
 *
 * ───────────────────────────────────────────────────────────────────────────*/
