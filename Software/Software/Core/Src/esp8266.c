#include "esp8266.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

#define ESP_RX_BUF_SIZE      8192U
#define ESP_CMD_BUF_SIZE     256U
#define ESP_AI_REQ_BUF_SIZE  1024U

static volatile uint8_t esp_rx_byte;
static volatile uint16_t esp_rx_len;
static char esp_rx_buf[ESP_RX_BUF_SIZE];
static const char *esp_last_error = "NONE";
static uint16_t clock_year;
static uint8_t clock_month;
static uint8_t clock_day;
static uint8_t clock_hour;
static uint8_t clock_minute;
static uint8_t clock_second;
static uint32_t clock_sync_tick;
static uint8_t clock_valid;
static Esp8266Info_t esp_last_info;
static uint8_t esp_last_info_valid;

static uint8_t Esp8266_SendCmd(const char *cmd, const char *ok, uint32_t timeout_ms);

static void Esp8266_Debug(const char *msg)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)msg, (uint16_t)strlen(msg), 1000);
}

static void Esp8266_DebugRx(const char *prefix)
{
    uint32_t primask;
    uint16_t len;

    Esp8266_Debug(prefix);
    primask = __get_PRIMASK();
    __disable_irq();
    len = esp_rx_len;
    if (primask == 0U) {
        __enable_irq();
    }
    if (len > 300U) {
        len = 300U;
    }
    if (len > 0U) {
        HAL_UART_Transmit(&huart1, (uint8_t *)esp_rx_buf, len, 1000);
    } else {
        Esp8266_Debug("EMPTY");
    }
    Esp8266_Debug("\r\n");
}

const char *Esp8266_GetLastError(void)
{
    return esp_last_error;
}

const Esp8266Info_t *Esp8266_GetLastInfo(void)
{
    return esp_last_info_valid ? &esp_last_info : NULL;
}

static void Esp8266_SetLastError(const char *error)
{
    esp_last_error = error;
}

static void Esp8266_ClearRx(void)
{
    uint32_t primask = __get_PRIMASK();

    __disable_irq();
    esp_rx_len = 0;
    esp_rx_buf[0] = '\0';
    if (!primask) __enable_irq();
}

void Esp8266_RxCpltCallback(void)
{
    if (esp_rx_len < ESP_RX_BUF_SIZE - 1U) {
        esp_rx_buf[esp_rx_len++] = (char)esp_rx_byte;
        esp_rx_buf[esp_rx_len] = '\0';
    }
    HAL_UART_Receive_IT(&huart3, (uint8_t *)&esp_rx_byte, 1);
}

void Esp8266_Init(void)
{
    Esp8266_ClearRx();
    Esp8266_SetLastError("NONE");
    HAL_UART_Receive_IT(&huart3, (uint8_t *)&esp_rx_byte, 1);
}

uint8_t Esp8266_TestAT(void)
{
    Esp8266_SetLastError("NONE");
    if (!Esp8266_SendCmd("AT\r\n", "OK", 2000)) {
        Esp8266_SetLastError("AT_NO_RESPONSE");
        return 0;
    }
    return 1;
}

static uint8_t Esp8266_WaitFor(const char *target, uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    while ((HAL_GetTick() - start) < timeout_ms) {
        if (strstr((const char *)esp_rx_buf, target) != NULL) {
            return 1;
        }
    }
    return 0;
}

static uint8_t Esp8266_WaitJoinDone(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    uint32_t last_report = start;

    while ((HAL_GetTick() - start) < timeout_ms) {
        if (strstr((const char *)esp_rx_buf, "WIFI GOT IP") != NULL ||
            strstr((const char *)esp_rx_buf, "GOT IP") != NULL ||
            strstr((const char *)esp_rx_buf, "\r\nOK\r\n") != NULL) {
            return 1;
        }

        if (strstr((const char *)esp_rx_buf, "FAIL") != NULL ||
            strstr((const char *)esp_rx_buf, "ERROR") != NULL ||
            strstr((const char *)esp_rx_buf, "NO AP") != NULL ||
            strstr((const char *)esp_rx_buf, "WRONG PASSWORD") != NULL) {
            return 0;
        }

        if ((HAL_GetTick() - last_report) >= 3000U) {
            last_report = HAL_GetTick();
            Esp8266_Debug("ESP:JOIN WAIT\r\n");
        }
    }

    return 0;
}

static uint8_t Esp8266_IsJoined(void)
{
    Esp8266_ClearRx();
    HAL_UART_Transmit(&huart3, (uint8_t *)"AT+CWJAP?\r\n", 11, 1000);
    if (Esp8266_WaitFor("+CWJAP:", 3000)) {
        return (strstr((const char *)esp_rx_buf, ESP8266_WIFI_SSID) != NULL) ? 1U : 0U;
    }
    return 0;
}

static uint8_t Esp8266_SendCmd(const char *cmd, const char *ok, uint32_t timeout_ms)
{
    Esp8266_ClearRx();
    HAL_UART_Transmit(&huart3, (uint8_t *)cmd, (uint16_t)strlen(cmd), 1000);
    return Esp8266_WaitFor(ok, timeout_ms);
}

