/* hal_gpio.c — HAL_GPIO implementation for NUC1261 / MeterV5.2.
 *
 * Porting to a new MCU: rewrite bsp_gpio.c only; this file stays unchanged. */

#include "hal_gpio.h"
#include "bsp_gpio.h"

void HAL_GPIO_Write(HAL_GPIO_Pin_t pin, uint8_t logical_val)
{
    switch (pin)
    {
        case HAL_GPIO_LED_G: BSP_GPIO_LED_G_Write(logical_val); break;
        case HAL_GPIO_LED_R: BSP_GPIO_LED_R_Write(logical_val); break;
        default: break;
    }
}

void HAL_GPIO_Toggle(HAL_GPIO_Pin_t pin)
{
    switch (pin)
    {
        case HAL_GPIO_LED_G: BSP_GPIO_LED_G_Toggle(); break;
        case HAL_GPIO_LED_R: BSP_GPIO_LED_R_Toggle(); break;
        default: break;
    }
}
