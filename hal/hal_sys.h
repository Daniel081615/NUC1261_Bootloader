/* hal_sys.h — MCU-independent system utilities (HAL layer)
 *
 * Implementation: hal_sys.c  →  bsp_init.c  →  NUC1261 vendor API
 * Upper layers (Middleware, Protocol, App) include only this header;
 * they never depend on NUC1261.h or bsp_init.h directly. */

#ifndef HAL_SYS_H
#define HAL_SYS_H

#include <stdint.h>

/* One-shot board + clock + WDT initialisation (wraps BSP_Init). */
void    HAL_System_Init(void);

/* Returns the 6-bit device ID sampled from GPIO during HAL_System_Init. */
uint8_t HAL_GetDeviceID(void);

/* Feed the watchdog counter to prevent an MCU reset.
 * Call periodically inside any loop that may run longer than the WDT timeout. */
void    HAL_WDT_Feed(void);

/* Toggle the red status LED — used as a protocol activity indicator. */
void    HAL_LED_RToggle(void);

#endif /* HAL_SYS_H */
