#include "Huidu.h"
#include "Motor.h"
#include "OLED.h"
#include "Delay.h"

uint16_t huidu_adc[5]; // 五路灰度ADC数组（全局定义）

// 5路传感器从左到右：第1~5路 → PA4~PA0（通道4~0）
const uint8_t adc_channel[SENSOR_NUM] = {
    ADC_Channel_4,
    ADC_Channel_3,
    ADC_Channel_2,
    ADC_Channel_1,
    ADC_Channel_0
};

// 偏移权重：左负右正，越外侧权重越大
const int16_t weight[SENSOR_NUM] = {
    -2, -1, 0, 1, 2
};

// ========== 岔路口状态机内部变量（文件内静态） ==========
static uint8_t  detect_state = 0;   // 0=空闲循迹 1=等待对侧触发 2=停车延时 3=右转找线 4=左转找线
static uint16_t detect_timer = 0;   // 计时（单位：函数调用次数，需根据主循环周期调整）
static uint8_t  first_side = 255;   // 首触发侧：0=最左 4=最右 255=无
static uint8_t  cross_enable = 1;   // 岔路口功能总开关，默认开启

// 转向启动延时：进入转向状态后，先转一段时间再检测黑线，避免原残留黑线误触发
#define TURN_START_DELAY  30

static void ADC_HardwareInit(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;
    ADC_InitTypeDef ADC_InitStruct;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1, ENABLE);

    GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStruct);

    ADC_InitStruct.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStruct.ADC_ScanConvMode = DISABLE;
    ADC_InitStruct.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStruct.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStruct.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStruct.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStruct);

    ADC_Cmd(ADC1, ENABLE);
    ADC_ResetCalibration(ADC1);
    while(ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while(ADC_GetCalibrationStatus(ADC1));
}

