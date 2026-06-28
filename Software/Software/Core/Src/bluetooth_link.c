#include "bluetooth_link.h"
#include "actuators.h"
#include "voice_module.h"
#include "usart.h"
#include <string.h>

static uint8_t bt_auto_mode = 0U;

static char Bt_ToLower(char c)
{
    if (c >= 'A' && c <= 'Z') {
        return (char)(c - 'A' + 'a');
    }
    return c;
}

static uint8_t Bt_TextEquals(const char *a, const char *b)
{
    if (a == NULL || b == NULL) {
        return 0U;
    }
    while (*a != '\0' && *b != '\0') {
        if (Bt_ToLower(*a) != Bt_ToLower(*b)) {
            return 0U;
        }
        a++;
        b++;
    }
    return (*a == '\0' && *b == '\0') ? 1U : 0U;
}

static int Bt_LightPercent(uint16_t adc)
{
    if (adc <= 100U) return 100;
    if (adc >= 3800U) return 0;
    return (int)((3800U - adc) * 100U / 3700U);
}

static int Bt_SoilPercent(uint16_t adc)
{
    if (adc <= 900U) return 100;
    if (adc >= 4095U) return 0;
    return (int)((4095U - adc) * 100U / 3195U);
}

static void Bt_Send(const char *text)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)text, (uint16_t)strlen(text), 1000);
}

static void Bt_SendInt(int value)
{
    char tmp[12];
    char out[12];
    uint8_t i = 0;
    uint8_t j = 0;
    uint8_t negative = 0;

    if (value < 0) {
        negative = 1U;
        value = -value;
    }

    do {
        tmp[i++] = (char)('0' + (value % 10));
        value /= 10;
    } while (value > 0 && i < sizeof(tmp));

    if (negative) {
        out[j++] = '-';
    }
    while (i > 0U) {
        out[j++] = tmp[--i];
    }
    out[j] = '\0';
    Bt_Send(out);
}

static void Bt_SendJsonString(const char *text)
{
    Bt_Send("\"");
    while (text != NULL && *text != '\0') {
        if (*text == '\"' || *text == '\\') {
            char esc[3];
            esc[0] = '\\';
            esc[1] = *text;
            esc[2] = '\0';
            Bt_Send(esc);
        } else if ((uint8_t)*text >= 32U) {
            char one[2];
            one[0] = *text;
            one[1] = '\0';
            Bt_Send(one);
        }
        text++;
    }
    Bt_Send("\"");
}

static uint8_t Bt_Contains(const char *line, const char *needle)
{
    return (line != NULL && needle != NULL && strstr(line, needle) != NULL) ? 1U : 0U;
}

static const char *Bt_FindJsonValue(const char *line, const char *key)
{
    size_t key_len;
    const char *p;

    if (line == NULL || key == NULL || key[0] == '\0') {
        return NULL;
    }

    key_len = strlen(key);
    p = line;
    while ((p = strstr(p, key)) != NULL) {
        if (p > line && p[-1] == '\"' && p[key_len] == '\"') {
            const char *q = p + key_len + 1U;
            while (*q == ' ' || *q == '\t') {
                q++;
            }
            if (*q == ':') {
                q++;
                while (*q == ' ' || *q == '\t') {
                    q++;
                }
                return q;
            }
        }
        p += key_len;
    }

    return NULL;
}

static uint8_t Bt_ParseIntValue(const char *p, int *out)
{
    int value = 0;
    uint8_t negative = 0U;
    uint8_t has_digit = 0U;

    if (p == NULL || out == NULL) {
        return 0U;
    }

    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (*p == '\"') {
        p++;
    }
    if (*p == '-') {
        negative = 1U;
        p++;
    }
    while (*p >= '0' && *p <= '9') {
        value = value * 10 + (*p - '0');
        has_digit = 1U;
        p++;
    }
    if (!has_digit) {
        return 0U;
    }

    *out = negative ? -value : value;
    return 1U;
}

static uint8_t Bt_ParseJsonInt(const char *line, const char *key, int *out)
{
    return Bt_ParseIntValue(Bt_FindJsonValue(line, key), out);
}

static uint8_t Bt_ReadJsonToken(const char *p, char *out, uint16_t out_size)
{
    uint16_t i = 0U;
    uint8_t quoted = 0U;

    if (p == NULL || out == NULL || out_size == 0U) {
        return 0U;
    }

    while (*p == ' ' || *p == '\t') {
        p++;
    }
    if (*p == '\"') {
        quoted = 1U;
        p++;
    }

    while (*p != '\0') {
        if (quoted) {
            if (*p == '\"') {
                break;
            }
        } else if (*p == ',' || *p == '}' || *p == ']' || *p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') {
            break;
        }

        if (i + 1U < out_size) {
            out[i++] = *p;
        }
        p++;
    }

    out[i] = '\0';
    return (i > 0U) ? 1U : 0U;
}