static void CopyUntil(char *dst, uint16_t dst_size, const char *src, char end_char)
{
    uint16_t i = 0;

    if (dst_size == 0U) return;
    while (src[i] != '\0' && src[i] != end_char && i < dst_size - 1U) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static uint8_t ExtractJsonString(const char *key, char *dst, uint16_t dst_size)
{
    const char *p = strstr((const char *)esp_rx_buf, key);

    if (p == NULL) return 0;
    p = strchr(p, ':');
    if (p == NULL) return 0;
    p++;
    while (*p == ' ') p++;
    if (*p != '\"') return 0;
    p++;
    CopyUntil(dst, dst_size, p, '\"');
    return (dst[0] != '\0') ? 1 : 0;
}

static uint8_t ExtractJsonStringFrom(const char *base, const char *key, char *dst, uint16_t dst_size)
{
    const char *p = strstr(base, key);

    if (p == NULL) return 0;
    p = strchr(p, ':');
    if (p == NULL) return 0;
    p++;
    while (*p == ' ') p++;
    if (*p != '\"') return 0;
    p++;
    CopyUntil(dst, dst_size, p, '\"');
    return (dst[0] != '\0') ? 1 : 0;
}

static uint8_t ExtractJsonValueFrom(const char *base, const char *key, char *dst, uint16_t dst_size)
{
    const char *p = strstr(base, key);
    uint16_t i = 0;

    if (p == NULL || dst_size == 0U) return 0;
    p = strchr(p, ':');
    if (p == NULL) return 0;
    p++;
    while (*p == ' ' || *p == '\"') p++;
    while (*p != '\0' && *p != ',' && *p != '}' && *p != '\"' && i < dst_size - 1U) {
        dst[i++] = *p++;
    }
    dst[i] = '\0';
    return (dst[0] != '\0') ? 1 : 0;
}

static int ParseIntText(const char *text)
{
    int val = 0;
    uint8_t digits = 0;

    if (text == NULL) return -1;
    while (*text == ' ' || *text == '\"') text++;
    while (*text >= '0' && *text <= '9') {
        val = val * 10 + (*text - '0');
        text++;
        digits++;
    }
    return (digits > 0U) ? val : -1;
}

static const char *WeatherCodeToText(int code)
{
    switch (code) {
    case 0:
    case 1:
        return "Sunny";
    case 2:
    case 3:
    case 45:
    case 48:
        return "Cloudy";
    case 51:
    case 53:
    case 55:
    case 56:
    case 57:
    case 61:
    case 63:
    case 65:
    case 66:
    case 67:
    case 80:
    case 81:
    case 82:
        return "Rain";
    case 71:
    case 73:
    case 75:
    case 77:
    case 85:
    case 86:
        return "Overcast";
    case 95:
    case 96:
    case 99:
        return "Thunder";
    default:
        return "Weather";
    }
}

static void AppendText(char *dst, uint16_t dst_size, const char *src)
{
    uint16_t len = (uint16_t)strlen(dst);
    uint16_t i = 0;

    while (src[i] != '\0' && len < dst_size - 1U) {
        dst[len++] = src[i++];
    }
    dst[len] = '\0';
}

static uint8_t AppendChecked(char *dst, uint16_t dst_size, const char *src)
{
    uint16_t len = (uint16_t)strlen(dst);
    uint16_t i = 0;

    while (src[i] != '\0') {
        if (len >= dst_size - 1U) return 0;
        dst[len++] = src[i++];
    }
    dst[len] = '\0';
    return 1;
}

static uint8_t AppendUInt(char *dst, uint16_t dst_size, uint16_t val)
{
    char tmp[6];
    uint8_t i = 0;

    do {
        tmp[i++] = (char)('0' + (val % 10U));
        val /= 10U;
    } while (val > 0U && i < sizeof(tmp));

    while (i > 0U) {
        char one[2];
        one[0] = tmp[--i];
        one[1] = '\0';
        if (!AppendChecked(dst, dst_size, one)) return 0;
    }
    return 1;
}

static uint8_t BuildJoinCmd(char *cmd, uint16_t cmd_size)
{
    cmd[0] = '\0';
    return AppendChecked(cmd, cmd_size, "AT+CWJAP=\"") &&
           AppendChecked(cmd, cmd_size, ESP8266_WIFI_SSID) &&
           AppendChecked(cmd, cmd_size, "\",\"") &&
           AppendChecked(cmd, cmd_size, ESP8266_WIFI_PASS) &&
           AppendChecked(cmd, cmd_size, "\"\r\n");
}

static uint8_t BuildBaiduRequest(char *req, uint16_t req_size)
{
    req[0] = '\0';
    return AppendChecked(req, req_size, "GET /location/ip?ak=") &&
           AppendChecked(req, req_size, ESP8266_BAIDU_AK) &&
           AppendChecked(req, req_size, "&coor=bd09ll HTTP/1.1\r\n") &&
           AppendChecked(req, req_size, "Host: api.map.baidu.com\r\n") &&
           AppendChecked(req, req_size, "Connection: close\r\n\r\n");
}

static uint8_t BuildWeatherRequest(char *req, uint16_t req_size, const Esp8266Info_t *info)
{
    req[0] = '\0';
    return AppendChecked(req, req_size, "GET /v1/forecast?latitude=") &&
           AppendChecked(req, req_size, info->latitude) &&
           AppendChecked(req, req_size, "&longitude=") &&
           AppendChecked(req, req_size, info->longitude) &&
           AppendChecked(req, req_size, "&current=weather_code&timezone=auto") &&
           AppendChecked(req, req_size, " HTTP/1.1\r\n") &&
           AppendChecked(req, req_size, "Host: api.open-meteo.com\r\n") &&
           AppendChecked(req, req_size, "Connection: close\r\n\r\n");
}

static uint8_t IsDoubaoConfigured(void)
{
    return (ESP8266_DOUBAO_API_KEY[0] != '\0' && ESP8266_DOUBAO_MODEL[0] != '\0') ? 1U : 0U;
}

static uint8_t BuildDoubaoBody(char *body, uint16_t body_size, const SensorData_t *data, const Esp8266Info_t *info)
{
    char content[360];
    int temp = 0;
    int hum = 0;

    if (body_size == 0U || data == NULL || !IsDoubaoConfigured()) {
        return 0;
    }

    if (data->dht11_ok) {
        temp = data->temperature / 10;
        hum = data->humidity / 10;
    }

    snprintf(content, sizeof(content),
             "You are an agriculture expert. Judge if sensor data suits crop growth. "
             "Return only one code: OK,SOIL_DRY,TEMP_HIGH,TEMP_LOW,LIGHT_LOW,CO2_HIGH,DATA_BAD. "
             "temp=%dC,hum=%d%%,co2=%uppm,light_adc=%u,soil_adc=%u,weather=%s.",
             temp, hum, data->co2_ok ? data->co2_ppm : 0U, data->light_adc, data->soil_adc,
             (info != NULL && info->weather_ok) ? info->weather_text : "unknown");

    snprintf(body, body_size,
             "{\"model\":\"%s\",\"messages\":[{\"role\":\"system\",\"content\":\"Return one code only.\"},"
             "{\"role\":\"user\",\"content\":\"%s\"}],\"temperature\":0,\"max_tokens\":8}",
             ESP8266_DOUBAO_MODEL, content);
    return (strlen(body) < body_size - 1U) ? 1U : 0U;
}

static uint8_t BuildDoubaoRequest(char *req, uint16_t req_size, const SensorData_t *data, const Esp8266Info_t *info)
{
    char body[720];
    uint16_t body_len;

    if (req_size == 0U || data == NULL || !BuildDoubaoBody(body, sizeof(body), data, info)) {
        return 0;
    }
    body_len = (uint16_t)strlen(body);

    req[0] = '\0';
    return AppendChecked(req, req_size, "POST /api/v3/chat/completions HTTP/1.1\r\n") &&
           AppendChecked(req, req_size, "Host: ark.cn-beijing.volces.com\r\n") &&
           AppendChecked(req, req_size, "Authorization: Bearer ") &&
           AppendChecked(req, req_size, ESP8266_DOUBAO_API_KEY) &&
           AppendChecked(req, req_size, "\r\nContent-Type: application/json\r\nConnection: close\r\nContent-Length: ") &&
           AppendUInt(req, req_size, body_len) &&
           AppendChecked(req, req_size, "\r\n\r\n") &&
           AppendChecked(req, req_size, body);
}

static const char *DoubaoCodeToAdvice(const char *resp)
{
    if (resp == NULL) return "DOUBAO_NO_RESPONSE";
    if (strstr(resp, "SOIL_DRY") != NULL) return "SOIL_DRY";
    if (strstr(resp, "TEMP_HIGH") != NULL) return "TEMP_HIGH";
    if (strstr(resp, "TEMP_LOW") != NULL) return "TEMP_LOW";
    if (strstr(resp, "LIGHT_LOW") != NULL) return "LIGHT_LOW";
    if (strstr(resp, "CO2_HIGH") != NULL) return "CO2_HIGH";
    if (strstr(resp, "OK") != NULL) return "OK";
    return "DATA_BAD";
}

static void CopyDoubaoAdviceText(char *advice, uint16_t advice_size, const char *code)
{
    const char *text = "DATA_BAD";

    if (advice_size == 0U) return;
    if (strcmp(code, "OK") == 0) {
        text = "DOUBAO_OK";
    } else if (strcmp(code, "SOIL_DRY") == 0) {
        text = "DOUBAO_SOIL_DRY";
    } else if (strcmp(code, "TEMP_HIGH") == 0) {
        text = "DOUBAO_TEMP_HIGH";
    } else if (strcmp(code, "TEMP_LOW") == 0) {
        text = "DOUBAO_TEMP_LOW";
    } else if (strcmp(code, "LIGHT_LOW") == 0) {
        text = "DOUBAO_LIGHT_LOW";
    } else if (strcmp(code, "CO2_HIGH") == 0) {
        text = "DOUBAO_CO2_HIGH";
    }
    strncpy(advice, text, advice_size - 1U);
    advice[advice_size - 1U] = '\0';
}

static uint8_t Esp8266_AnalyzeCropHttpClient(const SensorData_t *data, const Esp8266Info_t *info, char *advice, uint16_t advice_size)
{
    char body[720];
    char cmd[360];
    uint16_t body_len;
    const char *code;

    if (!BuildDoubaoBody(body, sizeof(body), data, info)) {
        Esp8266_SetLastError("DOUBAO_REQ_TOO_LONG");
        return 0;
    }
    body_len = (uint16_t)strlen(body);

    Esp8266_Debug("ESP:DOUBAO HTTPCLIENT\r\n");
    cmd[0] = '\0';
    if (!AppendChecked(cmd, sizeof(cmd), "AT+HTTPCPOST=\"https://ark.cn-beijing.volces.com/api/v3/chat/completions\",") ||
        !AppendUInt(cmd, sizeof(cmd), body_len) ||
        !AppendChecked(cmd, sizeof(cmd), ",2,\"Content-Type: application/json\",\"Authorization: Bearer ") ||
        !AppendChecked(cmd, sizeof(cmd), ESP8266_DOUBAO_API_KEY) ||
        !AppendChecked(cmd, sizeof(cmd), "\"\r\n")) {
        Esp8266_SetLastError("DOUBAO_HTTP_CMD_TOO_LONG");
        return 0;
    }

    Esp8266_ClearRx();
    HAL_UART_Transmit(&huart3, (uint8_t *)cmd, (uint16_t)strlen(cmd), 1000);
    if (!Esp8266_WaitFor(">", 5000)) {
        Esp8266_DebugRx("ESP:DOUBAO HTTP PROMPT:");
        Esp8266_SetLastError("DOUBAO_HTTP_PROMPT_FAIL");
        return 0;
    }

    Esp8266_ClearRx();
    HAL_UART_Transmit(&huart3, (uint8_t *)body, body_len, 5000);
    if (!Esp8266_WaitFor("\"choices\"", 45000)) {
        Esp8266_DebugRx("ESP:DOUBAO HTTP RESP:");
        Esp8266_SetLastError("DOUBAO_HTTP_RESPONSE_FAIL");
        return 0;
    }

    code = DoubaoCodeToAdvice((const char *)esp_rx_buf);
    CopyDoubaoAdviceText(advice, advice_size, code);
    return 1;
}

static uint8_t BuildCipsendCmd(char *cmd, uint16_t cmd_size, uint16_t len)
{
    cmd[0] = '\0';
    return AppendChecked(cmd, cmd_size, "AT+CIPSEND=") &&
           AppendUInt(cmd, cmd_size, len) &&
           AppendChecked(cmd, cmd_size, "\r\n");
}

static uint8_t Esp8266_StartConnection(const char *type, const char *host, const char *port, uint32_t timeout_ms)
{
    char cmd[96];

    cmd[0] = '\0';
    if (!AppendChecked(cmd, sizeof(cmd), "AT+CIPSTART=\"") ||
        !AppendChecked(cmd, sizeof(cmd), type) ||
        !AppendChecked(cmd, sizeof(cmd), "\",\"") ||
        !AppendChecked(cmd, sizeof(cmd), host) ||
        !AppendChecked(cmd, sizeof(cmd), "\",") ||
        !AppendChecked(cmd, sizeof(cmd), port) ||
        !AppendChecked(cmd, sizeof(cmd), "\r\n")) {
        return 0;
    }

    return Esp8266_SendCmd(cmd, "CONNECT", timeout_ms);
}

static void Esp8266_SetSslSni(const char *host)
{
    char cmd[96];

    if (host == NULL || host[0] == '\0') {
        return;
    }

    cmd[0] = '\0';
    if (!AppendChecked(cmd, sizeof(cmd), "AT+CIPSSLCSNI=\"") ||
        !AppendChecked(cmd, sizeof(cmd), host) ||
        !AppendChecked(cmd, sizeof(cmd), "\"\r\n")) {
        return;
    }

    (void)Esp8266_SendCmd(cmd, "OK", 2000);
}

static void Esp8266_ConfigSslClient(const char *host)
{
    (void)Esp8266_SendCmd("AT+SYSLOG=1\r\n", "OK", 1000);
    (void)Esp8266_SendCmd("AT+CIPSSLSIZE=4096\r\n", "OK", 2000);
    (void)Esp8266_SendCmd("AT+CIPSSLCCONF=0\r\n", "OK", 2000);
    Esp8266_SetSslSni(host);
}
static uint8_t Parse2Digits(const char *p)
{
    if (p[0] < '0' || p[0] > '9' || p[1] < '0' || p[1] > '9') return 255U;
    return (uint8_t)((p[0] - '0') * 10 + (p[1] - '0'));
}

static uint16_t Parse4Digits(const char *p)
{
    uint16_t val = 0;
    uint8_t i;

    for (i = 0; i < 4U; i++) {
        if (p[i] < '0' || p[i] > '9') return 0U;
        val = (uint16_t)(val * 10U + (uint16_t)(p[i] - '0'));
    }
    return val;
}

static uint8_t MonthFromText(const char *p)
{
    static const char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
    uint8_t i;

    for (i = 0; i < 12U; i++) {
        if (strncmp(p, &months[i * 3U], 3U) == 0) return (uint8_t)(i + 1U);
    }
    return 0U;
}

static uint8_t IsLeapYear(uint16_t year)
{
    return ((year % 4U == 0U && year % 100U != 0U) || (year % 400U == 0U)) ? 1U : 0U;
}

static uint8_t DaysInMonth(uint16_t year, uint8_t month)
{
    static const uint8_t days[] = {31U,28U,31U,30U,31U,30U,31U,31U,30U,31U,30U,31U};

    if (month == 0U || month > 12U) return 31U;
    if (month == 2U && IsLeapYear(year)) return 29U;
    return days[month - 1U];
}

static void Put2(char *dst, uint8_t *pos, uint8_t val)
{
    dst[(*pos)++] = (char)('0' + (val / 10U));
    dst[(*pos)++] = (char)('0' + (val % 10U));
}

static void Put4(char *dst, uint8_t *pos, uint16_t val)
{
    dst[(*pos)++] = (char)('0' + (val / 1000U) % 10U);
    dst[(*pos)++] = (char)('0' + (val / 100U) % 10U);
    dst[(*pos)++] = (char)('0' + (val / 10U) % 10U);
    dst[(*pos)++] = (char)('0' + val % 10U);
}

static void Clock_AddOneSecond(uint16_t *year, uint8_t *month, uint8_t *day,
                               uint8_t *hour, uint8_t *minute, uint8_t *second)
{
    (*second)++;
    if (*second < 60U) return;
    *second = 0U;
    (*minute)++;
    if (*minute < 60U) return;
    *minute = 0U;
    (*hour)++;
    if (*hour < 24U) return;
    *hour = 0U;
    (*day)++;
    if (*day <= DaysInMonth(*year, *month)) return;
    *day = 1U;
    (*month)++;
    if (*month <= 12U) return;
    *month = 1U;
    (*year)++;
}

static void Clock_Format(char *buf, uint16_t buf_size, uint16_t year, uint8_t month, uint8_t day,
                         uint8_t hour, uint8_t minute, uint8_t second)
{
    uint8_t pos = 0;

    if (buf_size < 20U) {
        if (buf_size > 0U) buf[0] = '\0';
        return;
    }
    Put4(buf, &pos, year);
    buf[pos++] = '-';
    Put2(buf, &pos, month);
    buf[pos++] = '-';
    Put2(buf, &pos, day);
    buf[pos++] = ' ';
    Put2(buf, &pos, hour);
    buf[pos++] = ':';
    Put2(buf, &pos, minute);
    buf[pos++] = ':';
    Put2(buf, &pos, second);
    buf[pos] = '\0';
}

static uint8_t Clock_SetFromText(const char *time_text)
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;

    if (time_text == NULL || strlen(time_text) < 19U) return 0;
    year = Parse4Digits(time_text);
    month = Parse2Digits(time_text + 5);
    day = Parse2Digits(time_text + 8);
    hour = Parse2Digits(time_text + 11);
    minute = Parse2Digits(time_text + 14);
    second = Parse2Digits(time_text + 17);
    if (year == 0U || month == 0U || month > 12U || day == 0U ||
        day > DaysInMonth(year, month) || hour > 23U || minute > 59U || second > 59U) {
        return 0;
    }

    clock_year = year;
    clock_month = month;
    clock_day = day;
    clock_hour = hour;
    clock_minute = minute;
    clock_second = second;
    clock_sync_tick = HAL_GetTick();
    clock_valid = 1U;
    return 1;
}

