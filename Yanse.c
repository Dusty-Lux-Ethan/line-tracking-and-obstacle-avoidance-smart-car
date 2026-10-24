#include "Yanse.h"
#include "Delay.h"

// ===================== 软件I2C引脚（对应你手写接线图） =====================
// SCL = PB6，SDA = PB7
#define SCL_H  GPIO_SetBits(GPIOB, GPIO_Pin_6)
#define SCL_L  GPIO_ResetBits(GPIOB, GPIO_Pin_6)
#define SDA_H  GPIO_SetBits(GPIOB, GPIO_Pin_7)
#define SDA_L  GPIO_ResetBits(GPIOB, GPIO_Pin_7)

// TCS34725 I2C地址
#define TCS_ADDR_WRITE  0x52
#define TCS_ADDR_READ   0x53

// 寄存器地址（带命令位0x80）
#define TCS_ENABLE      0x80
#define TCS_ATIME       0x81
#define TCS_CONTROL     0x8F
#define TCS_CDATAL      0x94
#define TCS_RDATAL      0x96
#define TCS_GDATAL      0x98
#define TCS_BDATAL      0x9A

// ===================== 底层软件I2C驱动 =====================
static void I2C_Delay(void)
{
    uint8_t i;
    for(i = 0; i < 12; i++);
}

static void I2C_Start(void)
{
    SDA_H;
    SCL_H;
    I2C_Delay();
    SDA_L;
    I2C_Delay();
    SCL_L;
}

static void I2C_Stop(void)
{
    SDA_L;
    SCL_H;
    I2C_Delay();
    SDA_H;
    I2C_Delay();
}

static uint8_t I2C_SendByte(uint8_t data)
{
    uint8_t i;
    for(i = 0; i < 8; i++)
    {
        if(data & 0x80) SDA_H;
        else SDA_L;
        data <<= 1;
        I2C_Delay();
        SCL_H;
        I2C_Delay();
        SCL_L;
    }
    SDA_H;
    I2C_Delay();
    SCL_H;
    I2C_Delay();
    uint8_t ack = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_7);
    SCL_L;
    return ack;
}

static uint8_t I2C_ReadByte(uint8_t ack)
{
    uint8_t i, data = 0;
    SDA_H;
    for(i = 0; i < 8; i++)
    {
        data <<= 1;
        SCL_H;
        I2C_Delay();
        if(GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_7)) data |= 0x01;
        SCL_L;
        I2C_Delay();
    }
    if(ack) SDA_L;
    else SDA_H;
    I2C_Delay();
    SCL_H;
    I2C_Delay();
    SCL_L;
    return data;
}

// ===================== 寄存器操作 =====================
static void TCS_WriteReg(uint8_t reg, uint8_t data)
{
    I2C_Start();
    I2C_SendByte(TCS_ADDR_WRITE);
    I2C_SendByte(reg);
    I2C_SendByte(data);
    I2C_Stop();
}

static uint16_t TCS_ReadReg16(uint8_t reg)
{
    uint16_t data;
    I2C_Start();
    I2C_SendByte(TCS_ADDR_WRITE);
    I2C_SendByte(reg);
    I2C_Start();
    I2C_SendByte(TCS_ADDR_READ);
    data = I2C_ReadByte(1);
    data |= (uint16_t)I2C_ReadByte(0) << 8;
    I2C_Stop();
    return data;
}

// ===================== 对外接口函数 =====================
void Yanse_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    // PB6(SCL)、PB7(SDA) 开漏输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    SCL_H;
    SDA_H;
    Delay_ms(10);

    // 积分时间100ms
    TCS_WriteReg(TCS_ATIME, 0xD5);

    // 设置增益
    uint8_t gain_val = 0x00;
    #if TCS_GAIN == 4
        gain_val = 0x01;
    #elif TCS_GAIN == 16
        gain_val = 0x02;
    #elif TCS_GAIN == 60
        gain_val = 0x03;
    #endif
    TCS_WriteReg(TCS_CONTROL, gain_val);

    // 上电+开启颜色转换
    TCS_WriteReg(TCS_ENABLE, 0x01);
    Delay_ms(3);
    TCS_WriteReg(TCS_ENABLE, 0x03);
    Delay_ms(100);
}

void Yanse_ReadRGB(uint16_t *r, uint16_t *g, uint16_t *b, uint16_t *c)
{
    *c = TCS_ReadReg16(TCS_CDATAL);
    *r = TCS_ReadReg16(TCS_RDATAL);
    *g = TCS_ReadReg16(TCS_GDATAL);
    *b = TCS_ReadReg16(TCS_BDATAL);
}

uint8_t Yanse_IsRed(void)
{
    uint16_t r, g, b, c;
    Yanse_ReadRGB(&r, &g, &b, &c);
    if(r > RED_THRESHOLD && r > g * RED_RATIO && r > b * RED_RATIO)
        return 1;
    return 0;
}

uint8_t Yanse_IsGreen(void)
{
    uint16_t r, g, b, c;
    Yanse_ReadRGB(&r, &g, &b, &c);
    if(g > GREEN_THRESHOLD && g > r * GREEN_RATIO && g > b * GREEN_RATIO)
        return 1;
    return 0;
}

uint8_t Yanse_IsBlue(void)
{
    uint16_t r, g, b, c;
    Yanse_ReadRGB(&r, &g, &b, &c);
    if(b > BLUE_THRESHOLD && b > r * BLUE_RATIO && b > g * BLUE_RATIO)
        return 1;
    return 0;
}

// 获取颜色类型：0=无，1=红，2=绿，3=蓝
uint8_t Yanse_GetColor(void)
{
    if(Yanse_IsRed()) return 1;
    if(Yanse_IsGreen()) return 2;
    if(Yanse_IsBlue()) return 3;
    return 0;
}