#include "display_ui.h"
#include "actuators.h"
#include "bluetooth_link.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include <stdio.h>
#include <string.h>

#define UI_TXT_WAIT_SENSOR "\347\255\211\345\276\205\344\274\240\346\204\237\345\231\250\346\225\260\346\215\256\347\250\263\345\256\232"
#define UI_TXT_SOIL_DRY_WATER "\345\234\237\345\243\244\345\201\217\345\271\262\357\274\214\345\273\272\350\256\256\346\265\207\346\260\264"
#define UI_TXT_TEMP_HIGH_VENT "\346\270\251\345\272\246\345\201\217\351\253\230\357\274\214\345\273\272\350\256\256\351\200\232\351\243\216"
#define UI_TXT_TEMP_LOW_CLOSE "\346\270\251\345\272\246\345\201\217\344\275\216\357\274\214\344\277\235\346\214\201\346\243\232\345\256\244\345\205\263\351\227\255"
#define UI_TXT_LIGHT_WEAK "\345\205\211\347\205\247\345\201\217\345\274\261\357\274\214\346\263\250\346\204\217\350\241\245\345\205\211"
#define UI_TXT_CO2_HIGH_OPEN "CO2\345\201\217\351\253\230\357\274\214\346\211\223\345\274\200\351\243\216\346\211\207/\345\244\251\347\252\227"
#define UI_TXT_ENV_STABLE "\347\216\257\345\242\203\347\250\263\345\256\232\357\274\214\351\200\202\345\220\210\347\224\237\351\225\277"
#define UI_TXT_SOIL_DRY_PUMP "\345\234\237\345\243\244\345\201\217\345\271\262\357\274\214\345\273\272\350\256\256\346\211\223\345\274\200\346\260\264\346\263\265"
#define UI_TXT_TEMP_HIGH_FAN "\346\270\251\345\272\246\345\201\217\351\253\230\357\274\214\345\273\272\350\256\256\346\211\223\345\274\200\351\243\216\346\211\207"
#define UI_TXT_CO2_HIGH_VENT "CO2\345\201\217\351\253\230\357\274\214\345\273\272\350\256\256\351\200\232\351\243\216"
#define UI_TXT_PEST_RUNNING "\350\231\253\345\256\263\350\257\261\346\215\225\346\250\241\345\274\217\350\277\220\350\241\214\344\270\255"
#define UI_TXT_AUTO_RUNNING "\350\207\252\345\212\250\346\250\241\345\274\217\350\277\220\350\241\214\344\270\255"
#define UI_TXT_MANUAL_READY "\346\211\213\345\212\250\346\250\241\345\274\217\345\260\261\347\273\252"
#define UI_TXT_CONTROL "\346\216\247\345\210\266"
#define UI_TXT_DATA "\346\225\260\346\215\256"
#define UI_TXT_TIME_LOC_WEATHER "\346\227\266\351\227\264 / \344\275\215\347\275\256 / \345\244\251\346\260\224"
#define UI_TXT_TIME_EMPTY "\346\227\266\351\227\264: --"
#define UI_FMT_TIME "\346\227\266\351\227\264: %s"
#define UI_TXT_LOC_EMPTY "\344\275\215\347\275\256: --"
#define UI_FMT_LOC "\344\275\215\347\275\256: %s"
#define UI_TXT_LOC_OK "\344\275\215\347\275\256: \345\267\262\350\216\267\345\217\226"
#define UI_TXT_WEATHER_EMPTY "\345\244\251\346\260\224: --"
#define UI_FMT_WEATHER_TEMP "\345\244\251\346\260\224: %s %sC"
#define UI_FMT_WEATHER "\345\244\251\346\260\224: %s"
#define UI_TXT_WEATHER_OK "\345\244\251\346\260\224: \345\267\262\350\216\267\345\217\226"
#define UI_TXT_TEMP "\346\270\251\345\272\246"
#define UI_TXT_HUM "\346\271\277\345\272\246"
#define UI_TXT_LIGHT "\345\205\211\347\205\247"
#define UI_TXT_SOIL_HUM "\345\234\237\345\243\244\346\271\277\345\272\246"
#define UI_TXT_AI "\350\261\206\345\214\205AI\345\210\206\346\236\220"
#define UI_TXT_CONTROL_MODE "\346\216\247\345\210\266\346\250\241\345\274\217"
#define UI_TXT_MANUAL "\346\211\213\345\212\250"
#define UI_TXT_AUTO "\350\207\252\345\212\250"
#define UI_TXT_PUMP "\346\260\264\346\263\265"
#define UI_TXT_OFF "\345\205\263"
#define UI_TXT_ON "\345\274\200"
#define UI_TXT_FAN "\351\243\216\346\211\207"
#define UI_TXT_LAMP "\346\235\200\350\231\253\347\201\257"
#define UI_TXT_WINDOW "\345\244\251\347\252\227"
#define UI_TXT_CLOSED "\345\205\263\351\227\255"
#define UI_TXT_BUZZER "\350\234\202\351\270\243\345\231\250"
#define UI_TXT_FULL_OPEN "\345\205\250\345\274\200"
#define UI_TXT_HALF_OPEN "\345\215\212\345\274\200"
#define UI_TXT_LOCUST "\350\235\227\350\231\253"
#define UI_TXT_CABBAGE "\350\217\234\351\235\222\350\231\253"
#define UI_TXT_ADDR_CQ "\345\234\260\345\235\200: \351\207\215\345\272\206\345\270\202\345\267\264\345\215\227\345\214\272"
#define UI_TXT_WEATHER_CLEAR "\346\231\264"
#define UI_TXT_WEATHER_CLOUD "\345\244\232\344\272\221"
#define UI_TXT_WEATHER_RAIN "\351\233\250"
#define UI_TXT_WEATHER_THUNDER "\351\233\267\351\233\250"
#define UI_TXT_WEATHER_SNOW "\351\233\252"
#define UI_TXT_WEATHER_FOG "\351\233\276"
#define UI_TXT_WEATHER_OVERCAST "\351\230\264"
#define UI_FMT_WEATHER_CN "\345\244\251\346\260\224: %s"
#define UI_FMT_TEMP_VALUE "\346\270\251\345\272\246: %d.%d\302\260C"
#define UI_TXT_TEMP_EMPTY_LINE "\346\270\251\345\272\246: --\302\260C"
#define UI_FMT_HUM_VALUE "\346\271\277\345\272\246: %d.%d %%"
#define UI_TXT_HUM_EMPTY_LINE "\346\271\277\345\272\246: -- %"
#define UI_FMT_CO2_VALUE "CO2: %u ppm"
#define UI_TXT_CO2_EMPTY_LINE "CO2: -- ppm"
#define UI_FMT_LIGHT_VALUE "\345\205\211\347\205\247: %d%%"
#define UI_FMT_SOIL_VALUE "\345\234\237\345\243\244\346\271\277\345\272\246: %d%%"
#define UI_FMT_MODE_VALUE "\346\216\247\345\210\266\346\250\241\345\274\217: %s"
#define UI_FMT_MODE_SHORT "\346\250\241\345\274\217:%s"
#define UI_FMT_PUMP_VALUE "\346\260\264\346\263\265: %s"
#define UI_FMT_FAN_VALUE "\351\243\216\346\211\207: %s"
#define UI_FMT_LAMP_VALUE "\350\257\261\350\231\253\347\201\257: %s"
#define UI_FMT_WINDOW_VALUE "\345\244\251\347\252\227: %s"
#define UI_FMT_BUZZER_VALUE "\350\234\202\351\270\243\345\231\250: %s"
#define UI_FMT_LAMP_STATE "\350\257\261\350\231\253\347\201\257:%s"
#define UI_FMT_WINDOW_STATE "\345\244\251\347\252\227:%s"
#define UI_FMT_BUZZER_STATE "\350\234\202\351\270\243\345\231\250:%s"
#define UI_FMT_LAMP_SHORT "\350\231\253\347\201\257:%s"
#define UI_FMT_WINDOW_SHORT "\347\252\227:%s"
#define UI_FMT_BUZZER_SHORT "\350\234\202\351\270\243:%s"
#define UI_TXT_LIGHT_ZERO "\345\205\211\347\205\247: 0%"
#define UI_TXT_SOIL_ZERO "\345\234\237\345\243\244\346\271\277\345\272\246: 0%"
#define UI_TXT_DOUBAO_OK "\350\261\206\345\214\205\345\210\244\346\226\255: \347\216\257\345\242\203\351\200\202\345\220\210\347\224\237\351\225\277"
#define UI_TXT_DOUBAO_SOIL_DRY "\350\261\206\345\214\205\345\210\244\346\226\255: \345\234\237\345\243\244\345\201\217\345\271\262\357\274\214\345\273\272\350\256\256\346\265\207\346\260\264"
#define UI_TXT_DOUBAO_TEMP_HIGH "\350\261\206\345\214\205\345\210\244\346\226\255: \346\270\251\345\272\246\345\201\217\351\253\230\357\274\214\345\273\272\350\256\256\351\200\232\351\243\216"
#define UI_TXT_DOUBAO_TEMP_LOW "\350\261\206\345\214\205\345\210\244\346\226\255: \346\270\251\345\272\246\345\201\217\344\275\216\357\274\214\346\263\250\346\204\217\344\277\235\346\270\251"
#define UI_TXT_DOUBAO_LIGHT_LOW "\350\261\206\345\214\205\345\210\244\346\226\255: \345\205\211\347\205\247\345\201\217\345\274\261\357\274\214\345\273\272\350\256\256\350\241\245\345\205\211"
#define UI_TXT_DOUBAO_CO2_HIGH "\350\261\206\345\214\205\345\210\244\346\226\255: CO2\345\201\217\351\253\230\357\274\214\345\273\272\350\256\256\351\200\232\351\243\216"
#define UI_TXT_DOUBAO_DATA_BAD "\350\261\206\345\214\205\345\210\244\346\226\255: \346\225\260\346\215\256\344\270\215\350\266\263\357\274\214\350\257\267\347\273\247\347\273\255\350\247\202\345\257\237"
#define UI_TXT_DOUBAO_WAIT "\347\255\211\345\276\205\350\261\206\345\214\205AI\345\210\206\346\236\220..."
#define UI_TXT_DOUBAO_WIFI_FAIL "\350\261\206\345\214\205AI: WiFi\346\234\252\350\277\236\346\216\245"
#define UI_TXT_DOUBAO_CONNECT_FAIL "\350\261\206\345\214\205AI: \350\277\236\346\216\245\346\234\215\345\212\241\345\231\250\345\244\261\350\264\245"
#define UI_TXT_DOUBAO_SEND_FAIL "\350\261\206\345\214\205AI: \350\257\267\346\261\202\345\217\221\351\200\201\345\244\261\350\264\245"
#define UI_TXT_DOUBAO_RESPONSE_FAIL "\350\261\206\345\214\205AI: \345\223\215\345\272\224\350\266\205\346\227\266"
#define UI_TXT_DOUBAO_CONFIG_FAIL "\350\261\206\345\214\205AI: API\345\217\202\346\225\260\346\234\252\351\205\215\347\275\256"
#define UI_TXT_AI_WAIT "\346\255\243\345\234\250\346\261\207\350\201\232\347\225\252\350\214\204\346\270\251\345\256\244\345\244\232\346\250\241\346\200\201\346\225\260\346\215\256\357\274\214\347\255\211\345\276\205\346\270\251\346\271\277\345\272\246\343\200\201\345\205\211\347\205\247\343\200\201\345\234\237\345\243\244\344\270\216CO2\347\250\263\345\256\232\345\220\216\347\273\231\345\207\272AI\345\210\244\346\226\255"
#define UI_TXT_AI_DATA_OK "\347\225\252\350\214\204\345\275\223\345\211\215\347\216\257\345\242\203\350\276\203\347\250\263\345\256\232\357\274\214\346\270\251\346\271\277\345\272\246\343\200\201\345\205\211\347\205\247\343\200\201\345\234\237\345\243\244\346\260\264\345\210\206\344\270\216CO2\345\215\217\345\220\214\350\211\257\345\245\275\357\274\214\345\217\257\347\273\247\347\273\255\344\277\235\346\214\201\347\233\221\346\265\213"
#define UI_TXT_AI_DATA_SOIL_DRY "\345\234\237\345\243\244\346\271\277\345\272\246\345\201\217\344\275\216\357\274\214\347\225\252\350\214\204\346\240\271\347\263\273\345\220\270\346\260\264\345\217\227\351\231\220\357\274\214\350\213\245\346\214\201\347\273\255\345\271\262\346\227\261\344\274\232\345\275\261\345\223\215\345\235\220\346\236\234\344\270\216\350\206\250\345\244\247\357\274\214\345\272\224\351\207\215\347\202\271\345\205\263\346\263\250"
#define UI_TXT_AI_DATA_TEMP_HIGH "\346\270\251\345\272\246\345\201\217\351\253\230\357\274\214\347\225\252\350\214\204\350\212\261\347\262\211\346\264\273\346\200\247\345\222\214\345\235\220\346\236\234\347\216\207\345\217\257\350\203\275\344\270\213\351\231\215\357\274\214\351\234\200\345\205\263\346\263\250\346\243\232\345\206\205\347\203\255\347\247\257\347\264\257\344\270\216\345\217\266\351\235\242\350\222\270\350\205\276\345\216\213\345\212\233"
#define UI_TXT_AI_DATA_TEMP_LOW "\346\270\251\345\272\246\345\201\217\344\275\216\357\274\214\347\225\252\350\214\204\347\224\237\351\225\277\344\273\243\350\260\242\346\224\276\347\274\223\357\274\214\346\240\271\347\263\273\346\264\273\345\212\233\344\270\213\351\231\215\357\274\214\345\244\234\351\227\264\344\275\216\346\270\251\346\227\266\351\234\200\346\263\250\346\204\217\344\277\235\346\270\251\347\256\241\347\220\206"
#define UI_TXT_AI_DATA_HUM_HIGH "\347\251\272\346\260\224\346\271\277\345\272\246\345\201\217\351\253\230\357\274\214\347\225\252\350\214\204\345\217\266\351\235\242\346\230\223\347\273\223\351\234\262\345\271\266\345\242\236\345\212\240\347\227\205\345\256\263\351\243\216\351\231\251\357\274\214\351\234\200\347\273\223\345\220\210\345\244\251\346\260\224\345\210\244\346\226\255\351\200\232\351\243\216\346\227\266\346\234\272"
#define UI_TXT_AI_DATA_HUM_LOW "\347\251\272\346\260\224\346\271\277\345\272\246\345\201\217\344\275\216\357\274\214\347\225\252\350\214\204\350\222\270\350\205\276\345\212\240\345\277\253\344\270\224\345\271\274\346\236\234\346\230\223\345\217\227\350\203\201\350\277\253\357\274\214\345\273\272\350\256\256\345\205\263\346\263\250\345\234\237\345\243\244\346\260\264\345\210\206\344\270\216\346\243\232\345\206\205\344\277\235\346\271\277"
#define UI_TXT_AI_DATA_LIGHT_LOW "\345\205\211\347\205\247\345\201\217\345\274\261\357\274\214\347\225\252\350\214\204\345\205\211\345\220\210\344\275\234\347\224\250\347\247\257\347\264\257\344\270\215\350\266\263\357\274\214\346\236\234\345\256\236\350\206\250\345\244\247\345\222\214\350\275\254\350\211\262\345\217\257\350\203\275\345\217\227\351\231\220\357\274\214\345\272\224\345\205\263\346\263\250\350\277\236\347\273\255\351\230\264\351\233\250\345\275\261\345\223\215"
#define UI_TXT_AI_DATA_CO2_HIGH "CO2\346\265\223\345\272\246\345\201\217\351\253\230\357\274\214\347\225\252\350\214\204\345\217\266\347\211\207\344\273\243\350\260\242\345\216\213\345\212\233\345\242\236\345\212\240\357\274\214\350\213\245\351\200\232\351\243\216\344\270\215\350\266\263\345\217\257\350\203\275\345\275\261\345\223\215\351\225\277\346\234\237\347\224\237\351\225\277\347\250\263\345\256\232\346\200\247"
#define UI_TXT_AI_DATA_RAIN "\345\275\223\345\211\215\345\244\251\346\260\224\346\234\211\351\233\250\357\274\214\346\243\232\345\206\205\346\271\277\345\272\246\345\217\257\350\203\275\345\215\207\351\253\230\357\274\214\347\225\252\350\214\204\351\234\200\351\207\215\347\202\271\351\230\262\347\227\205\346\216\247\346\271\277\357\274\214\345\271\266\351\201\277\345\205\215\345\234\237\345\243\244\351\225\277\346\234\237\350\277\207\346\271\277"
#define UI_TXT_AI_CTRL_OK "\345\275\223\345\211\215\346\216\247\345\210\266\347\255\226\347\225\245\345\217\257\344\277\235\346\214\201\350\207\252\345\212\250\347\233\221\346\265\213\357\274\214\346\260\264\346\263\265\343\200\201\351\243\216\346\211\207\343\200\201\345\244\251\347\252\227\344\270\216\350\257\261\350\231\253\347\201\257\347\273\264\346\214\201\347\216\260\347\212\266\357\274\214\346\214\201\347\273\255\345\256\210\346\212\244\347\225\252\350\214\204\347\224\237\351\225\277"
#define UI_TXT_AI_CTRL_SOIL_DRY "\345\234\237\345\243\244\345\201\217\345\271\262\357\274\214\345\273\272\350\256\256\344\274\230\345\205\210\346\211\223\345\274\200\346\260\264\346\263\265\347\237\255\346\227\266\350\241\245\346\260\264\357\274\214\350\276\276\345\210\260\345\220\210\351\200\202\346\271\277\345\272\246\345\220\216\345\217\212\346\227\266\345\205\263\351\227\255\351\201\277\345\205\215\346\240\271\345\214\272\347\247\257\346\260\264"
#define UI_TXT_AI_CTRL_TEMP_HIGH "\346\270\251\345\272\246\345\201\217\351\253\230\357\274\214\345\273\272\350\256\256\351\243\216\346\211\207\344\270\216\345\244\251\347\252\227\350\201\224\345\212\250\351\200\232\351\243\216\351\231\215\346\270\251\357\274\214\345\277\205\350\246\201\346\227\266\345\205\250\345\274\200\345\244\251\347\252\227\344\273\245\344\277\235\346\212\244\347\225\252\350\214\204\345\235\220\346\236\234\347\216\257\345\242\203"
#define UI_TXT_AI_CTRL_TEMP_LOW "\346\270\251\345\272\246\345\201\217\344\275\216\357\274\214\345\273\272\350\256\256\345\205\263\351\227\255\345\244\251\347\252\227\345\271\266\345\207\217\345\260\221\351\243\216\346\211\207\350\277\220\350\241\214\357\274\214\344\277\235\346\214\201\346\243\232\345\206\205\347\203\255\351\207\217\357\274\214\351\201\277\345\205\215\347\225\252\350\214\204\345\244\234\351\227\264\345\217\227\345\206\267\350\203\201\350\277\253"
#define UI_TXT_AI_CTRL_HUM_HIGH "\346\271\277\345\272\246\345\201\217\351\253\230\357\274\214\345\273\272\350\256\256\345\215\212\345\274\200\345\244\251\347\252\227\345\271\266\344\275\216\351\200\237\351\200\232\351\243\216\346\216\222\346\271\277\357\274\214\351\201\277\345\205\215\345\217\266\351\235\242\347\273\223\351\234\262\350\257\261\345\217\221\347\225\252\350\214\204\347\227\205\345\256\263"
#define UI_TXT_AI_CTRL_HUM_LOW "\346\271\277\345\272\246\345\201\217\344\275\216\357\274\214\345\273\272\350\256\256\345\207\217\345\260\221\351\200\232\351\243\216\345\271\266\350\247\202\345\257\237\345\234\237\345\243\244\346\271\277\345\272\246\357\274\214\345\277\205\350\246\201\346\227\266\345\260\221\351\207\217\350\241\245\346\260\264\347\273\264\346\214\201\347\225\252\350\214\204\351\200\202\345\256\234\350\222\270\350\205\276"
#define UI_TXT_AI_CTRL_LIGHT_LOW "\345\205\211\347\205\247\344\270\215\350\266\263\357\274\214\345\273\272\350\256\256\344\277\235\346\214\201\350\257\261\350\231\253\347\201\257\345\205\263\351\227\255\345\271\266\344\274\230\345\205\210\345\210\251\347\224\250\350\207\252\347\204\266\345\205\211\357\274\214\345\277\205\350\246\201\346\227\266\347\273\223\345\220\210\350\241\245\345\205\211\346\217\220\351\253\230\347\225\252\350\214\204\345\205\211\345\220\210\346\225\210\347\216\207"
#define UI_TXT_AI_CTRL_CO2_HIGH "CO2\345\201\217\351\253\230\357\274\214\345\273\272\350\256\256\345\220\257\345\212\250\351\243\216\346\211\207\346\216\222\346\260\224\357\274\214\345\244\251\347\252\227\345\215\212\345\274\200\346\210\226\345\205\250\345\274\200\346\215\242\346\260\224\357\274\214\344\275\277\347\225\252\350\214\204\346\270\251\345\256\244\346\260\224\344\275\223\347\216\257\345\242\203\346\201\242\345\244\215\345\271\263\350\241\241"
#define UI_TXT_AI_CTRL_RAIN "\351\233\250\345\244\251\347\251\272\346\260\224\346\271\277\345\272\246\351\253\230\357\274\214\345\273\272\350\256\256\345\244\251\347\252\227\345\205\263\351\227\255\357\274\214\351\243\216\346\211\207\351\227\264\346\255\207\351\200\232\351\243\216\346\216\222\346\271\277\357\274\214\346\260\264\346\263\265\346\232\202\347\274\223\345\274\200\345\220\257\344\273\245\351\230\262\347\225\252\350\214\204\346\240\271\345\214\272\350\277\207\346\271\277"

