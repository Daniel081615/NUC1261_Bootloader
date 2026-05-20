/* hal_gpio.h — MCU-independent GPIO interface (HAL layer)
 *
 * Implementation: hal_gpio.c → NUC1261 registers (PD7, PF2)
 * Porting to a new MCU: rewrite hal_gpio.c only; this header stays unchanged.
 * Upper layers include only this header; never depend on NUC1261.h directly. */

#ifndef HAL_GPIO_H
#define HAL_GPIO_H

#include <stdint.h>

typedef enum {
    HAL_GPIO_LED_G,
    HAL_GPIO_LED_R,
} HAL_GPIO_Pin_t;

/* logical_val: 1 = asserted / ON, 0 = deasserted / OFF.
 * Active-low mapping is handled internally. */
void HAL_GPIO_Write(HAL_GPIO_Pin_t pin, uint8_t logical_val);
void HAL_GPIO_Toggle(HAL_GPIO_Pin_t pin);

#endif /* HAL_GPIO_H */
