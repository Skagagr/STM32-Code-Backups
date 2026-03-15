/**
 * @file    oled.h
 * @brief   SSD1315 OLED 驱动（128×64，I2C 接口）
 *
 * @details
 *  基于 I2C_Bus 抽象层驱动 SSD1315，提供：
 *    - 全屏刷新（GRAM 双缓冲，绘图后统一上屏）
 *    - ASCII 8×16 字符 / 字符串显示
 *    - 中文 16×16 汉字显示（UTF-8 编码，字模由 PCtoLCD2002 提取）
 *    - 整数 / 有符号整数 / 浮点数显示
 *    - 画点、水平线、垂直线、矩形边框、填充矩形
 *    - 亮度调节 / 开关显示
 *
 *  【坐标系】
 *    x : 列方向，0（左）~ 127（右）
 *    y : 行方向，0（上）~ 63（下）
 *    所有绘图函数操作内部 GRAM 缓冲区，调用 OLED_Refresh() 后才上屏。
 *
 *  典型使用流程：
 *  @code
 *      // 1. 声明句柄
 *      I2C_Bus_Ctx i2c1_ctx;
 *      OLED_Ctx    oled_ctx;
 *
 *      // 2. 初始化（CubeMX 外设初始化之后）
 *      I2C_Bus_Init(&i2c1_ctx, &hi2c1);
 *      OLED_Init(&oled_ctx, &i2c1_ctx);
 *
 *      // 3. 绘制内容
 *      OLED_Clear(&oled_ctx);
 *      OLED_ShowString(&oled_ctx, 0, 0, "Hello World!");
 *      OLED_ShowChinese(&oled_ctx, 0, 16, "你好世界");
 *      OLED_Refresh(&oled_ctx);   // ← 必须调用才会实际上屏
 *  @endcode
 *
 * @version 1.2.0
 * @date    2026-03-15
 */

#ifndef __OLED_H
#define __OLED_H

#include "main.h"
#include "i2c_bus.h"
#include <stdint.h>

/* ── 配置区 —— 按工程需求修改 ── */
#define OLED_I2C_ADDR   0x78U              /**< SSD1315 I2C 地址（8位格式）
                                                SA0=GND → 0x78，SA0=VCC → 0x7A */
#define OLED_WIDTH      128U               /**< 屏幕像素宽度 */
#define OLED_HEIGHT     64U                /**< 屏幕像素高度 */
#define OLED_PAGES      (OLED_HEIGHT / 8U) /**< 页数 = 8 */

/* ── 类型定义 ── */

/**
 * @brief OLED 模块上下文句柄
 */
typedef struct
{
    I2C_Bus_Ctx *bus;                          /**< I2C 总线句柄指针 */
    uint8_t      gram[OLED_PAGES][OLED_WIDTH]; /**< 显存缓冲区，按页×列组织 */
    uint8_t      inited;                       /**< 初始化标志：1=已初始化 */
} OLED_Ctx;

/* ── 公开 API ── */

/**
 * @brief  初始化 OLED 屏幕
 *
 * @details
 *  发送 SSD1315 初始化命令序列，清空 GRAM 并刷新屏幕。
 *  必须在 I2C_Bus_Init() 之后调用。
 *
 * @param  ctx  OLED 上下文指针
 * @param  bus  已初始化的 I2C 总线上下文指针
 * @retval 1    成功
 * @retval 0    失败或参数非法
 */
uint8_t OLED_Init(OLED_Ctx *ctx, I2C_Bus_Ctx *bus);

/**
 * @brief  将 GRAM 缓冲区内容刷新到屏幕
 *
 * @details
 *  遍历 8 个页，逐页逐字节写入 SSD1315 DDRAM。
 *  所有绘图操作完成后必须调用此函数才会实际显示。
 *
 * @param  ctx  OLED 上下文指针
 * @retval 1    成功
 * @retval 0    失败或未初始化
 */
uint8_t OLED_Refresh(OLED_Ctx *ctx);

/**
 * @brief  清空 GRAM 缓冲区（不立即刷屏）
 *
 * @details
 *  将内部 GRAM 全部置 0，需配合 OLED_Refresh() 才会清空屏幕显示。
 *
 * @param  ctx  OLED 上下文指针
 * @retval 1    成功
 * @retval 0    参数非法
 */
uint8_t OLED_Clear(OLED_Ctx *ctx);

/**
 * @brief  设置屏幕亮度（对比度）
 *
 * @param  ctx       OLED 上下文指针
 * @param  contrast  亮度值，0x00（最暗）~ 0xFF（最亮），默认 0xCF
 * @retval 1         成功
 * @retval 0         失败
 */
uint8_t OLED_SetContrast(OLED_Ctx *ctx, uint8_t contrast);

/**
 * @brief  开启 / 关闭显示
 *
 * @param  ctx     OLED 上下文指针
 * @param  enable  1=开显示，0=关显示（不断电，保留 GRAM 内容）
 * @retval 1       成功
 * @retval 0       失败
 */
uint8_t OLED_SetDisplay(OLED_Ctx *ctx, uint8_t enable);

/**
 * @brief  在指定坐标画一个像素点
 *
 * @param  ctx    OLED 上下文指针
 * @param  x      列坐标，0~127
 * @param  y      行坐标，0~63
 * @param  color  1=点亮，0=熄灭
 * @retval 1      成功
 * @retval 0      参数越界或非法
 */
