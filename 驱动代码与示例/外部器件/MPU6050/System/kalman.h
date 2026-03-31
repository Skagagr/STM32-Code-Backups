/**
 * @file    kalman.h
 * @brief   一维卡尔曼滤波器
 *
 * @details
 *  通用一维卡尔曼滤波器，可独立用于任何需要滤波的场合。
 *  本工程用于融合 MPU6050 加速度计角度和陀螺仪角速度，
 *  输出平滑、准确的倾斜角度。
 *
 *  典型使用流程：
 *  @code
 *      // 1. 声明并初始化
 *      Kalman_t kx;
 *      Kalman_Init(&kx);
 *
 *      // 2. 每次采样后更新（dt 单位：秒）
 *      float angle = Kalman_Update(&kx, acc_angle, gyro_rate, dt);
 *  @endcode
 *
 * @version 1.0.0
 * @date    2026-03-15
 */

#ifndef __KALMAN_H
#define __KALMAN_H

#include <stdint.h>

/* ── 配置区 —— 按工程需求修改 ── */
#define KALMAN_Q_ANGLE    0.001f  /**< 过程噪声：角度，值越小越信任陀螺仪 */
#define KALMAN_Q_BIAS     0.003f  /**< 过程噪声：陀螺仪零漂，值越小零漂修正越慢 */
#define KALMAN_R_MEASURE  0.03f   /**< 测量噪声：加速度计，值越大越信任陀螺仪 */

/* ── 类型定义 ── */

/**
 * @brief  卡尔曼滤波器状态
 */
typedef struct
{
    float angle;     /**< 当前估计角度（°）*/
    float bias;      /**< 当前估计陀螺仪零漂（°/s）*/
    float rate;      /**< 去除零漂后的陀螺仪角速度（°/s）*/
    float P[2][2];   /**< 误差协方差矩阵 */
} Kalman_t;

/* ── 公开 API ── */

/**
 * @brief  初始化卡尔曼滤波器
 *
 * @details
 *  将状态量和协方差矩阵清零，必须在使用前调用。
 *
 * @param  kf  卡尔曼滤波器指针
 */
void Kalman_Init(Kalman_t *kf);

/**
 * @brief  卡尔曼滤波更新
 *
 * @details
 *  输入加速度计角度和陀螺仪角速度，输出融合后的最优角度估计。
 *  内部自动修正陀螺仪零漂。
 *
 * @param  kf          卡尔曼滤波器指针
 * @param  acc_angle   加速度计计算的角度（°）
 * @param  gyro_rate   陀螺仪角速度（°/s）
 * @param  dt          采样时间间隔（s），例如 0.005 表示 200Hz
 * @retval             融合后的角度估计值（°）
 */
float Kalman_Update(Kalman_t *kf, float acc_angle, float gyro_rate, float dt);

#endif /* __KALMAN_H */
