/* hal_sys.c — HAL system utilities for NUC1261
 * Porting to a new MCU: rewrite this file only. */

#include "hal_sys.h"
#include "hal_gpio.h"
#include "bsp_init.h"

void HAL_System_Init(void)   { BSP_Init(); }
uint8_t HAL_GetDeviceID(void){ return BSP_GetDeviceID(); }
void HAL_WDT_Feed(void)      { BSP_WDT_Feed(); }
void HAL_LED_RToggle(void)   { HAL_GPIO_Toggle(HAL_GPIO_LED_R); }
