/*
 * tft_ili9341.c
 *
 *  Created on: 31 thg 3, 2026
 *      Author: khanh
 */

#include "tft_ili9341.h"
#include <stddef.h>

SPI_HandleTypeDef *tft_spi;
// ===== LOW LEVEL =====
static void TFT_Select() {
	HAL_GPIO_WritePin(TFT_CS_PORT, TFT_CS_PIN, GPIO_PIN_RESET);
}

static void TFT_Unselect() {
	HAL_GPIO_WritePin(TFT_CS_PORT, TFT_CS_PIN, GPIO_PIN_SET);
}

static void TFT_DC_Command() {
	HAL_GPIO_WritePin(TFT_DC_PORT, TFT_DC_PIN, GPIO_PIN_RESET);
}

static void TFT_DC_Data() {
	HAL_GPIO_WritePin(TFT_DC_PORT, TFT_DC_PIN, GPIO_PIN_SET);
}

static void TFT_Reset() {
	HAL_GPIO_WritePin(TFT_RST_PORT, TFT_RST_PIN, GPIO_PIN_RESET);
	HAL_Delay(5);
	HAL_GPIO_WritePin(TFT_RST_PORT, TFT_RST_PIN, GPIO_PIN_SET);
}

// ===== GỬI DATA =====
static void TFT_WriteCommand(uint8_t cmd) {
	TFT_DC_Command();
	TFT_Select();
	HAL_SPI_Transmit(tft_spi, &cmd, 1, 100);
	TFT_Unselect();
}

static void TFT_WriteData(uint8_t *buff, size_t size) {
	TFT_DC_Data();
	TFT_Select();
	HAL_SPI_Transmit(tft_spi, buff, size, 100);
	TFT_Unselect();
}

// ===== SET WINDOW =====
static void TFT_SetAddrWindow(uint16_t x0, uint16_t y0, uint16_t x1,
		uint16_t y1) {
	uint8_t data[4];

	TFT_WriteCommand(0x2A); // Column
	data[0] = x0 >> 8;
	data[1] = x0 & 0xFF;
	data[2] = x1 >> 8;
	data[3] = x1 & 0xFF;
	TFT_WriteData(data, 4);

	TFT_WriteCommand(0x2B); // Row
	data[0] = y0 >> 8;
	data[1] = y0 & 0xFF;
	data[2] = y1 >> 8;
	data[3] = y1 & 0xFF;
	TFT_WriteData(data, 4);

	TFT_WriteCommand(0x2C); // Write RAM
}

// ===== INIT =====
void TFT_Init(SPI_HandleTypeDef *spi) {
	tft_spi = spi;

	TFT_Reset();

	TFT_WriteCommand(0x01);
	HAL_Delay(100);

	TFT_WriteCommand(0x28);

	TFT_WriteCommand(0x3A);
	uint8_t data = 0x55;
	TFT_WriteData(&data, 1);

	TFT_WriteCommand(0x36);
	data = 0x48;
	TFT_WriteData(&data, 1);

	TFT_WriteCommand(0x11);
	HAL_Delay(120);

	TFT_WriteCommand(0x29);
}

// ===== DRAW PIXEL =====
void TFT_DrawPixel(uint16_t x, uint16_t y, uint16_t color) {
	if (x >= TFT_WIDTH || y >= TFT_HEIGHT)
		return;

	TFT_SetAddrWindow(x, y, x, y);

	uint8_t data[2] = { color >> 8, color & 0xFF };
	TFT_WriteData(data, 2);
}

