/**
 * @file    adc.c
 * @brief   ADC 采集模块实现
 *
 * @details
 *  使用 HAL 轮询方式单次采集，适合低频温度采样场景。
 *  温度计算采用 NTC 热敏电阻 Steinhart-Hart 简化公式（B 值公式）：
 *
 *  1/T = 1/T0 + (1/B) × ln(R/R0)
 *
 * @version 1.0.0
 * @date    2026-03-12
 */

#include "adc.h"
#include <math.h>

/* ─────────────────────────────────────────────────────────────────────────────
 * 内部变量
 * ───────────────────────────────────────────────────────────────────────────*/

static ADC_HandleTypeDef *_hadc;

/* ─────────────────────────────────────────────────────────────────────────────
 * 初始化
 * ───────────────────────────────────────────────────────────────────────────*/

void ADC_Module_Init(ADC_HandleTypeDef *hadc)
{
    _hadc = hadc;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * 采集
 * ───────────────────────────────────────────────────────────────────────────*/

uint16_t ADC_ReadRaw(void)
{
    HAL_ADC_Start(_hadc);
    HAL_ADC_PollForConversion(_hadc, 10);
    uint16_t value = HAL_ADC_GetValue(_hadc);
    HAL_ADC_Stop(_hadc);
    return value;
}

float ADC_ReadVoltage(void)
{
    return ADC_ReadRaw() * ADC_VREF / ADC_MAX_VALUE;
}

float ADC_ReadTemperature(void)
{
    uint16_t raw = ADC_ReadRaw();

    if (raw == 0 || raw == ADC_MAX_VALUE) return -999.0f;

    float r_ntc = NTC_R_PULLUP * raw / (ADC_MAX_VALUE - raw);

    float temp_k = 1.0f / (
        1.0f / NTC_T_NOMINAL +
        1.0f / NTC_B_VALUE * log(r_ntc / NTC_R_NOMINAL)
    );

    return temp_k - 273.15f;
}

/* ─────────────────────────────────────────────────────────────────────────────
 * CubeMX 配置说明
 * ───────────────────────────────────────────────────────────────────────────
 *
 * 【Analog → ADC1】
 *   ├─ IN0 打勾（对应引脚 PA0，接热敏电阻模块 AC 引脚）
 *   └─ Parameter Settings：
 *       ├─ Resolution            : 12 Bits
 *       ├─ Data Alignment        : Right alignment
 *       ├─ Scan Conversion Mode  : Disabled
 *       ├─ Continuous Conversion : Disabled
 *       └─ Discontinuous Convs   : Disabled
 *
 * 【Clock Configuration】
 *   └─ ADC Prescaler 建议设为 /6 或 /8
 *      确保 ADC 时钟 ≤ 14MHz（STM32F103 要求）
 *      72MHz / 6 = 12MHz
 *
 * 【NVIC Settings】
 *   └─ 无需开启 ADC 中断（本模块使用轮询方式）
 *
 * 【接线说明】
 *   ├─ 模块 AC  → PA0
 *   ├─ 模块 VCC → 3.3V
 *   └─ 模块 GND → GND
 *
 * 【Keil 链接器配置】
 *   └─ Options for Target → C/C++ → Misc Controls 添加：
 *      -lm
 *      （adc.c 使用了 math.h 的 log() 函数，必须链接数学库）
 *  【VSCode CMake配置】
 *    └─ CMakeLists.txt 添加：
 *          # 添加这段配置
 *          target_link_options(${CMAKE_PROJECT_NAME} PRIVATE
 *              -Wl,-Map=${CMAKE_PROJECT_NAME}.map
 *              -u _printf_float        # ← 添加这行，开启 sprintf() 浮点支持
 *          )
 *
 * ───────────────────────────────────────────────────────────────────────────*/