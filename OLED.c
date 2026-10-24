#include "stm32f10x.h"
#include "OLED_Font.h"

/* ===================== 引脚配置（更新为PB3=SCL，PB4=SDA） ===================== */
#define OLED_W_SCL(x)		GPIO_WriteBit(GPIOB, GPIO_Pin_3, (BitAction)(x))
#define OLED_W_SDA(x)		GPIO_WriteBit(GPIOB, GPIO_Pin_4, (BitAction)(x))

/**
 * @brief  I2C引脚初始化
 * @note   PB3-SCL时钟线，PB4-SDA数据线，开漏输出
 */
void OLED_I2C_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStructure;
 	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD; // 开漏输出，I2C标准要求
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
 	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
 	GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	// 总线空闲状态：两根线都拉高
	OLED_W_SCL(1);
	OLED_W_SDA(1);
}

/**
 * @brief  I2C起始信号
 * @note   时序：SCL高电平时，SDA从高变低
 */
void OLED_I2C_Start(void)
{
	OLED_W_SDA(1);
	OLED_W_SCL(1);
	OLED_W_SDA(0);
	OLED_W_SCL(0);
}

/**
 * @brief  I2C停止信号
 * @note   时序：SCL高电平时，SDA从低变高
 */
void OLED_I2C_Stop(void)
{
	OLED_W_SDA(0);
	OLED_W_SCL(1);
	OLED_W_SDA(1);
}

/**
 * @brief  I2C发送一个字节
 * @param  Byte 要发送的8位数据
 * @note   高位先发，发完跳过应答
 */
void OLED_I2C_SendByte(uint8_t Byte)
{
	uint8_t i;
	for (i = 0; i < 8; i++)
	{
		OLED_W_SDA(!!(Byte & (0x80 >> i)));
		OLED_W_SCL(1);
		OLED_W_SCL(0);
	}
	OLED_W_SCL(1);
	OLED_W_SCL(0);
}

/**
 * @brief  OLED写命令
 * @param  Command 命令值
 * @note   0x00表示后续是命令
 */
void OLED_WriteCommand(uint8_t Command)
{
	OLED_I2C_Start();
	OLED_I2C_SendByte(0x78);		// OLED设备地址（写）
	OLED_I2C_SendByte(0x00);		// 命令标志
	OLED_I2C_SendByte(Command); 
	OLED_I2C_Stop();
}

/**
 * @brief  OLED写数据
 * @param  Data 显存数据
 * @note   0x40表示后续是数据
 */
void OLED_WriteData(uint8_t Data)
{
	OLED_I2C_Start();
	OLED_I2C_SendByte(0x78);
	OLED_I2C_SendByte(0x40);		// 数据标志
	OLED_I2C_SendByte(Data);
	OLED_I2C_Stop();
}

/**
 * @brief  设置光标位置
 * @param  Y 页地址 0~7
 * @param  X 列地址 0~127
 */
void OLED_SetCursor(uint8_t Y, uint8_t X)
{
	OLED_WriteCommand(0xB0 | Y);					    // 设置页（Y坐标）
	OLED_WriteCommand(0x10 | ((X & 0xF0) >> 4));	// 列地址高4位
	OLED_WriteCommand(0x00 | (X & 0x0F));			// 列地址低4位
}

/**
 * @brief  OLED清屏
 * @note   8页全部写0，屏幕全黑
 */
void OLED_Clear(void)
{  
	uint8_t i, j;
	for (j = 0; j < 8; j++)
	{
		OLED_SetCursor(j, 0);
		for(i = 0; i < 128; i++)
		{
			OLED_WriteData(0x00);
		}
	}
}

/**
 * @brief  显示单个8x16字符
 * @param  Line   行号 1~4
 * @param  Column 列号 1~16
 * @param  Char   字符
 * @note   16像素高的字符分上下两页显示
 */
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char)
{      	
	uint8_t i;
	// 上半页
	OLED_SetCursor((Line - 1) * 2, (Column - 1) * 8);
	for (i = 0; i < 8; i++)
	{
		OLED_WriteData(OLED_F8x16[Char - ' '][i]);
	}
	// 下半页
	OLED_SetCursor((Line - 1) * 2 + 1, (Column - 1) * 8);
	for (i = 0; i < 8; i++)
	{
		OLED_WriteData(OLED_F8x16[Char - ' '][i + 8]);
	}
}