#if ESP8266_USE_SNTP
static uint8_t Parse1Or2Digits(const char **pp, uint8_t *val)
{
    const char *p = *pp;
    uint8_t digits = 0;
    uint8_t parsed = 0;

    while (digits < 2U && *p >= '0' && *p <= '9') {
        parsed = (uint8_t)(parsed * 10U + (uint8_t)(*p - '0'));
        p++;
        digits++;
    }
    if (digits == 0U) return 0;

    *pp = p;
    *val = parsed;
    return 1;
}

static uint8_t Esp8266_ParseSntpTimeText(const char *time_text, char *dst, uint16_t dst_size)
{
    const char *p = time_text;
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;

    if (time_text == NULL) return 0;

    while (*p == ' ') p++;
    if (p[0] != '\0' && p[1] != '\0' && p[2] != '\0' && p[3] == ' ') {
        p += 4;
    }

    while (*p == ' ') p++;
    month = MonthFromText(p);
    if (month == 0U) return 0;
    p += 3;

    while (*p == ' ') p++;
    if (!Parse1Or2Digits(&p, &day)) return 0;

    while (*p == ' ') p++;
    hour = Parse2Digits(p);
    if (hour == 255U) return 0;
    p += 2;
    if (*p != ':') return 0;
    p++;

    minute = Parse2Digits(p);
    if (minute == 255U) return 0;
    p += 2;
    if (*p != ':') return 0;
    p++;

    second = Parse2Digits(p);
    if (second == 255U) return 0;
    p += 2;

    while (*p == ' ') p++;
    year = Parse4Digits(p);
    if (year == 0U || month > 12U || day == 0U || day > DaysInMonth(year, month) ||
        hour > 23U || minute > 59U || second > 59U) {
        return 0;
    }

    Clock_Format(dst, dst_size, year, month, day, hour, minute, second);
    return 1;
}
#endif

