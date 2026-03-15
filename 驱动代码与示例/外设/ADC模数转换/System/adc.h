/**
 * @file    adc.h
 * @brief   ADC 采集模块接口
 *
 * @details
 *  封装 STM32 HAL ADC 单次采集，
 *  将原始 ADC 值转换为电压，再映射为温度。
 *
 * @version 1.0.0
 * @date    2026-03-14
 */

#ifndef __ADC_H
#define __ADC_H

#include "main.h"
#include <stdint.h>

/* ─────────────────────────────────────────────────────────────────────────────
 * 配置区
 * ───────────────────────────────────────────────────────────────────────────*/

/** @brief ADC 参考电压（V），STM32F103 供电 3.3V */
#define ADC_VREF        3.3f

/** @brief ADC 分辨率最大值（12位 = 4095） */
#define ADC_MAX_VALUE   4095

/** @brief 热敏电阻上拉电阻阻值（Ω），对应电路图 R1 = 10K */
#define NTC_R_PULLUP    10000.0f

/** @brief 热敏电阻 25°C 标称阻值（Ω），常见 NTC 为 10K */
#define NTC_R_NOMINAL   10000.0f

/** @brief 热敏电阻标称温度（K） */
#define NTC_T_NOMINAL   298.15f

/** @brief 热敏电阻 B 值，常见 NTC 10K 约为 3950 */
#define NTC_B_VALUE     3950.0f

/* ─────────────────────────────────────────────────────────────────────────────
 * 公开 API
 * ───────────────────────────────────────────────────────────────────────────*/

/**
 * @brief  初始化 ADC 模块
 * @param  hadc  已完成硬件初始化的 HAL ADC 句柄指针
 */
void ADC_Module_Init(ADC_HandleTypeDef *hadc);

/**
 * @brief  读取原始 ADC 值（0 ~ 4095）
 * @retval 原始 ADC 值
 */
uint16_t ADC_ReadRaw(void);

/**
 * @brief  读取电压值
 * @retval 电压（V），范围 0.0 ~ 3.3
 */
float ADC_ReadVoltage(void);

/**
 * @brief  读取温度值（通过 NTC B 值公式计算）
 * @retval 温度（°C）
 */
float ADC_ReadTemperature(void);

#endif /* __ADC_H */