// ===== FILL SCREEN =====
//void TFT_FillScreen(uint16_t color) {
//	TFT_SetAddrWindow(0, 0, TFT_WIDTH - 1, TFT_HEIGHT - 1);
//
//	uint8_t data[2] = { color >> 8, color & 0xFF };
//
//	TFT_DC_Data();
//	TFT_Select();
//
//	for (uint32_t i = 0; i < TFT_WIDTH * TFT_HEIGHT; i++) {
//		HAL_SPI_Transmit(tft_spi, data, 2, 10);
//	}
//
//	TFT_Unselect();
//}
void TFT_FillScreen(uint16_t color) {
	// Dùng lại hàm DrawRect đã được tối ưu bên dưới cho gọn code
	TFT_DrawRect(0, 0, TFT_WIDTH, TFT_HEIGHT, color);
}
// ===== DRAW RECT =====
//void TFT_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
//		uint16_t color) {
//	for (uint16_t i = 0; i < h; i++) {
//		for (uint16_t j = 0; j < w; j++) {
//			TFT_DrawPixel(x + j, y + i, color);
//		}
//	}
//}
void TFT_DrawRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
		uint16_t color) {
	if (x >= TFT_WIDTH || y >= TFT_HEIGHT)
		return;
	if (x + w - 1 >= TFT_WIDTH)
		w = TFT_WIDTH - x;
	if (y + h - 1 >= TFT_HEIGHT)
		h = TFT_HEIGHT - y;

	// Cài đặt vùng cửa sổ một lần duy nhất
	TFT_SetAddrWindow(x, y, x + w - 1, y + h - 1);

	// Tạo bộ đệm 64 pixel (128 bytes) để truyền nhanh qua SPI
	uint8_t buffer[128];
	for (int i = 0; i < 64; i++) {
		buffer[i * 2] = color >> 8;
		buffer[i * 2 + 1] = color & 0xFF;
	}

	uint32_t total_pixels = w * h;
	uint32_t chunks = total_pixels / 64;     // Số khối 64 pixel
	uint32_t remainder = total_pixels % 64;  // Số pixel lẻ còn lại

	TFT_DC_Data();
	TFT_Select();

	// Bơm dữ liệu liên tục theo khối
	for (uint32_t i = 0; i < chunks; i++) {
		HAL_SPI_Transmit(tft_spi, buffer, 128, HAL_MAX_DELAY);
	}
	// Bơm nốt phần lẻ
	if (remainder) {
		HAL_SPI_Transmit(tft_spi, buffer, remainder * 2, HAL_MAX_DELAY);
	}

	TFT_Unselect();
}

static uint16_t UTF8_Decode(const char **str) {
	uint8_t c = (uint8_t) **str;

	if (c < 0x80) {
		(*str)++;
		return c;
	} else if ((c & 0xE0) == 0xC0) {
		uint16_t res = ((c & 0x1F) << 6) | ((*str)[1] & 0x3F);
		(*str) += 2;
		return res;
	} else if ((c & 0xF0) == 0xE0) {
		uint16_t res = ((c & 0x0F) << 12) | (((*str)[1] & 0x3F) << 6)
				| ((*str)[2] & 0x3F);
		(*str) += 3;
		return res;
	}

	(*str)++;
	return '?';
}

static int Font_FindChar(FontDef *font, uint16_t unicode) {
	const char *map = font->char_map;
	int index = 0;

	while (*map) {
		const char *tmp = map;
		uint16_t code = UTF8_Decode(&tmp);

		if (code == unicode) {
			return index;
		}

		map = tmp;
		index++;
	}

	return -1; // không tìm thấy
}

void TFT_DrawChar(uint16_t x, uint16_t y, uint16_t unicode, FontDef *font, uint16_t color, uint16_t bg) {
    int index = Font_FindChar(font, unicode);
    if (index < 0) return;

    uint8_t bytes_per_col = (font->height + 7) / 8;
    uint8_t *ch = (uint8_t*)font->data + index * font->width * bytes_per_col;

    TFT_SetAddrWindow(x, y, x + font->width - 1, y + font->height - 1);
    TFT_DC_Data();
    TFT_Select();

    // Tạo buffer tạm cho toàn bộ ký tự (ví dụ font tối đa 32x32 = 1024 pixels * 2 bytes = 2048 bytes)
    // Nếu RAM eo hẹp, bạn có thể vẽ theo từng dòng (row-by-row)
    for (uint8_t j = 0; j < font->height; j++) {
        uint8_t line_buffer[font->width * 2]; // Buffer cho 1 dòng của ký tự
        for (uint8_t i = 0; i < font->width; i++) {
            uint8_t byte = ch[i * bytes_per_col + (j / 8)];
            uint8_t bit = (byte >> (j % 8)) & 0x01;
            uint16_t pixel = bit ? color : bg;

            line_buffer[i * 2] = pixel >> 8;
            line_buffer[i * 2 + 1] = pixel & 0xFF;
        }
        HAL_SPI_Transmit(tft_spi, line_buffer, font->width * 2, 10);
    }
    TFT_Unselect();
}

void TFT_DrawString(uint16_t x, uint16_t y, char *str, FontDef *font,
		uint16_t color, uint16_t bg) {

	while (*str) {
		uint16_t unicode = UTF8_Decode((const char**) &str);

		TFT_DrawChar(x, y, unicode, font, color, bg);
		x += font->width;
	}
}
