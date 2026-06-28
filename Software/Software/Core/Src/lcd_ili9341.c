#include "lcd_ili9341.h"
#include "spi.h"
#include <stddef.h>

#define ILI9341_CMD_SWRESET 0x01U
#define ILI9341_CMD_SLPOUT  0x11U
#define ILI9341_CMD_DISPON  0x29U
#define ILI9341_CMD_CASET   0x2AU
#define ILI9341_CMD_PASET   0x2BU
#define ILI9341_CMD_RAMWR   0x2CU
#define ILI9341_CMD_MADCTL  0x36U
#define ILI9341_CMD_PIXFMT  0x3AU

#define ILI9341_MADCTL_MX   0x40U
#define ILI9341_MADCTL_BGR  0x08U

static const uint8_t font5x7[96][5] = {
    {0x00,0x00,0x00,0x00,0x00}, {0x00,0x00,0x5F,0x00,0x00}, {0x00,0x07,0x00,0x07,0x00}, {0x14,0x7F,0x14,0x7F,0x14},
    {0x24,0x2A,0x7F,0x2A,0x12}, {0x23,0x13,0x08,0x64,0x62}, {0x36,0x49,0x55,0x22,0x50}, {0x00,0x05,0x03,0x00,0x00},
    {0x00,0x1C,0x22,0x41,0x00}, {0x00,0x41,0x22,0x1C,0x00}, {0x14,0x08,0x3E,0x08,0x14}, {0x08,0x08,0x3E,0x08,0x08},
    {0x00,0x50,0x30,0x00,0x00}, {0x08,0x08,0x08,0x08,0x08}, {0x00,0x60,0x60,0x00,0x00}, {0x20,0x10,0x08,0x04,0x02},
    {0x3E,0x51,0x49,0x45,0x3E}, {0x00,0x42,0x7F,0x40,0x00}, {0x42,0x61,0x51,0x49,0x46}, {0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10}, {0x27,0x45,0x45,0x45,0x39}, {0x3C,0x4A,0x49,0x49,0x30}, {0x01,0x71,0x09,0x05,0x03},
    {0x36,0x49,0x49,0x49,0x36}, {0x06,0x49,0x49,0x29,0x1E}, {0x00,0x36,0x36,0x00,0x00}, {0x00,0x56,0x36,0x00,0x00},
    {0x08,0x14,0x22,0x41,0x00}, {0x14,0x14,0x14,0x14,0x14}, {0x00,0x41,0x22,0x14,0x08}, {0x02,0x01,0x51,0x09,0x06},
    {0x32,0x49,0x79,0x41,0x3E}, {0x7E,0x11,0x11,0x11,0x7E}, {0x7F,0x49,0x49,0x49,0x36}, {0x3E,0x41,0x41,0x41,0x22},
    {0x7F,0x41,0x41,0x22,0x1C}, {0x7F,0x49,0x49,0x49,0x41}, {0x7F,0x09,0x09,0x09,0x01}, {0x3E,0x41,0x49,0x49,0x7A},
    {0x7F,0x08,0x08,0x08,0x7F}, {0x00,0x41,0x7F,0x41,0x00}, {0x20,0x40,0x41,0x3F,0x01}, {0x7F,0x08,0x14,0x22,0x41},
    {0x7F,0x40,0x40,0x40,0x40}, {0x7F,0x02,0x0C,0x02,0x7F}, {0x7F,0x04,0x08,0x10,0x7F}, {0x3E,0x41,0x41,0x41,0x3E},
    {0x7F,0x09,0x09,0x09,0x06}, {0x3E,0x41,0x51,0x21,0x5E}, {0x7F,0x09,0x19,0x29,0x46}, {0x46,0x49,0x49,0x49,0x31},
    {0x01,0x01,0x7F,0x01,0x01}, {0x3F,0x40,0x40,0x40,0x3F}, {0x1F,0x20,0x40,0x20,0x1F}, {0x3F,0x40,0x38,0x40,0x3F},
    {0x63,0x14,0x08,0x14,0x63}, {0x07,0x08,0x70,0x08,0x07}, {0x61,0x51,0x49,0x45,0x43}, {0x00,0x7F,0x41,0x41,0x00},
    {0x02,0x04,0x08,0x10,0x20}, {0x00,0x41,0x41,0x7F,0x00}, {0x04,0x02,0x01,0x02,0x04}, {0x40,0x40,0x40,0x40,0x40},
    {0x00,0x01,0x02,0x04,0x00}, {0x20,0x54,0x54,0x54,0x78}, {0x7F,0x48,0x44,0x44,0x38}, {0x38,0x44,0x44,0x44,0x20},
    {0x38,0x44,0x44,0x48,0x7F}, {0x38,0x54,0x54,0x54,0x18}, {0x08,0x7E,0x09,0x01,0x02}, {0x0C,0x52,0x52,0x52,0x3E},
    {0x7F,0x08,0x04,0x04,0x78}, {0x00,0x44,0x7D,0x40,0x00}, {0x20,0x40,0x44,0x3D,0x00}, {0x7F,0x10,0x28,0x44,0x00},
    {0x00,0x41,0x7F,0x40,0x00}, {0x7C,0x04,0x18,0x04,0x78}, {0x7C,0x08,0x04,0x04,0x78}, {0x38,0x44,0x44,0x44,0x38},
    {0x7C,0x14,0x14,0x14,0x08}, {0x08,0x14,0x14,0x18,0x7C}, {0x7C,0x08,0x04,0x04,0x08}, {0x48,0x54,0x54,0x54,0x20},
    {0x04,0x3F,0x44,0x40,0x20}, {0x3C,0x40,0x40,0x20,0x7C}, {0x1C,0x20,0x40,0x20,0x1C}, {0x3C,0x40,0x30,0x40,0x3C},
    {0x44,0x28,0x10,0x28,0x44}, {0x0C,0x50,0x50,0x50,0x3C}, {0x44,0x64,0x54,0x4C,0x44}, {0x00,0x08,0x36,0x41,0x00},
    {0x00,0x00,0x7F,0x00,0x00}, {0x00,0x41,0x36,0x08,0x00}, {0x08,0x04,0x08,0x10,0x08}, {0x00,0x00,0x00,0x00,0x00}
};

