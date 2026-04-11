/*
 * tft_ili9341.h
 *
 *  Created on: 31 thg 3, 2026
 *      Author: khanh
 */

#ifndef INC_TFT_ILI9341_H_
#define INC_TFT_ILI9341_H_

#include "main.h"              // 🔥 thêm dòng này
#include "stm32f4xx_hal.h"
#include <stdint.h>
#include "font.h"

// ===== TFT SIZE =====
#define TFT_WIDTH   240
#define TFT_HEIGHT  320

// ===== TFT PIN =====
#define TFT_CS_PORT     GPIOB
#define TFT_CS_PIN      CS_Pin

#define TFT_DC_PORT     GPIOB
#define TFT_DC_PIN      DC_Pin

#define TFT_RST_PORT    GPIOB
#define TFT_RST_PIN     RESET_Pin

extern SPI_HandleTypeDef *tft_spi;
// ===== COLOR =====
#define BLACK   0x0000
#define WHITE   0xFFFF
#define RED     0xF800
#define GREEN   0x07E0
#define BLUE    0x001F
#define YELLOW  0xFFE0

#define CYAN        0x07FF
#define MAGENTA     0xF81F
#define ORANGE      0xFD20
#define PINK        0xF81F
#define PURPLE      0x780F
#define BROWN       0xA145
#define GRAY        0x8410
#define DARKGRAY    0x4208
#define LIGHTGRAY   0xC618

#define NAVY        0x000F
#define DARKGREEN   0x03E0
#define DARKCYAN    0x03EF
#define MAROON      0x7800
#define OLIVE       0x7BE0

#define GOLD        0xFEA0
#define SKYBLUE     0x867D
#define VIOLET      0x915C
#define LIME        0x07E0

#define BG_COLOR    0x0841   // nền tối xám xanh
#define BTN_COLOR   0x2D6B   // xanh dịu
#define TXT_COLOR   0xFFFF   // trắng
#define ALERT_OK    0x07E0   // xanh lá
#define ALERT_ERR   0xF800   // đỏ
#define WARNING     0xFD20   // cam

// ===== API =====
void TFT_Init(SPI_HandleTypeDef *spi);
void TFT_FillScreen(uint16_t color);
void TFT_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void TFT_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
		uint16_t color);
void TFT_DrawString(uint16_t x, uint16_t y, char *str, FontDef *font,
		uint16_t color, uint16_t bg);
void TFT_DrawString(uint16_t x, uint16_t y, char *str, FontDef *font,
		uint16_t color, uint16_t bg);
#endif /* INC_TFT_ILI9341_H_ */
