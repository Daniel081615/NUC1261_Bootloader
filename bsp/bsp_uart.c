/* bsp_uart.c — NUC1261 UART1 + SysTick hardware implementation.
 * This is the ONLY file above the Library layer that includes NUC1261.h.
 * Porting to a new MCU: rewrite this file only. */

#include "NUC1261.h"
#include "bsp_config.h"
#include "bsp_uart.h"

/* ================================================================
 *  IRQ callback
 * ============================================================== */
static BSP_UART_IRQ_t s_irq_cb = NULL;

void BSP_UART_RegisterCallback(BSP_UART_IRQ_t cb) { s_irq_cb = cb; }

/* ================================================================
 *  IRQ entry points
 * ============================================================== */

/* UART0/2 shared IRQ — UART0 not used; stub suppresses spurious IRQ. */
void UART02_IRQHandler(void) {}

void UART1_IRQHandler(void)
{
    if (s_irq_cb != NULL) s_irq_cb();
}

/* ================================================================
 *  Hardware init
 * ============================================================== */
void BSP_UART_HW_Init(uint32_t baud_rate)
{
    SYS_ResetModule(UART1_RST);
    CLK_SetModuleClock(UART1_MODULE, CLK_CLKSEL1_UARTSEL_HXT, CLK_CLKDIV0_UART(1));
    UART_Open(UART1, baud_rate);
}

void BSP_UART_RS485_AUD(void)
{
    UART1->FUNCSEL = UART_FUNCSEL_RS485;
    UART1->ALTCTL  = UART_ALTCTL_RS485AUD_Msk;
    UART1->MODEM   = (UART1->MODEM & ~UART_MODEM_RTSACTLV_Msk)
                   | UART_RTS_IS_HIGH_LEV_ACTIVE;
    UART1->TOUT    = 0u;
    UART1->FIFO   &= ~(UART_FIFO_RFITL_Msk | UART_FIFO_RTSTRGLV_Msk);
}

/* ================================================================
 *  FIFO access
 * ============================================================== */
_Bool   BSP_UART_RxReady(void)       { return (_Bool)UART_IS_RX_READY(UART1); }
uint8_t BSP_UART_RxRead (void)       { return (uint8_t)UART_READ(UART1); }
_Bool   BSP_UART_TxFull (void)       { return (_Bool)UART_IS_TX_FULL(UART1); }
void    BSP_UART_TxWrite(uint8_t b)  { UART_WRITE(UART1, b); }

/* ================================================================
 *  Interrupt status / enable
 * ============================================================== */
void  BSP_UART_LatchIntStatus(void) { (void)UART1->INTSTS; }
_Bool BSP_UART_RxIntFlag(void) { return (_Bool)UART_GET_INT_FLAG(UART1, UART_INTSTS_RDAINT_Msk);  }
_Bool BSP_UART_TxIntFlag(void) { return (_Bool)UART_GET_INT_FLAG(UART1, UART_INTSTS_THREINT_Msk); }

void BSP_UART_EnableRxInt (void) { UART_EnableInt (UART1, UART_INTEN_RDAIEN_Msk);  }
void BSP_UART_EnableTxInt (void) { UART_EnableInt (UART1, UART_INTEN_THREIEN_Msk); }
void BSP_UART_DisableTxInt(void) { UART_DisableInt(UART1, UART_INTEN_THREIEN_Msk); }

/* ================================================================
 *  Critical section
 * ============================================================== */
void BSP_UART_EnterCritical(void) { __disable_irq(); }
void BSP_UART_ExitCritical (void) { __enable_irq();  }

/* ================================================================
 *  SysTick — 1 ms counter
 * ============================================================== */
static volatile uint32_t s_ms_tick = 0u;

void SysTick_Handler(void) { s_ms_tick++; }

void     BSP_SysTick_Init(void) { SysTick_Config(SystemCoreClock / 1000u); }
uint32_t BSP_GetTickMs   (void) { return s_ms_tick; }