static void ILI9341_Select(void)
{
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
}

static void ILI9341_Unselect(void)
{
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
}

static void ILI9341_WriteCommand(uint8_t cmd)
{
    HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET);
    ILI9341_Select();
    (void)HAL_SPI_Transmit(&hspi2, &cmd, 1U, 100U);
    ILI9341_Unselect();
}

static void ILI9341_WriteData(const uint8_t *data, uint16_t len)
{
    if (data == NULL || len == 0U) {
        return;
    }

    HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);
    ILI9341_Select();
    (void)HAL_SPI_Transmit(&hspi2, (uint8_t *)data, len, 1000U);
    ILI9341_Unselect();
}

static void ILI9341_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t data[4];

    data[0] = (uint8_t)(x0 >> 8);
    data[1] = (uint8_t)x0;
    data[2] = (uint8_t)(x1 >> 8);
    data[3] = (uint8_t)x1;
    ILI9341_WriteCommand(ILI9341_CMD_CASET);
    ILI9341_WriteData(data, sizeof(data));

    data[0] = (uint8_t)(y0 >> 8);
    data[1] = (uint8_t)y0;
    data[2] = (uint8_t)(y1 >> 8);
    data[3] = (uint8_t)y1;
    ILI9341_WriteCommand(ILI9341_CMD_PASET);
    ILI9341_WriteData(data, sizeof(data));

    ILI9341_WriteCommand(ILI9341_CMD_RAMWR);
}

static void ILI9341_SendInitCommand(uint8_t cmd, const uint8_t *data, uint8_t len)
{
    ILI9341_WriteCommand(cmd);
    ILI9341_WriteData(data, len);
}

