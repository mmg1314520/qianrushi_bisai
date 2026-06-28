#include "voice_module.h"
#include "usart.h"
#include <string.h>

#define VOICE_FRAME_HEADER_0       0xAAU
#define VOICE_FRAME_HEADER_1       0x55U
#define VOICE_FRAME_CMD_QUERY      0x03U
#define VOICE_FRAME_CMD_LIGHT      0x00U
#define VOICE_FRAME_BROADCAST      0xFFU
#define VOICE_FRAME_END            0xFBU
#define VOICE_FRAME_LEN            5U

#define VOICE_CMD_TEMP             0x01U
#define VOICE_CMD_WEATHER          0x02U
#define VOICE_CMD_GROWTH           0x03U
#define VOICE_CMD_ACTION           0x04U
#define VOICE_CMD_PEST             0x05U
#define VOICE_CMD_ENV              0x06U

#define VOICE_CMD_BLUE_LIGHT       0x0DU
#define VOICE_CMD_YELLOW_LIGHT     0x0EU
#define VOICE_CMD_FLOW_LIGHT       0x0FU
#define VOICE_CMD_GRADIENT_LIGHT   0x10U

#define VOICE_ID_TEMP_20           0x20U
#define VOICE_ID_WEATHER_SUNNY     0x30U
#define VOICE_ID_WEATHER_CLOUDY    0x31U
#define VOICE_ID_WEATHER_RAINY     0x32U
#define VOICE_ID_WEATHER_OVERCAST  0x33U
#define VOICE_ID_WEATHER_UNKNOWN   0x34U
#define VOICE_ID_GROWTH_GOOD       0x40U
#define VOICE_ID_GROWTH_NORMAL     0x41U
#define VOICE_ID_GROWTH_WEAK       0x42U
#define VOICE_ID_GROWTH_STABLE     0x43U
#define VOICE_ID_GROWTH_RISK       0x44U
#define VOICE_ID_GROWTH_OBSERVE    0x45U
#define VOICE_ID_ADVICE_WATER      0x50U
#define VOICE_ID_ADVICE_FAN        0x51U
#define VOICE_ID_ADVICE_OBSERVE    0x52U
#define VOICE_ID_ADVICE_PEST       0x53U
#define VOICE_ID_ADVICE_COOL       0x54U
#define VOICE_ID_ADVICE_SOIL       0x55U
#define VOICE_ID_ADVICE_LINKAGE    0x56U
#define VOICE_ID_ADVICE_KEEP       0x57U
#define VOICE_ID_ENV_OK            0x60U
#define VOICE_ID_ENV_SOIL_LOW      0x61U
#define VOICE_ID_ENV_SOIL_HIGH     0x62U
#define VOICE_ID_ENV_LIGHT_LOW     0x63U
#define VOICE_ID_ENV_CO2_HIGH      0x64U
#define VOICE_ID_ENV_TEMP_HIGH     0x65U
#define VOICE_ID_ENV_VENT_LOW      0x66U
#define VOICE_ID_ENV_TOMATO_OK     0x67U
#define VOICE_ID_PEST_NONE         0x70U
#define VOICE_ID_PEST_PENDING      0x71U
#define VOICE_ID_DATA_UPDATING     0x97U

#define VOICE_SOIL_LOW_PCT         35
#define VOICE_SOIL_HIGH_PCT        80
#define VOICE_LIGHT_LOW_PCT        30
#define VOICE_CO2_HIGH_PPM         1200U
#define VOICE_TEMP_HIGH_C          32
#define VOICE_REPLY_GAP_MS         650U

static volatile uint8_t voice_rx_byte;
static volatile uint8_t voice_frame_ready;
static volatile uint8_t voice_pending_frame[VOICE_FRAME_LEN];
static uint8_t voice_frame_buf[VOICE_FRAME_LEN];
static uint8_t voice_frame_pos;

