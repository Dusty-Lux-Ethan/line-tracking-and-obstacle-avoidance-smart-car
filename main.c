#include "stm32f10x.h"
#include "Usart3.h"
#include "Motor.h"
#include "Huidu.h"
#include "OLED.h"
#include "Delay.h"
#include "Yanse.h"
#include "Duoji.h"
#include "Ecoh.h"
#include "Buzzer.h"
#include <string.h>
#include <stdio.h>

//36.45.151.152.16.177.318.644.872.873.973.978

#define MODE_MANUAL     0   // 手动模式
#define MODE_AUTO       1   // 自动循迹+颜色识别
#define MODE_AVOID      2   // 避障迷宫模式
#define MODE_PUSHBOX    3   // 推箱子模式
#define MODE_FULL_AUTO  4   // 一键全自动：有黑线=循迹+颜色+推箱，无黑线=避障

// ===================== 状态定义 =====================
// 颜色任务状态
#define COLOR_IDLE      0
#define COLOR_RED       1
#define COLOR_GREEN     2
#define COLOR_BLUE      3
#define COLOR_LOCK      4

// 避障任务状态
#define AVOID_IDLE      0   // 正常前进
#define AVOID_STOP      1   // 检测到障碍停车
#define AVOID_SCAN_L    2   // 向左扫描测距
#define AVOID_SCAN_R    3   // 向右扫描测距
#define AVOID_TURN      4   // 转向
#define AVOID_GO        5   // 转向完成继续前进
#define AVOID_BACK      6   // 倒车后退状态
#define AVOID_THRESHOLD 20  // 避障距离阈值：小于20cm判定有障碍

// 推箱子任务状态
#define PUSH_IDLE       0   // 循迹寻箱
#define PUSH_SCAN       1   // 扫描定位箱子
#define PUSH_ALIGN      2   // 转向对准箱子
#define PUSH_FORWARD    3   // 推箱前进
#define PUSH_STOP       4   // 到达终点停车
#define WHITE_THRESHOLD  2000  // 白色区域灰度阈值，按实际标定修改
#define PUSH_IN_TIME     10   // 触发白边后继续推进时间(ms)

uint32_t all_white_timer = 0;   // 五路全白持续计时，单位ms
uint32_t no_line_timer = 0;
uint8_t  push_enter_black = 0;

