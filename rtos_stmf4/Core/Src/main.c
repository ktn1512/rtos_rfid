/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "rc522.h"
#include "tft_ili9341.h"
#include "font.h"
#include "stdio.h"
#include "string.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim3;

osThreadId defaultTaskHandle;
/* USER CODE BEGIN PV */
uint8_t uid[5];
uint8_t status;

#define MAX_CARDS 10
uint8_t card_list[MAX_CARDS][4];
uint8_t card_count = 0;

char buffer[50];

char password[5] = "1234";
char input_pass[5];

typedef enum {
	ACTION_NONE = 0,
	ACTION_SCAN,
	ACTION_PASSWORD,
	ACTION_ADD,
	ACTION_DELETE,
	ACTION_CHANGE_PASS
} Action_t;

typedef struct MenuItem {
	char *name;
	struct MenuItem *parent;
	struct MenuItem *child;
	struct MenuItem *next;
	Action_t action;
} MenuItem;

MenuItem *current_menu;
MenuItem *selected_item;

// ROOT
MenuItem menu_root = { "MỞ KHÓA CỬA RFID", NULL, NULL, NULL, ACTION_NONE };

// LEVEL 1
MenuItem menu_scan = { "QUẸT THẺ", &menu_root, NULL, NULL, ACTION_SCAN };
MenuItem menu_pass =
		{ "NHẬP MẬT KHẨU", &menu_root, NULL, NULL, ACTION_PASSWORD };
MenuItem menu_setting = { "CÀI ĐẶT", &menu_root, NULL, NULL, ACTION_NONE };

// LEVEL 2 (SETTING)
MenuItem menu_add = { "THÊM THẺ", &menu_setting, NULL, NULL, ACTION_ADD };
MenuItem menu_delete = { "XÓA THẺ", &menu_setting, NULL, NULL, ACTION_DELETE };
MenuItem menu_change = { "ĐỔI MẬT KHẨU", &menu_setting, NULL, NULL,
		ACTION_CHANGE_PASS };

char keypad_map[4][4] = { { '1', '4', '7', '*' }, { '2', '5', '8', '0' }, { '3',
		'6', '9', '#' }, { 'A', 'B', 'C', 'D' } };

uint8_t door_open = 0;
uint32_t door_time = 0;

osMutexId spiMutex;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_TIM3_Init(void);
void StartDefaultTask(void const *argument);

/* USER CODE BEGIN PFP */
void TFT_DrawMenu(void);
void Keypad_Task(void const *argument);
osThreadId keypadTaskHandle;
void RFID_Task(void const *argument);
uint8_t CheckPassword(void);

osThreadId rfidTaskHandle;
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
char Keypad_Scan(void) {
	uint16_t row_pins[4] = { R1_Pin, R2_Pin, R3_Pin, R4_Pin };
	uint16_t col_pins[4] = { C1_Pin, C2_Pin, C3_Pin, C4_Pin };

	for (int i = 0; i < 4; i++) {
		// set all row HIGH
		HAL_GPIO_WritePin(GPIOA, R1_Pin | R2_Pin | R3_Pin | R4_Pin,
				GPIO_PIN_SET);

		// kéo từng row xuống LOW
		HAL_GPIO_WritePin(GPIOA, row_pins[i], GPIO_PIN_RESET);

		for (int j = 0; j < 4; j++) {
			if (HAL_GPIO_ReadPin(GPIOB, col_pins[j]) == GPIO_PIN_RESET) {
				return keypad_map[i][j];
			}
		}
	}

	return 0;
}
char Keypad_GetKey(void) {
	char key = Keypad_Scan();

	if (key) {
		osDelay(150);
		while (Keypad_Scan())
			;
		return key;
	}

	return 0;
}

uint8_t compare_uid(uint8_t *a, uint8_t *b) {
	for (int i = 0; i < 4; i++) {
		if (a[i] != b[i])
			return 0;
	}
	return 1;
}

int find_card(uint8_t *uid) {
	for (int i = 0; i < card_count; i++) {
		if (compare_uid(uid, card_list[i])) {
			return i;
		}
	}
	return -1;
}

void add_card(uint8_t *uid) {
	if (card_count >= MAX_CARDS)
		return;

	if (find_card(uid) == -1) {
		memcpy(card_list[card_count], uid, 4);
		card_count++;
	}
}

void delete_card(uint8_t *uid) {
	int index = find_card(uid);

	if (index == -1)
		return;

	for (int i = index; i < card_count - 1; i++) {
		memcpy(card_list[i], card_list[i + 1], 4);
	}

	card_count--;
}