static void VoiceModule_DebugFrame(const char *prefix, const uint8_t *frame)
{
    static const char hex[] = "0123456789ABCDEF";
    char buf[48];
    uint8_t pos = 0;
    uint8_t i;

    while (*prefix != '\0' && pos < sizeof(buf) - 1U) {
        buf[pos++] = *prefix++;
    }
    for (i = 0; i < VOICE_FRAME_LEN && pos < sizeof(buf) - 4U; i++) {
        if (i > 0U) {
            buf[pos++] = ' ';
        }
        buf[pos++] = hex[(frame[i] >> 4) & 0x0FU];
        buf[pos++] = hex[frame[i] & 0x0FU];
    }
    buf[pos++] = '\r';
    buf[pos++] = '\n';
    HAL_UART_Transmit(&huart1, (uint8_t *)buf, pos, 100);
}

static int VoiceModule_LightPercent(uint16_t adc)
{
    if (adc <= 100U) return 100;
    if (adc >= 3800U) return 0;
    return (int)((3800U - adc) * 100U / 3700U);
}

static int VoiceModule_SoilPercent(uint16_t adc)
{
    if (adc <= 900U) return 100;
    if (adc >= 4095U) return 0;
    return (int)((4095U - adc) * 100U / 3195U);
}

static uint8_t VoiceModule_MapTemperature(const SensorData_t *data)
{
    int temp_c;

    if (data == NULL || !data->dht11_ok) {
        return VOICE_ID_DATA_UPDATING;
    }

    temp_c = (data->temperature >= 0) ? ((data->temperature + 5) / 10) : (data->temperature / 10);
    if (temp_c < 20 || temp_c > 35) {
        return VOICE_ID_DATA_UPDATING;
    }

    return (uint8_t)(VOICE_ID_TEMP_20 + (uint8_t)(temp_c - 20));
}

static uint8_t VoiceModule_MapWeather(const Esp8266Info_t *wifi_info)
{
    const Esp8266Info_t *last_info;
    const char *weather;

    if (wifi_info == NULL || !wifi_info->weather_ok || wifi_info->weather_text[0] == '\0') {
        last_info = Esp8266_GetLastInfo();
        if (last_info != NULL && last_info->weather_ok && last_info->weather_text[0] != '\0') {
            wifi_info = last_info;
        }
    }

    if (wifi_info == NULL || !wifi_info->weather_ok || wifi_info->weather_text[0] == '\0') {
        return VOICE_ID_WEATHER_UNKNOWN;
    }

    weather = wifi_info->weather_text;
    if (strstr(weather, "Rain") != NULL || strstr(weather, "Thunder") != NULL) {
        return VOICE_ID_WEATHER_RAINY;
    }
    if (strstr(weather, "Cloud") != NULL) {
        return VOICE_ID_WEATHER_CLOUDY;
    }
    if (strstr(weather, "Overcast") != NULL) {
        return VOICE_ID_WEATHER_OVERCAST;
    }
    if (strstr(weather, "Sunny") != NULL) {
        return VOICE_ID_WEATHER_SUNNY;
    }

    return VOICE_ID_WEATHER_UNKNOWN;
}

static void VoiceModule_DebugWeatherState(const Esp8266Info_t *wifi_info)
{
    const Esp8266Info_t *last_info = wifi_info;

    if (last_info == NULL || !last_info->weather_ok || last_info->weather_text[0] == '\0') {
        last_info = Esp8266_GetLastInfo();
    }

    if (last_info != NULL && last_info->weather_ok && last_info->weather_text[0] != '\0') {
        HAL_UART_Transmit(&huart1, (uint8_t *)"VOICE WEATHER:", 14, 100);
        HAL_UART_Transmit(&huart1, (uint8_t *)last_info->weather_text, (uint16_t)strlen(last_info->weather_text), 100);
        HAL_UART_Transmit(&huart1, (uint8_t *)"\r\n", 2, 100);
    } else {
        HAL_UART_Transmit(&huart1, (uint8_t *)"VOICE WEATHER:NOT READY\r\n", 25, 100);
    }
}

