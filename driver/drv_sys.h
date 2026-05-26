/* drv_sys.h — MCU system-level driver interface (Driver layer)
 *
 * Wraps BSP system init so HAL never includes bsp_init.h directly.
 * Porting to a new MCU: rewrite drv_sys.c only; this header stays unchanged. */

#ifndef DRV_SYS_H
#define DRV_SYS_H

#include <stdint.h>

/* Full board + clock + WDT initialisation sequence. */
void    DRV_SYS_Init(void);

/* Returns the 6-bit device ID sampled from GPIO during DRV_SYS_Init. */
uint8_t DRV_SYS_GetDeviceID(void);

/* Feed the watchdog counter. */
void    DRV_SYS_WDT_Feed(void);

#endif /* DRV_SYS_H */
