/* bsp_gpio.c — NUC1261 / MeterV5.2 GPIO register implementation.
 *
 * Porting to a new MCU: rewrite this file only. */

#include "NUC1261.h"
#include "bsp_gpio.h"

void BSP_GPIO_LED_G_Write(uint8_t logical_val)
{
    PD7 = logical_val ? 0u : 1u;   /* active-low */
}

void BSP_GPIO_LED_G_Toggle(void)
{
    PD7 ^= 1u;
}

void BSP_GPIO_LED_R_Write(uint8_t logical_val)
{
    PF2 = logical_val ? 0u : 1u;   /* active-low */
}

void BSP_GPIO_LED_R_Toggle(void)
{
    PF2 ^= 1u;
}
