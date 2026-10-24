#include "Ecoh.h"
#include "Delay.h"
#include "core_cm3.h"

#define ECHO_TIMEOUT_US     30000UL  // 单次测距超时30ms，对应约5米
#define MIN_INTERVAL_MS    30       // 两次测距最小间隔（改小更快，最低不建议低于15）
#define TIMEOUT_RESET_CNT  3        // 连续3次超时强制复位模块
#define SAMPLE_CNT         2        // 采样次数，越多越准但越慢，建议1~3

static uint32_t last_measure_ms = 0;
static uint8_t  timeout_cnt = 0;
static uint16_t last_dist = 600;

// 高精度微秒计时（修正整数误差）
static uint32_t get_us(void)
{
    uint32_t ms = Delay_GetTick();
    uint32_t tick = SysTick->VAL;
    // 72000 tick = 1ms = 1000us，按比例计算，消除除法截断误差
    return ms * 1000UL + (72000UL - tick) * 1000UL / 72000UL;
}

void Ecoh_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    // Trig PB12 推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // Echo PB13 上拉输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_ResetBits(GPIOB, GPIO_Pin_12);
    last_measure_ms = 0;
    timeout_cnt = 0;
    last_dist = 600;
}

// 单次测距底层函数
static uint16_t measure_once(void)
{
    uint32_t start_us;
    uint32_t time_us;
    uint16_t distance;

    // 20us宽触发脉冲，兼容所有模块
    GPIO_SetBits(GPIOB, GPIO_Pin_12);
    Delay_us(20);
    GPIO_ResetBits(GPIOB, GPIO_Pin_12);

    // 等待上升沿
    start_us = get_us();
    while(GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_13) == Bit_RESET)
    {
        if(get_us() - start_us > ECHO_TIMEOUT_US)
        {
            return 600;
        }
    }
    start_us = get_us();

    // 等待下降沿
    while(GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_13) == Bit_SET)
    {
        if(get_us() - start_us > ECHO_TIMEOUT_US)
        {
            return 600;
        }
    }

    // 声速343m/s（20℃常温），换算0.0343cm/μs，除以2往返
    time_us = get_us() - start_us;
    distance = (uint16_t)(time_us * 0.0343f / 2.0f);
    
    return distance;
}

uint16_t Ecoh_GetDistance(void)
{
    uint32_t now_ms = Delay_GetTick();
    uint32_t sum = 0;
    uint8_t valid_cnt = 0;
    uint8_t i;

    // 最小间隔限制，不到时间直接返回上次值
    if(now_ms - last_measure_ms < MIN_INTERVAL_MS)
    {
        return last_dist;
    }
    last_measure_ms = now_ms;

    // 连续超时达到阈值，强制复位模块
    if(timeout_cnt >= TIMEOUT_RESET_CNT)
    {
        GPIO_ResetBits(GPIOB, GPIO_Pin_12);
        Delay_ms(10);
        timeout_cnt = 0;
    }

    // 多次采样取平均，提升精度
    for(i = 0; i < SAMPLE_CNT; i++)
    {
        uint16_t d = measure_once();
        if(d != 600)
        {
            sum += d;
            valid_cnt++;
        }
        Delay_us(100); // 两次采样间小幅延时，避免余波干扰
    }

    // 全部超时才算超时
    if(valid_cnt == 0)
    {
        timeout_cnt++;
        last_dist = 600;
        return 600;
    }

    // 计算平均值
    timeout_cnt = 0;
    last_dist = (uint16_t)(sum / valid_cnt);
    return last_dist;
}