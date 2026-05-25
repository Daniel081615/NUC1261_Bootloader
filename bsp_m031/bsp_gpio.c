/* bsp_gpio.c — M031LE3AE GPIO register implementation.
 *
 * LED_G = PF5 (active-low), LED_R = PB1 (active-low).
 * Porting to a new board: rewrite this file only. */

#include "M031Series.h"
#include "bsp_gpio.h"

void BSP_GPIO_LED_G_Write(uint8_t logical_val)
{
    PF5 = logical_val ? 0u : 1u;   /* active-low */
}

void BSP_GPIO_LED_G_Toggle(void)
{
    PF5 ^= 1u;
}

void BSP_GPIO_LED_R_Write(uint8_t logical_val)
{
    PB1 = logical_val ? 0u : 1u;   /* active-low */
}

void BSP_GPIO_LED_R_Toggle(void)
{
    PB1 ^= 1u;
}