static uint8_t ui_ready = 0U;

static lv_obj_t *screen_data;
static lv_obj_t *screen_drive;
static lv_obj_t *label_time_data;
static lv_obj_t *label_location_data;
static lv_obj_t *label_weather_data;
static lv_obj_t *icon_weather_data;
static lv_obj_t *label_time_drive;
static lv_obj_t *label_location_drive;
static lv_obj_t *label_weather_drive;
static lv_obj_t *icon_weather_drive;
static lv_obj_t *label_temp;
static lv_obj_t *label_hum;
static lv_obj_t *label_co2;
static lv_obj_t *bar_light;
static lv_obj_t *bar_soil;
static lv_obj_t *label_light;
static lv_obj_t *label_soil;
static lv_obj_t *label_ai_data;

static lv_obj_t *label_mode;
static lv_obj_t *label_pump;
static lv_obj_t *label_fan;
static lv_obj_t *label_lamp;
static lv_obj_t *label_window;
static lv_obj_t *label_buzzer;
static lv_obj_t *label_ai_drive;
static lv_obj_t *box_mode;
static lv_obj_t *box_pump;
static lv_obj_t *box_fan;
static lv_obj_t *box_lamp;
static lv_obj_t *box_window;
static lv_obj_t *box_buzzer;

static lv_style_t style_screen;
static lv_style_t style_value;
static lv_style_t style_button;
static lv_style_t style_button_label;
static lv_style_t style_bar;
static lv_style_t style_bar_indic;
static lv_style_t style_ai;
static uint8_t pending_page = 0U;

