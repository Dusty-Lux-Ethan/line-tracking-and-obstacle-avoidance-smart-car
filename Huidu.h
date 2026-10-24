#ifndef __HUIDU_H
#define __HUIDU_H
#include "stm32f10x.h"

#define SENSOR_NUM      5

// ============== 循迹核心调试参数 ==============
#define LINE_THRESHOLD  2000    // 黑线阈值：ADC大于此值=检测到黑线（黑线ADC值更大）
#define TRACK_BASE_SPEED 25     // 直行基准速度（0~100）
#define TRACK_KP          10    // 转向灵敏度：越大转向越猛
#define DEAD_ZONE       1       // 死区：小偏移不修正
#define MOTOR_MAX_SPEED 100    // 电机最大速度限制

// ============== 岔路口检测参数 ==============
#define CROSS_CONFIRM_TIME   300     // 双侧触发确认窗口(ms)
#define CROSS_STOP_TIME      300     // 双侧触发后停车时间(ms)
#define CROSS_RIGHT_SPEED    20      // 右转找线转速
#define CROSS_RIGHT_TIME     800     // 右转最大找线时间(ms)
#define CROSS_LEFT_SPEED     20      // 左转找线转速
#define CROSS_LEFT_TIME      800     // 左转最大找线时间(ms)
#define TURN_START_DELAY     30      // 转向启动延时（新增）


// 函数声明
void Huidu_Init(void);
void Huidu_Track(void);
void Huidu_PrintADC(void);
uint16_t Huidu_GetMidADC(void);
void Huidu_SetCrossEnable(uint8_t en);
uint8_t Huidu_GetCrossState(void);

// 外部变量声明
extern uint16_t huidu_adc[5];
uint16_t ADC_ReadChannel(uint8_t ch);
extern const uint8_t adc_channel[SENSOR_NUM];
extern const int16_t weight[SENSOR_NUM];

#endif