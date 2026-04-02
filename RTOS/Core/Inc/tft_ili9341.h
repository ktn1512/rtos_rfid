/*
 * tft_ili9341.h
 *
 *  Created on: 31 thg 3, 2026
 *      Author: khanh
 */

#ifndef INC_TFT_ILI9341_H_
#define INC_TFT_ILI9341_H_

#include "main.h"              // 🔥 thêm dòng này
#include "stm32f1xx_hal.h"
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

// ===== API =====
void TFT_Init(SPI_HandleTypeDef *spi);
void TFT_FillScreen(uint16_t color);
void TFT_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void TFT_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
		uint16_t color);
void TFT_DrawChar(uint16_t x, uint16_t y, char c, FontDef *font, uint16_t color,
		uint16_t bg);
void TFT_DrawString(uint16_t x, uint16_t y, char *str, FontDef *font,
		uint16_t color, uint16_t bg);
#endif /* INC_TFT_ILI9341_H_ */