/**
 * @brief  显示字符串
 * @param  Line   起始行 1~4
 * @param  Column 起始列 1~16
 * @param  String 字符串
 */
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String)
{
	uint8_t i;
	for (i = 0; String[i] != '\0'; i++)
	{
		OLED_ShowChar(Line, Column + i, String[i]);
	}
}

/**
 * @brief  次方辅助函数（显示数字用）
 */
uint32_t OLED_Pow(uint32_t X, uint32_t Y)
{
	uint32_t Result = 1;
	while (Y--)
	{
		Result *= X;
	}
	return Result;
}

/**
 * @brief  显示十进制正整数
 * @param  Line   行
 * @param  Column 列
 * @param  Number 数字
 * @param  Length 位数
 */
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i;
	for (i = 0; i < Length; i++)							
	{
		OLED_ShowChar(Line, Column + i, Number / OLED_Pow(10, Length - i - 1) % 10 + '0');
	}
}

// 带符号数字、十六进制、二进制函数原理一致
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length)
{
	uint8_t i;
	uint32_t Number1;
	if (Number >= 0)
	{
		OLED_ShowChar(Line, Column, '+');
		Number1 = Number;
	}
	else
	{
		OLED_ShowChar(Line, Column, '-');
		Number1 = -Number;
	}
	for (i = 0; i < Length; i++)							
	{
		OLED_ShowChar(Line, Column + i + 1, Number1 / OLED_Pow(10, Length - i - 1) % 10 + '0');
	}
}

void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i, SingleNumber;
	for (i = 0; i < Length; i++)							
	{
		SingleNumber = Number / OLED_Pow(16, Length - i - 1) % 16;
		if (SingleNumber < 10)
		{
			OLED_ShowChar(Line, Column + i, SingleNumber + '0');
		}
		else
		{
			OLED_ShowChar(Line, Column + i, SingleNumber - 10 + 'A');
		}
	}
}

void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length)
{
	uint8_t i;
	for (i = 0; i < Length; i++)							
	{
		OLED_ShowChar(Line, Column + i, Number / OLED_Pow(2, Length - i - 1) % 2 + '0');
	}
}

/**
 * @brief  OLED初始化
 * @note   固定初始化命令序列
 */
void OLED_Init(void)
{
	uint32_t i, j;
	
	// 上电延时，等待稳定
	for (i = 0; i < 1000; i++)			
	{
		for (j = 0; j < 1000; j++);
	}
	
	OLED_I2C_Init();			
	
	OLED_WriteCommand(0xAE);	// 关闭显示
	OLED_WriteCommand(0xD5);	// 时钟分频
	OLED_WriteCommand(0x80);
	OLED_WriteCommand(0xA8);	// 多路复用率
	OLED_WriteCommand(0x3F);
	OLED_WriteCommand(0xD3);	// 偏移
	OLED_WriteCommand(0x00);
	OLED_WriteCommand(0x40);	// 起始行
	OLED_WriteCommand(0xA1);	// 左右方向正常
	OLED_WriteCommand(0xC8);	// 上下方向正常
	OLED_WriteCommand(0xDA);	// COM引脚配置
	OLED_WriteCommand(0x12);
	OLED_WriteCommand(0x81);	// 对比度
	OLED_WriteCommand(0xCF);
	OLED_WriteCommand(0xD9);	// 预充电
	OLED_WriteCommand(0xF1);
	OLED_WriteCommand(0xDB);	// VCOM电压
	OLED_WriteCommand(0x30);
	OLED_WriteCommand(0xA4);	// 全局显示开启
	OLED_WriteCommand(0xA6);	// 正常显示
	OLED_WriteCommand(0x8D);	// 电荷泵
	OLED_WriteCommand(0x14);
	OLED_WriteCommand(0xAF);	// 开启显示
		
	OLED_Clear();
}