uint8_t Esp8266_GetClock(char *buf, uint16_t buf_size)
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint32_t elapsed;

    if (!clock_valid) {
        if (buf_size > 0U) buf[0] = '\0';
        return 0;
    }

    year = clock_year;
    month = clock_month;
    day = clock_day;
    hour = clock_hour;
    minute = clock_minute;
    second = clock_second;
    elapsed = (HAL_GetTick() - clock_sync_tick) / 1000U;
    while (elapsed-- > 0U) {
        Clock_AddOneSecond(&year, &month, &day, &hour, &minute, &second);
    }

    Clock_Format(buf, buf_size, year, month, day, hour, minute, second);
    return 1;
}

static uint8_t Esp8266_ParseHttpDate(Esp8266Info_t *info)
{
    const char *p = strstr((const char *)esp_rx_buf, "Date:");
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;

    if (p == NULL) return 0;
    p = strchr(p, ',');
    if (p == NULL) return 0;
    p++;
    while (*p == ' ') p++;

    day = Parse2Digits(p);
    month = MonthFromText(p + 3);
    year = Parse4Digits(p + 7);
    hour = Parse2Digits(p + 12);
    minute = Parse2Digits(p + 15);
    second = Parse2Digits(p + 18);
    if (day == 255U || month == 0U || year == 0U || hour == 255U || minute == 255U || second == 255U) {
        return 0;
    }

    hour = (uint8_t)(hour + 8U);
    if (hour >= 24U) {
        hour = (uint8_t)(hour - 24U);
        day++;
        if (day > DaysInMonth(year, month)) {
            day = 1U;
            month++;
            if (month > 12U) {
                month = 1U;
                year++;
            }
        }
    }

    Clock_Format(info->time, sizeof(info->time), year, month, day, hour, minute, second);
    return 1;
}


