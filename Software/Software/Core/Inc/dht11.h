#ifndef __DHT11_H
#define __DHT11_H

#include "main.h"

uint8_t DHT11_Init(void);
uint8_t DHT11_ReadData(uint8_t *humi, uint8_t *temp);
uint8_t DHT11_GetDbgStep(void);       /* 返回最后失败步骤：0=成功 2=响应低超时 3=响应高超时 4=起始位超时 5=数据超时 6=校验错 */
const uint8_t* DHT11_GetRawData(void); /* 返回最后一次读取的原始5字节数据 */

#endif