static uint8_t VoiceModule_MapEnv(const SensorData_t *data)
{
    int soil_pct;
    int light_pct;
    int temp_c;

    if (data == NULL || !data->dht11_ok) {
        return VOICE_ID_DATA_UPDATING;
    }

    soil_pct = VoiceModule_SoilPercent(data->soil_adc);
    light_pct = VoiceModule_LightPercent(data->light_adc);
    temp_c = data->temperature / 10;

    if (soil_pct < VOICE_SOIL_LOW_PCT) return VOICE_ID_ENV_SOIL_LOW;
    if (soil_pct > VOICE_SOIL_HIGH_PCT) return VOICE_ID_ENV_SOIL_HIGH;
    if (light_pct < VOICE_LIGHT_LOW_PCT) return VOICE_ID_ENV_LIGHT_LOW;
    if (data->co2_ok && data->co2_ppm > VOICE_CO2_HIGH_PPM) return VOICE_ID_ENV_CO2_HIGH;
    if (temp_c > VOICE_TEMP_HIGH_C) return VOICE_ID_ENV_TEMP_HIGH;

    return VOICE_ID_ENV_TOMATO_OK;
}

static uint8_t VoiceModule_MapGrowth(const SensorData_t *data)
{
    uint8_t env_id = VoiceModule_MapEnv(data);

    if (env_id == VOICE_ID_DATA_UPDATING) return VOICE_ID_DATA_UPDATING;
    if (env_id == VOICE_ID_ENV_TOMATO_OK || env_id == VOICE_ID_ENV_OK) return VOICE_ID_GROWTH_GOOD;
    if (env_id == VOICE_ID_ENV_TEMP_HIGH || env_id == VOICE_ID_ENV_CO2_HIGH) return VOICE_ID_GROWTH_RISK;
    if (env_id == VOICE_ID_ENV_LIGHT_LOW || env_id == VOICE_ID_ENV_SOIL_LOW) return VOICE_ID_GROWTH_WEAK;

    return VOICE_ID_GROWTH_OBSERVE;
}

static uint8_t VoiceModule_MapAdvice(const SensorData_t *data)
{
    int soil_pct;
    int temp_c;

    if (data == NULL || !data->dht11_ok) {
        return VOICE_ID_DATA_UPDATING;
    }

    soil_pct = VoiceModule_SoilPercent(data->soil_adc);
    temp_c = data->temperature / 10;

    if (soil_pct < VOICE_SOIL_LOW_PCT) return VOICE_ID_ADVICE_WATER;
    if (temp_c > VOICE_TEMP_HIGH_C) return VOICE_ID_ADVICE_COOL;
    if (data->co2_ok && data->co2_ppm > VOICE_CO2_HIGH_PPM) return VOICE_ID_ADVICE_LINKAGE;
    if (soil_pct > VOICE_SOIL_HIGH_PCT) return VOICE_ID_ADVICE_SOIL;

    return VOICE_ID_ADVICE_OBSERVE;
}

static void VoiceModule_HandleQuery(uint8_t command_id, const SensorData_t *data, const Esp8266Info_t *wifi_info)
{
    uint8_t voice_id = VOICE_ID_DATA_UPDATING;

    switch (command_id) {
    case VOICE_CMD_TEMP:
        voice_id = VoiceModule_MapTemperature(data);
        (void)VoiceModule_BroadcastId(voice_id);
        break;
    case VOICE_CMD_WEATHER:
        VoiceModule_DebugWeatherState(wifi_info);
        voice_id = VoiceModule_MapWeather(wifi_info);
        (void)VoiceModule_BroadcastId(voice_id);
        break;
    case VOICE_CMD_GROWTH:
        voice_id = VoiceModule_MapGrowth(data);
        (void)VoiceModule_BroadcastId(voice_id);
        HAL_Delay(VOICE_REPLY_GAP_MS);
        (void)VoiceModule_BroadcastId(VoiceModule_MapAdvice(data));
        break;
    case VOICE_CMD_ACTION:
        voice_id = VoiceModule_MapAdvice(data);
        (void)VoiceModule_BroadcastId(voice_id);
        break;
    case VOICE_CMD_PEST:
        (void)VoiceModule_BroadcastId(VOICE_ID_PEST_PENDING);
        break;
    case VOICE_CMD_ENV:
        voice_id = VoiceModule_MapEnv(data);
        (void)VoiceModule_BroadcastId(voice_id);
        break;
    default:
        break;
    }
}

