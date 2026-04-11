/*
 * rc522.h
 *
 *  Created on: 2 thg 4, 2026
 *      Author: khanh
 */

#ifndef RC522_H
#define RC522_H

#include "stm32f4xx_hal.h"

// ===== SPI HANDLE =====
extern SPI_HandleTypeDef hspi1;

// ===== GPIO CONFIG =====
#define RC522_CS_PORT GPIOA
#define RC522_CS_PIN  GPIO_PIN_4

#define RC522_RST_PORT GPIOA
#define RC522_RST_PIN  GPIO_PIN_8

// ===== STATUS =====
#define MI_OK       0
#define MI_NOTAGERR 1
#define MI_ERR      2

// ===== REGISTER =====
#define RC522_CommandReg       0x01
#define RC522_ComIEnReg        0x02
#define RC522_ComIrqReg        0x04
#define RC522_ErrorReg         0x06
#define RC522_Status1Reg       0x07
#define RC522_Status2Reg       0x08
#define RC522_FIFODataReg      0x09
#define RC522_FIFOLevelReg     0x0A
#define RC522_ControlReg       0x0C
#define RC522_BitFramingReg    0x0D
#define RC522_ModeReg          0x11
#define RC522_TxControlReg     0x14
#define RC522_TxAutoReg        0x15
#define RC522_TModeReg         0x2A
#define RC522_TPrescalerReg    0x2B
#define RC522_TReloadRegH      0x2C
#define RC522_TReloadRegL      0x2D

// ===== COMMAND =====
#define PCD_Idle        0x00
#define PCD_Auth        0x0E
#define PCD_Transceive  0x0C
#define PCD_Reset       0x0F

// ===== PICC =====
#define PICC_REQIDL     0x26
#define PICC_ANTICOLL   0x93

// ===== FUNCTION =====
void RC522_Init(void);
void RC522_Reset(void);
void RC522_WriteRegister(uint8_t addr, uint8_t val);
uint8_t RC522_ReadRegister(uint8_t addr);

void RC522_SetBitMask(uint8_t reg, uint8_t mask);
void RC522_ClearBitMask(uint8_t reg, uint8_t mask);

void RC522_AntennaOn(void);
void RC522_AntennaOff(void);

uint8_t RC522_ToCard(uint8_t command, uint8_t *sendData, uint8_t sendLen,
                     uint8_t *backData, uint16_t *backLen);

uint8_t RC522_Request(uint8_t reqMode, uint8_t *TagType);
uint8_t RC522_Anticoll(uint8_t *serNum);
uint8_t RC522_Check(uint8_t *id);

#endif /* INC_RC522_H_ */
