/* hal_sys.c — HAL system utilities for NUC1261
 * Porting to a new MCU: rewrite drv_sys.c only; this file stays unchanged. */

#include "hal_sys.h"
#include "hal_gpio.h"
#include "drv_sys.h"

void    HAL_System_Init(void)    { DRV_SYS_Init(); }
uint8_t HAL_GetDeviceID(void)    { return DRV_SYS_GetDeviceID(); }
void    HAL_WDT_Feed(void)       { DRV_SYS_WDT_Feed(); }
void    HAL_LED_RToggle(void)    { HAL_GPIO_Toggle(HAL_GPIO_LED_R); }