void Servo_SetAngle(uint8_t angle) {
	uint16_t pulse = 500 + (angle * 2000) / 180;
	__HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_3, pulse);
}

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

	/* USER CODE BEGIN 1 */

	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_SPI1_Init();
	MX_TIM3_Init();
	/* USER CODE BEGIN 2 */
	menu_root.child = &menu_scan;

	menu_scan.next = &menu_pass;
	menu_pass.next = &menu_setting;

	menu_setting.child = &menu_add;
	menu_add.next = &menu_delete;
	menu_delete.next = &menu_change;

	// default
	current_menu = &menu_root;
	selected_item = menu_root.child;
	HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3);
	RC522_Init();
	HAL_Delay(100);

	TFT_Init(&hspi1);
	TFT_FillScreen(BLACK);

	TFT_DrawMenu();

	/* USER CODE END 2 */

	/* USER CODE BEGIN RTOS_MUTEX */
	/* add mutexes, ... */
	osMutexDef(spiMutex);
	spiMutex = osMutexCreate(osMutex(spiMutex));
	/* USER CODE END RTOS_MUTEX */

	/* USER CODE BEGIN RTOS_SEMAPHORES */
	/* add semaphores, ... */
	/* USER CODE END RTOS_SEMAPHORES */

	/* USER CODE BEGIN RTOS_TIMERS */
	/* start timers, add new ones, ... */
	/* USER CODE END RTOS_TIMERS */

	/* USER CODE BEGIN RTOS_QUEUES */
	/* add queues, ... */
	/* USER CODE END RTOS_QUEUES */

	/* Create the thread(s) */
	/* definition and creation of defaultTask */
	osThreadDef(defaultTask, StartDefaultTask, osPriorityNormal, 0, 128);
	defaultTaskHandle = osThreadCreate(osThread(defaultTask), NULL);

	/* USER CODE BEGIN RTOS_THREADS */
	osThreadDef(keypadTask, Keypad_Task, osPriorityLow, 0, 256);
	keypadTaskHandle = osThreadCreate(osThread(keypadTask), NULL);

	osThreadDef(rfidTask, RFID_Task, osPriorityNormal, 0, 256);
	rfidTaskHandle = osThreadCreate(osThread(rfidTask), NULL);
	/* add threads, ... */
	/* USER CODE END RTOS_THREADS */

	/* Start scheduler */
	osKernelStart();

	/* We should never get here as control is now taken by the scheduler */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1) {
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

	/** Configure the main internal regulator output voltage
	 */
	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
	RCC_OscInitStruct.PLL.PLLM = 8;
	RCC_OscInitStruct.PLL.PLLN = 84;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 4;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
		Error_Handler();
	}
	HAL_RCC_MCOConfig(RCC_MCO2, RCC_MCO2SOURCE_SYSCLK, RCC_MCODIV_1);
}

/**
 * @brief SPI1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_SPI1_Init(void) {

	/* USER CODE BEGIN SPI1_Init 0 */

	/* USER CODE END SPI1_Init 0 */

	/* USER CODE BEGIN SPI1_Init 1 */

	/* USER CODE END SPI1_Init 1 */
	/* SPI1 parameter configuration*/
	hspi1.Instance = SPI1;
	hspi1.Init.Mode = SPI_MODE_MASTER;
	hspi1.Init.Direction = SPI_DIRECTION_2LINES;
	hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
	hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
	hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
	hspi1.Init.NSS = SPI_NSS_SOFT;
	hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_32;
	hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
	hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
	hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
	hspi1.Init.CRCPolynomial = 10;
	if (HAL_SPI_Init(&hspi1) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN SPI1_Init 2 */

	/* USER CODE END SPI1_Init 2 */

}