uint8_t OLED_DrawPixel(OLED_Ctx *ctx, uint8_t x, uint8_t y, uint8_t color);

/**
 * @brief  画水平线
 *
 * @param  ctx    OLED 上下文指针
 * @param  x      起始列，0~127
 * @param  y      行坐标，0~63
 * @param  len    线长（像素数）
 * @param  color  1=点亮，0=熄灭
 * @retval 1      成功
 * @retval 0      参数非法
 */
uint8_t OLED_DrawHLine(OLED_Ctx *ctx, uint8_t x, uint8_t y, uint8_t len, uint8_t color);

/**
 * @brief  画垂直线
 *
 * @param  ctx    OLED 上下文指针
 * @param  x      列坐标，0~127
 * @param  y      起始行，0~63
 * @param  len    线长（像素数）
 * @param  color  1=点亮，0=熄灭
 * @retval 1      成功
 * @retval 0      参数非法
 */
uint8_t OLED_DrawVLine(OLED_Ctx *ctx, uint8_t x, uint8_t y, uint8_t len, uint8_t color);

/**
 * @brief  画矩形边框
 *
 * @param  ctx    OLED 上下文指针
 * @param  x      左上角列，0~127
 * @param  y      左上角行，0~63
 * @param  w      宽度（像素）
 * @param  h      高度（像素）
 * @param  color  1=点亮，0=熄灭
 * @retval 1      成功
 * @retval 0      参数非法
 */
uint8_t OLED_DrawRect(OLED_Ctx *ctx, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);

/**
 * @brief  画填充矩形
 *
 * @param  ctx    OLED 上下文指针
 * @param  x      左上角列，0~127
 * @param  y      左上角行，0~63
 * @param  w      宽度（像素）
 * @param  h      高度（像素）
 * @param  color  1=点亮，0=熄灭
 * @retval 1      成功
 * @retval 0      参数非法
 */
uint8_t OLED_FillRect(OLED_Ctx *ctx, uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t color);

/**
 * @brief  显示单个 ASCII 字符（8×16）
 *
 * @param  ctx   OLED 上下文指针
 * @param  x     列坐标，0~127
 * @param  y     行坐标，0~63，建议为 8 的倍数
 * @param  ch    ASCII 字符（0x20 ~ 0x7E）
 * @retval 1     成功
 * @retval 0     参数非法或字符超范围
 */
uint8_t OLED_ShowChar(OLED_Ctx *ctx, uint8_t x, uint8_t y, char ch);

/**
 * @brief  显示 ASCII 字符串（8×16）
 *
 * @details
 *  每字符宽 8 像素，超出屏幕宽度自动截断。
 *
 * @param  ctx   OLED 上下文指针
 * @param  x     起始列坐标
 * @param  y     起始行坐标
 * @param  str   以 '\0' 结尾的字符串指针
 * @retval 1     成功
 * @retval 0     参数非法
 */
uint8_t OLED_ShowString(OLED_Ctx *ctx, uint8_t x, uint8_t y, const char *str);

/**
 * @brief  显示无符号整数
 *
 * @param  ctx    OLED 上下文指针
 * @param  x      起始列坐标
 * @param  y      起始行坐标
 * @param  num    待显示的无符号整数
 * @param  width  显示宽度（位数），0 表示自动宽度
 * @retval 1      成功
 * @retval 0      参数非法
 */
uint8_t OLED_ShowNum(OLED_Ctx *ctx, uint8_t x, uint8_t y, uint32_t num, uint8_t width);

/**
 * @brief  显示有符号整数
 *
 * @param  ctx    OLED 上下文指针
 * @param  x      起始列坐标
 * @param  y      起始行坐标
 * @param  num    待显示的有符号整数
 * @param  width  显示宽度（含符号位），0 表示自动宽度
 * @retval 1      成功
 * @retval 0      参数非法
 */
uint8_t OLED_ShowInt(OLED_Ctx *ctx, uint8_t x, uint8_t y, int32_t num, uint8_t width);

/**
 * @brief  显示浮点数
 *
 * @details
 *  依赖 printf 浮点支持，CMakeLists.txt 需加：
 *  target_link_options(${PROJECT_NAME}.elf PRIVATE -u _printf_float)
 *
 * @param  ctx      OLED 上下文指针
 * @param  x        起始列坐标
 * @param  y        起始行坐标
 * @param  num      待显示的浮点数
 * @param  decimal  小数位数，0~4
 * @retval 1        成功
 * @retval 0        参数非法
 */
uint8_t OLED_ShowFloat(OLED_Ctx *ctx, uint8_t x, uint8_t y, float num, uint8_t decimal);

/**
 * @brief  显示中文字符串（UTF-8 编码，16×16）
 *
 * @details
 *  在 font_chinese.h 的 g_chinese_font[] 中查表绘制。
 *  未收录的汉字跳过（留空白），不影响后续字符显示。
 *  源文件需保存为 UTF-8 编码（VSCode 默认）。
 *
 * @param  ctx   OLED 上下文指针
 * @param  x     起始列坐标（每字宽 16，建议 0/16/32...）
 * @param  y     起始行坐标（每字高 16，建议 0/16/32/48）
 * @param  str   UTF-8 编码的中文字符串指针
 * @retval 1     成功
 * @retval 0     参数非法
 */
uint8_t OLED_ShowChinese(OLED_Ctx *ctx, uint8_t x, uint8_t y, const char *str);

#endif /* __OLED_H */