int main(void)
{
    uint32_t last_handshake = 0;
    uint32_t tick_cnt = 0;
    
    // 颜色任务变量
    uint8_t color_state = COLOR_IDLE;
    uint32_t color_timer = 0;
    uint8_t color_check_cnt = 0;

    // 避障任务变量
    uint8_t avoid_state = AVOID_IDLE;
    uint32_t avoid_timer = 0;
    uint16_t dist_left = 0;    // 左侧距离
    uint16_t dist_right = 0;   // 右侧距离
    uint16_t dist_front = 0;   // 前方距离
    uint8_t avoid_retry = 0;   // 倒车重试计数
    uint8_t avoid_lock = 0;    // 紧急停车锁定标志

    // 推箱子任务变量
    uint8_t  push_state = PUSH_IDLE;
    uint32_t push_timer = 0;
    uint16_t push_min_dist = 600;
    uint8_t  push_min_angle = 65;
    uint8_t  push_enter_white = 0;
    uint8_t  push_scan_step = 0;
    uint16_t beep_cnt = 0;     // 蜂鸣器计时器，单位ms

    // ========== 外设初始化 ==========
    Delay_Init();
    USART3_Init(9600);   // 蓝牙串口
    Motor_Init();         // 电机驱动
    Huidu_Init();         // 灰度循迹
    OLED_Init();          // OLED显示屏
    Yanse_Init();         // 颜色传感器
    Duoji_Init();        // 舵机
    Ecoh_Init();         // 超声波
    Buzzer_Init();       // 蜂鸣器

    // 舵机初始归中：正前方65°（适配130°最大行程）
    Duoji_SetAngle(90);

    // 开机界面
    OLED_ShowString(1, 1, "Smart Car V2.0");
    OLED_ShowString(2, 1, "Mode: MANUAL");
    OLED_ShowString(3, 1, "BT: Wait Link  ");
    OLED_ShowString(4, 1, "State: STOP    ");

    while(1)
    {
        Delay_ms(1);
        tick_cnt++;

        // ===== 蜂鸣器计时处理 =====
        if(beep_cnt > 0)
        {
            beep_cnt--;
            if(beep_cnt == 0)
            {
                Buzzer_OFF(); // 计时到，自动关闭蜂鸣
            }
        }

        // ========== 1. 手动模式 ==========
        if(car_mode == MODE_MANUAL)
        {
            Motor_State_Update();
            color_state = COLOR_IDLE;
            avoid_state = AVOID_IDLE;
        }
        // ========== 2. 自动循迹+颜色识别模式 ==========
        else if(car_mode == MODE_AUTO)
        {
            // ---- 颜色任务状态机 ----
            switch(color_state)
            {
                case COLOR_IDLE:
                {
                    Huidu_Track();
                    if(tick_cnt % 10 == 0)
                    {
                        uint8_t color = Yanse_GetColor();
                        if(color != 0)
                        {
                            color_check_cnt++;
                            if(color_check_cnt >= 3)
                            {
                                color_state = color;
                                color_timer = 0;
                                color_check_cnt = 0;
                                Motor_SetSpeed(0, 0);
                                
                                if(color == 1) USART3_SendString("[任务A] 检测到红色，执行左转绕行\r\n");
                                if(color == 2) USART3_SendString("[任务A] 检测到绿色，停车后直行\r\n");
                                if(color == 3) USART3_SendString("[任务A] 检测到蓝色，执行右转绕行\r\n");
                            }
                        }
                        else color_check_cnt = 0;
                    }
                    break;
                }
                case COLOR_RED:
                {
                    color_timer++;
                    if(color_timer < 1000) Motor_SetSpeed(0, 0);
                    else if(color_timer < 1450) Motor_SetSpeed(-25, 25);
                    else
                    {
                        color_state = COLOR_LOCK;
                        color_timer = 0;
                        USART3_SendString("[任务A] 红色绕行完成，恢复循迹\r\n");
                    }
                    break;
                }
                case COLOR_GREEN:
                {
                    color_timer++;
                    if(color_timer < 2000) Motor_SetSpeed(0, 0);
									else if(color_timer < 3500) Motor_SetSpeed(1.073*25, 25);
                    else
                    {
                        color_state = COLOR_LOCK;
                        color_timer = 0;
                        USART3_SendString("[任务A] 绿色处理完成，恢复直行\r\n");
                    }
                    break;
                }
                case COLOR_BLUE:
                {
                    color_timer++;
                    if(color_timer < 1000) Motor_SetSpeed(0, 0);
                    else if(color_timer < 1450) Motor_SetSpeed(25, -25);
                    else
                    {
                        color_state = COLOR_LOCK;
                        color_timer = 0;
                        USART3_SendString("[任务A] 蓝色绕行完成，恢复循迹\r\n");
                    }
                    break;
                }
                case COLOR_LOCK:
                {
                    Huidu_Track();
                    color_timer++;
                    if(color_timer > 500) color_state = COLOR_IDLE;
                    break;
                }
            }
        }
				
        // ========== 3. 避障迷宫模式（任务B） ==========
        else if(car_mode == MODE_AVOID)
        {
            switch(avoid_state)
            {
                case AVOID_IDLE:
                {
                    if(avoid_lock == 1) break;
                    Motor_SetSpeed(1.08*25, 25);
                    if(tick_cnt % 50 == 0)
                    {
                        dist_front = Ecoh_GetDistance();
                        if(dist_front < AVOID_THRESHOLD)
                        {
                            avoid_state = AVOID_STOP;
                            avoid_timer = 0;
                            Motor_SetSpeed(0, 0);
                            USART3_SendString("[任务B] 检测到前方障碍，停车\r\n");
                            OLED_ShowString(4, 1, "State: 避障中   ");
                        }
                    }
                    break;
                }
                case AVOID_STOP:
                {
                    Motor_SetSpeed(0, 0);
                    avoid_timer++;
                    if(avoid_timer > 500)
                    {
                        if(avoid_lock == 1) break;
                        avoid_state = AVOID_SCAN_R;
                        avoid_timer = 0;
                        Duoji_SetAngle(180);
                    }
                    break;
                }
                case AVOID_SCAN_R:
                {
                    avoid_timer++;
                    if(avoid_timer > 400)
                    {
                        dist_right = Ecoh_GetDistance();
                        if(dist_right >= AVOID_THRESHOLD)
                        {
                            Duoji_SetAngle(90);
                            avoid_state = AVOID_TURN;
                            avoid_timer = 0;
                            dist_left = 0;
                            avoid_retry = 0;
                            USART3_SendString("[任务B] 右侧通畅，右转通过\r\n");
                        }
                        else
                        {
                            avoid_state = AVOID_SCAN_L;
                            avoid_timer = 0;
                            Duoji_SetAngle(0);
                            USART3_SendString("[任务B] 右侧有障碍，检测左侧\r\n");
                        }
                    }
                    break;
                }
                case AVOID_SCAN_L:
                {
                    avoid_timer++;
                    if(avoid_timer > 400)
                    {
                        dist_left = Ecoh_GetDistance();
                        Duoji_SetAngle(90);
                        avoid_timer = 0;
                        
                        char buf[64];
                        sprintf(buf, "[任务B] 左:%dcm 右:%dcm\r\n", dist_left, dist_right);
                        USART3_SendString(buf);
                        
                        if(dist_left >= AVOID_THRESHOLD)
                        {
                            avoid_state = AVOID_TURN;
                            avoid_retry = 0;
                            USART3_SendString("[任务B] 左侧通畅，左转通过\r\n");
                        }
                        else
                        {
                            avoid_retry++;
                            if(avoid_retry >= 3)
                            {
                                avoid_state = AVOID_STOP;
                                avoid_lock = 1;
                                USART3_SendString("[任务B] 三面均有障碍，重试3次失败，紧急停车锁定\r\n");
                                OLED_ShowString(4, 1, "State: 锁定停车 ");
                            }
                            else
                            {
                                avoid_state = AVOID_BACK;
                                USART3_SendString("[任务B] 三面均有障碍，倒车重试\r\n");
                            }
                        }
                    }
                    break;
                }
                case AVOID_BACK:
                {
                    Motor_SetSpeed(-20, -20);
                    avoid_timer++;
                    if(avoid_timer > 800)
                    {
                        avoid_state = AVOID_SCAN_R;
                        avoid_timer = 0;
                        Duoji_SetAngle(180);
                    }
                    break;
                }
                case AVOID_TURN:
                {
                    if(dist_left == 0)
                    {
                        Motor_SetSpeed(-30, 30);
                    }
                    else
                    {
                        Motor_SetSpeed(30, -30);
                    }
                    avoid_timer++;
                    if(avoid_timer > 400)
                    {
                        avoid_state = AVOID_GO;
                        avoid_timer = 0;
                    }
                    break;
                }
                case AVOID_GO:
                {
                    Motor_SetSpeed(1.073*25, 25);
                    avoid_timer++;
                    if(avoid_timer > 100)
                    {
                        avoid_state = AVOID_IDLE;
                    }
                    break;
                }
            }
        }
				
        // ========== 4. 推箱子模式（任务C） ==========
        else if(car_mode == MODE_PUSHBOX)
        {
            switch(push_state)
            {
                // 状态1：沿循迹轨道巡航寻箱
                case PUSH_IDLE:
                {
                    Huidu_Track(); // 沿黑线自动前进，适配轨道上的箱子
                    
                    if(tick_cnt % 50 == 0)
                    {
                        dist_front = Ecoh_GetDistance();
                        // 前方30cm内检测到障碍物，判定为箱子
                        if(dist_front < 30)
                        {
                            push_state = PUSH_SCAN;
                            push_timer = 0;
                            push_min_dist = 600;
                            push_min_angle = 65;
                            push_scan_step = 0;
                            Motor_SetSpeed(0, 0);
                            
                            // 识别成功，蜂鸣器短响300ms
                            Buzzer_ON();
                            beep_cnt = 1000;
                            
                            USART3_SendString("[任务C] 发现目标，开始扫描定位\r\n");
                        }
                    }
                    break;
                }

                // 状态2：舵机扫描定位箱子
                case PUSH_SCAN:
                {
                    push_timer++;
                    // 第1步：转到最左侧
                    if(push_scan_step == 0 && push_timer > 400)
                    {
                        Duoji_SetAngle(0);
                        push_timer = 0;
                        push_scan_step = 1;
                    }
                    // 第2步：测左侧距离，转到正中
                    else if(push_scan_step == 1 && push_timer > 400)
                    {
                        uint16_t d = Ecoh_GetDistance();
                        if(d < push_min_dist) { push_min_dist = d; push_min_angle = 0; }
                        Duoji_SetAngle(90);
                        push_timer = 0;
                        push_scan_step = 2;
                    }
                    // 第3步：测正中距离，转到最右侧
                    else if(push_scan_step == 2 && push_timer > 400)
                    {
                        uint16_t d = Ecoh_GetDistance();
                        if(d < push_min_dist) { push_min_dist = d; push_min_angle = 65; }
                        Duoji_SetAngle(180);
                        push_timer = 0;
                        push_scan_step = 3;
                    }
                    // 第4步：测右侧距离，归中，进入对准
                    else if(push_scan_step == 3 && push_timer > 400)
                    {
                        uint16_t d = Ecoh_GetDistance();
                        if(d < push_min_dist) { push_min_dist = d; push_min_angle = 130; }
                        Duoji_SetAngle(90);
                        push_scan_step = 0;
                        push_state = PUSH_ALIGN;
                        push_timer = 0;
                        
                        char buf[64];
                        sprintf(buf, "[任务C] 定位完成，最近距离:%dcm 角度:%d°\r\n", push_min_dist, push_min_angle);
                        USART3_SendString(buf);
                    }
                    break;
                }

                // 状态3：转向对准箱子
                case PUSH_ALIGN:
                {
                    push_timer++;
                    
                    if(push_min_angle < 65)
                    {
                        Motor_SetSpeed(-20, 20); // 箱子在左，原地左转
                    }
                    else if(push_min_angle > 65)
                    {
                        Motor_SetSpeed(20, -20); // 箱子在右，原地右转
                    }

                    if(push_timer > 600)
                    {
                        dist_front = Ecoh_GetDistance();
                        if(dist_front <= push_min_dist + 2)
                        {
                            Motor_SetSpeed(0, 0);
                            push_state = PUSH_FORWARD;
                            push_timer = 0;
                            push_enter_white = 0;
                            USART3_SendString("[任务C] 对准完成，开始推箱\r\n");
                        }
                    }
                    break;
                }

                // 状态4：推箱前进+循迹+5路全白终点检测
case PUSH_FORWARD:
{
    int32_t error = 0;
    uint8_t line_cnt = 0;
    int16_t speed_l, speed_r;
    int16_t base_speed = 20; // 默认基准速度
    uint8_t i;

    // 1. 超声波距离闭环：计算整体基准速度
    dist_front = Ecoh_GetDistance();
    if(dist_front > 25)
    {
        base_speed = 30;    // 距离远，加速跟上
    }
    else if(dist_front < 12)
    {
        base_speed = 15;    // 距离近，减速防顶翻
    }
    else
    {
        base_speed = 20;    // 正常距离匀速
    }

    // 2. 每10ms刷新全部5路灰度 + 循迹计算
    if(tick_cnt % 2 == 0)
    {
        // 刷新全部5路ADC值
        for(i = 0; i < SENSOR_NUM; i++)
        {
            huidu_adc[i] = ADC_ReadChannel(adc_channel[i]);
        }

        // 计算黑线偏移（和循迹算法完全一致：大于阈值=黑线）
        error = 0;
        line_cnt = 0;
        for(i = 0; i < SENSOR_NUM; i++)
        {
            if(huidu_adc[i] < LINE_THRESHOLD)
            {
                error += weight[i];
                line_cnt++;
            }
        }

        // 死区：小偏移不修正
        if(error > -DEAD_ZONE && error < DEAD_ZONE)
        {
            error = 0;
        }
    }

    // 3. 循迹差速 + 基准速度叠加
    speed_l = base_speed - TRACK_KP * error;
    speed_r = base_speed + TRACK_KP * error;

    // 速度限幅
    if(speed_l < 0) speed_l = 0;
    if(speed_l > MOTOR_MAX_SPEED) speed_l = MOTOR_MAX_SPEED;
    if(speed_r < 0) speed_r = 0;
    if(speed_r > MOTOR_MAX_SPEED) speed_r = MOTOR_MAX_SPEED;

    // 输出到电机（左轮保留机械差速补偿）
    Motor_SetSpeed(1.073f * speed_l, speed_r);

    // 4. 终点判断：先确认无黑线，再判断5路全白
if(tick_cnt % 5 == 0)
{
    uint8_t has_black_line = 0;
    uint8_t all_white = 1;
    
    // 先遍历：有没有任何一路检测到黑线
    for(i = 0; i < SENSOR_NUM; i++)
    {
        if(huidu_adc[i] < LINE_THRESHOLD)
        {
            has_black_line = 1; // 还在黑线上
            break;
        }
    }

    // 只有完全脱离黑线，才判断是不是全白终点
    if(has_black_line == 0)
    {
        for(i = 0; i < SENSOR_NUM; i++)
        {
            if(huidu_adc[i] < WHITE_THRESHOLD)
            {
                all_white = 0;
                break;
            }
        }
    }
    else
    {
        all_white = 0; // 还在黑线上，不可能是终点
    }

    if(all_white && push_enter_white == 0)
    {
        push_enter_white = 1;
        push_timer = 0;
        USART3_SendString("[任务C] 5路全白，到达终点，继续推进\r\n");
    }
}

    // 5. 推进计时，到点停车
    if(push_enter_white == 1)
    {
        push_timer++;
        if(push_timer >= PUSH_IN_TIME)
        {
            push_state = PUSH_STOP;
            Motor_SetSpeed(0, 0);
            USART3_SendString("[任务C] 箱子到位，推箱完成\r\n");
            OLED_ShowString(4, 1, "State: 完成    ");
        }
    }
    break;
}
                // 状态5：任务完成停车
                case PUSH_STOP:
                {
                    Motor_SetSpeed(0, 0);
                    break;
                }
            }
        }

				
// ========== 5. 一键全自动模式 ==========
else if(car_mode == MODE_FULL_AUTO)
{
    // 优先级1：颜色状态机运行中，优先完整执行颜色处理
    if(color_state != COLOR_IDLE)
    {
        switch(color_state)
        {
            case COLOR_RED:
            {
                color_timer++;
                if(color_timer < 1000) 
                    Motor_SetSpeed(0, 0); 								// 前1秒停车
               else if(color_timer < 1300) 
								 Motor_SetSpeed(1.073*25, 25);
								else if(color_timer < 1650) 
                    Motor_SetSpeed(-25, 25);      // 左转450ms，与原模式参数一致
                else
                {
                    color_state = COLOR_LOCK;
                    color_timer = 0;
                    USART3_SendString("[任务A] 红色绕行完成，进入锁定延时\r\n");
                }
                break;
            }
            
            case COLOR_GREEN:
            {
                color_timer++;
                if(color_timer < 2000) 
                    Motor_SetSpeed(0, 0);          // 前2秒停车
                else if(color_timer < 3000) 
                    Motor_SetSpeed(25, 1.025*25); // 直行1.5秒
                else
                {
                    color_state = COLOR_LOCK;
                    color_timer = 0;
                    USART3_SendString("[任务A] 绿色处理完成，进入锁定延时\r\n");
                }
                break;
            }
            
            case COLOR_BLUE:
            {
                color_timer++;
                if(color_timer < 1000) 
                    Motor_SetSpeed(0, 0);								// 前1秒停车
                else if(color_timer < 1200) 
								 Motor_SetSpeed(1.073*25, 25);
								else if(color_timer < 1650) 
                    Motor_SetSpeed(25, -25);      // 右转350ms，与原模式参数一致
                else if(color_timer < 2350) 
                    Motor_SetSpeed(25, 1.1*25);
								else
                {
                    color_state = COLOR_LOCK;
                    color_timer = 0;
                    USART3_SendString("[任务A] 蓝色绕行完成，进入锁定延时\r\n");
                }
                break;
            }
            
            case COLOR_LOCK:
            {
                Huidu_Track();                   // 锁定阶段开启循迹，尽快驶离颜色块
                color_timer++;
                if(color_timer > 500)
                {
                    color_state = COLOR_IDLE;
                    color_timer = 0;
                    color_check_cnt = 0;          // 复位颜色检测计数，防止误触发
                    USART3_SendString("[全自动] 颜色任务结束，恢复寻迹模式\r\n");
                    OLED_ShowString(4, 1, "State: 寻迹中   ");
                }
                break;
            }
            
            default:
                color_state = COLOR_IDLE;
                break;
        }
    }
    // 优先级2：推箱子状态机运行中，执行推箱全流程
    else if(push_state != PUSH_IDLE)
    {
        switch(push_state)
        {
            // 扫描定位箱子
            case PUSH_SCAN:
            {
                push_timer++;
                if(push_scan_step == 0 && push_timer > 400)
                {
                    Duoji_SetAngle(0);
                    push_timer = 0;
                    push_scan_step = 1;
                }
                else if(push_scan_step == 1 && push_timer > 400)
                {
                    uint16_t d = Ecoh_GetDistance();
                    if(d < push_min_dist) 
                    { 
                        push_min_dist = d; 
                        push_min_angle = 0;
                    }
                    Duoji_SetAngle(90);
                    push_timer = 0;
                    push_scan_step = 2;
                }
                else if(push_scan_step == 2 && push_timer > 400)
                {
                    uint16_t d = Ecoh_GetDistance();
                    if(d < push_min_dist) 
                    { 
                        push_min_dist = d; 
                        push_min_angle = 65;
                    }
                    Duoji_SetAngle(180);
                    push_timer = 0;
                    push_scan_step = 3;
                }
                else if(push_scan_step == 3 && push_timer > 400)
                {
                    uint16_t d = Ecoh_GetDistance();
                    if(d < push_min_dist) 
                    { 
                        push_min_dist = d; 
                        push_min_angle = 130;
                    }
                    Duoji_SetAngle(90);
                    push_scan_step = 0;
                    push_state = PUSH_ALIGN;
                    push_timer = 0;
                    
                    char buf[64];
                    sprintf(buf, "[任务C] 定位完成，最近距离:%dcm 角度:%d°\r\n", push_min_dist, push_min_angle);
                    USART3_SendString(buf);
                }
                break;
            }
            
            // 转向对准箱子
            case PUSH_ALIGN:
            {
                push_timer++;
                if(push_min_angle < 65)
                    Motor_SetSpeed(-20, 20);
                else if(push_min_angle > 65)
                    Motor_SetSpeed(20, -20);
                
                if(push_timer > 600)
                {
                    dist_front = Ecoh_GetDistance();
                    if(dist_front <= push_min_dist + 2)
                    {
                        Motor_SetSpeed(0, 0);
                        push_state = PUSH_FORWARD;
                        push_timer = 0;
                        push_enter_black = 0;
                        USART3_SendString("[任务C] 对准完成，开始推箱\r\n");
                    }
                }
                break;
            }
            
            // 推箱前进 + 循迹 + 五路全黑终点检测
            case PUSH_FORWARD:
            {
                int32_t error = 0;
                uint8_t line_cnt = 0;
                int16_t speed_l, speed_r;
                int16_t base_speed = 20;
                uint8_t i;
                
                // 超声波距离闭环调速
                dist_front = Ecoh_GetDistance();
                if(dist_front > 25)
                    base_speed = 30;
                else if(dist_front < 12)
                    base_speed = 15;
                else
                    base_speed = 20;
                
                // 每2ms刷新灰度，计算循迹偏移（修复：error累加权重，与底层逻辑一致）
                if(tick_cnt % 2 == 0)
                {
                    for(i = 0; i < SENSOR_NUM; i++)
                        huidu_adc[i] = ADC_ReadChannel(adc_channel[i]);
                    
                    error = 0;
                    line_cnt = 0;
                    for(i = 0; i < SENSOR_NUM; i++)
                    {
                        if(huidu_adc[i] > LINE_THRESHOLD)
                        {
                            error += weight[i];  // 修复：原为减号，导致循迹方向反向
                            line_cnt++;
                        }
                    }
                    if(error > -DEAD_ZONE && error < DEAD_ZONE)
                        error = 0;
                }
                
                // 差速输出
                speed_l = base_speed + TRACK_KP * error;
                speed_r = base_speed - TRACK_KP * error;
                
                if(speed_l < 0) speed_l = 0;
                if(speed_l > MOTOR_MAX_SPEED) speed_l = MOTOR_MAX_SPEED;
                if(speed_r < 0) speed_r = 0;
                if(speed_r > MOTOR_MAX_SPEED) speed_r = MOTOR_MAX_SPEED;
                
                Motor_SetSpeed(1.073f * speed_l, speed_r);
                
                // 终点判断：五路全黑（全部检测到黑线）
                if(tick_cnt % 5 == 0)
                {
                    uint8_t all_black = 1;
                    
                    for(i = 0; i < SENSOR_NUM; i++)
                    {
                        if(huidu_adc[i] <= LINE_THRESHOLD)
                        {
                            all_black = 0;
                            break;
                        }
                    }
                    
                    if(all_black && push_enter_black == 0)
                    {
                        push_enter_black = 1;
                        push_timer = 0;
                        USART3_SendString("[任务C] 五路全黑，到达终点，继续推进\r\n");
                    }
                }
                
                // 推进计时到，推箱完成
                if(push_enter_black == 1)
                {
                    push_timer++;
                    if(push_timer >= PUSH_IN_TIME)
                    {
                        push_state = PUSH_STOP;
                        Motor_SetSpeed(0, 0);
                        USART3_SendString("[任务C] 箱子到位，推箱完成\r\n");
                        OLED_ShowString(4, 1, "State: 推箱完成 ");
											Delay_s(100);
                    }
                }
                break;
            }
            
            // 推箱完成，延时后自动复位恢复寻迹
            case PUSH_STOP:
            {
                Motor_SetSpeed(0, 0);
                push_timer++;
                if(push_timer > 1000)
                {
                    push_state = PUSH_IDLE;
                    push_enter_black = 0;
                    push_scan_step = 0;
                    push_min_dist = 600;
                    push_min_angle = 65;
                    Duoji_SetAngle(90);
                    USART3_SendString("[全自动] 推箱任务结束，恢复寻迹模式\r\n");
                    OLED_ShowString(4, 1, "State: 寻迹中   ");
                }
                break;
            }
            
            default:
                push_state = PUSH_IDLE;
                break;
        }
    }
    // 优先级3：避障模式激活（丢线超时+遇障触发后）
    else if(avoid_state != AVOID_IDLE)
    {
        switch(avoid_state)
        {
            // 避障空闲：默认直行，检测到障碍才启动避障动作
            case AVOID_IDLE:
            {
                if(avoid_lock == 1) break;
                Motor_SetSpeed(1.08*25, 25);
                
                // 每50ms检测前方障碍
                if(tick_cnt % 50 == 0)
                {
                    dist_front = Ecoh_GetDistance();
                    if(dist_front < AVOID_THRESHOLD)
                    {
                        avoid_state = AVOID_STOP;
                        avoid_timer = 0;
                        Motor_SetSpeed(0, 0);
                        USART3_SendString("[任务B] 检测到前方障碍，停车\r\n");
                        OLED_ShowString(4, 1, "State: 避障中   ");
                    }
                }
                
                // 每10ms检测黑线：找到黑线自动退出避障
                if(tick_cnt % 10 == 0)
                {
                    uint8_t has_line = 0;
                    uint8_t i;
                    for(i = 0; i < SENSOR_NUM; i++)
                    {
                        huidu_adc[i] = ADC_ReadChannel(adc_channel[i]);
                        if(huidu_adc[i] > LINE_THRESHOLD)
                        {
                            has_line = 1;
                            break;
                        }
                    }
                    
                    if(has_line)
                    {
                        avoid_state = AVOID_IDLE;
                        avoid_retry = 0;
                        avoid_lock = 0;
                        no_line_timer = 0;
                        Duoji_SetAngle(90);
                        USART3_SendString("[全自动] 检测到黑线，退出避障，恢复寻迹\r\n");
                        OLED_ShowString(4, 1, "State: 寻迹中   ");
                    }
                }
                break;
            }
            
            case AVOID_STOP:
            {
                Motor_SetSpeed(0, 0);
                avoid_timer++;
                if(avoid_timer > 500)
                {
                    if(avoid_lock == 1) break;
                    avoid_state = AVOID_SCAN_R;
                    avoid_timer = 0;
                    Duoji_SetAngle(180);
                }
                break;
            }
            
            case AVOID_SCAN_R:
            {
                avoid_timer++;
                if(avoid_timer > 400)
                {
                    dist_right = Ecoh_GetDistance();
                    if(dist_right >= AVOID_THRESHOLD)
                    {
                        Duoji_SetAngle(90);
                        avoid_state = AVOID_TURN;
                        avoid_timer = 0;
                        dist_left = 0;
                        avoid_retry = 0;
                        USART3_SendString("[任务B] 右侧通畅，右转通过\r\n");
                    }
                    else
                    {
                        avoid_state = AVOID_SCAN_L;
                        avoid_timer = 0;
                        Duoji_SetAngle(0);
                        USART3_SendString("[任务B] 右侧有障碍，检测左侧\r\n");
                    }
                }
                break;
            }
            
            case AVOID_SCAN_L:
            {
                avoid_timer++;
                if(avoid_timer > 400)
                {
                    dist_left = Ecoh_GetDistance();
                    Duoji_SetAngle(90);
                    avoid_timer = 0;
                    
                    char buf[64];
                    sprintf(buf, "[任务B] 左:%dcm 右:%dcm\r\n", dist_left, dist_right);
                    USART3_SendString(buf);
                    
                    if(dist_left >= AVOID_THRESHOLD)
                    {
                        avoid_state = AVOID_TURN;
                        avoid_retry = 0;
                        USART3_SendString("[任务B] 左侧通畅，左转通过\r\n");
                    }
                    else
                    {
                        avoid_retry++;
                        if(avoid_retry >= 3)
                        {
                            avoid_state = AVOID_STOP;
                            avoid_lock = 1;
                            USART3_SendString("[任务B] 三面均有障碍，重试3次失败，紧急停车锁定\r\n");
                            OLED_ShowString(4, 1, "State: 锁定停车 ");
                        }
                        else
                        {
                            avoid_state = AVOID_BACK;
                            USART3_SendString("[任务B] 三面均有障碍，倒车重试\r\n");
                        }
                    }
                }
                break;
            }
            
            case AVOID_BACK:
            {
                Motor_SetSpeed(-20, -20);
                avoid_timer++;
                if(avoid_timer > 800)
                {
                    avoid_state = AVOID_SCAN_R;
                    avoid_timer = 0;
                    Duoji_SetAngle(180);
                }
                break;
            }
            
            case AVOID_TURN:
            {
                if(dist_left == 0)
                    Motor_SetSpeed(-25, 25);
                else
                    Motor_SetSpeed(28, -28);
                
                avoid_timer++;
                if(avoid_timer > 440)
                {
                    avoid_state = AVOID_GO;
                    avoid_timer = 0;
                }
                break;
            }
            
            case AVOID_GO:
            {
                Motor_SetSpeed(1.08*25, 25);
                avoid_timer++;
                if(avoid_timer > 100)
                {
                    avoid_state = AVOID_IDLE;
                }
                break;
            }
            
            default:
                avoid_state = AVOID_IDLE;
                break;
        }
    }
    // 基础状态：灰度寻迹 + 多事件检测
    else
    {
        // 1. 执行灰度循迹（自带岔路口状态机）
        Huidu_Track();
        
        // 2. 颜色识别检测（3次确认防误触）
        if(tick_cnt % 10 == 0)
        {
            uint8_t color = Yanse_GetColor();
            if(color != 0)
            {
                color_check_cnt++;
                if(color_check_cnt >= 3)
                {
                    color_state = color;
                    color_timer = 0;
                    color_check_cnt = 0;
                    Motor_SetSpeed(0, 0);
                    
                    if(color == COLOR_RED)
                        USART3_SendString("[全自动] 检测到红色，执行左转绕行\r\n");
                    if(color == COLOR_GREEN)
                        USART3_SendString("[全自动] 检测到绿色，停车后直行\r\n");
                    if(color == COLOR_BLUE)
                        USART3_SendString("[全自动] 检测到蓝色，执行右转绕行\r\n");
                    
                    OLED_ShowString(4, 1, "State: 颜色处理 ");
                }
            }
            else
            {
                color_check_cnt = 0;
            }
        }
        
        // 3. 无黑线持续计时
        if(tick_cnt % 10 == 0)
        {
            uint8_t has_line = 0;
            uint8_t i;
            
            for(i = 0; i < SENSOR_NUM; i++)
            {
                if(huidu_adc[i] > LINE_THRESHOLD)
                {
                    has_line = 1;
                    break;
                }
            }
            
            if(has_line)
                no_line_timer = 0;
            else
                no_line_timer += 10;
        }
        
        // 4. 前方障碍检测：按丢线时长分配任务（修复：丢线500ms以上才触发避障）
        if(tick_cnt % 50 == 0)
        {
            dist_front = Ecoh_GetDistance();
            if(dist_front < AVOID_THRESHOLD)
            {
                Motor_SetSpeed(0, 0);
                
                if(no_line_timer > 30)
                {
                    // 无黑线超0.5秒 → 判定为开阔区域，启动避障模式
                    avoid_state = AVOID_STOP;
                    avoid_timer = 0;
                    avoid_retry = 0;
                    avoid_lock = 0;
                    USART3_SendString("[全自动] 丢线区域遇障，启动避障模式\r\n");
                    OLED_ShowString(4, 1, "State: 避障中   ");
                }
                else
                {
                    // 丢线不足0.5秒 → 判定为轨道内障碍物，启动推箱子
                    push_state = PUSH_SCAN;
                    push_timer = 0;
                    push_min_dist = 600;
                    push_min_angle = 65;
                    push_scan_step = 0;
                    push_enter_black = 0;
                    
                    Buzzer_ON();
                    beep_cnt = 1000;
                    
                    USART3_SendString("[全自动] 轨道区域遇障，启动推箱任务\r\n");
                    OLED_ShowString(4, 1, "State: 推箱中   ");
                }
            }
        }
    }
}



// ========== 每秒蓝牙握手 ==========
        if(tick_cnt - last_handshake >= 5000)
        {
            last_handshake = tick_cnt;
            USART3_SendString("[握手] 姓名:郑文奥 专业:自动化 班级:z2601\r\n");
        }

        // ========== 蓝牙指令处理 ==========
        if(usart3_rx_flag)
        {
            usart3_rx_flag = 0;
            if(strcmp((char*)usart3_rx_buf, "HELLO\r\n") == 0)
            {
                USART3_SendString("[确认] 连接成功，当前手动模式\r\n");
                OLED_ShowString(3, 1, "BT: Connected  ");
            }
            else if(strcmp((char*)usart3_rx_buf, "GO\r\n") == 0)
            {
                if(car_mode == MODE_MANUAL)
                {
                    motion_state = MOTION_FORWARD;
                    USART3_SendString("[状态] 小车直行前进\r\n");
                    OLED_ShowString(4, 1, "State: FORWARD ");
                }
            }
            else if(strcmp((char*)usart3_rx_buf, "STOP\r\n") == 0)
            {
                motion_state = MOTION_STOP;
                USART3_SendString("[状态] 小车已停止\r\n");
                OLED_ShowString(4, 1, "State: STOP    ");
            }
            else if(strcmp((char*)usart3_rx_buf, "BACK\r\n") == 0)
            {
                if(car_mode == MODE_MANUAL)
                {
                    motion_state = MOTION_BACKWARD;
                    USART3_SendString("[状态] 小车后退\r\n");
                    OLED_ShowString(4, 1, "State: BACKWARD");
                }
            }
            else if(strcmp((char*)usart3_rx_buf, "LEFT\r\n") == 0)
            {
                if(car_mode == MODE_MANUAL)
                {
                    motion_state = MOTION_LEFT;
                    USART3_SendString("[状态] 小车左转\r\n");
                    OLED_ShowString(4, 1, "State: LEFT    ");
                }
            }
            else if(strcmp((char*)usart3_rx_buf, "RIGHT\r\n") == 0)
            {
                if(car_mode == MODE_MANUAL)
                {
                    motion_state = MOTION_RIGHT;
                    USART3_SendString("[状态] 小车右转\r\n");
                    OLED_ShowString(4, 1, "State: RIGHT   ");
                }
            }
            // 切换自动循迹+颜色模式
            else if(strcmp((char*)usart3_rx_buf, "MODE_AUTO\r\n") == 0)
            {
                car_mode = MODE_AUTO;
                motion_state = MOTION_STOP;
                color_state = COLOR_IDLE;
                USART3_SendString("[模式] 已切换自动循迹+颜色识别模式\r\n");
                OLED_ShowString(2, 1, "Mode: AUTO     ");
                OLED_ShowString(4, 1, "State: TRACK   ");
            }
            // 切换手动模式
            else if(strcmp((char*)usart3_rx_buf, "MODE_MANUAL\r\n") == 0)
            {
                car_mode = MODE_MANUAL;
                motion_state = MOTION_STOP;
                Duoji_SetAngle(90);
                USART3_SendString("[模式] 已切换手动控制模式\r\n");
                OLED_ShowString(2, 1, "Mode: MANUAL   ");
                OLED_ShowString(4, 1, "State: STOP    ");
            }
            // 切换避障迷宫模式
            else if(strcmp((char*)usart3_rx_buf, "MODE_AVOID\r\n") == 0)
            {
                car_mode = MODE_AVOID;
                motion_state = MOTION_STOP;
                avoid_state = AVOID_IDLE;
                avoid_retry = 0;
                avoid_lock = 0;
                Duoji_SetAngle(90);
                USART3_SendString("[模式] 已切换避障迷宫模式\r\n");
                OLED_ShowString(2, 1, "Mode: AVOID    ");
                OLED_ShowString(4, 1, "State: 前进中   ");
            }
            // 切换推箱子模式
            else if(strcmp((char*)usart3_rx_buf, "MODE_PUSHBOX\r\n") == 0)
            {
                car_mode = MODE_PUSHBOX;
                motion_state = MOTION_STOP;
                push_state = PUSH_IDLE;
                push_enter_white = 0;
                push_scan_step = 0;
                Duoji_SetAngle(90);
                USART3_SendString("[模式] 已切换推箱子任务模式\r\n");
                OLED_ShowString(2, 1, "Mode: PUSHBOX  ");
                OLED_ShowString(4, 1, "State: 寻箱中   ");
            }
						else if(strcmp((char*)usart3_rx_buf, "RUN_ALL\r\n") == 0)
{
    car_mode = MODE_FULL_AUTO;
    motion_state = MOTION_STOP;
    color_state = COLOR_IDLE;
    push_state = PUSH_IDLE;
    avoid_state = AVOID_IDLE;
    push_enter_white = 0;
    Duoji_SetAngle(90);
    USART3_SendString("[模式] 已启动一键全自动任务\r\n");
    OLED_ShowString(2, 1, "Mode: FULL_AUTO ");
    OLED_ShowString(4, 1, "State: 运行中   ");
}
            // 调试：灰度ADC
            else if(strcmp((char*)usart3_rx_buf, "ADC\r\n") == 0)
            {
                Huidu_PrintADC();
                USART3_SendString("[调试] OLED已显示灰度ADC数值\r\n");
            }
            // 调试：颜色RGB
            else if(strcmp((char*)usart3_rx_buf, "COLOR\r\n") == 0)
            {
                uint16_t r, g, b, c;
                char buf[64];
                Yanse_ReadRGB(&r, &g, &b, &c);
                sprintf(buf, "[调试] R:%d G:%d B:%d C:%d\r\n", r, g, b, c);
                USART3_SendString(buf);
            }
            // 调试：测距离
            else if(strcmp((char*)usart3_rx_buf, "DIST\r\n") == 0)
            {
                uint16_t d = Ecoh_GetDistance();
                char buf[64];
                sprintf(buf, "[调试] 当前距离：%d cm\r\n", d);
                USART3_SendString(buf);
            }
            // 调试：设置舵机角度
            else if(strncmp((char*)usart3_rx_buf, "SERVO=", 6) == 0)
            {
                uint8_t ang = 0;
                sscanf((char*)usart3_rx_buf, "SERVO=%hhu", &ang);
                Duoji_SetAngle(ang);
                char buf[64];
                sprintf(buf, "[调试] 舵机已设为%d度\r\n", ang);
                USART3_SendString(buf);
            }

            usart3_rx_cnt = 0;
        }
    }
}
