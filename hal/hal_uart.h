/* hal_uart.h — MCU-independent UART interface (HAL layer)
 *
 * Implementation: hal_uart.c → uart_drv.c → NUC1261 host UART
 * Host UART channel (UART0/UART1) is selected by BTLD_HOST_UART_CH in bsp_config.h.
 * Porting to a new MCU: rewrite hal_uart.c only; this header stays unchanged.
 * Upper layers include only this header; never depend on uart_drv.h directly. */

#ifndef HAL_UART_H
#define HAL_UART_H

#include <stdint.h>

/* Initialise UART1 (RS485) and bind device_id for outgoing frames. */
void           HAL_UART_Init(uint8_t device_id);

/* Configure SysTick for 1 ms time base. */
void           HAL_SysTick_Init(void);

/* Millisecond counter — monotonically increasing since HAL_SysTick_Init(). */
uint32_t       HAL_GetTickMs(void);

/* ISR-driven receive; Poll is a no-op but must be called each loop iteration
 * to satisfy the protocol ops contract. */
void           HAL_UART_Poll(void);
_Bool          HAL_UART_HasPacket(void);
const uint8_t *HAL_UART_GetPacket(void);

/* Build a 100-byte response frame and enqueue it for ISR-driven transmit. */
void           HAL_UART_SendRsp(uint8_t cmd,
                                const uint8_t *payload,
                                uint16_t len);

#endif /* HAL_UART_H */