static uint8_t Esp8266_ParseLocation(Esp8266Info_t *info)
{
    char province[32] = {0};
    char city[32] = {0};
    char district[32] = {0};
    uint8_t ok = 0;

    info->location[0] = '\0';
    if (ExtractJsonString("\"province\"", province, sizeof(province))) {
        AppendText(info->location, sizeof(info->location), province);
        ok = 1;
    }
    if (ExtractJsonString("\"city\"", city, sizeof(city))) {
        if (strcmp(city, province) != 0) {
            AppendText(info->location, sizeof(info->location), city);
        }
        ok = 1;
    }
    if (ExtractJsonString("\"district\"", district, sizeof(district))) {
        AppendText(info->location, sizeof(info->location), district);
        ok = 1;
    }
    ExtractJsonString("\"x\"", info->longitude, sizeof(info->longitude));
    ExtractJsonString("\"y\"", info->latitude, sizeof(info->latitude));
    return ok;
}

static uint8_t Esp8266_ParseWeather(Esp8266Info_t *info)
{
    const char *now = strstr((const char *)esp_rx_buf, "\"now\"");
    const char *current = strstr((const char *)esp_rx_buf, "\"current\"");
    char code_text[8] = {0};
    uint8_t ok = 0;

    if (now == NULL) now = (current != NULL) ? current : (const char *)esp_rx_buf;
    info->weather_text[0] = '\0';
    info->weather_temp[0] = '\0';
    info->weather_humidity[0] = '\0';
    info->weather_wind[0] = '\0';

    if (ExtractJsonValueFrom(now, "\"weather_code\"", code_text, sizeof(code_text))) {
        const char *text = WeatherCodeToText(ParseIntText(code_text));
        CopyUntil(info->weather_text, sizeof(info->weather_text), text, '\0');
        ok = 1;
    }
    if (ExtractJsonStringFrom(now, "\"text\"", info->weather_text, sizeof(info->weather_text))) {
        ok = 1;
    }
    if (ExtractJsonValueFrom(now, "\"temp\"", info->weather_temp, sizeof(info->weather_temp))) {
        ok = 1;
    }
    if (ExtractJsonValueFrom(now, "\"rh\"", info->weather_humidity, sizeof(info->weather_humidity))) {
        ok = 1;
    }
    if (ExtractJsonStringFrom(now, "\"wind_dir\"", info->weather_wind, sizeof(info->weather_wind))) {
        char wind_class[12] = {0};
        if (ExtractJsonStringFrom(now, "\"wind_class\"", wind_class, sizeof(wind_class))) {
            AppendText(info->weather_wind, sizeof(info->weather_wind), " ");
            AppendText(info->weather_wind, sizeof(info->weather_wind), wind_class);
        }
        ok = 1;
    }
    return ok;
}

