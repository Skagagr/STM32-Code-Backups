#include "sht3x.h"
#include "i2c_bus.h"
#include "stm32f1xx_hal.h"

uint8_t SHT30_Init(SHT30_Ctx *ctx, I2C_Bus_Ctx *bus)
{
    if (ctx == NULL || bus == NULL) return 0;

    ctx->bus = bus;

    return 1;
}

// 第一步：发送测量命令
static uint8_t SHT30_StartMeasure(SHT30_Ctx *ctx)
{
    uint8_t buf[2] = {0x2c, 0x06};
    return I2C_Bus_Write(ctx->bus, SHT30_I2C_ADDR, buf, 2);
}

// 第二步：读取6字节原始数据
static uint8_t SHT30_ReadRaw(SHT30_Ctx *ctx)
{
    return I2C_Bus_Read(ctx->bus, SHT30_I2C_ADDR, ctx->buf, 6);
}

uint8_t SHT30_Read(SHT30_Ctx *ctx, float *temp, float *humi)
{
                    // 未进行buf[2]和buf[5]CRC校验
    float SRH, ST;
    // 1. 发测量命令
    if (!SHT30_StartMeasure(ctx))
        return 0;
    // 2. 延时等待采集完成（高重复精度约需15ms）
    HAL_Delay(15);
    // 3. 读6字节
    if (!SHT30_ReadRaw(ctx))
        return 0;
    // 4. 换算温度
    ST = (ctx->buf[0] << 8) | ctx->buf[1];
    *temp = -45 + 175 * (float)ST / 65535;
    // 5. 换算湿度
    SRH = (ctx->buf[3] << 8) | ctx->buf[4];
    *humi = 100 * (float)SRH / 65535;
    // 6. 返回结果
    return 1;
}