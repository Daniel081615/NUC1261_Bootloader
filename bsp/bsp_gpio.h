/* bsp_gpio.h — Board-level GPIO operations (NUC1261, MeterV5.2)
 *
 * Porting to a new board: rewrite bsp_gpio.c only; this header stays unchanged. */

#ifndef BSP_GPIO_H
#define BSP_GPIO_H

#include <stdint.h>

/* LED_G = PD7 (active-low), LED_R = PF2 (active-low).
 * logical_val: 1 = ON, 0 = OFF — polarity inversion is inside the .c */
void BSP_GPIO_LED_G_Write(uint8_t logical_val);
void BSP_GPIO_LED_G_Toggle(void);
void BSP_GPIO_LED_R_Write(uint8_t logical_val);
void BSP_GPIO_LED_R_Toggle(void);

#endif /* BSP_GPIO_H */
