/* hal_uart.c — NUC1261 UART HAL implementation.
 *
 * Thin wrappers over uart_drv.c.
 * Porting to a new MCU: rewrite this file only. */

#include "hal_uart.h"
#include "uart_drv.h"

void           HAL_UART_Init(uint8_t device_id) { UART_Init(device_id); }
void           HAL_SysTick_Init(void)            { BL_SysTickInit(); }
uint32_t       HAL_GetTickMs(void)               { return BL_GetTickMs(); }
void           HAL_UART_Poll(void)               { UART_Poll(); }
_Bool          HAL_UART_HasPacket(void)          { return UART_HasPacket(); }
const uint8_t *HAL_UART_GetPacket(void)          { return UART_GetPacket(); }

void HAL_UART_SendRsp(uint8_t cmd, const uint8_t *payload, uint16_t len)
{
    UART_SendRsp(cmd, payload, len);
}