static void VoiceModule_HandleLight(uint8_t command_id)
{
    switch (command_id) {
    case VOICE_CMD_BLUE_LIGHT:
        HAL_UART_Transmit(&huart1, (uint8_t *)"VOICE:BLUE LIGHT\r\n", 18, 100);
        break;
    case VOICE_CMD_YELLOW_LIGHT:
        HAL_UART_Transmit(&huart1, (uint8_t *)"VOICE:YELLOW LIGHT\r\n", 20, 100);
        break;
    case VOICE_CMD_FLOW_LIGHT:
        HAL_UART_Transmit(&huart1, (uint8_t *)"VOICE:FLOW LIGHT\r\n", 18, 100);
        break;
    case VOICE_CMD_GRADIENT_LIGHT:
        HAL_UART_Transmit(&huart1, (uint8_t *)"VOICE:GRADIENT LIGHT\r\n", 22, 100);
        break;
    default:
        break;
    }
}

void VoiceModule_Init(void)
{
    voice_frame_pos = 0;
    voice_frame_ready = 0;
    HAL_UART_Receive_IT(&huart6, (uint8_t *)&voice_rx_byte, 1);
}

void VoiceModule_RxCpltCallback(void)
{
    uint8_t ch = voice_rx_byte;

    if (voice_frame_pos == 0U && ch != VOICE_FRAME_HEADER_0) {
        HAL_UART_Receive_IT(&huart6, (uint8_t *)&voice_rx_byte, 1);
        return;
    }

    if (voice_frame_pos == 1U && ch != VOICE_FRAME_HEADER_1) {
        voice_frame_pos = (ch == VOICE_FRAME_HEADER_0) ? 1U : 0U;
        voice_frame_buf[0] = VOICE_FRAME_HEADER_0;
        HAL_UART_Receive_IT(&huart6, (uint8_t *)&voice_rx_byte, 1);
        return;
    }

    voice_frame_buf[voice_frame_pos++] = ch;
    if (voice_frame_pos >= VOICE_FRAME_LEN) {
        if (voice_frame_buf[0] == VOICE_FRAME_HEADER_0 &&
            voice_frame_buf[1] == VOICE_FRAME_HEADER_1 &&
            voice_frame_buf[4] == VOICE_FRAME_END) {
            memcpy((void *)voice_pending_frame, voice_frame_buf, VOICE_FRAME_LEN);
            voice_frame_ready = 1U;
        }
        voice_frame_pos = 0;
    }

    HAL_UART_Receive_IT(&huart6, (uint8_t *)&voice_rx_byte, 1);
}

uint8_t VoiceModule_BroadcastId(uint8_t voice_id)
{
    uint8_t frame[VOICE_FRAME_LEN] = {
        VOICE_FRAME_HEADER_0,
        VOICE_FRAME_HEADER_1,
        VOICE_FRAME_BROADCAST,
        voice_id,
        VOICE_FRAME_END,
    };

    if (HAL_UART_Transmit(&huart6, frame, VOICE_FRAME_LEN, 100) != HAL_OK) {
        return 0;
    }
    VoiceModule_DebugFrame("VOICE TX:", frame);
    return 1;
}

void VoiceModule_Process(const SensorData_t *data, const Esp8266Info_t *wifi_info)
{
    uint8_t frame[VOICE_FRAME_LEN];
    uint32_t primask;

    if (!voice_frame_ready) {
        return;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    memcpy(frame, (const void *)voice_pending_frame, VOICE_FRAME_LEN);
    voice_frame_ready = 0U;
    if (primask == 0U) {
        __enable_irq();
    }

    VoiceModule_DebugFrame("VOICE RX:", frame);
    if (frame[2] == VOICE_FRAME_CMD_QUERY) {
        VoiceModule_HandleQuery(frame[3], data, wifi_info);
    } else if (frame[2] == VOICE_FRAME_CMD_LIGHT) {
        VoiceModule_HandleLight(frame[3]);
    }
}