static lv_obj_t *CreateValueLabel(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, const char *text)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_add_style(label, &style_value, 0);
    lv_obj_set_pos(label, x, y);
    return label;
}

static lv_obj_t *CreateLineLabel(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, const char *text)
{
    lv_obj_t *label = CreateValueLabel(parent, x, y, text);
    lv_obj_set_width(label, w);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    return label;
}

static lv_obj_t *CreateValueBox(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, const char *text, lv_obj_t **out_box)
{
    lv_obj_t *box;
    lv_obj_t *label;

    box = lv_obj_create(parent);
    lv_obj_add_style(box, &style_ai, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(box, w, h);
    lv_obj_set_pos(box, x, y);
    lv_obj_add_flag(box, LV_OBJ_FLAG_CLICKABLE);

    label = lv_label_create(box);
    lv_label_set_text(label, text);
    lv_obj_add_style(label, &style_value, 0);
    lv_obj_set_width(label, (lv_coord_t)(w - 12));
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_center(label);

    if (out_box != NULL) {
        *out_box = box;
    }
    return label;
}

static lv_obj_t *CreateAdviceBox(lv_obj_t *parent, lv_coord_t y, lv_obj_t **body_label)
{
    lv_obj_t *box;
    lv_obj_t *title;
    lv_obj_t *body;

    box = lv_obj_create(parent);
    lv_obj_add_style(box, &style_ai, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(box, 220, 114);
    lv_obj_set_pos(box, 10, y);

    title = lv_label_create(box);
    lv_label_set_text(title, UI_TXT_AI);
    lv_obj_add_style(title, &style_value, 0);
    lv_obj_set_pos(title, 0, 0);

    body = lv_label_create(box);
    lv_label_set_text(body, UI_TXT_DOUBAO_WAIT);
    lv_obj_add_style(body, &style_value, 0);
    lv_obj_set_width(body, 204);
    lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(body, 0, 20);

    if (body_label != NULL) {
        *body_label = body;
    }
    return box;
}

static void SetLabelText(lv_obj_t *label, const char *text)
{
    if (label != NULL) {
        lv_label_set_text(label, text != NULL ? text : "--");
    }
}

static uint8_t TextIsAscii(const char *text)
{
    if (text == NULL || text[0] == '\0') {
        return 0U;
    }
    while (*text != '\0') {
        if ((uint8_t)*text < 32U || (uint8_t)*text > 126U) {
            return 0U;
        }
        text++;
    }
    return 1U;
}

static uint8_t TextContainsAscii(const char *text, const char *pattern)
{
    if (text == NULL || pattern == NULL) {
        return 0U;
    }
    return strstr(text, pattern) != NULL ? 1U : 0U;
}

static void UpdateWeatherIcon(lv_obj_t *icon, const char *weather);

static const char *WeatherTextCn(const char *weather)
{
    if (weather == NULL || weather[0] == '\0') {
        return "--";
    }
    if (!TextIsAscii(weather)) {
        return UI_TXT_WEATHER_CLEAR;
    }
    if (TextContainsAscii(weather, "Thunder")) {
        return UI_TXT_WEATHER_THUNDER;
    }
    if (TextContainsAscii(weather, "Rain") || TextContainsAscii(weather, "Drizzle") || TextContainsAscii(weather, "Showers")) {
        return UI_TXT_WEATHER_RAIN;
    }
    if (TextContainsAscii(weather, "Snow")) {
        return UI_TXT_WEATHER_SNOW;
    }
    if (TextContainsAscii(weather, "Fog") || TextContainsAscii(weather, "Mist")) {
        return UI_TXT_WEATHER_FOG;
    }
    if (TextContainsAscii(weather, "Overcast")) {
        return UI_TXT_WEATHER_OVERCAST;
    }
    if (TextContainsAscii(weather, "Cloud")) {
        return UI_TXT_WEATHER_CLOUD;
    }
    return UI_TXT_WEATHER_CLEAR;
}

static const char *TimePart(const char *time_text)
{
    uint16_t len;

    if (time_text == NULL || time_text[0] == '\0') {
        return "--:--:--";
    }

    len = (uint16_t)strlen(time_text);
    if (len >= 8U && time_text[len - 3U] == ':' && time_text[len - 6U] == ':') {
        return &time_text[len - 8U];
    }
    return time_text;
}

static void SetTopInfo(const char *time_text, const char *weather_text)
{
    char text[64];
    const char *weather_cn = WeatherTextCn(weather_text);

    SetLabelText(label_time_data, TimePart(time_text));
    SetLabelText(label_time_drive, TimePart(time_text));
    SetLabelText(label_location_data, UI_TXT_ADDR_CQ);
    SetLabelText(label_location_drive, UI_TXT_ADDR_CQ);

    snprintf(text, sizeof(text), UI_FMT_WEATHER_CN, weather_cn);
    SetLabelText(label_weather_data, text);
    SetLabelText(label_weather_drive, text);
    UpdateWeatherIcon(icon_weather_data, weather_text);
    UpdateWeatherIcon(icon_weather_drive, weather_text);
    lv_obj_update_layout(label_weather_data);
    lv_obj_update_layout(label_weather_drive);
    lv_obj_align_to(icon_weather_data, label_weather_data, LV_ALIGN_OUT_RIGHT_MID, 4, 0);
    lv_obj_align_to(icon_weather_drive, label_weather_drive, LV_ALIGN_OUT_RIGHT_MID, 4, 0);
}

static void UpdateWeatherIcon(lv_obj_t *icon, const char *weather)
{
    lv_obj_t *part;
    uint8_t rainy;
    uint8_t cloudy;
    uint8_t unknown;

    if (icon == NULL) {
        return;
    }

    unknown = (weather == NULL || weather[0] == '\0') ? 1U : 0U;
    rainy = (TextContainsAscii(weather, "Rain") || TextContainsAscii(weather, "Drizzle") ||
             TextContainsAscii(weather, "Showers") || TextContainsAscii(weather, "Thunder")) ? 1U : 0U;
    cloudy = (rainy || TextContainsAscii(weather, "Cloud") || TextContainsAscii(weather, "Overcast") ||
              TextContainsAscii(weather, "Fog") || TextContainsAscii(weather, "Mist")) ? 1U : 0U;

    lv_obj_clean(icon);
    lv_obj_remove_style_all(icon);
    lv_obj_set_size(icon, 32, 20);
    lv_obj_set_style_bg_opa(icon, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(icon, 0, 0);

    if (!cloudy && !unknown) {
        static const lv_coord_t rays[8][4] = {
            {15, 0, 15, 3}, {15, 17, 15, 20}, {2, 10, 5, 10}, {25, 10, 29, 10},
            {6, 2, 8, 4}, {23, 2, 21, 4}, {6, 18, 8, 16}, {23, 18, 21, 16}
        };
        uint8_t i;

        for (i = 0U; i < 8U; i++) {
            part = lv_obj_create(icon);
            lv_obj_remove_style_all(part);
            lv_obj_set_pos(part, rays[i][0], rays[i][1]);
            lv_obj_set_size(part, (lv_coord_t)(rays[i][2] - rays[i][0] + 1), (lv_coord_t)(rays[i][3] - rays[i][1] + 1));
            if (lv_obj_get_width(part) < 2) lv_obj_set_width(part, 2);
            if (lv_obj_get_height(part) < 2) lv_obj_set_height(part, 2);
            lv_obj_set_style_bg_color(part, lv_color_hex(0xffd34d), 0);
            lv_obj_set_style_bg_opa(part, LV_OPA_COVER, 0);
            lv_obj_set_style_radius(part, 1, 0);
        }

        part = lv_obj_create(icon);
        lv_obj_remove_style_all(part);
        lv_obj_set_pos(part, 8, 4);
        lv_obj_set_size(part, 14, 14);
        lv_obj_set_style_radius(part, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(part, lv_color_hex(0xffd34d), 0);
        lv_obj_set_style_bg_opa(part, LV_OPA_COVER, 0);
        return;
    }

    if (unknown) {
        part = lv_obj_create(icon);
        lv_obj_remove_style_all(part);
        lv_obj_set_pos(part, 8, 5);
        lv_obj_set_size(part, 16, 10);
        lv_obj_set_style_radius(part, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(part, lv_color_hex(0x7f8c8d), 0);
        lv_obj_set_style_bg_opa(part, LV_OPA_COVER, 0);
        return;
    }

    part = lv_obj_create(icon);
    lv_obj_remove_style_all(part);
    lv_obj_set_pos(part, 7, 8);
    lv_obj_set_size(part, 19, 9);
    lv_obj_set_style_radius(part, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(part, lv_color_hex(0xcfd8dc), 0);
    lv_obj_set_style_bg_opa(part, LV_OPA_COVER, 0);

    part = lv_obj_create(icon);
    lv_obj_remove_style_all(part);
    lv_obj_set_pos(part, 6, 10);
    lv_obj_set_size(part, 8, 7);
    lv_obj_set_style_radius(part, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(part, lv_color_hex(0xcfd8dc), 0);
    lv_obj_set_style_bg_opa(part, LV_OPA_COVER, 0);

    part = lv_obj_create(icon);
    lv_obj_remove_style_all(part);
    lv_obj_set_pos(part, 12, 4);
    lv_obj_set_size(part, 11, 11);
    lv_obj_set_style_radius(part, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(part, lv_color_hex(0xdde6ea), 0);
    lv_obj_set_style_bg_opa(part, LV_OPA_COVER, 0);

    if (rainy) {
        uint8_t i;
        for (i = 0U; i < 3U; i++) {
            part = lv_obj_create(icon);
            lv_obj_remove_style_all(part);
            lv_obj_set_pos(part, (lv_coord_t)(9 + i * 6), 17);
            lv_obj_set_size(part, 2, 4);
            lv_obj_set_style_radius(part, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(part, lv_color_hex(0x58a6ff), 0);
            lv_obj_set_style_bg_opa(part, LV_OPA_COVER, 0);
        }
    }
}

static int LightPercent(uint16_t adc)
{
    if (adc <= 100U) return 100;
    if (adc >= 3800U) return 0;
    return (int)((3800U - adc) * 100U / 3700U);
}

static int SoilPercent(uint16_t adc)
{
    if (adc <= 900U) return 100;
    if (adc >= 4095U) return 0;
    return (int)((4095U - adc) * 100U / 3195U);
}

static const char *LocalDataAiText(const SensorData_t *data, const char *weather)
{
    int temp;
    int hum;
    int light;
    int soil;

    if (data == NULL || !data->dht11_ok) {
        return UI_TXT_AI_WAIT;
    }

    temp = data->temperature / 10;
    hum = data->humidity / 10;
    light = LightPercent(data->light_adc);
    soil = SoilPercent(data->soil_adc);

    if (TextContainsAscii(weather, "Rain") || TextContainsAscii(weather, "Thunder") ||
        TextContainsAscii(weather, "Drizzle") || TextContainsAscii(weather, "Showers")) {
        return UI_TXT_AI_DATA_RAIN;
    }
    if (soil < 35) {
        return UI_TXT_AI_DATA_SOIL_DRY;
    }
    if (temp > 32) {
        return UI_TXT_AI_DATA_TEMP_HIGH;
    }
    if (temp < 18) {
        return UI_TXT_AI_DATA_TEMP_LOW;
    }
    if (hum > 85) {
        return UI_TXT_AI_DATA_HUM_HIGH;
    }
    if (hum < 45) {
        return UI_TXT_AI_DATA_HUM_LOW;
    }
    if (light < 20) {
        return UI_TXT_AI_DATA_LIGHT_LOW;
    }
    if (data->co2_ok && data->co2_ppm > 1200U) {
        return UI_TXT_AI_DATA_CO2_HIGH;
    }
    return UI_TXT_AI_DATA_OK;
}

static const char *LocalControlAiText(const SensorData_t *data, const char *weather)
{
    int temp;
    int hum;
    int light;
    int soil;

    if (data == NULL || !data->dht11_ok) {
        return UI_TXT_AI_WAIT;
    }

    temp = data->temperature / 10;
    hum = data->humidity / 10;
    light = LightPercent(data->light_adc);
    soil = SoilPercent(data->soil_adc);

    if (TextContainsAscii(weather, "Rain") || TextContainsAscii(weather, "Thunder") ||
        TextContainsAscii(weather, "Drizzle") || TextContainsAscii(weather, "Showers")) {
        return UI_TXT_AI_CTRL_RAIN;
    }
    if (soil < 35) {
        return UI_TXT_AI_CTRL_SOIL_DRY;
    }
    if (temp > 32) {
        return UI_TXT_AI_CTRL_TEMP_HIGH;
    }
    if (temp < 18) {
        return UI_TXT_AI_CTRL_TEMP_LOW;
    }
    if (hum > 85) {
        return UI_TXT_AI_CTRL_HUM_HIGH;
    }
    if (hum < 45) {
        return UI_TXT_AI_CTRL_HUM_LOW;
    }
    if (light < 20) {
        return UI_TXT_AI_CTRL_LIGHT_LOW;
    }
    if (data->co2_ok && data->co2_ppm > 1200U) {
        return UI_TXT_AI_CTRL_CO2_HIGH;
    }
    return UI_TXT_AI_CTRL_OK;
}

static void NextButtonEvent(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        pending_page = 2U;
    }
}

static void PrevButtonEvent(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        pending_page = 1U;
    }
}

static void ControlBoxEvent(lv_event_t *e)
{
    lv_obj_t *target;

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    target = lv_event_get_target(e);
    if (target == box_mode) {
        BluetoothLink_SetAutoMode(BluetoothLink_IsAutoMode() ? 0U : 1U);
    } else if (target == box_window) {
        ActuatorState_t state;
        Actuators_GetState(&state);
        if (state.sky_window_state == 0U) {
            SkyWindow_HalfOpen();
        } else if (state.sky_window_state == 1U) {
            SkyWindow_FullOpen();
        } else {
            SkyWindow_Close();
        }
    } else if (target == box_pump) {
        Pump_Toggle();
    } else if (target == box_fan) {
        Fan_Toggle();
    } else if (target == box_lamp) {
        PestLamp_Toggle();
    } else if (target == box_buzzer) {
        ActuatorState_t state;
        Actuators_GetState(&state);
        if (state.buzzer_mode == 0U) {
            Buzzer_SetMode(1U);
        } else if (state.buzzer_mode == 1U) {
            Buzzer_SetMode(2U);
        } else {
            Buzzer_SetMode(0U);
        }
    }
}

static void AddControlEvent(lv_obj_t *box)
{
    if (box != NULL) {
        lv_obj_add_event_cb(box, ControlBoxEvent, LV_EVENT_CLICKED, NULL);
    }
}

static void InitStyles(void)
{
    lv_style_init(&style_screen);
    lv_style_set_bg_color(&style_screen, lv_color_hex(0xeaf7df));
    lv_style_set_bg_opa(&style_screen, LV_OPA_COVER);
    lv_style_set_text_color(&style_screen, lv_color_hex(0x21401f));

    lv_style_init(&style_value);
    lv_style_set_text_color(&style_value, lv_color_hex(0x21401f));
    lv_style_set_text_font(&style_value, LV_FONT_DEFAULT);

    lv_style_init(&style_button);
    lv_style_set_radius(&style_button, 6);
    lv_style_set_bg_color(&style_button, lv_color_hex(0x9bd36a));
    lv_style_set_bg_opa(&style_button, LV_OPA_COVER);
    lv_style_set_border_width(&style_button, 0);

    lv_style_init(&style_button_label);
    lv_style_set_text_color(&style_button_label, lv_color_hex(0x21401f));

    lv_style_init(&style_bar);
    lv_style_set_bg_color(&style_bar, lv_color_hex(0xcde6bd));
    lv_style_set_radius(&style_bar, 3);
    lv_style_set_border_width(&style_bar, 0);

    lv_style_init(&style_bar_indic);
    lv_style_set_bg_color(&style_bar_indic, lv_color_hex(0x5aa832));
    lv_style_set_radius(&style_bar_indic, 3);

    lv_style_init(&style_ai);
    lv_style_set_radius(&style_ai, 8);
    lv_style_set_bg_color(&style_ai, lv_color_hex(0xd9efc8));
    lv_style_set_border_color(&style_ai, lv_color_hex(0x8ccf62));
    lv_style_set_border_width(&style_ai, 1);
    lv_style_set_pad_all(&style_ai, 6);
}

static void CreateDataScreen(void)
{
    lv_obj_t *btn;
    lv_obj_t *label;

    screen_data = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen_data);
    lv_obj_add_style(screen_data, &style_screen, 0);

    label_time_data = CreateLineLabel(screen_data, 10, 8, 138, UI_TXT_TIME_EMPTY);
    label_location_data = CreateLineLabel(screen_data, 10, 24, 150, UI_TXT_ADDR_CQ);
    label_weather_data = CreateValueLabel(screen_data, 10, 40, UI_TXT_WEATHER_EMPTY);
    icon_weather_data = lv_obj_create(screen_data);
    lv_obj_remove_style_all(icon_weather_data);
    UpdateWeatherIcon(icon_weather_data, NULL);
    lv_obj_align_to(icon_weather_data, label_weather_data, LV_ALIGN_OUT_RIGHT_MID, 4, 0);

    btn = lv_btn_create(screen_data);
    lv_obj_add_style(btn, &style_button, 0);
    lv_obj_set_size(btn, 66, 28);
    lv_obj_set_pos(btn, 164, 8);
    lv_obj_add_event_cb(btn, NextButtonEvent, LV_EVENT_CLICKED, NULL);
    label = lv_label_create(btn);
    lv_label_set_text(label, UI_TXT_CONTROL);
    lv_obj_add_style(label, &style_button_label, 0);
    lv_obj_center(label);

    label_temp = CreateValueBox(screen_data, 10, 70, 104, 32, UI_TXT_TEMP_EMPTY_LINE, NULL);
    label_hum = CreateValueBox(screen_data, 124, 70, 104, 32, UI_TXT_HUM_EMPTY_LINE, NULL);
    label_co2 = CreateValueBox(screen_data, 10, 108, 104, 32, UI_TXT_CO2_EMPTY_LINE, NULL);
    label_light = CreateValueBox(screen_data, 124, 108, 104, 32, UI_TXT_LIGHT_ZERO, NULL);
    label_soil = CreateValueBox(screen_data, 10, 146, 218, 38, UI_TXT_SOIL_ZERO, NULL);

    bar_light = lv_bar_create(screen_data);
    lv_obj_set_size(bar_light, 104, 7);
    lv_obj_set_pos(bar_light, 124, 136);
    lv_obj_add_style(bar_light, &style_bar, LV_PART_MAIN);
    lv_obj_add_style(bar_light, &style_bar_indic, LV_PART_INDICATOR);
    lv_bar_set_range(bar_light, 0, 100);
    lv_obj_add_flag(bar_light, LV_OBJ_FLAG_HIDDEN);

    bar_soil = lv_bar_create(screen_data);
    lv_obj_set_size(bar_soil, 218, 8);
    lv_obj_set_pos(bar_soil, 10, 172);
    lv_obj_add_style(bar_soil, &style_bar, LV_PART_MAIN);
    lv_obj_add_style(bar_soil, &style_bar_indic, LV_PART_INDICATOR);
    lv_bar_set_range(bar_soil, 0, 100);
    lv_obj_add_flag(bar_soil, LV_OBJ_FLAG_HIDDEN);

    (void)label;
    CreateAdviceBox(screen_data, 194, &label_ai_data);
}

static void CreateDriveScreen(void)
{
    lv_obj_t *btn;
    lv_obj_t *label;

    screen_drive = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen_drive);
    lv_obj_add_style(screen_drive, &style_screen, 0);

    label_time_drive = CreateLineLabel(screen_drive, 10, 8, 138, UI_TXT_TIME_EMPTY);
    label_location_drive = CreateLineLabel(screen_drive, 10, 24, 150, UI_TXT_ADDR_CQ);
    label_weather_drive = CreateValueLabel(screen_drive, 10, 40, UI_TXT_WEATHER_EMPTY);
    icon_weather_drive = lv_obj_create(screen_drive);
    lv_obj_remove_style_all(icon_weather_drive);
    UpdateWeatherIcon(icon_weather_drive, NULL);
    lv_obj_align_to(icon_weather_drive, label_weather_drive, LV_ALIGN_OUT_RIGHT_MID, 4, 0);

    btn = lv_btn_create(screen_drive);
    lv_obj_add_style(btn, &style_button, 0);
    lv_obj_set_size(btn, 66, 28);
    lv_obj_set_pos(btn, 164, 8);
    lv_obj_add_event_cb(btn, PrevButtonEvent, LV_EVENT_CLICKED, NULL);
    label = lv_label_create(btn);
    lv_label_set_text(label, UI_TXT_DATA);
    lv_obj_add_style(label, &style_button_label, 0);
    lv_obj_center(label);

    label_mode = CreateValueBox(screen_drive, 10, 70, 104, 32, UI_TXT_CONTROL_MODE, &box_mode);
    label_window = CreateValueBox(screen_drive, 124, 70, 104, 32, UI_TXT_WINDOW, &box_window);
    label_pump = CreateValueBox(screen_drive, 10, 108, 104, 32, UI_TXT_PUMP, &box_pump);
    label_fan = CreateValueBox(screen_drive, 124, 108, 104, 32, UI_TXT_FAN, &box_fan);
    label_lamp = CreateValueBox(screen_drive, 10, 146, 104, 32, UI_TXT_LAMP, &box_lamp);
    label_buzzer = CreateValueBox(screen_drive, 124, 146, 104, 32, UI_TXT_BUZZER, &box_buzzer);
    AddControlEvent(box_mode);
    AddControlEvent(box_window);
    AddControlEvent(box_pump);
    AddControlEvent(box_fan);
    AddControlEvent(box_lamp);
    AddControlEvent(box_buzzer);

    (void)label;
    CreateAdviceBox(screen_drive, 194, &label_ai_drive);
}

void DisplayUI_Init(void)
{
    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();
    InitStyles();
    CreateDriveScreen();
    CreateDataScreen();
    lv_scr_load(screen_data);
    ui_ready = 1U;
}

void DisplayUI_ShowStatus(const char *status)
{
    (void)status;
}

void DisplayUI_Update(const SensorData_t *data, const Esp8266Info_t *info)
{
    char text[128];
    char clock_text[32];
    const char *weather_src = NULL;
    ActuatorState_t state;
    int light;
    int soil;

    if (!ui_ready || data == NULL) {
        return;
    }

    if (!Esp8266_GetClock(clock_text, sizeof(clock_text))) {
        if (info != NULL && info->time_ok) {
            snprintf(clock_text, sizeof(clock_text), "%s", info->time);
        } else {
            clock_text[0] = '\0';
        }
    }

    if (info != NULL && info->weather_ok && info->weather_text[0] != '\0') {
        weather_src = info->weather_text;
    } else {
        weather_src = NULL;
    }
    SetTopInfo(clock_text, weather_src);

    if (data->dht11_ok) {
        snprintf(text, sizeof(text), UI_FMT_TEMP_VALUE, data->temperature / 10, data->temperature < 0 ? -data->temperature % 10 : data->temperature % 10);
        SetLabelText(label_temp, text);
        snprintf(text, sizeof(text), UI_FMT_HUM_VALUE, data->humidity / 10, data->humidity < 0 ? -data->humidity % 10 : data->humidity % 10);
        SetLabelText(label_hum, text);
    } else {
        SetLabelText(label_temp, UI_TXT_TEMP_EMPTY_LINE);
        SetLabelText(label_hum, UI_TXT_HUM_EMPTY_LINE);
    }

    if (data->co2_ok) {
        snprintf(text, sizeof(text), UI_FMT_CO2_VALUE, data->co2_ppm);
        SetLabelText(label_co2, text);
    } else {
        SetLabelText(label_co2, UI_TXT_CO2_EMPTY_LINE);
    }

    light = LightPercent(data->light_adc);
    soil = SoilPercent(data->soil_adc);
    snprintf(text, sizeof(text), UI_FMT_LIGHT_VALUE, light);
    SetLabelText(label_light, text);
    lv_bar_set_value(bar_light, light, LV_ANIM_OFF);

    snprintf(text, sizeof(text), UI_FMT_SOIL_VALUE, soil);
    SetLabelText(label_soil, text);
    lv_bar_set_value(bar_soil, soil, LV_ANIM_OFF);

    SetLabelText(label_ai_data, LocalDataAiText(data, weather_src));

    Actuators_GetState(&state);
    snprintf(text, sizeof(text), UI_FMT_MODE_SHORT, BluetoothLink_IsAutoMode() ? UI_TXT_AUTO : UI_TXT_MANUAL);
    SetLabelText(label_mode, text);
    snprintf(text, sizeof(text), UI_FMT_PUMP_VALUE, state.pump_on ? UI_TXT_ON : UI_TXT_OFF);
    SetLabelText(label_pump, text);
    snprintf(text, sizeof(text), UI_FMT_FAN_VALUE, state.fan_on ? UI_TXT_ON : UI_TXT_OFF);
    SetLabelText(label_fan, text);
    snprintf(text, sizeof(text), UI_FMT_LAMP_STATE, state.pest_lamp_on ? UI_TXT_ON : UI_TXT_OFF);
    SetLabelText(label_lamp, text);
    snprintf(text, sizeof(text), UI_FMT_WINDOW_STATE, state.sky_window_state >= 2U ? UI_TXT_FULL_OPEN : (state.sky_window_state == 1U ? UI_TXT_HALF_OPEN : UI_TXT_CLOSED));
    SetLabelText(label_window, text);
    snprintf(text, sizeof(text), UI_FMT_BUZZER_STATE, state.buzzer_mode == 1U ? UI_TXT_LOCUST : (state.buzzer_mode == 2U ? UI_TXT_CABBAGE : UI_TXT_OFF));
    SetLabelText(label_buzzer, text);

    SetLabelText(label_ai_drive, LocalControlAiText(data, weather_src));
}

void DisplayUI_Process(void)
{
    if (!ui_ready) {
        return;
    }

    if (pending_page == 1U && screen_data != NULL) {
        pending_page = 0U;
        lv_scr_load(screen_data);
    } else if (pending_page == 2U && screen_drive != NULL) {
        pending_page = 0U;
        lv_scr_load(screen_drive);
    }

    lv_timer_handler();
}
