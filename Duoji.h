#ifndef __DUOJI_H
#define __DUOJI_H

#include "stm32f10x.h"

/**
 * @brief  舵机初始化
 * @note   PB8引脚，TIM4通道3输出50Hz PWM
 */
void Duoji_Init(void);

/**
 * @brief  设置舵机角度
 * @param  angle 角度，范围0~180
 * @note   0°对应最左，90°对应正前方，180°对应最右
 */
void Duoji_SetAngle(uint8_t angle);

#endif