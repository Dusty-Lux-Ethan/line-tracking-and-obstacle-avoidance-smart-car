#ifndef __ECOH_H
#define __ECOH_H

#include "stm32f10x.h"

/**
 * @brief  超声波模块初始化
 * @note   Trig=PB12输出，Echo=PB13输入
 */
void Ecoh_Init(void);

/**
 * @brief  测量一次距离
 * @retval 距离值，单位：厘米；最大值约400cm
 * @note   阻塞式测量，一次约几十毫秒，避障足够用
 */
uint16_t Ecoh_GetDistance(void);

#endif