/*
 * font.h
 *
 * Created on: Dec 2, 2025
 * Author: khanh
 */

#ifndef INC_FONT_H_
#define INC_FONT_H_

#include <stdint.h>
#include <stddef.h> // Để dùng NULL

typedef struct {
	uint8_t width;        // Chiều rộng mỗi ký tự
	uint8_t height;       // Chiều cao mỗi ký tự
	const uint8_t *data;  // Con trỏ tới mảng dữ liệu font

	/* * [MỚI] Con trỏ tới chuỗi ánh xạ (Map) dùng cho UTF-8.
	 * - Nếu là Font tiếng Việt: Trỏ tới chuỗi chứa các ký tự (VD: " !#...áà...")
	 * - Nếu là Font ASCII thường: Để là NULL
	 */
	const char *char_map;
} FontDef;

extern FontDef Font_5x7;
extern FontDef Font_11x16;
extern FontDef Font_11x16_times;
extern FontDef Font_8x12_times;
extern FontDef Font_10x12_times;
extern FontDef Font_9x16_times;
#endif /* INC_FONT_H_ */
