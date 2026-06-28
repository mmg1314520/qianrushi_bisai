#include "co2.h"

/*
 * CO2/TVOC 传感器协议分析：
 * 传感器通常使用 UART 以固定周期（如 1s）主动上报 16 字节的数据帧。
 * 帧结构示例：FE 00 11 00 07 00 06 01 9F 00 00 00 00 00 00 BC
 * * 其中：
 * FE       — 帧头 (Header)
 * [7][8]   — 数据区高低字节组合，用于表示检测数值
 * BC       — 校验和 (Checksum)，前 15 字节之和的低 8 位
 */

#define FRAME_LEN    16         /* 完整帧长度：16 字节 */
#define FRAME_DATA   15         /* 校验和之前的字节数：前 15 个字节用于计算校验 */

/* 静态全局变量，用于缓存帧数据 */
static volatile uint8_t  fbuf[FRAME_LEN];    /* 帧接收缓冲区 */
static volatile uint8_t  fidx = 0;           /* 当前缓冲区索引（指示下个字节存入位置） */
static volatile uint32_t last_tick = 0;       /* 记录上次接收字节的时间，用于超时清理残留数据 */

static volatile uint8_t  data_ready = 0;     /* 数据处理标志位：1 表示已有新数据，0 表示待机 */
static          uint16_t co2_val    = 0;     /* 解析出的 CO2/TVOC 数值（ppb 或 ppm） */

/**
 * @brief 校验和计算函数
 * @param buf 缓冲区指针
 * @param len 需要累加的长度
 * @return 计算得到的 8 位校验和
 */
static uint8_t CalcChecksum(const volatile uint8_t *buf, uint8_t len)
{
    uint16_t sum = 0;
    for (uint8_t i = 0; i < len; i++) {
        sum += buf[i];
    }
    return (uint8_t)(sum & 0xFF); // 取累加和的低 8 位
}

/**
 * @brief 串口数据喂入函数（需在 UART 接收中断回调函数中调用）
 * @param byte 串口收到的单个字节
 */
void CO2_FeedByte(uint8_t byte)
{
    uint32_t now = HAL_GetTick(); // 获取当前系统时间

    /* * 超时重置逻辑：如果当前缓冲里有数据，但超过 100ms 没收到新数据，
     * 说明之前的帧断开了或数据出错，强制清空重置索引，重新寻找帧头。
     */
    if (fidx > 0 && (now - last_tick > 100)) {
        fidx = 0;
    }
    last_tick = now;

    /* 寻找帧头 0xFE：只有在索引为 0 时才判断帧头 */
    if (fidx == 0) {
        if (byte == 0xFE) {
            fbuf[fidx++] = byte;
        }
        return; // 等待下一个字节
    }

    /* 存入字节到缓冲区 */
    fbuf[fidx++] = byte;

    /* 若帧未收满 16 字节，直接返回，等待下一次中断 */
    if (fidx < FRAME_LEN) {
        return;
    }

    /* 帧已收齐：进行校验和验证 */
    uint8_t cksum = CalcChecksum(fbuf, FRAME_DATA);
    if (cksum == fbuf[FRAME_DATA]) {
        /* * 校验通过，提取数值。
         * 根据传感器规格手册，第 7 字节为高位，第 8 字节为低位。
         * 移位并或运算组合成 16 位整数。
         */
        co2_val    = ((uint16_t)fbuf[7] << 8) | fbuf[8];
        data_ready = 1; // 标记数据就绪，供 CO2_Read 读取
    }

    /* 一帧处理结束，重置索引，准备接收下一帧 */
    fidx = 0;
}

/**
 * @brief 初始化函数（占位符）
 */
void CO2_Init(void)
{
    // 若需要串口配置，通常在 main.c 或协议层统一完成
}

/**
 * @brief 用户读取 CO2 数值的接口
 * @param ppm 用于存放读取结果的指针
 * @return 1 表示读取到新值，0 表示暂无新数据
 */
uint8_t CO2_Read(uint16_t *ppm)
{
    if (data_ready) {
        data_ready = 0;   // 读取后清除标志，防止重复处理同一数据
        *ppm = co2_val;   // 返回数值
        return 1;
    }
    *ppm = 0;
    return 0;
}
