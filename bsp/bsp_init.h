/* bsp_init.h — Board-level system initialisation (NUC1261 / MeterV5.2) */

#ifndef BSP_INIT_H
#define BSP_INIT_H

#include <stdint.h>

/*
 * BSP_Init() — call once before any driver or service initialisation.
 * Sequence: pin-mux → clock/PLL → WDT → Data Flash CONFIG verify.
 * Caller must invoke SYS_UnlockReg() before calling BSP_Init().
 */
void    BSP_Init(void);

/* Returns the 6-bit device ID sampled from PB2-PB7 during BSP_Init. */
uint8_t BSP_GetDeviceID(void);

/* Feed the watchdog counter — safe to call from any layer above BSP. */
void    BSP_WDT_Feed(void);

#endif /* BSP_INIT_H */
