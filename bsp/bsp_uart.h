/* bsp_uart.h — NUC1261 host-UART + SysTick hardware interface.
 * Host UART channel selected by BTLD_HOST_UART_CH in bsp_config.h.
 * All NUC1261.h register access is confined to bsp_uart.c.
 * Porting to a new MCU: rewrite bsp_uart.c only. */

#ifndef BSP_UART_H
#define BSP_UART_H

#include <stdint.h>

/* ================================================================
 *  IRQ callback (registered by uart_drv during init)
 * ============================================================== */
typedef void (*BSP_UART_IRQ_t)(void);
void BSP_UART_RegisterCallback(BSP_UART_IRQ_t cb);

/* ================================================================
 *  Hardware init
 * ============================================================== */
void BSP_UART_HW_Init  (uint32_t baud_rate);
void BSP_UART_RS485_AUD(void);

/* ================================================================
 *  FIFO access (ISR context)
 * ============================================================== */
_Bool   BSP_UART_RxReady(void);
uint8_t BSP_UART_RxRead (void);
_Bool   BSP_UART_TxFull (void);
void    BSP_UART_TxWrite(uint8_t byte);
_Bool   BSP_UART_TxEmpty(void);   /* true when TX FIFO and shift register are both empty */

/* ================================================================
 *  TX pin MFP control
 * ============================================================== */
void BSP_UART_TxPinDisable(void);   /* TXD → GPIO after last bit */
void BSP_UART_TxPinEnable (void);   /* GPIO → TXD before sending */

/* ================================================================
 *  Interrupt status / enable
 * ============================================================== */
void  BSP_UART_LatchIntStatus(void);   /* call once at ISR entry */
_Bool BSP_UART_RxIntFlag     (void);
_Bool BSP_UART_TxIntFlag     (void);
void  BSP_UART_EnableRxInt   (void);
void  BSP_UART_EnableTxInt   (void);
void  BSP_UART_DisableTxInt  (void);

/* ================================================================
 *  Critical section
 * ============================================================== */
void BSP_UART_EnterCritical(void);
void BSP_UART_ExitCritical (void);

/* ================================================================
 *  SysTick — 1 ms counter
 * ============================================================== */
void     BSP_SysTick_Init(void);
uint32_t BSP_GetTickMs   (void);

#endif /* BSP_UART_H */
