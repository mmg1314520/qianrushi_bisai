#ifndef __PROTOCOL_H__
#define __PROTOCOL_H__

#include "main.h"
#include "sensors.h"

/* 协议初始化（启动 HAL 中断接收） */
void Protocol_Init(void);

/* 发送传感器数据 */
void Protocol_SendData(const SensorData_t *data);

/* 主循环调用：处理接收到的命令行 */
void Protocol_Process(void);

#endif