/**
 * @brief TIM3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM3_Init(void) {

	/* USER CODE BEGIN TIM3_Init 0 */

	/* USER CODE END TIM3_Init 0 */

	TIM_ClockConfigTypeDef sClockSourceConfig = { 0 };
	TIM_MasterConfigTypeDef sMasterConfig = { 0 };
	TIM_OC_InitTypeDef sConfigOC = { 0 };

	/* USER CODE BEGIN TIM3_Init 1 */

	/* USER CODE END TIM3_Init 1 */
	htim3.Instance = TIM3;
	htim3.Init.Prescaler = 83;
	htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim3.Init.Period = 19999;
	htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&htim3) != HAL_OK) {
		Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK) {
		Error_Handler();
	}
	if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig)
			!= HAL_OK) {
		Error_Handler();
	}
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = 0;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3)
			!= HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN TIM3_Init 2 */

	/* USER CODE END TIM3_Init 2 */
	HAL_TIM_MspPostInit(&htim3);

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	/* USER CODE BEGIN MX_GPIO_Init_1 */

	/* USER CODE END MX_GPIO_Init_1 */

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOH_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOA,
	R1_Pin | R2_Pin | R3_Pin | R4_Pin | SDA_Pin | RST_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOB, RESET_Pin | DC_Pin | CS_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin : PC13 */
	GPIO_InitStruct.Pin = GPIO_PIN_13;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/*Configure GPIO pins : R1_Pin R2_Pin R3_Pin R4_Pin
	 SDA_Pin */
	GPIO_InitStruct.Pin = R1_Pin | R2_Pin | R3_Pin | R4_Pin | SDA_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pins : RESET_Pin DC_Pin */
	GPIO_InitStruct.Pin = RESET_Pin | DC_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/*Configure GPIO pin : CS_Pin */
	GPIO_InitStruct.Pin = CS_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(CS_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : PC9 */
	GPIO_InitStruct.Pin = GPIO_PIN_9;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF0_MCO;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/*Configure GPIO pin : RST_Pin */
	GPIO_InitStruct.Pin = RST_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(RST_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : C1_Pin C2_Pin C3_Pin C4_Pin */
	GPIO_InitStruct.Pin = C1_Pin | C2_Pin | C3_Pin | C4_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLUP;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/* USER CODE BEGIN MX_GPIO_Init_2 */

	/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void Menu_Next(void) {
	if (selected_item->next)
		selected_item = selected_item->next;
}

void Menu_Prev(void) {
	MenuItem *tmp = current_menu->child;

	if (tmp == selected_item)
		return;

	while (tmp->next != selected_item)
		tmp = tmp->next;

	selected_item = tmp;
}

void Menu_Enter(void) {
	if (selected_item->child) {
		current_menu = selected_item;
		selected_item = current_menu->child;
	}
}

void Menu_Back(void) {
	if (current_menu->parent) {
		selected_item = current_menu;
		current_menu = current_menu->parent;
	}
}

Action_t GetAction(void) {
	if (selected_item)
		return selected_item->action;

	return ACTION_NONE;
}

void TFT_DrawMenu(void) {
	TFT_DrawString(10, 10, current_menu->name, &Font_11x16_times, CYAN, BLACK);

	MenuItem *item = current_menu->child;
	int y = 40;

	while (item) {
		if (item == selected_item) {
			TFT_DrawString(10, y, ">", &Font_11x16_times, GREEN, BLACK);
			TFT_DrawString(30, y, item->name, &Font_11x16_times, GREEN, BLACK);
		} else {
			// IN KHOẢNG TRẮNG ĐỂ XÓA DẤU ">" CŨ
			TFT_DrawString(10, y, " ", &Font_11x16_times, BLACK, BLACK);
			TFT_DrawString(30, y, item->name, &Font_11x16_times, WHITE, BLACK);
		}

		item = item->next;
		y += 30;
	}

	char buf[30];
	// Thêm khoảng trắng vào đuôi để dọn dẹp số cũ
	sprintf(buf, "Thẻ: %d", card_count);
	TFT_DrawString(10, 200, buf, &Font_11x16_times, YELLOW, BLACK);
}

void Keypad_Task(void const *argument) {
	char key;

	while (1) {
		key = Keypad_GetKey();

		if (key) {

			// ===== NAVIGATION =====
			if (key == 'D')
				Menu_Next();
			else if (key == 'C')
				Menu_Prev();

			// ===== ENTER =====
			else if (key == 'A') {

				Action_t action = GetAction();

				// ===== VÀO CÀI ĐẶT (cần pass) =====
				if (selected_item == &menu_setting) {

					osMutexWait(spiMutex, osWaitForever);
					TFT_FillScreen(BLACK);
					TFT_DrawString(10, 50, "NHẬP MẬT KHẨU:", &Font_11x16_times,
					WHITE, BLACK);
					osMutexRelease(spiMutex);

					if (CheckPassword()) {
						Menu_Enter();
					} else {
						osMutexWait(spiMutex, osWaitForever);
						TFT_DrawString(10, 100, "SAI MẬT KHẨU RỒI!!!!",
								&Font_11x16_times,
								RED, BLACK);
						osMutexRelease(spiMutex);
						osDelay(1000);
					}
				}

				// ===== CHANGE PASSWORD =====
				else if (action == ACTION_CHANGE_PASS) {

					// nhập pass cũ
					osMutexWait(spiMutex, osWaitForever);
					TFT_FillScreen(BLACK);
					TFT_DrawString(10, 50, "MẬT KHẨU CŨ:", &Font_11x16_times,
					WHITE, BLACK);
					osMutexRelease(spiMutex);

					if (CheckPassword()) {

						// nhập pass mới
						osMutexWait(spiMutex, osWaitForever);
						TFT_FillScreen(BLACK);
						TFT_DrawString(10, 50, "NHẬP MẬT KHẨU MỚI:",
								&Font_11x16_times,
								WHITE, BLACK);
						osMutexRelease(spiMutex);

						int idx = 0;
						char k;

						while (idx < 4) {
							k = Keypad_GetKey();
							if (k >= '0' && k <= '9') {
								password[idx++] = k;

								osMutexWait(spiMutex, osWaitForever);
								TFT_DrawString(10 + idx * 10, 120, "*",
										&Font_11x16_times, WHITE, BLACK);
								osMutexRelease(spiMutex);
							}
						}
						password[4] = '\0';

						osMutexWait(spiMutex, osWaitForever);
						TFT_DrawString(10, 160, "ĐỔI MẬT KHẨU HOÀN TẤT!!!",
								&Font_11x16_times, GREEN, BLACK);
						osMutexRelease(spiMutex);

						osDelay(1000);
					} else {
						osMutexWait(spiMutex, osWaitForever);
						TFT_DrawString(10, 100, "SAI MẬT KHẨU RỒI!!!!",
								&Font_11x16_times,
								RED, BLACK);
						osMutexRelease(spiMutex);
						osDelay(1000);
					}
				} else if (action == ACTION_PASSWORD) {

					osMutexWait(spiMutex, osWaitForever);
					TFT_FillScreen(BLACK);
					TFT_DrawString(10, 50, "NHẬP MẬT KHẨU:", &Font_11x16_times,
					WHITE, BLACK);
					osMutexRelease(spiMutex);

					if (CheckPassword()) {

						osMutexWait(spiMutex, osWaitForever);

						TFT_DrawString(10, 100, "MỞ CỬA!!!", &Font_11x16_times,
						GREEN, BLACK);
						osMutexRelease(spiMutex);

						// mở cửa
						Servo_SetAngle(90);  // mở cửa
						door_open = 1;
						door_time = osKernelSysTick();

					} else {

						osMutexWait(spiMutex, osWaitForever);
						TFT_DrawString(10, 100, "SAI MẬT KHẨU RỒI!!!!",
								&Font_11x16_times,
								RED, BLACK);
						osMutexRelease(spiMutex);

						osDelay(1000);
					}
				}
				// ===== ENTER bình thường =====
				else {
					Menu_Enter();
				}
			}

			// ===== BACK =====
			else if (key == 'B') {
				Menu_Back();
			}

			// ===== VẼ MENU =====
			osMutexWait(spiMutex, osWaitForever);
			TFT_FillScreen(BLACK);
			TFT_DrawMenu();
			osMutexRelease(spiMutex);
		}
		if (door_open && (osKernelSysTick() - door_time > 5000)) {
			Servo_SetAngle(0);  // đóng cửa
			door_open = 0;
		}
		osDelay(50);
	}
}
uint8_t CheckPassword(void) {
	int idx = 0;
	char key;

	while (idx < 4) {
		key = Keypad_GetKey();
		if (key >= '0' && key <= '9') {
			input_pass[idx++] = key;

			osMutexWait(spiMutex, osWaitForever);
			TFT_DrawString(10 + idx * 10, 150, "*", &Font_11x16_times, WHITE,
			BLACK);
			osMutexRelease(spiMutex);
		}
	}

	input_pass[4] = '\0';

	if (strcmp(input_pass, password) == 0)
		return 1;

	return 0;
}

void RFID_Task(void const *argument) {
	for (;;) {
		uint8_t local_uid[5];
		Action_t action = GetAction();

		// ❌ không ở SCAN/ADD/DELETE thì bỏ qua
		if (action != ACTION_SCAN && action != ACTION_ADD
				&& action != ACTION_DELETE) {
			osDelay(100);
			continue;
		}
		// đọc thẻ
		osMutexWait(spiMutex, osWaitForever);
		uint8_t result = RC522_Check(local_uid);
		osMutexRelease(spiMutex);

		if (result == MI_OK) {

			int index = find_card(local_uid);

			// clear vùng thông báo
			osMutexWait(spiMutex, osWaitForever);
			TFT_DrawRect(0, 170, 240, 60, BLACK);
			osMutexRelease(spiMutex);

			// ===== ADD CARD =====
			if (action == ACTION_ADD) {
				if (index == -1) {
					add_card(local_uid);

					osMutexWait(spiMutex, osWaitForever);
					TFT_DrawString(10, 170, "ĐÃ THÊM THẺ", &Font_11x16_times,
					GREEN,
					BLACK);
					osDelay(1000);
					osMutexRelease(spiMutex);

				} else {
					osMutexWait(spiMutex, osWaitForever);
					TFT_DrawString(10, 170, "THẺ NÀY ĐÃ CÓ RỒI!!!",
							&Font_11x16_times,
							YELLOW, BLACK);
					osDelay(1000);
					osMutexRelease(spiMutex);
				}
			}

			// ===== DELETE CARD =====
			else if (action == ACTION_DELETE) {
				if (index != -1) {
					delete_card(local_uid);

					osMutexWait(spiMutex, osWaitForever);
					TFT_DrawString(10, 170, "ĐÃ XÓA THẺ!!!", &Font_11x16_times,
					RED,
					BLACK);
					osDelay(1000);

					osMutexRelease(spiMutex);

				} else {
					osMutexWait(spiMutex, osWaitForever);
					TFT_DrawString(10, 170, "KHÔNG THẤY GÌ HẾT!!!!",
							&Font_11x16_times,
							YELLOW, BLACK);
					osDelay(1000);

					osMutexRelease(spiMutex);
				}
			}

			// ===== SCAN / NORMAL =====
			else if (action == ACTION_SCAN) {
				if (index != -1) {

					osMutexWait(spiMutex, osWaitForever);

					// clear vùng status
					TFT_DrawRect(0, 170, 240, 60, BLACK);

					// hiển thị trạng thái
					TFT_DrawString(10, 170, "QUẸT THẺ...", &Font_11x16_times,
					GREEN, BLACK);
					HAL_Delay(1000);
					TFT_DrawString(10, 170, "MỞ CỬA!!!", &Font_11x16_times,
					GREEN, BLACK);
					HAL_Delay(1000);
					osMutexRelease(spiMutex);

					Servo_SetAngle(90);  // mở cửa
					door_open = 1;
					door_time = osKernelSysTick();
				} else {
					osMutexWait(spiMutex, osWaitForever);
					TFT_DrawString(10, 170, "KHÔNG CHO VÀO!!!",
							&Font_11x16_times,
							RED, BLACK);
					osDelay(1000);

					osMutexRelease(spiMutex);

					osDelay(1000);
				}
			}

			// ===== HIỂN THỊ UID =====
			char buf1[20];
			char buf2[30];

			// dòng 1: card index
			sprintf(buf1, "Thẻ: %d", card_count);

			// dòng 2: UID
			sprintf(buf2, "%02X %02X %02X %02X", local_uid[0], local_uid[1],
					local_uid[2], local_uid[3]);

			osMutexWait(spiMutex, osWaitForever);

			// clear vùng hiển thị
			TFT_DrawRect(0, 170, 240, 60, BLACK);

			// vẽ dòng 1
			TFT_DrawString(10, 170, buf1, &Font_11x16_times, CYAN, BLACK);

			// vẽ dòng 2
			TFT_DrawString(10, 200, buf2, &Font_11x16_times, WHITE, BLACK);

			osMutexRelease(spiMutex);
		}
		if (door_open && (osKernelSysTick() - door_time > 5000)) {
			Servo_SetAngle(0);  // đóng cửa
			door_open = 0;
		}
		osDelay(100);
	}
}
/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartDefaultTask */
/**
 * @brief  Function implementing the defaultTask thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const *argument) {
	/* USER CODE BEGIN 5 */
	/* Infinite loop */
	for (;;) {
		osDelay(1);
	}
	/* USER CODE END 5 */
}

/**
 * @brief  Period elapsed callback in non blocking mode
 * @note   This function is called  when TIM4 interrupt took place, inside
 * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
 * a global variable "uwTick" used as application time base.
 * @param  htim : TIM handle
 * @retval None
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	/* USER CODE BEGIN Callback 0 */

	/* USER CODE END Callback 0 */
	if (htim->Instance == TIM4) {
		HAL_IncTick();
	}
	/* USER CODE BEGIN Callback 1 */

	/* USER CODE END Callback 1 */
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1) {
	}
	/* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
