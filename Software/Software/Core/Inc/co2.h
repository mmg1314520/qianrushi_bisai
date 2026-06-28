#ifndef __CO2_H__
#define __CO2_H__

#include "main.h"

/* 初始化 CO2 传感器（启动 USART2 中断接收） */
void CO2_Init(void);

/* 读取 TVOC 浓度，成功返回 1，ppb 存放结果 */
uint8_t CO2_Read(uint16_t *ppb);

/* 由 HAL_UART_RxCpltCallback 调用，送入一个接收字节 */
void CO2_FeedByte(uint8_t byte);

#endif