static uint8_t Bt_ParseJsonString(const char *line, const char *key, char *out, uint16_t out_size)
{
    const char *p = Bt_FindJsonValue(line, key);

    if (p == NULL || *p != '\"') {
        return 0U;
    }
    return Bt_ReadJsonToken(p, out, out_size);
}

static uint8_t Bt_ParseJsonBool(const char *line, const char *key, uint8_t *out)
{
    char token[12];

    if (out == NULL || !Bt_ReadJsonToken(Bt_FindJsonValue(line, key), token, sizeof(token))) {
        return 0U;
    }

    if (Bt_TextEquals(token, "true") || Bt_TextEquals(token, "on") || strcmp(token, "1") == 0) {
        *out = 1U;
        return 1U;
    }
    if (Bt_TextEquals(token, "false") || Bt_TextEquals(token, "off") || strcmp(token, "0") == 0) {
        *out = 0U;
        return 1U;
    }
    return 0U;
}

static int Bt_StateFromToken(const char *device, const char *token)
{
    int value;

    if (Bt_ParseIntValue(token, &value)) {
        return value;
    }

    if (device != NULL && Bt_TextEquals(device, "window")) {
        if (Bt_TextEquals(token, "full")) return 2;
        if (Bt_TextEquals(token, "half")) return 1;
        if (Bt_TextEquals(token, "open")) return 2;
        if (Bt_TextEquals(token, "on") || Bt_TextEquals(token, "true")) return 1;
        return 0;
    }

    if (device != NULL && Bt_TextEquals(device, "buzzer")) {
        if (Bt_TextEquals(token, "locust")) return 1;
        if (Bt_TextEquals(token, "cabbage_worm")) return 2;
        if (Bt_TextEquals(token, "on") || Bt_TextEquals(token, "true")) return 1;
        return 0;
    }

    if (Bt_TextEquals(token, "on") || Bt_TextEquals(token, "true")) return 1;
    return 0;
}

static uint8_t Bt_ParseJsonState(const char *line, const char *key, const char *device, int *state)
{
    char token[20];
    const char *p = Bt_FindJsonValue(line, key);

    if (state == NULL || p == NULL) {
        return 0U;
    }

    if (Bt_ParseIntValue(p, state)) {
        return 1U;
    }

    if (!Bt_ReadJsonToken(p, token, sizeof(token))) {
        return 0U;
    }
    *state = Bt_StateFromToken(device, token);
    return 1U;
}

static const char *Bt_SelectCropAdvice(const SensorData_t *data)
{
    int temp;
    int soil;
    int light;

    if (data == NULL || !data->dht11_ok) {
        return "Waiting for stable sensor data";
    }

    temp = data->temperature / 10;
    soil = Bt_SoilPercent(data->soil_adc);
    light = Bt_LightPercent(data->light_adc);

    if (temp > 32) return "Temperature is high";
    if (temp < 18) return "Temperature is low";
    if (soil < 35) return "Soil is dry";
    if (light < 20) return "Light is weak";
    if (data->co2_ok && data->co2_ppm > 1200U) return "CO2 is high";
    return "Growth is stable";
}

static const char *Bt_SelectControlAdvice(const SensorData_t *data)
{
    int temp;
    int soil;
    int light;

    if (data == NULL || !data->dht11_ok) {
        return "Keep WiFi on and collect all data";
    }

    temp = data->temperature / 10;
    soil = Bt_SoilPercent(data->soil_adc);
    light = Bt_LightPercent(data->light_adc);

    if (soil < 35) return "Irrigation suggested";
    if (temp > 32 || (data->co2_ok && data->co2_ppm > 1200U)) return "Ventilation suggested";
    if (light < 20) return "Supplement light suggested";
    return bt_auto_mode ? "Auto mode active" : "Manual mode active";
}

static void Bt_ApplyControl(const char *device, int state)
{
    if (bt_auto_mode) {
        return;
    }

    if (Bt_TextEquals(device, "pump")) {
        if (state) Pump_On(); else Pump_Off();
    } else if (Bt_TextEquals(device, "fan")) {
        if (state) Fan_On(); else Fan_Off();
    } else if (Bt_TextEquals(device, "buzzer")) {
        if (state > 2) {
            state = 0;
        }
        Buzzer_SetMode((uint8_t)state);
    } else if (Bt_TextEquals(device, "window")) {
        if (state > 2) {
            state = 0;
        }
        SkyWindow_SetState((uint8_t)state);
    } else if (Bt_TextEquals(device, "pestLamp") || Bt_TextEquals(device, "pest_lamp")) {
        if (state) PestLamp_On(); else PestLamp_Off();
    }
}

