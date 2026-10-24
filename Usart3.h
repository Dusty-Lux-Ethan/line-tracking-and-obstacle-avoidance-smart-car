#ifndef __USART3_H
#define __USART3_H

#include "stm32f10x.h"

#define USART3_RX_BUF_SIZE 128
extern uint8_t usart3_rx_buf[USART3_RX_BUF_SIZE];
extern uint16_t usart3_rx_cnt;
extern uint8_t usart3_rx_flag;

void USART3_Init(uint32_t baudrate);
void USART3_SendString(char *str);

#endif
