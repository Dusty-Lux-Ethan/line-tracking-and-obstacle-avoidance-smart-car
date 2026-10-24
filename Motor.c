#include "Motor.h"

Car_ModeTypeDef car_mode = MODE_MANUAL;
Motion_StateTypeDef motion_state = MOTION_STOP;

#define PWM_MAX  999  // PWM满量程

void Motor_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    // 方向引脚 PA8/9/10/11
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // STBY使能引脚 PA15
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_SetBits(GPIOA, GPIO_Pin_15); // 拉高使能驱动板

    // PWM引脚 PA6/PA7 复用推挽
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // TIM3时基：1kHz计数，1000级分辨率
    TIM_TimeBaseStructure.TIM_Period = PWM_MAX;
    TIM_TimeBaseStructure.TIM_Prescaler = 71;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);

    // PWM通道配置
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 0;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;

    TIM_OC1Init(TIM3, &TIM_OCInitStructure); // PA6 左电机
    TIM_OC2Init(TIM3, &TIM_OCInitStructure); // PA7 右电机

    TIM_OC1PreloadConfig(TIM3, TIM_OCPreload_Enable);
    TIM_OC2PreloadConfig(TIM3, TIM_OCPreload_Enable);
    TIM_Cmd(TIM3, ENABLE);

    // 上电默认停止
    GPIO_ResetBits(GPIOA, GPIO_Pin_8 | GPIO_Pin_9 | GPIO_Pin_10 | GPIO_Pin_11);
    TIM_SetCompare1(TIM3, 0);
    TIM_SetCompare2(TIM3, 0);
}

// 直接设置左右电机速度（范围：-100~100，负为反转，正为正转）
void Motor_SetSpeed(int16_t speed_left, int16_t speed_right)
{
    // 左电机方向
    if(speed_left >= 0)
    {
        GPIO_SetBits(GPIOA, GPIO_Pin_8);
        GPIO_ResetBits(GPIOA, GPIO_Pin_9);
    }
    else
    {
        GPIO_ResetBits(GPIOA, GPIO_Pin_8);
        GPIO_SetBits(GPIOA, GPIO_Pin_9);
        speed_left = -speed_left;
    }

    // 右电机方向
    if(speed_right >= 0)
    {
        GPIO_SetBits(GPIOA, GPIO_Pin_10);
        GPIO_ResetBits(GPIOA, GPIO_Pin_11);
    }
    else
    {
        GPIO_ResetBits(GPIOA, GPIO_Pin_10);
        GPIO_SetBits(GPIOA, GPIO_Pin_11);
        speed_right = -speed_right;
    }

    // 限幅
    if(speed_left > 100) speed_left = 100;
    if(speed_right > 100) speed_right = 100;

    // 转换为PWM值输出
    TIM_SetCompare1(TIM3, speed_left * PWM_MAX / 100);
    TIM_SetCompare2(TIM3, speed_right * PWM_MAX / 100);
}

// 手动模式状态机更新
void Motor_State_Update(void)
{
    switch(motion_state)
    {
        case MOTION_STOP:
            Motor_SetSpeed(0, 0);
            break;

        case MOTION_FORWARD:
            Motor_SetSpeed(1.073*30, 30); // 手动默认直行速度
            break;

        case MOTION_BACKWARD:
            Motor_SetSpeed(-30, -30);
            break;

        case MOTION_LEFT:
            Motor_SetSpeed(-20, 20); // 原地左转
            break;

        case MOTION_RIGHT:
            Motor_SetSpeed(20, -20); // 原地右转
            break;

        default:
            Motor_SetSpeed(0, 0);
            break;
    }
}