#if ESP8266_USE_SNTP
static uint8_t Esp8266_GetTime(Esp8266Info_t *info)
{
    const char *p;
    char raw_time[32];
    uint8_t retry;

    Esp8266_Debug("ESP:SNTP CFG\r\n");
    if (!Esp8266_SendCmd("AT+CIPSNTPCFG=1,8,\"ntp.aliyun.com\",\"cn.ntp.org.cn\"\r\n", "OK", 3000)) {
        Esp8266_SetLastError("SNTP_CFG_FAIL");
        return 0;
    }

    Esp8266_Debug("ESP:SNTP TIME\r\n");
    for (retry = 0; retry < 5U; retry++) {
        Esp8266_ClearRx();
        HAL_UART_Transmit(&huart3, (uint8_t *)"AT+CIPSNTPTIME?\r\n", 17, 1000);
        if (!Esp8266_WaitFor("+CIPSNTPTIME:", 5000)) {
            Esp8266_SetLastError("SNTP_TIME_FAIL");
            return 0;
        }

        p = strstr((const char *)esp_rx_buf, "+CIPSNTPTIME:");
        if (p == NULL) return 0;
        p += strlen("+CIPSNTPTIME:");
        while (*p == ' ') p++;
        CopyUntil(raw_time, sizeof(raw_time), p, '\r');
        if (raw_time[0] != '\0' && strstr(raw_time, "1970") == NULL &&
            Esp8266_ParseSntpTimeText(raw_time, info->time, sizeof(info->time))) {
            return 1;
        }
        HAL_Delay(1000);
    }

    Esp8266_SetLastError("SNTP_1970");
    return 0;
}
#endif

