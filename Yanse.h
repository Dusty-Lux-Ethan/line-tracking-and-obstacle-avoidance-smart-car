#ifndef __YANSE_H
#define __YANSE_H

#include "stm32f10x.h"

// ========== 颜色识别核心调试参数（现场校准改这里） ==========
// 红色判定
#define RED_THRESHOLD    500    // R通道阈值：大于此值判定红色分量足够
#define RED_RATIO        1.4f    // 红/绿、红/蓝最小比值：抗白光干扰

// 绿色判定
#define GREEN_THRESHOLD  500    // G通道阈值
#define GREEN_RATIO      1.4f    // 绿/红、绿/蓝最小比值

// 蓝色判定
#define BLUE_THRESHOLD   500    // B通道阈值
#define BLUE_RATIO       1.3f    // 蓝/红、蓝/绿最小比值

// 传感器配置
#define TCS_GAIN         4       // 增益：1/4/16/60，光线暗调大

// 函数声明
void Yanse_Init(void);              // TCS34725初始化
void Yanse_ReadRGB(uint16_t *r, uint16_t *g, uint16_t *b, uint16_t *c); // 读原始值
uint8_t Yanse_IsRed(void);          // 判断红色
uint8_t Yanse_IsGreen(void);        // 判断绿色
uint8_t Yanse_IsBlue(void);         // 判断蓝色
uint8_t Yanse_GetColor(void);       // 获取颜色类型：0=无，1=红，2=绿，3=蓝

#endif
