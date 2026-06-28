#ifndef __LCD_ILI9341_H__
#define __LCD_ILI9341_H__

#include "main.h"

#define ILI9341_WIDTH   240U
#define ILI9341_HEIGHT  320U

#define ILI9341_BLACK   0x0000U
#define ILI9341_WHITE   0xFFFFU
#define ILI9341_RED     0xF800U
#define ILI9341_GREEN   0x07E0U
#define ILI9341_BLUE    0x001FU
#define ILI9341_CYAN    0x07FFU
#define ILI9341_MAGENTA 0xF81FU
#define ILI9341_YELLOW  0xFFE0U
#define ILI9341_ORANGE  0xFD20U
#define ILI9341_GRAY    0x8410U

void ILI9341_Init(void);
void ILI9341_FillScreen(uint16_t color);
void ILI9341_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ILI9341_DrawRGB565(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *colors);
void ILI9341_DrawString(uint16_t x, uint16_t y, const char *text,
                        uint16_t color, uint16_t bg, uint8_t scale);

#endif /* __LCD_ILI9341_H__ */
