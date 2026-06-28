#ifndef __ESP8266_H__
#define __ESP8266_H__

#include "main.h"
#include "sensors.h"

/* Fill these three values before using WIFI:SYNC. */
#define ESP8266_WIFI_SSID    "abciq"
#define ESP8266_WIFI_PASS    "12345678"
#define ESP8266_BAIDU_AK     "WsBxFrBjeH1Pn7iYt3SFevf4dAfx0eNU"
#define ESP8266_USE_SNTP     0
#define ESP8266_DOUBAO_API_KEY "ark-33add3b2-dc31-43d5-afd5-8778a4422cbc-fe71c"
#define ESP8266_DOUBAO_MODEL   "ep-20260521185248-q8r4c"

typedef struct {
    char time[24];
    char location[96];
    char longitude[24];
    char latitude[24];
    char weather_text[32];
    char weather_temp[8];
    char weather_humidity[8];
    char weather_wind[24];
    char ai_advice[96];
    char ai_error[32];
    uint8_t wifi_ok;
    uint8_t time_ok;
    uint8_t location_ok;
    uint8_t weather_ok;
    uint8_t ai_ok;
} Esp8266Info_t;

void Esp8266_Init(void);
void Esp8266_RxCpltCallback(void);
uint8_t Esp8266_TestAT(void);
uint8_t Esp8266_Sync(Esp8266Info_t *info);
uint8_t Esp8266_AnalyzeCrop(const SensorData_t *data, const Esp8266Info_t *info, char *advice, uint16_t advice_size);
uint8_t Esp8266_GetClock(char *buf, uint16_t buf_size);
const Esp8266Info_t *Esp8266_GetLastInfo(void);
const char *Esp8266_GetLastError(void);

#endif