static uint8_t Esp8266_GetBaiduLocation(Esp8266Info_t *info)
{
    char req[ESP_CMD_BUF_SIZE];
    char send_cmd[32];
    uint16_t req_len;

    Esp8266_Debug("ESP:LOC CONNECT\r\n");
    Esp8266_SendCmd("AT+CIPCLOSE\r\n", "OK", 1000);

    if (!Esp8266_StartConnection("SSL", "api.map.baidu.com", "443", 15000)) {
        Esp8266_DebugRx("ESP:LOC SSL RX:");
        Esp8266_SendCmd("AT+CIPCLOSE\r\n", "OK", 1000);
        if (!Esp8266_StartConnection("TCP", "api.map.baidu.com", "80", 12000)) {
            Esp8266_DebugRx("ESP:LOC TCP RX:");
            Esp8266_SetLastError("BAIDU_CONNECT_FAIL");
            return 0;
        }
    }

    if (!BuildBaiduRequest(req, sizeof(req))) {
        Esp8266_SetLastError("BAIDU_REQ_TOO_LONG");
        return 0;
    }
    req_len = (uint16_t)strlen(req);

    if (!BuildCipsendCmd(send_cmd, sizeof(send_cmd), req_len)) {
        Esp8266_SetLastError("CIPSEND_CMD_FAIL");
        return 0;
    }
    Esp8266_Debug("ESP:LOC SEND\r\n");
    Esp8266_ClearRx();
    HAL_UART_Transmit(&huart3, (uint8_t *)send_cmd, (uint16_t)strlen(send_cmd), 1000);
    if (!Esp8266_WaitFor(">", 3000)) {
        Esp8266_SetLastError("CIPSEND_PROMPT_FAIL");
        return 0;
    }

    Esp8266_ClearRx();
    HAL_UART_Transmit(&huart3, (uint8_t *)req, (uint16_t)req_len, 3000);
    if (!Esp8266_WaitFor("\"address_detail\"", 8000)) {
        Esp8266_DebugRx("ESP:LOC RESP:");
        Esp8266_SetLastError("BAIDU_RESPONSE_FAIL");
        return 0;
    }
    Esp8266_WaitFor("CLOSED", 3000);
    if (Esp8266_ParseHttpDate(info)) {
        info->time_ok = 1U;
    }

    if (!Esp8266_ParseLocation(info)) {
        Esp8266_DebugRx("ESP:LOC PARSE:");
        return 0;
    }
    return 1;
}

static uint8_t Esp8266_GetBaiduWeather(Esp8266Info_t *info)
{
    char req[ESP_CMD_BUF_SIZE];
    char send_cmd[32];
    uint16_t req_len;

    if (info->longitude[0] == '\0' || info->latitude[0] == '\0') {
        Esp8266_SetLastError("WEATHER_NO_LOCATION");
        return 0;
    }

    Esp8266_Debug("ESP:WEATHER CONNECT\r\n");
    Esp8266_SendCmd("AT+CIPCLOSE\r\n", "OK", 1000);

    if (!Esp8266_StartConnection("TCP", "api.open-meteo.com", "80", 12000)) {
        Esp8266_DebugRx("ESP:WEATHER RX:");
        Esp8266_SetLastError("WEATHER_CONNECT_FAIL");
        return 0;
    }

    if (!BuildWeatherRequest(req, sizeof(req), info)) {
        Esp8266_SetLastError("WEATHER_REQ_TOO_LONG");
        return 0;
    }
    req_len = (uint16_t)strlen(req);

    if (!BuildCipsendCmd(send_cmd, sizeof(send_cmd), req_len)) {
        Esp8266_SetLastError("WEATHER_CIPSEND_FAIL");
        return 0;
    }
    Esp8266_Debug("ESP:WEATHER SEND\r\n");
    Esp8266_ClearRx();
    HAL_UART_Transmit(&huart3, (uint8_t *)send_cmd, (uint16_t)strlen(send_cmd), 1000);
    if (!Esp8266_WaitFor(">", 3000)) {
        Esp8266_SetLastError("WEATHER_PROMPT_FAIL");
        return 0;
    }

    Esp8266_ClearRx();
    HAL_UART_Transmit(&huart3, (uint8_t *)req, req_len, 3000);
    if (!Esp8266_WaitFor("\"weather_code\"", 8000)) {
        Esp8266_DebugRx("ESP:WEATHER RESP:");
        Esp8266_SetLastError("WEATHER_RESPONSE_FAIL");
        return 0;
    }
    Esp8266_WaitFor("CLOSED", 3000);

    if (!Esp8266_ParseWeather(info)) {
        Esp8266_DebugRx("ESP:WEATHER PARSE:");
        return 0;
    }
    return 1;
}

