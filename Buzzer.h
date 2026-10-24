#ifndef __BUZZER_H
#define __BUZZER_H
#include "stm32f10x.h"

// 蜂鸣器：PB15引脚，低电平触发
void Buzzer_Init(void);   // 初始化引脚，默认关闭
void Buzzer_ON(void);     // 开启蜂鸣器
void Buzzer_OFF(void);    // 关闭蜂鸣器

#endif