#include "protocol.h"
#include "actuators.h"
#include "co2.h"
#include "esp8266.h"
#include "voice_module.h"
#include "bluetooth_link.h"
#include "usart.h"
#include <string.h>

/* USART1 命令接收环形缓冲区 */
#define CMD_BUF_SIZE   256
static volatile uint8_t cmd_rbuf[CMD_BUF_SIZE];
static volatile uint16_t cmd_head = 0;
static volatile uint16_t cmd_tail = 0;

/* HAL_UART_Receive_IT 单字节接收缓冲区（USART1 和 USART2） */
static volatile uint8_t rx_byte1;
static volatile uint8_t rx_byte2;

/* 将字节推入环形缓冲区 */
static void CmdBuf_Push(uint8_t byte)
{
    uint16_t next = (cmd_head + 1U) % CMD_BUF_SIZE;

    if (next == cmd_tail) {
        cmd_tail = (cmd_tail + 1U) % CMD_BUF_SIZE;
    }
    cmd_rbuf[cmd_head] = byte;
    cmd_head = next;
}

/* 从环形缓冲区取出一行（以 \n 结尾），返回 1 表示取到完整行 */
static uint8_t CmdBuf_GetLine(char *line, uint16_t maxlen)
{
    uint16_t scan = cmd_tail;
    uint16_t len = 0;
    uint16_t i;
    if (line == NULL || maxlen == 0U) {
        return 0;
    }

    while (scan != cmd_head && len < maxlen - 1U) {
        uint8_t c = cmd_rbuf[scan];
        scan = (scan + 1U) % CMD_BUF_SIZE;
        if (c == '\n') {
            for (i = 0U; i < len; i++) {
                line[i] = (char)cmd_rbuf[cmd_tail];
                cmd_tail = (cmd_tail + 1U) % CMD_BUF_SIZE;
            }
            cmd_tail = (cmd_tail + 1U) % CMD_BUF_SIZE;
            line[len] = '\0';
            /* 去除末尾 \r */
            if (len > 0U && line[len - 1U] == '\r') line[len - 1U] = '\0';
            return 1;
        }
        len++;
    }
    if (len >= maxlen - 1U) {
        cmd_tail = scan;
    }
    line[0] = '\0';
    return 0;
}

/* 手动整数转字符串（不依赖 microlib stdio） */
static int ItoaSmall(char *buf, int val)
{
    char tmp[8];
    int i = 0, j = 0;
    if (val < 0) { buf[j++] = '-'; val = -val; }
    do { tmp[i++] = '0' + (val % 10); val /= 10; } while (val > 0);
    while (i > 0) buf[j++] = tmp[--i];
    return j;
}

static void Uart1_SendString(const char *str)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)str, (uint16_t)strlen(str), 1000);
}

static void Cmd_WifiSync(void)
{
    Esp8266Info_t info;

    Uart1_SendString("WIFI SYNC START\r\n");
    if (Esp8266_Sync(&info)) {
        if (info.wifi_ok) {
            Uart1_SendString("WIFI:OK\r\n");
        } else {
            Uart1_SendString("WIFI:FAIL\r\n");
        }

        if (info.time_ok) {
            Uart1_SendString("TIME:");
            Uart1_SendString(info.time);
            Uart1_SendString("\r\n");
        } else {
            Uart1_SendString("TIME:FAIL\r\n");
            Uart1_SendString("ERR:");
            Uart1_SendString(Esp8266_GetLastError());
            Uart1_SendString("\r\n");
        }

        if (info.location_ok) {
            Uart1_SendString("LOC:");
            Uart1_SendString(info.location);
            if (info.longitude[0] != '\0' && info.latitude[0] != '\0') {
                Uart1_SendString(" LNG:");
                Uart1_SendString(info.longitude);
                Uart1_SendString(" LAT:");
                Uart1_SendString(info.latitude);
            }
            Uart1_SendString("\r\n");
        } else {
            Uart1_SendString("LOC:FAIL\r\n");
        }

        if (info.weather_ok) {
            Uart1_SendString("WEATHER:");
            Uart1_SendString(info.weather_text);
            if (info.weather_temp[0] != '\0') {
                Uart1_SendString(" TEMP:");
                Uart1_SendString(info.weather_temp);
                Uart1_SendString("C");
            }
            if (info.weather_humidity[0] != '\0') {
                Uart1_SendString(" RH:");
                Uart1_SendString(info.weather_humidity);
                Uart1_SendString("%");
            }
            if (info.weather_wind[0] != '\0') {
                Uart1_SendString(" WIND:");
                Uart1_SendString(info.weather_wind);
            }
            Uart1_SendString("\r\n");
        } else {
            Uart1_SendString("WEATHER:FAIL\r\n");
        }
    } else {
        Uart1_SendString("WIFI SYNC FAIL\r\n");
        Uart1_SendString("ERR:");
        Uart1_SendString(Esp8266_GetLastError());
        Uart1_SendString("\r\n");
    }
}

static void Cmd_WifiAt(void)
{
    if (Esp8266_TestAT()) {
        Uart1_SendString("ESP8266:OK\r\n");
    } else {
        Uart1_SendString("ESP8266:FAIL\r\n");
        Uart1_SendString("ERR:");
        Uart1_SendString(Esp8266_GetLastError());
        Uart1_SendString("\r\n");
    }
}

