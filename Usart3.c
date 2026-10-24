#include "Usart3.h"

uint8_t usart3_rx_buf[USART3_RX_BUF_SIZE];
uint16_t usart3_rx_cnt = 0;
uint8_t usart3_rx_flag = 0;

void USART3_Init(uint32_t baudrate)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

    // TX PB10 复用推挽
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    // RX PB11 浮空输入
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = baudrate;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART3, &USART_InitStructure);

    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
    NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(USART3, ENABLE);
}

void USART3_SendString(char *str)
{
    while(*str)
    {
        USART_SendData(USART3, *str);
        while(USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET);
        str++;
    }
}

void USART3_IRQHandler(void)
{
    uint8_t ch;
    if(USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)
    {
        ch = USART_ReceiveData(USART3);
        if(usart3_rx_cnt < USART3_RX_BUF_SIZE - 1)
        {
            usart3_rx_buf[usart3_rx_cnt++] = ch;
            if(ch == '\n')
            {
                usart3_rx_buf[usart3_rx_cnt] = '\0';
                usart3_rx_flag = 1;
            }
        }
    }
}