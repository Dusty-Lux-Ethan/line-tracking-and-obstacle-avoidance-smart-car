#include "Buzzer.h"

// 硬件定义：蜂鸣器接PB15，低电平触发
#define BUZZER_PORT  GPIOB
#define BUZZER_PIN   GPIO_Pin_15

void Buzzer_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    // 开启GPIOB端口时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    
    // 配置PB15为推挽输出，50MHz速率
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Pin = BUZZER_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BUZZER_PORT, &GPIO_InitStructure);
    
    // 默认输出高电平，蜂鸣器关闭（低电平触发）
    GPIO_SetBits(BUZZER_PORT, BUZZER_PIN);
}

void Buzzer_ON(void)
{
    // 输出低电平，启动蜂鸣器
    GPIO_ResetBits(BUZZER_PORT, BUZZER_PIN);
}

void Buzzer_OFF(void)
{
    // 输出高电平，关闭蜂鸣器
    GPIO_SetBits(BUZZER_PORT, BUZZER_PIN);
}