uint8_t Esp8266_AnalyzeCrop(const SensorData_t *data, const Esp8266Info_t *info, char *advice, uint16_t advice_size)
{
    char req[ESP_AI_REQ_BUF_SIZE];
    char send_cmd[32];
    uint16_t req_len;
    const char *code;

    if (advice != NULL && advice_size > 0U) {
        advice[0] = '\0';
    }
    if (data == NULL || advice == NULL || advice_size == 0U) {
        return 0;
    }
    if (!IsDoubaoConfigured()) {
        Esp8266_SetLastError("DOUBAO_NOT_CONFIGURED");
        return 0;
    }

    Esp8266_Debug("ESP:DOUBAO CONNECT\r\n");
    Esp8266_SendCmd("AT+CIPCLOSE\r\n", "OK", 1000);
    Esp8266_ConfigSslClient("ark.cn-beijing.volces.com");
    if (!Esp8266_StartConnection("SSL", "ark.cn-beijing.volces.com", "443", 30000)) {
        Esp8266_DebugRx("ESP:DOUBAO SSL RX:");
        if (Esp8266_AnalyzeCropHttpClient(data, info, advice, advice_size)) {
            return 1;
        }
        if (strcmp(Esp8266_GetLastError(), "DOUBAO_HTTP_RESPONSE_FAIL") != 0 &&
            strcmp(Esp8266_GetLastError(), "DOUBAO_HTTP_PROMPT_FAIL") != 0) {
            Esp8266_SetLastError("DOUBAO_CONNECT_FAIL");
        }
        return 0;
    }

    if (!BuildDoubaoRequest(req, sizeof(req), data, info)) {
        Esp8266_SetLastError("DOUBAO_REQ_TOO_LONG");
        return 0;
    }
    req_len = (uint16_t)strlen(req);
    if (!BuildCipsendCmd(send_cmd, sizeof(send_cmd), req_len)) {
        Esp8266_SetLastError("DOUBAO_CIPSEND_FAIL");
        return 0;
    }

    Esp8266_Debug("ESP:DOUBAO SEND\r\n");
    Esp8266_ClearRx();
    HAL_UART_Transmit(&huart3, (uint8_t *)send_cmd, (uint16_t)strlen(send_cmd), 1000);
    if (!Esp8266_WaitFor(">", 3000)) {
        Esp8266_SetLastError("DOUBAO_PROMPT_FAIL");
        return 0;
    }

    Esp8266_ClearRx();
    HAL_UART_Transmit(&huart3, (uint8_t *)req, req_len, 5000);
    if (!Esp8266_WaitFor("\"choices\"", 45000)) {
        Esp8266_DebugRx("ESP:DOUBAO RESP:");
        Esp8266_SetLastError("DOUBAO_RESPONSE_FAIL");
        return 0;
    }
    Esp8266_WaitFor("CLOSED", 5000);

    code = DoubaoCodeToAdvice((const char *)esp_rx_buf);
    CopyDoubaoAdviceText(advice, advice_size, code);
    return 1;
}

uint8_t Esp8266_Sync(Esp8266Info_t *info)
{
    char cmd[ESP_CMD_BUF_SIZE];

    if (info == NULL) return 0;
    memset(info, 0, sizeof(*info));
    Esp8266_SetLastError("NONE");

    Esp8266_Debug("ESP:AT\r\n");
    if (!Esp8266_SendCmd("AT\r\n", "OK", 2000)) {
        Esp8266_SetLastError("AT_NO_RESPONSE");
        return 0;
    }
    Esp8266_SendCmd("ATE0\r\n", "OK", 2000);
    Esp8266_Debug("ESP:CWMODE\r\n");
    if (!Esp8266_SendCmd("AT+CWMODE=1\r\n", "OK", 3000)) {
        Esp8266_SetLastError("CWMODE_FAIL");
        return 0;
    }
    Esp8266_Debug("ESP:CIPMODE\r\n");
    Esp8266_SendCmd("AT+CIPMODE=0\r\n", "OK", 2000);
    Esp8266_Debug("ESP:CIPMUX\r\n");
    Esp8266_SendCmd("AT+CIPMUX=0\r\n", "OK", 2000);
    Esp8266_Debug("ESP:DNS\r\n");
    Esp8266_SendCmd("AT+CIPDNS_CUR=1,\"223.5.5.5\",\"114.114.114.114\"\r\n", "OK", 2000);

    if (!BuildJoinCmd(cmd, sizeof(cmd))) {
        Esp8266_SetLastError("CWJAP_CMD_TOO_LONG");
        return 0;
    }
    Esp8266_Debug("ESP:CHECK AP\r\n");
    info->wifi_ok = Esp8266_IsJoined();
    if (!info->wifi_ok) {
        Esp8266_Debug("ESP:JOIN\r\n");
        Esp8266_ClearRx();
        HAL_UART_Transmit(&huart3, (uint8_t *)cmd, (uint16_t)strlen(cmd), 1000);
        info->wifi_ok = Esp8266_WaitJoinDone(15000);
        if (!info->wifi_ok) {
            Esp8266_Debug("ESP:JOIN QUERY\r\n");
            info->wifi_ok = Esp8266_IsJoined();
        }
    }
    if (!info->wifi_ok) {
        Esp8266_SetLastError("WIFI_JOIN_FAIL");
        return 0;
    }
    Esp8266_Debug("ESP:WIFI OK\r\n");
    Esp8266_Debug("ESP:DNS SET\r\n");
    Esp8266_SendCmd("AT+CIPDNS_CUR=1,\"223.5.5.5\",\"114.114.114.114\"\r\n", "OK", 2000);

    info->location_ok = Esp8266_GetBaiduLocation(info);
    Esp8266_Debug(info->location_ok ? "ESP:LOC OK\r\n" : "ESP:LOC FAIL\r\n");

#if ESP8266_USE_SNTP
    if (!info->time_ok) {
        info->time_ok = Esp8266_GetTime(info);
        Esp8266_Debug(info->time_ok ? "ESP:TIME OK\r\n" : "ESP:TIME FAIL\r\n");
    }
#else
    if (!info->time_ok) {
        Esp8266_Debug("ESP:TIME HTTP FAIL\r\n");
    } else {
        Esp8266_Debug("ESP:TIME HTTP OK\r\n");
    }
#endif

    if (info->time_ok) {
        Clock_SetFromText(info->time);
    }
    if (info->location_ok) {
        info->weather_ok = Esp8266_GetBaiduWeather(info);
        Esp8266_Debug(info->weather_ok ? "ESP:WEATHER OK\r\n" : "ESP:WEATHER FAIL\r\n");
    }

    memcpy(&esp_last_info, info, sizeof(esp_last_info));
    esp_last_info_valid = 1U;
    return info->wifi_ok;
}