void BluetoothLink_SendTelemetry(const SensorData_t *data, const Esp8266Info_t *wifi_info)
{
    const Esp8266Info_t *last_info;
    ActuatorState_t actuator_state;

    if (data == NULL) {
        return;
    }

    Actuators_GetState(&actuator_state);
    last_info = Esp8266_GetLastInfo();
    if ((wifi_info == NULL || !wifi_info->wifi_ok) && last_info != NULL) {
        wifi_info = last_info;
    }

    Bt_Send("{\"type\":\"telemetry\"");
    Bt_Send(",\"auto\":");
    Bt_Send(bt_auto_mode ? "true" : "false");

    if (data->dht11_ok) {
        Bt_Send(",\"temperature\":");
        Bt_SendInt(data->temperature / 10);
        Bt_Send(",\"humidity\":");
        Bt_SendInt(data->humidity / 10);
    }
    Bt_Send(",\"light\":");
    Bt_SendInt(Bt_LightPercent(data->light_adc));
    Bt_Send(",\"soil\":");
    Bt_SendInt(Bt_SoilPercent(data->soil_adc));
    if (data->co2_ok) {
        Bt_Send(",\"co2\":");
        Bt_SendInt((int)data->co2_ppm);
    }

    if (wifi_info != NULL && wifi_info->location_ok && wifi_info->location[0] != '\0') {
        Bt_Send(",\"location\":");
        Bt_SendJsonString(wifi_info->location);
    }
    if (wifi_info != NULL && wifi_info->weather_ok && wifi_info->weather_text[0] != '\0') {
        Bt_Send(",\"weather\":");
        Bt_SendJsonString(wifi_info->weather_text);
    }

    Bt_Send(",\"cropAdvice\":");
    Bt_SendJsonString(Bt_SelectCropAdvice(data));
    Bt_Send(",\"controlAdvice\":");
    Bt_SendJsonString(Bt_SelectControlAdvice(data));
    Bt_Send(",\"controls\":{");
    Bt_Send("\"pump\":"); Bt_SendInt(actuator_state.pump_on);
    Bt_Send(",\"fan\":"); Bt_SendInt(actuator_state.fan_on);
    Bt_Send(",\"window\":"); Bt_SendInt(actuator_state.sky_window_state);
    Bt_Send(",\"buzzer\":"); Bt_SendInt(actuator_state.buzzer_mode);
    Bt_Send(",\"pestLamp\":"); Bt_SendInt(actuator_state.pest_lamp_on);
    Bt_Send("}}\n");
}

void BluetoothLink_ProcessLine(const char *line)
{
    char type[16];

    if (line == NULL || line[0] == '\0') {
        return;
    }

    if (!Bt_ParseJsonString(line, "type", type, sizeof(type))) {
        type[0] = '\0';
    }

    if (Bt_TextEquals(type, "mode")) {
        uint8_t auto_mode;
        if (Bt_ParseJsonBool(line, "auto", &auto_mode)) {
            bt_auto_mode = auto_mode ? 1U : 0U;
        }
        return;
    }

    if (Bt_TextEquals(type, "voice") || Bt_TextEquals(type, "broadcast") ||
        Bt_Contains(line, "\"voiceId\"") || Bt_Contains(line, "\"id\"")) {
        int id = 0;
        if (!Bt_ParseJsonInt(line, "voiceId", &id)) {
            (void)Bt_ParseJsonInt(line, "id", &id);
        }
        if (id > 0 && id <= 255) {
            VoiceModule_BroadcastId((uint8_t)id);
        }
        return;
    }

    if (Bt_TextEquals(type, "control")) {
        char device[16];
        int state;
        if (Bt_ParseJsonString(line, "device", device, sizeof(device)) &&
            Bt_ParseJsonState(line, "state", device, &state)) {
            Bt_ApplyControl(device, state);
        }
        return;
    }

    if (Bt_Contains(line, "\"controls\"")) {
        int state;
        if (Bt_ParseJsonState(line, "pump", "pump", &state)) Bt_ApplyControl("pump", state);
        if (Bt_ParseJsonState(line, "fan", "fan", &state)) Bt_ApplyControl("fan", state);
        if (Bt_ParseJsonState(line, "buzzer", "buzzer", &state)) Bt_ApplyControl("buzzer", state);
        if (Bt_ParseJsonState(line, "window", "window", &state)) Bt_ApplyControl("window", state);
        if (Bt_ParseJsonState(line, "pestLamp", "pestLamp", &state)) Bt_ApplyControl("pestLamp", state);
        if (Bt_ParseJsonState(line, "pest_lamp", "pestLamp", &state)) Bt_ApplyControl("pestLamp", state);
    }
}

uint8_t BluetoothLink_IsAutoMode(void)
{
    return bt_auto_mode;
}

void BluetoothLink_SetAutoMode(uint8_t auto_mode)
{
    bt_auto_mode = auto_mode ? 1U : 0U;
}