uint16_t ADC_ReadChannel(uint8_t ch)
{
    ADC_RegularChannelConfig(ADC1, ch, 1, ADC_SampleTime_239Cycles5);
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while(!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
    return ADC_GetConversionValue(ADC1);
}

void Huidu_Init(void)
{
    ADC_HardwareInit();
}

// 设置岔路口功能启用/禁用
void Huidu_SetCrossEnable(uint8_t en)
{
    cross_enable = en;
    if(en == 0)
    {
        detect_state = 0; // 禁用时重置状态机
        detect_timer = 0;
        first_side = 255;
    }
}

// 获取岔路口状态机当前状态
uint8_t Huidu_GetCrossState(void)
{
    return detect_state;
}

// 循迹主函数（需在主循环中周期性调用）
void Huidu_Track(void)
{
    uint8_t i;
    uint16_t adc_val[SENSOR_NUM];
    int32_t error = 0;
    uint8_t line_cnt = 0;
    int16_t speed_l, speed_r;
    uint8_t has_line = 0; // 任意传感器检测到黑线标志

    // 1. 扫描5路传感器
    for(i = 0; i < SENSOR_NUM; i++)
    {
        adc_val[i] = ADC_ReadChannel(adc_channel[i]);
    }
    // 同步到全局数组
    for(i = 0; i < SENSOR_NUM; i++)
    {
        huidu_adc[i] = adc_val[i];
    }

    // 2. 计算偏移量与黑线数量（黑线ADC更大，大于阈值=检测到黑线）
    for(i = 0; i < SENSOR_NUM; i++)
    {
        if(adc_val[i] > LINE_THRESHOLD)
        {
            error += weight[i];
            line_cnt++;
        }
    }
    has_line = (line_cnt > 0) ? 1 : 0;

    // ========== 3. 岔路口状态机 ==========
    if(cross_enable)
    {
        switch(detect_state)
        {
            // 状态0：空闲，检测单侧触发，进入等待状态
            case 0:
            {
                // 最左侧触发，最右侧未触发 → 记录首侧，开始计时等待
                if(adc_val[0] > LINE_THRESHOLD && adc_val[4] <= LINE_THRESHOLD)
                {
                    first_side = 0;
                    detect_state = 1;
                    detect_timer = 0;
                }
                // 最右侧触发，最左侧未触发 → 记录首侧，开始计时等待
                else if(adc_val[4] > LINE_THRESHOLD && adc_val[0] <= LINE_THRESHOLD)
                {
                    first_side = 4;
                    detect_state = 1;
                    detect_timer = 0;
                }
                // 左右同时触发 → 直接判定岔路口，进入停车
                else if(adc_val[0] > LINE_THRESHOLD && adc_val[4] > LINE_THRESHOLD)
                {
                    detect_state = 2;
                    detect_timer = 0;
                    first_side = 0; // 同时触发默认右转，可修改为4默认左转
                }
                break;
            }

            // 状态1：等待对侧传感器触发（此状态不打断正常循迹）
            case 1:
            {
                detect_timer++;
                // 对侧也触发了 → 判定为岔路口，进入停车
                if( (first_side == 0 && adc_val[4] > LINE_THRESHOLD) ||
                    (first_side == 4 && adc_val[0] > LINE_THRESHOLD) )
                {
                    detect_state = 2;
                    detect_timer = 0;
                    Motor_SetSpeed(0, 0);
                }
                // 超时未检测到对侧 → 重置，继续正常循迹
                else if(detect_timer > CROSS_CONFIRM_TIME)
                {
                    detect_state = 0;
                    first_side = 255;
                }
                // 注意：此状态不设置电机、不return，继续执行下方普通循迹
                break;
            }

            // 状态2：停车延时，停稳后再转向
            case 2:
            {
                Motor_SetSpeed(0, 0);
                detect_timer++;
                // 停车时间到 → 进入对应转向找线
                if(detect_timer > CROSS_STOP_TIME)
                {
                    if(first_side == 0)
                    {
                        detect_state = 3; // 左先触发→右转找线
                    }
                    else
                    {
                        detect_state = 4; // 右先触发→左转找线
                    }
                    detect_timer = 0;
                }
                break;
            }

            // 状态3：原地右转找线
            case 3:
            {
                Motor_SetSpeed(CROSS_RIGHT_SPEED, -CROSS_RIGHT_SPEED);
                detect_timer++;
                
                // 启动延时过后，再检测黑线，避免原残留黑线误触发
                if(detect_timer > TURN_START_DELAY)
                {
                    // 任意传感器检测到黑线 → 对准完成，回到空闲循迹
                    if(has_line)
                    {
                        Motor_SetSpeed(0, 0);
                        detect_state = 0;
                        first_side = 255;
                        detect_timer = 0;
                    }
                }
                
                // 右转超时，强制退出
                if(detect_timer > CROSS_RIGHT_TIME)
                {
                    detect_state = 0;
                    first_side = 255;
                }
                break;
            }

            // 状态4：原地左转找线
            case 4:
            {
                Motor_SetSpeed(-CROSS_LEFT_SPEED, CROSS_LEFT_SPEED);
                detect_timer++;
                
                // 启动延时过后，再检测黑线
                if(detect_timer > TURN_START_DELAY)
                {
                    // 任意传感器检测到黑线 → 对准完成，回到空闲循迹
                    if(has_line)
                    {
                        Motor_SetSpeed(0, 0);
                        detect_state = 0;
                        first_side = 255;
                        detect_timer = 0;
                    }
                }
                
                // 左转超时，强制退出
                if(detect_timer > CROSS_LEFT_TIME)
                {
                    detect_state = 0;
                    first_side = 255;
                }
                break;
            }

            default:
            {
                detect_state = 0;
                break;
            }
        }

        // 只有停车、转向状态（≥2）才跳过普通循迹；等待状态（1）继续执行循迹
        if(detect_state >= 2)
        {
            return;
        }
    }

    // ========== 4. 普通循迹逻辑（岔路口空闲/等待时执行） ==========
    if(line_cnt > 0)
    {
        // 死区：小偏移不修正
        if(error > -DEAD_ZONE && error < DEAD_ZONE)
        {
            Motor_SetSpeed(TRACK_BASE_SPEED, TRACK_BASE_SPEED);
            return;
        }
        // 比例差速计算
        speed_l = TRACK_BASE_SPEED + TRACK_KP * error;
        speed_r = TRACK_BASE_SPEED - TRACK_KP * error;

        // 限幅
        if(speed_l < 0) speed_l = 0;
        if(speed_l > MOTOR_MAX_SPEED) speed_l = MOTOR_MAX_SPEED;
        if(speed_r < 0) speed_r = 0;
        if(speed_r > MOTOR_MAX_SPEED) speed_r = MOTOR_MAX_SPEED;

        // 输出到电机（左电机机械差速补偿）
        Motor_SetSpeed(1.138f * speed_l, speed_r);
        return;
    }
    else
    {
        // 完全丢线且未触发岔路口，保持直行
        Motor_SetSpeed(TRACK_BASE_SPEED, TRACK_BASE_SPEED);
        return;
    }
}

// OLED显示5路ADC值，用于校准阈值
void Huidu_PrintADC(void)
{
    uint8_t i;
    uint16_t adc_val[SENSOR_NUM];
    
    for(i = 0; i < SENSOR_NUM; i++)
    {
        adc_val[i] = ADC_ReadChannel(adc_channel[i]);
    }
    
    OLED_Clear();
    Delay_ms(1);
    
    OLED_ShowString(1, 1, "L1:");
    OLED_ShowNum(1, 4, adc_val[0], 4);
    
    OLED_ShowString(2, 1, "L2:");
    OLED_ShowNum(2, 4, adc_val[1], 4);
    
    OLED_ShowString(3, 1, "L3:");
    OLED_ShowNum(3, 4, adc_val[2], 4);
    
    OLED_ShowString(4, 1, "L4:");
    OLED_ShowNum(4, 4, adc_val[3], 4);
    
    OLED_ShowString(1, 9, "L5:");
    OLED_ShowNum(1, 12, adc_val[4], 4);
}

// 获取中间路（第3路）灰度ADC值
uint16_t Huidu_GetMidADC(void)
{
    return huidu_adc[2];
}