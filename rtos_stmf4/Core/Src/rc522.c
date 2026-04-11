/*
 * rc522.c
 *
 *  Created on: 2 thg 4, 2026
 *      Author: khanh
 */

#include "rc522.h"

// ===== LOW LEVEL =====
static void RC522_Select() {
	HAL_GPIO_WritePin(RC522_CS_PORT, RC522_CS_PIN, GPIO_PIN_RESET);
}

static void RC522_Unselect() {
	HAL_GPIO_WritePin(RC522_CS_PORT, RC522_CS_PIN, GPIO_PIN_SET);
}

// ===== SPI =====
void RC522_WriteRegister(uint8_t addr, uint8_t val) {
	uint8_t data[2];

	data[0] = (addr << 1) & 0x7E;
	data[1] = val;

	RC522_Select();
	HAL_SPI_Transmit(&hspi1, data, 2, HAL_MAX_DELAY);
	RC522_Unselect();
}

uint8_t RC522_ReadRegister(uint8_t addr) {
	uint8_t tx = ((addr << 1) & 0x7E) | 0x80;
	uint8_t rx = 0;

	RC522_Select();
	HAL_SPI_Transmit(&hspi1, &tx, 1, HAL_MAX_DELAY);
	HAL_SPI_Receive(&hspi1, &rx, 1, HAL_MAX_DELAY);
	RC522_Unselect();

	return rx;
}

// ===== BASIC =====
void RC522_SetBitMask(uint8_t reg, uint8_t mask) {
	RC522_WriteRegister(reg, RC522_ReadRegister(reg) | mask);
}

void RC522_ClearBitMask(uint8_t reg, uint8_t mask) {
	RC522_WriteRegister(reg, RC522_ReadRegister(reg) & (~mask));
}

// ===== RESET =====
void RC522_Reset(void) {
	RC522_WriteRegister(RC522_CommandReg, PCD_Reset);
	HAL_Delay(50);
}

// ===== ANTENNA =====
void RC522_AntennaOn(void) {
	uint8_t temp = RC522_ReadRegister(RC522_TxControlReg);
	if (!(temp & 0x03)) {
		RC522_SetBitMask(RC522_TxControlReg, 0x03);
	}
}

void RC522_AntennaOff(void) {
	RC522_ClearBitMask(RC522_TxControlReg, 0x03);
}

// ===== INIT =====
void RC522_Init(void) {
	// CS idle
	HAL_GPIO_WritePin(RC522_CS_PORT, RC522_CS_PIN, GPIO_PIN_SET);

	// Reset cứng
	HAL_GPIO_WritePin(RC522_RST_PORT, RC522_RST_PIN, GPIO_PIN_RESET);
	HAL_Delay(10);
	HAL_GPIO_WritePin(RC522_RST_PORT, RC522_RST_PIN, GPIO_PIN_SET);
	HAL_Delay(50);

	RC522_Reset();

	RC522_WriteRegister(RC522_TModeReg, 0x8D);
	RC522_WriteRegister(RC522_TPrescalerReg, 0x3E);
	RC522_WriteRegister(RC522_TReloadRegL, 30);
	RC522_WriteRegister(RC522_TReloadRegH, 0);
	RC522_WriteRegister(RC522_TxAutoReg, 0x40);
	RC522_WriteRegister(RC522_ModeReg, 0x3D);

	RC522_AntennaOn();
}
// ===== CORE =====
uint8_t RC522_ToCard(uint8_t command, uint8_t *sendData, uint8_t sendLen,
		uint8_t *backData, uint16_t *backLen) {

	uint8_t status = MI_ERR;
	uint8_t irqEn = 0x00;
	uint8_t waitIRq = 0x00;
	uint8_t lastBits;
	uint8_t n;
	uint16_t i;

	if (command == PCD_Auth) {
		irqEn = 0x12;
		waitIRq = 0x10;
	} else if (command == PCD_Transceive) {
		irqEn = 0x77;
		waitIRq = 0x30;
	}

	RC522_WriteRegister(RC522_ComIEnReg, irqEn | 0x80);
	RC522_ClearBitMask(RC522_ComIrqReg, 0x80);
	RC522_SetBitMask(RC522_FIFOLevelReg, 0x80);

	RC522_WriteRegister(RC522_CommandReg, PCD_Idle);

	for (i = 0; i < sendLen; i++) {
		RC522_WriteRegister(RC522_FIFODataReg, sendData[i]);
	}

	RC522_WriteRegister(RC522_CommandReg, command);

	if (command == PCD_Transceive) {
		RC522_SetBitMask(RC522_BitFramingReg, 0x80);
	}

	i = 2000;
	do {
		n = RC522_ReadRegister(RC522_ComIrqReg);
		i--;
	} while ((i != 0) && !(n & 0x01) && !(n & waitIRq));

	RC522_ClearBitMask(RC522_BitFramingReg, 0x80);

	if (i != 0) {
		if (!(RC522_ReadRegister(RC522_ErrorReg) & 0x1B)) {
			status = MI_OK;

			if (command == PCD_Transceive) {
				n = RC522_ReadRegister(RC522_FIFOLevelReg);
				lastBits = RC522_ReadRegister(RC522_ControlReg) & 0x07;

				*backLen = lastBits ? (n - 1) * 8 + lastBits : n * 8;

				if (n == 0)
					n = 1;
				if (n > 16)
					n = 16;

				for (i = 0; i < n; i++) {
					backData[i] = RC522_ReadRegister(RC522_FIFODataReg);
				}
			}
		}
	}

	return status;
}

// ===== REQUEST =====
uint8_t RC522_Request(uint8_t reqMode, uint8_t *TagType) {
	uint16_t backBits;

	RC522_WriteRegister(RC522_BitFramingReg, 0x07);

	TagType[0] = reqMode;

	if (RC522_ToCard(PCD_Transceive, TagType, 1, TagType, &backBits) != MI_OK
			|| backBits != 0x10)
		return MI_ERR;

	return MI_OK;
}

// ===== ANTICOLL =====
uint8_t RC522_Anticoll(uint8_t *serNum) {
	uint8_t status;
	uint8_t i;
	uint8_t check = 0;
	uint16_t len;

	RC522_WriteRegister(RC522_BitFramingReg, 0x00);

	serNum[0] = PICC_ANTICOLL;
	serNum[1] = 0x20;

	status = RC522_ToCard(PCD_Transceive, serNum, 2, serNum, &len);

	if (status == MI_OK) {
		for (i = 0; i < 4; i++)
			check ^= serNum[i];
		if (check != serNum[4])
			status = MI_ERR;
	}

	return status;
}

// ===== CHECK CARD =====
uint8_t RC522_Check(uint8_t *id) {
	uint8_t status;
	uint8_t type[2];

	status = RC522_Request(PICC_REQIDL, type);
	if (status != MI_OK)
		return MI_ERR;

	status = RC522_Anticoll(id);
	return status;
}
