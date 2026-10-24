#ifndef __MOTOR_H
#define __MOTOR_H

#include "stm32f10x.h"

// 工作模式
typedef enum
{
    MODE_MANUAL = 0,  // 手动
    MODE_AUTO   = 1,  // 自动循迹+颜色
    MODE_AVOID  = 2,  // 避障
    MODE_PUSHBOX= 3   // 推箱子任务
} Car_ModeTypeDef;

// 运动状态（手动模式用）
typedef enum
{
    MOTION_STOP = 0,
    MOTION_FORWARD,
    MOTION_BACKWARD,
    MOTION_LEFT,
    MOTION_RIGHT
} Motion_StateTypeDef;

extern Car_ModeTypeDef car_mode;
extern Motion_StateTypeDef motion_state;

void Motor_Init(void);
void Motor_SetSpeed(int16_t speed_left, int16_t speed_right); // 自动循迹用：直接设左右速度
void Motor_State_Update(void); // 手动模式用：状态机更新运动

#endif