/* 解析并执行命令 */
static void Cmd_Parse(const char *cmd)
{
    if (strncmp(cmd, "PUMP:", 5) == 0) {
        if (cmd[5] == '1') Pump_On();
        else if (cmd[5] == '0') Pump_Off();
    }
    else if (strncmp(cmd, "FAN:", 4) == 0) {
        if (cmd[4] == '1') Fan_On();
        else if (cmd[4] == '0') Fan_Off();
    }
    else if (strncmp(cmd, "PEST:", 5) == 0 || strncmp(cmd, "LAMP:", 5) == 0) {
        if (cmd[5] == '1') PestLamp_On();
        else if (cmd[5] == '0') PestLamp_Off();
    }
    else if (strncmp(cmd, "BUZZ:", 5) == 0) {
        if (cmd[5] >= '0' && cmd[5] <= '2') {
            Buzzer_SetMode((uint8_t)(cmd[5] - '0'));
        }
    }
    else if (strncmp(cmd, "SKY:", 4) == 0) {
        if (cmd[4] >= '0' && cmd[4] <= '2') {
            SkyWindow_SetState((uint8_t)(cmd[4] - '0'));
        }
    }
    else if (strncmp(cmd, "SERVO:", 6) == 0) {
        int angle = 0;
        const char *p = cmd + 6;
        while (*p >= '0' && *p <= '9') { angle = angle * 10 + (*p - '0'); p++; }
        if (angle > 180) angle = 180;
        SkyWindow_SetAngle((uint8_t)angle);
    }
    else if (strcmp(cmd, "ALL") == 0) {
        /* 预留：全体控制 */
    }
    else if (strcmp(cmd, "WIFI:SYNC") == 0) {
        Cmd_WifiSync();
    }
    else if (strcmp(cmd, "WIFI:AT") == 0) {
        Cmd_WifiAt();
    }
}

/* ========== HAL 串口接收完成回调（USART1 和 USART2 共用） ========== */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        /* 回显 */
        /* 推入环形缓冲区 */
        CmdBuf_Push(rx_byte1);
        /* 继续接收下一个字节 */
        HAL_UART_Receive_IT(&huart1, (uint8_t *)&rx_byte1, 1);
    }
    else if (huart->Instance == USART2) {
        /* USART2：CO2 传感器数据 */
        CO2_FeedByte(rx_byte2);
        HAL_UART_Receive_IT(&huart2, (uint8_t *)&rx_byte2, 1);
    }
    else if (huart->Instance == USART3) {
        Esp8266_RxCpltCallback();
    }
    else if (huart->Instance == USART6) {
        VoiceModule_RxCpltCallback();
    }
}

/* ========== printf 重定向到 USART1 ========== */
int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 10);
    return ch;
}

/* ========== 公开函数 ========== */

/* 初始化：启动 USART1 和 USART2 单字节中断接收 */
void Protocol_Init(void)
{
    HAL_UART_Receive_IT(&huart1, (uint8_t *)&rx_byte1, 1);
    HAL_UART_Receive_IT(&huart2, (uint8_t *)&rx_byte2, 1);
}

/* 发送传感器数据 */
void Protocol_SendData(const SensorData_t *data)
{
    char buf[64];
    int pos = 0;
    int16_t temp = data->temperature;
    int16_t hum  = data->humidity;

    /* 温度 T:xx.x */
    buf[pos++] = 'T'; buf[pos++] = ':';
    if (temp < 0) { buf[pos++] = '-'; temp = -temp; }
    pos += ItoaSmall(buf + pos, temp / 10);
    buf[pos++] = '.';
    pos += ItoaSmall(buf + pos, temp % 10);

    /* 湿度 H:xx.x */
    buf[pos++] = ' '; buf[pos++] = 'H'; buf[pos++] = ':';
    if (hum < 0) { buf[pos++] = '-'; hum = -hum; }
    pos += ItoaSmall(buf + pos, hum / 10);
    buf[pos++] = '.';
    pos += ItoaSmall(buf + pos, hum % 10);

    /* CO2/TVOC 值 C:xxxx */
    buf[pos++] = ' '; buf[pos++] = 'C'; buf[pos++] = ':';
    pos += ItoaSmall(buf + pos, (int)data->co2_ppm);

    /* 光敏 L:xx% */
    {
        uint16_t v = data->light_adc;
        int pct;
        if (v <= 100) pct = 100;
        else if (v >= 3800) pct = 0;
        else pct = (int)((3800 - v) * 100 / 3700);
        buf[pos++] = ' '; buf[pos++] = 'L'; buf[pos++] = ':';
        pos += ItoaSmall(buf + pos, pct);
        buf[pos++] = '%';
    }

    /* 土壤湿度 S:xx% */
    {
        uint16_t v = data->soil_adc;
        int pct;
        if (v <= 900) pct = 100;
        else if (v >= 4095) pct = 0;
        else pct = (int)((4095 - v) * 100 / 3195);
        buf[pos++] = ' '; buf[pos++] = 'S'; buf[pos++] = ':';
        pos += ItoaSmall(buf + pos, pct);
        buf[pos++] = '%';
    }
    buf[pos++] = '\r'; buf[pos++] = '\n';

    HAL_UART_Transmit(&huart1, (uint8_t *)buf, pos, 100);
}

/* 主循环调用：处理收到的命令行 */
void Protocol_Process(void)
{
    char line[CMD_BUF_SIZE];
    uint8_t processed = 0U;

    while (processed < 8U && CmdBuf_GetLine(line, sizeof(line))) {
        const char *cmd = line;

        while (*cmd == ' ' || *cmd == '\t') {
            cmd++;
        }
        /* 跳过空行 */
        if (cmd[0] == '\0') continue;
        /* 执行命令 */
        if (cmd[0] == '{') {
            BluetoothLink_ProcessLine(cmd);
        } else {
            Cmd_Parse(cmd);
        }
        processed++;
    }
}
