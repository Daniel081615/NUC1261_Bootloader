/* drv_sys.c — NUC1261 system-level driver (Driver layer)
 *
 * Porting to a new MCU: rewrite this file only. */

#include "bsp_init.h"
#include "drv_sys.h"

void    DRV_SYS_Init(void)        { BSP_Init(); }
uint8_t DRV_SYS_GetDeviceID(void) { return BSP_GetDeviceID(); }
void    DRV_SYS_WDT_Feed(void)    { BSP_WDT_Feed(); }
