/* bsp_gpio.h — Board-level GPIO operations (M031LE3AE)
 *
 * Interface identical to bsp/bsp_gpio.h (NUC1261).
 * Porting to a new board: rewrite bsp_gpio.c only; this header stays unchanged. */

#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include <stdint.h>

/* LED_G = PF5 (active-low), LED_R = PB1 (active-low).
 * logical_val: 1 = ON, 0 = OFF — polarity inversion is inside the .c */
void BSP_GPIO_LED_G_Write(uint8_t logical_val);
void BSP_GPIO_LED_G_Toggle(void);
void BSP_GPIO_LED_R_Write(uint8_t logical_val);
void BSP_GPIO_LED_R_Toggle(void);

#endif /* BSP_GPIO_H */
