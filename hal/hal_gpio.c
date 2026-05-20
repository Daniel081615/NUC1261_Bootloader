/* hal_gpio.c — NUC1261 GPIO implementation of the HAL_GPIO interface.
 *
 * Pin mapping (MeterV5.2 board):
 *   LED_G = PD7  (active-low: logical 1 → PD7 = 0)
 *   LED_R = PF2  (active-low: logical 1 → PF2 = 0)
 *
 * Porting to a new MCU: rewrite this file only. */

#include "hal_gpio.h"
#include "NUC1261.h"

void HAL_GPIO_Write(HAL_GPIO_Pin_t pin, uint8_t logical_val)
{
    switch (pin)
    {
        case HAL_GPIO_LED_G: PD7 = logical_val ? 0u : 1u; break;
        case HAL_GPIO_LED_R: PF2 = logical_val ? 0u : 1u; break;
        default: break;
    }
}

void HAL_GPIO_Toggle(HAL_GPIO_Pin_t pin)
{
    switch (pin)
    {
        case HAL_GPIO_LED_G: PD7 ^= 1u; break;
        case HAL_GPIO_LED_R: PF2 ^= 1u; break;
        default: break;
    }
}
