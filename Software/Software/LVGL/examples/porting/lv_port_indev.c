/**
 * @file lv_port_indev.c
 */

#include "lv_port_indev.h"
#include "lcd_ili9341.h"
#include "spi.h"
#include "usart.h"
#include <string.h>

#define XPT2046_CMD_READ_X  0xD0U
#define XPT2046_CMD_READ_Y  0x90U

#define XPT2046_X_MIN       200U
#define XPT2046_X_MAX       3900U
#define XPT2046_Y_MIN       200U
#define XPT2046_Y_MAX       3900U
#define XPT2046_PRESS_MIN   80U
#define XPT2046_Y_OFFSET    0
#define TOUCH_CAL_X_MIN     11
#define TOUCH_CAL_X_MAX     220
#define TOUCH_CAL_Y_MIN     0
#define TOUCH_CAL_Y_MAX     319

static lv_indev_t *indev_touchpad;
static uint32_t last_touch_print_tick = 0U;

static void Touch_SendString(const char *text)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)text, (uint16_t)strlen(text), 100U);
}

static void Touch_SendUInt(uint32_t value)
{
    char tmp[10];
    char out[10];
    uint8_t i = 0U;
    uint8_t j = 0U;

    do {
        tmp[i++] = (char)('0' + (value % 10U));
        value /= 10U;
    } while (value > 0U && i < sizeof(tmp));

    while (i > 0U) {
        out[j++] = tmp[--i];
    }
    out[j] = '\0';
    Touch_SendString(out);
}

static void Touch_PrintPoint(uint16_t raw_x, uint16_t raw_y, lv_coord_t x, lv_coord_t y)
{
    uint32_t now = HAL_GetTick();

    if ((now - last_touch_print_tick) < 250U) {
        return;
    }
    last_touch_print_tick = now;

    Touch_SendString("TOUCH RAW_X:");
    Touch_SendUInt(raw_x);
    Touch_SendString(" RAW_Y:");
    Touch_SendUInt(raw_y);
    Touch_SendString(" X:");
    Touch_SendUInt((uint32_t)x);
    Touch_SendString(" Y:");
    Touch_SendUInt((uint32_t)y);
    Touch_SendString("\r\n");
}

static uint16_t Xpt2046_Read12(uint8_t cmd)
{
    uint8_t tx[3];
    uint8_t rx[3];

    tx[0] = cmd;
    tx[1] = 0U;
    tx[2] = 0U;
    rx[0] = 0U;
    rx[1] = 0U;
    rx[2] = 0U;

    HAL_GPIO_WritePin(LCD_TOUCH_CS_GPIO_Port, LCD_TOUCH_CS_Pin, GPIO_PIN_RESET);
    (void)HAL_SPI_TransmitReceive(&hspi2, tx, rx, 3U, 100U);
    HAL_GPIO_WritePin(LCD_TOUCH_CS_GPIO_Port, LCD_TOUCH_CS_Pin, GPIO_PIN_SET);

    return (uint16_t)(((uint16_t)rx[1] << 5) | ((uint16_t)rx[2] >> 3));
}

static uint16_t Xpt2046_Map(uint16_t value, uint16_t in_min, uint16_t in_max, uint16_t out_max)
{
    if (value <= in_min) {
        return 0U;
    }
    if (value >= in_max) {
        return out_max;
    }
    return (uint16_t)(((uint32_t)(value - in_min) * out_max) / (in_max - in_min));
}

static lv_coord_t Xpt2046_ClampCoord(int32_t value, uint16_t max)
{
    if (value < 0) {
        return 0;
    }
    if (value > (int32_t)max) {
        return (lv_coord_t)max;
    }
    return (lv_coord_t)value;
}

static lv_coord_t Xpt2046_CalibrateCoord(lv_coord_t value, int32_t in_min, int32_t in_max, uint16_t out_max)
{
    int32_t scaled;

    if (value <= in_min) {
        return 0;
    }
    if (value >= in_max) {
        return (lv_coord_t)out_max;
    }

    scaled = ((int32_t)value - in_min) * (int32_t)out_max / (in_max - in_min);
    return Xpt2046_ClampCoord(scaled, out_max);
}

static uint8_t Xpt2046_ReadPoint(lv_coord_t *x, lv_coord_t *y, uint16_t *raw_x_out, uint16_t *raw_y_out)
{
    uint16_t raw_x;
    uint16_t raw_y;
    lv_coord_t mapped_x;
    lv_coord_t mapped_y;

    raw_x = Xpt2046_Read12(XPT2046_CMD_READ_X);
    raw_y = Xpt2046_Read12(XPT2046_CMD_READ_Y);
    if (raw_x_out != NULL) {
        *raw_x_out = raw_x;
    }
    if (raw_y_out != NULL) {
        *raw_y_out = raw_y;
    }

    if (raw_x < XPT2046_PRESS_MIN || raw_y < XPT2046_PRESS_MIN) {
        return 0U;
    }

    mapped_x = (lv_coord_t)((ILI9341_WIDTH - 1U) - Xpt2046_Map(raw_x, XPT2046_X_MIN, XPT2046_X_MAX, ILI9341_WIDTH - 1U));
    mapped_y = Xpt2046_ClampCoord((int32_t)((ILI9341_HEIGHT - 1U) - Xpt2046_Map(raw_y, XPT2046_Y_MIN, XPT2046_Y_MAX, ILI9341_HEIGHT - 1U)) + XPT2046_Y_OFFSET,
                                  ILI9341_HEIGHT - 1U);

    *x = Xpt2046_CalibrateCoord(mapped_x, TOUCH_CAL_X_MIN, TOUCH_CAL_X_MAX, ILI9341_WIDTH - 1U);
    *y = Xpt2046_CalibrateCoord(mapped_y, TOUCH_CAL_Y_MIN, TOUCH_CAL_Y_MAX, ILI9341_HEIGHT - 1U);
    return 1U;
}

static void touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    static lv_coord_t last_x = 0;
    static lv_coord_t last_y = 0;
    lv_coord_t x = 0;
    lv_coord_t y = 0;
    uint16_t raw_x = 0U;
    uint16_t raw_y = 0U;

    (void)indev_drv;
    if (Xpt2046_ReadPoint(&x, &y, &raw_x, &raw_y)) {
        last_x = x;
        last_y = y;
        Touch_PrintPoint(raw_x, raw_y, x, y);
        data->point.x = last_x;
        data->point.y = last_y;
        data->state = LV_INDEV_STATE_PR;
    } else {
        data->point.x = last_x;
        data->point.y = last_y;
        data->state = LV_INDEV_STATE_REL;
    }
}

void lv_port_indev_init(void)
{
    static lv_indev_drv_t indev_drv;

    HAL_GPIO_WritePin(LCD_TOUCH_CS_GPIO_Port, LCD_TOUCH_CS_Pin, GPIO_PIN_SET);
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    indev_touchpad = lv_indev_drv_register(&indev_drv);
    (void)indev_touchpad;
}
