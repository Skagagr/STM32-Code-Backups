/**
 * @file    epd.h
 * @brief   2.13寸墨水屏驱动头文件（SSD1680Z8，250x122，黑白）
 *
 * @details
 *
 *  设计目标：
 *   - 提供统一墨水屏驱动API接口
 *   - 屏蔽底层SPI/GPIO实现细节
 *   - 支持显示/清屏/休眠/唤醒基础功能
 *
 *  依赖：
 *   - STM32 HAL库
 *   - SPI1外设
 *   - GPIO控制引脚（RST/DC/CS/BUSY）
 *
 *  约束：
 *   - RAM按128字节对齐（实际122像素）
 *   - 必须遵循SSD1680初始化时序
 *   - BUSY必须轮询等待完成
 *
 * @version 1.0
 * @date    2025-04-18
 */

#ifndef __EPD_H
#define __EPD_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

/* -----------------------------------------------------------------------
 * 硬件配置
 * ----------------------------------------------------------------------- */

/* SPI接口（固定SPI1） */
#define EPD_SPI         SPI1

/* GPIO端口（控制线统一GPIOB） */
#define EPD_GPIO_PORT   GPIOB

/* 控制引脚定义（SSD1680标准控制信号） */
#define EPD_RST_PIN     GPIO_PIN_12   // 复位：低有效
#define EPD_DC_PIN      GPIO_PIN_13   // 数据/命令选择
#define EPD_CS_PIN      GPIO_PIN_14   // SPI片选：低有效
#define EPD_BUSY_PIN    GPIO_PIN_15   // 忙信号：高=忙

/* -----------------------------------------------------------------------
 * 显示参数
 * ----------------------------------------------------------------------- */

/* 物理分辨率（SSD1680 RAM按字节对齐） */
#define EPD_WIDTH           128
#define EPD_HEIGHT          250

/* 实际可见区域 */
#define EPD_PIXEL_WIDTH     122
#define EPD_PIXEL_HEIGHT    250

/* -----------------------------------------------------------------------
 * SSD1680指令集（硬件寄存器定义）
 * ----------------------------------------------------------------------- */

#define EPD_CMD_DRIVER_OUTPUT           0x01
#define EPD_CMD_GATE_DRIVING_VOLTAGE    0x03
#define EPD_CMD_SOURCE_DRIVING_VOLTAGE  0x04
#define EPD_CMD_BOOSTER_SOFT_START      0x0C
#define EPD_CMD_DEEP_SLEEP              0x10
#define EPD_CMD_DATA_ENTRY_MODE         0x11
#define EPD_CMD_SW_RESET                0x12
#define EPD_CMD_TEMP_SENSOR_CTRL        0x18
#define EPD_CMD_ACTIVATE_DISPLAY        0x20
#define EPD_CMD_DISPLAY_UPDATE_CTRL1    0x21
#define EPD_CMD_DISPLAY_UPDATE_CTRL2    0x22
#define EPD_CMD_WRITE_BW_RAM            0x24
#define EPD_CMD_WRITE_RED_RAM           0x26
#define EPD_CMD_WRITE_VCOM              0x2C
#define EPD_CMD_WRITE_LUT               0x32
#define EPD_CMD_BORDER_WAVEFORM         0x3C
#define EPD_CMD_SET_RAM_X               0x44
#define EPD_CMD_SET_RAM_Y               0x45
#define EPD_CMD_SET_RAM_X_COUNTER       0x4E
#define EPD_CMD_SET_RAM_Y_COUNTER       0x4F
#define EPD_CMD_NOP                     0x7F

/* -----------------------------------------------------------------------
 * 设备句柄
 * ----------------------------------------------------------------------- */

/**
 * @brief 墨水屏驱动上下文
 *
 * 说明：
 * - SPI句柄由驱动内部初始化
 * - 不依赖CubeMX生成代码结构
 */
typedef struct
{
    SPI_HandleTypeDef hspi;
} EPD_Ctx_t;

/* -----------------------------------------------------------------------
 * API接口
 * ----------------------------------------------------------------------- */

/**
 * @brief 初始化墨水屏
 *
 * 时序约束：
 * - 上电后必须延时≥10ms
 * - 必须执行硬件复位
 * - 必须完成初始化序列配置
 *
 * @param ctx 驱动上下文
 */
void EPD_Init(EPD_Ctx_t *ctx);

/**
 * @brief 全屏显示图像
 *
 * 说明：
 * - 数据必须为1bit黑白格式
 * - 数据大小：EPD_WIDTH/8 * EPD_HEIGHT
 *
 * @param ctx   驱动上下文
 * @param image  图像缓冲区
 */
void EPD_Display(EPD_Ctx_t *ctx, const uint8_t *image);

/**
 * @brief 清屏（全白）
 *
 * @param ctx 驱动上下文
 */
void EPD_Clear(EPD_Ctx_t *ctx);

/**
 * @brief 进入深度休眠
 *
 * 说明：
 * - 功耗最低模式
 * - RAM内容是否保留取决于指令参数
 *
 * @param ctx 驱动上下文
 */
void EPD_Sleep(EPD_Ctx_t *ctx);

/**
 * @brief 唤醒设备
 *
 * 说明：
 * - 必须硬件复位
 * - 必须重新初始化寄存器
 *
 * @param ctx 驱动上下文
 */
void EPD_Wakeup(EPD_Ctx_t *ctx);

#endif /* __EPD_H */