void ILI9341_Init(void)
{
    static const uint8_t cmd_ef[] = {0x03, 0x80, 0x02};
    static const uint8_t cmd_cf[] = {0x00, 0xC1, 0x30};
    static const uint8_t cmd_ed[] = {0x64, 0x03, 0x12, 0x81};
    static const uint8_t cmd_e8[] = {0x85, 0x00, 0x78};
    static const uint8_t cmd_cb[] = {0x39, 0x2C, 0x00, 0x34, 0x02};
    static const uint8_t cmd_f7[] = {0x20};
    static const uint8_t cmd_ea[] = {0x00, 0x00};
    static const uint8_t cmd_c0[] = {0x23};
    static const uint8_t cmd_c1[] = {0x10};
    static const uint8_t cmd_c5[] = {0x3E, 0x28};
    static const uint8_t cmd_c7[] = {0x86};
    static const uint8_t cmd_36[] = {ILI9341_MADCTL_MX | ILI9341_MADCTL_BGR};
    static const uint8_t cmd_3a[] = {0x55};
    static const uint8_t cmd_b1[] = {0x00, 0x18};
    static const uint8_t cmd_b6[] = {0x08, 0x82, 0x27};
    static const uint8_t cmd_f2[] = {0x00};
    static const uint8_t cmd_26[] = {0x01};
    static const uint8_t cmd_e0[] = {0x0F,0x31,0x2B,0x0C,0x0E,0x08,0x4E,0xF1,0x37,0x07,0x10,0x03,0x0E,0x09,0x00};
    static const uint8_t cmd_e1[] = {0x00,0x0E,0x14,0x03,0x11,0x07,0x31,0xC1,0x48,0x08,0x0F,0x0C,0x31,0x36,0x0F};

    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_TOUCH_CS_GPIO_Port, LCD_TOUCH_CS_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_RESET);

    HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(10U);
    HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(20U);
    HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);
    HAL_Delay(120U);

    ILI9341_WriteCommand(ILI9341_CMD_SWRESET);
    HAL_Delay(150U);

    ILI9341_SendInitCommand(0xEF, cmd_ef, sizeof(cmd_ef));
    ILI9341_SendInitCommand(0xCF, cmd_cf, sizeof(cmd_cf));
    ILI9341_SendInitCommand(0xED, cmd_ed, sizeof(cmd_ed));
    ILI9341_SendInitCommand(0xE8, cmd_e8, sizeof(cmd_e8));
    ILI9341_SendInitCommand(0xCB, cmd_cb, sizeof(cmd_cb));
    ILI9341_SendInitCommand(0xF7, cmd_f7, sizeof(cmd_f7));
    ILI9341_SendInitCommand(0xEA, cmd_ea, sizeof(cmd_ea));
    ILI9341_SendInitCommand(0xC0, cmd_c0, sizeof(cmd_c0));
    ILI9341_SendInitCommand(0xC1, cmd_c1, sizeof(cmd_c1));
    ILI9341_SendInitCommand(0xC5, cmd_c5, sizeof(cmd_c5));
    ILI9341_SendInitCommand(0xC7, cmd_c7, sizeof(cmd_c7));
    ILI9341_SendInitCommand(ILI9341_CMD_MADCTL, cmd_36, sizeof(cmd_36));
    ILI9341_SendInitCommand(ILI9341_CMD_PIXFMT, cmd_3a, sizeof(cmd_3a));
    ILI9341_SendInitCommand(0xB1, cmd_b1, sizeof(cmd_b1));
    ILI9341_SendInitCommand(0xB6, cmd_b6, sizeof(cmd_b6));
    ILI9341_SendInitCommand(0xF2, cmd_f2, sizeof(cmd_f2));
    ILI9341_SendInitCommand(0x26, cmd_26, sizeof(cmd_26));
    ILI9341_SendInitCommand(0xE0, cmd_e0, sizeof(cmd_e0));
    ILI9341_SendInitCommand(0xE1, cmd_e1, sizeof(cmd_e1));

    ILI9341_WriteCommand(ILI9341_CMD_SLPOUT);
    HAL_Delay(120U);
    ILI9341_WriteCommand(ILI9341_CMD_DISPON);
    HAL_Delay(20U);

    HAL_GPIO_WritePin(LCD_BL_GPIO_Port, LCD_BL_Pin, GPIO_PIN_SET);
    ILI9341_FillScreen(ILI9341_BLACK);
}

void ILI9341_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    uint8_t line[ILI9341_WIDTH * 2U];
    uint16_t i;
    uint16_t row;

    if (x >= ILI9341_WIDTH || y >= ILI9341_HEIGHT || w == 0U || h == 0U) {
        return;
    }
    if ((uint32_t)x + w > ILI9341_WIDTH) {
        w = (uint16_t)(ILI9341_WIDTH - x);
    }
    if ((uint32_t)y + h > ILI9341_HEIGHT) {
        h = (uint16_t)(ILI9341_HEIGHT - y);
    }

    for (i = 0; i < w; i++) {
        line[i * 2U] = (uint8_t)(color >> 8);
        line[i * 2U + 1U] = (uint8_t)color;
    }

    ILI9341_SetAddressWindow(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));
    HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);
    ILI9341_Select();
    for (row = 0; row < h; row++) {
        (void)HAL_SPI_Transmit(&hspi2, line, (uint16_t)(w * 2U), 1000U);
    }
    ILI9341_Unselect();
}

