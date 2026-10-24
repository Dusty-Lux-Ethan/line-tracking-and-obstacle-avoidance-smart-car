#include "Duoji.h"

/**
 * @brief  舵机初始化
 * @note   TIM4-CH3(PB8)输出50Hz PWM，周期20ms
 *         计数频率1MHz，1个计数=1微秒
 */
void Duoji_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    // 1. 开时钟：GPIOB和TIM4
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);

    // 2. 配置PB8为复用推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // 3. 配置定时器时基：1MHz计数频率，周期2000=20ms（50Hz）
    TIM_TimeBaseStructure.TIM_Period = 20000 - 1;
    TIM_TimeBaseStructure.TIM_Prescaler = 72 - 1;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);

    // 4. 配置PWM输出模式
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 1500; // 默认65°归中
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC3Init(TIM4, &TIM_OCInitStructure);

    // 使能预装载
    TIM_OC3PreloadConfig(TIM4, TIM_OCPreload_Enable);

    // 5. 开启定时器
    TIM_Cmd(TIM4, ENABLE);
}

/**
 * @brief  设置舵机角度
 * @param  angle 0~130度（适配舵机最大行程）
 * @note   0.5ms(500)=0°，2.5ms(2500)=180°
 *         公式：高电平时间 = 500 + angle * 2000 / 180
 */
void Duoji_SetAngle(uint8_t angle)
{
    uint16_t pulse;

   

    // 角度转PWM高电平时间（单位：微秒）
    pulse = 500 + (uint32_t)angle * 2000 / 180;

    // 设置比较值，改变PWM占空比
    TIM_SetCompare3(TIM4, pulse);
}