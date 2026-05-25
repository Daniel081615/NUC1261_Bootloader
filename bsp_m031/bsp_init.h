/* bsp_init.h — Board-level system initialisation (M031LE3AE)
 *
 * Interface identical to bsp/bsp_init.h (NUC1261). */

#ifndef BSP_INIT_H
#define BSP_INIT_H

#include <stdint.h>

/*
 * BSP_Init() — call once before any driver or service initialisation.
 * Sequence: pin-mux → clock (HIRC 48 MHz) → WDT (LIRC 38.4 kHz) → LED init → DeviceID sample.
 */
void    BSP_Init(void);

/* Returns the 6-bit device ID sampled from GPIO at BSP_Init time.
 * NOTE: DeviceID GPIO pins (default PB2-PB7) must be confirmed for target hardware. */
uint8_t BSP_GetDeviceID(void);

/* Feed the watchdog counter — safe to call from any layer above BSP. */
void    BSP_WDT_Feed(void);

#endif /* BSP_INIT_H */