void ILI9341_FillScreen(uint16_t color)
{
    ILI9341_FillRect(0U, 0U, ILI9341_WIDTH, ILI9341_HEIGHT, color);
}

void ILI9341_DrawRGB565(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *colors)
{
    uint32_t pixel_count;
    uint32_t offset = 0U;

    if (colors == NULL || x >= ILI9341_WIDTH || y >= ILI9341_HEIGHT || w == 0U || h == 0U) {
        return;
    }
    if ((uint32_t)x + w > ILI9341_WIDTH) {
        w = (uint16_t)(ILI9341_WIDTH - x);
    }
    if ((uint32_t)y + h > ILI9341_HEIGHT) {
        h = (uint16_t)(ILI9341_HEIGHT - y);
    }

    pixel_count = (uint32_t)w * (uint32_t)h;
    ILI9341_SetAddressWindow(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));
    HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);
    ILI9341_Select();
    while (pixel_count > 0U) {
        uint8_t chunk[128U * 2U];
        uint16_t count = pixel_count > 128U ? 128U : (uint16_t)pixel_count;
        uint16_t i;

        for (i = 0U; i < count; i++) {
            uint16_t color = colors[offset + i];
            chunk[i * 2U] = (uint8_t)(color >> 8);
            chunk[i * 2U + 1U] = (uint8_t)color;
        }
        (void)HAL_SPI_Transmit(&hspi2, chunk, (uint16_t)(count * 2U), 1000U);
        offset += count;
        pixel_count -= count;
    }
    ILI9341_Unselect();
}

static void ILI9341_DrawChar(uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bg, uint8_t scale)
{
    uint8_t pixels[6U * 3U * 8U * 3U * 2U];
    const uint8_t *bitmap;
    uint16_t pos = 0U;
    uint16_t draw_w;
    uint16_t draw_h;
    uint8_t col;
    uint8_t row;
    uint8_t sx;
    uint8_t sy;
    uint16_t pixel_color;

    if (scale == 0U) {
        scale = 1U;
    }
    if (scale > 3U) {
        scale = 3U;
    }
    if ((uint8_t)ch < 32U || (uint8_t)ch > 127U) {
        ch = '?';
    }
    if (x >= ILI9341_WIDTH || y >= ILI9341_HEIGHT) {
        return;
    }

    draw_w = (uint16_t)(6U * scale);
    draw_h = (uint16_t)(8U * scale);
    if ((uint32_t)x + draw_w > ILI9341_WIDTH || (uint32_t)y + draw_h > ILI9341_HEIGHT) {
        return;
    }

    bitmap = font5x7[(uint8_t)ch - 32U];
    for (row = 0; row < 8U; row++) {
        for (sy = 0; sy < scale; sy++) {
            for (col = 0; col < 6U; col++) {
                uint8_t bits = (col < 5U) ? bitmap[col] : 0U;
                pixel_color = ((bits & (1U << row)) != 0U) ? color : bg;
                for (sx = 0; sx < scale; sx++) {
                    pixels[pos++] = (uint8_t)(pixel_color >> 8);
                    pixels[pos++] = (uint8_t)pixel_color;
                }
            }
        }
    }

    ILI9341_SetAddressWindow(x, y, (uint16_t)(x + draw_w - 1U), (uint16_t)(y + draw_h - 1U));
    ILI9341_WriteData(pixels, pos);
}

void ILI9341_DrawString(uint16_t x, uint16_t y, const char *text,
                        uint16_t color, uint16_t bg, uint8_t scale)
{
    uint16_t cursor_x = x;
    uint16_t char_w;

    if (text == NULL) {
        return;
    }
    if (scale == 0U) {
        scale = 1U;
    }
    char_w = (uint16_t)(6U * scale);

    while (*text != '\0' && cursor_x < ILI9341_WIDTH) {
        ILI9341_DrawChar(cursor_x, y, *text, color, bg, scale);
        cursor_x = (uint16_t)(cursor_x + char_w);
        text++;
    }
}
