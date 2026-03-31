/**
 * @file    kalman.c
 * @brief   一维卡尔曼滤波器实现
 *
 * @details
 *  状态向量：[angle, bias]
 *  预测步骤：利用陀螺仪角速度积分预测角度
 *  更新步骤：用加速度计角度修正预测值，同时估计零漂
 *
 * @version 1.0.0
 * @date    2026-03-15
 */

#include "kalman.h"
#include <string.h>

/* ─────────────────────────────────────────────────────────────────────────────
 * 初始化
 * ───────────────────────────────────────────────────────────────────────────*/

void Kalman_Init(Kalman_t *kf)
{
    if (kf == NULL) return;
    kf->angle   = 0.0f;
    kf->bias    = 0.0f;
    kf->rate    = 0.0f;
    kf->P[0][0] = 0.0f;
    kf->P[0][1] = 0.0f;
    kf->P[1][0] = 0.0f;
    kf->P[1][1] = 0.0f;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * 卡尔曼滤波更新
 * ───────────────────────────────────────────────────────────────────────────*/

float Kalman_Update(Kalman_t *kf, float acc_angle, float gyro_rate, float dt)
{
    if (kf == NULL) return 0.0f;

    /* ── 预测步骤 ── */

    /* 去除零漂后的角速度 */
    kf->rate = gyro_rate - kf->bias;

    /* 角度预测：用陀螺仪积分 */
    kf->angle += dt * kf->rate;

    /* 更新误差协方差矩阵 */
    kf->P[0][0] += dt * (dt * kf->P[1][1] - kf->P[0][1] - kf->P[1][0] + KALMAN_Q_ANGLE);
    kf->P[0][1] -= dt * kf->P[1][1];
    kf->P[1][0] -= dt * kf->P[1][1];
    kf->P[1][1] += KALMAN_Q_BIAS * dt;

    /* ── 更新步骤 ── */

    /* 计算卡尔曼增益 */
    float S = kf->P[0][0] + KALMAN_R_MEASURE;
    float K0 = kf->P[0][0] / S;
    float K1 = kf->P[1][0] / S;

    /* 用加速度计角度修正预测值 */
    float y = acc_angle - kf->angle;
    kf->angle += K0 * y;
    kf->bias  += K1 * y;

    /* 更新协方差矩阵 */
    float P00_temp = kf->P[0][0];
    float P01_temp = kf->P[0][1];
    kf->P[0][0] -= K0 * P00_temp;
    kf->P[0][1] -= K0 * P01_temp;
    kf->P[1][0] -= K1 * P00_temp;
    kf->P[1][1] -= K1 * P01_temp;

    return kf->angle;
}
