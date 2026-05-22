/* bsp_uart.c — NUC1261 host-UART + SysTick hardware implementation.
 * This is the ONLY file above the Library layer that includes NUC1261.h.
 * Porting to a new MCU: rewrite this file only.
 * Host UART is selected at compile time via BTLD_HOST_UART_CH in bsp_config.h. */

#include "NUC1261.h"
#include "bsp_config.h"
#include "bsp_uart.h"

/* ================================================================
 *  Compile-time UART channel selection
 *  BTLD_HOST_UART_CH == 1 → UART1 (Master: PE13/PE12/PE11-nRTS)
 *  BTLD_HOST_UART_CH == 0 → UART0 (Sub:    PD0/PD1/PA3-nRTS)
 * ============================================================== */
#if (BTLD_HOST_UART_CH == 1U)
#  define HOST_UART         UART1
#  define HOST_UART_RST     UART1_RST
#  define HOST_UART_MODULE  UART1_MODULE
#else
#  define HOST_UART         UART0
#  define HOST_UART_RST     UART0_RST
#  define HOST_UART_MODULE  UART0_MODULE
#endif

/* ================================================================
 *  IRQ callback
 * ============================================================== */
static BSP_UART_IRQ_t s_irq_cb = NULL;

void BSP_UART_RegisterCallback(BSP_UART_IRQ_t cb) { s_irq_cb = cb; }

/* ================================================================
 *  IRQ entry points
 * ============================================================== */

void UART02_IRQHandler(void)
{
#if (BTLD_HOST_UART_CH == 0U)
    if (s_irq_cb != NULL) s_irq_cb();
#endif
}

void UART1_IRQHandler(void)
{
#if (BTLD_HOST_UART_CH == 1U)
    if (s_irq_cb != NULL) s_irq_cb();
#endif
}

/* ================================================================
 *  Hardware init
 * ============================================================== */
void BSP_UART_HW_Init(uint32_t baud_rate)
{
    SYS_ResetModule(HOST_UART_RST);
    CLK_SetModuleClock(HOST_UART_MODULE, CLK_CLKSEL1_UARTSEL_HXT, CLK_CLKDIV0_UART(1));
    UART_Open(HOST_UART, baud_rate);
}

void BSP_UART_RS485_AUD(void)
{
    HOST_UART->FUNCSEL = UART_FUNCSEL_RS485;
    HOST_UART->ALTCTL  = UART_ALTCTL_RS485AUD_Msk;
    HOST_UART->MODEM   = (HOST_UART->MODEM & ~UART_MODEM_RTSACTLV_Msk)
                       | UART_RTS_IS_HIGH_LEV_ACTIVE;
    HOST_UART->TOUT    = 0u;
    HOST_UART->FIFO   &= ~(UART_FIFO_RFITL_Msk | UART_FIFO_RTSTRGLV_Msk);
}

/* ================================================================
 *  FIFO access
 * ============================================================== */
_Bool   BSP_UART_RxReady(void)       { return (_Bool)UART_IS_RX_READY(HOST_UART); }
uint8_t BSP_UART_RxRead (void)       { return (uint8_t)UART_READ(HOST_UART); }
_Bool   BSP_UART_TxFull (void)       { return (_Bool)UART_IS_TX_FULL(HOST_UART); }
void    BSP_UART_TxWrite(uint8_t b)  { UART_WRITE(HOST_UART, b); }

/* ================================================================
 *  Interrupt status / enable
 * ============================================================== */
void  BSP_UART_LatchIntStatus(void) { (void)HOST_UART->INTSTS; }
_Bool BSP_UART_RxIntFlag(void) { return (_Bool)UART_GET_INT_FLAG(HOST_UART, UART_INTSTS_RDAINT_Msk);  }
_Bool BSP_UART_TxIntFlag(void) { return (_Bool)UART_GET_INT_FLAG(HOST_UART, UART_INTSTS_THREINT_Msk); }

void BSP_UART_EnableRxInt (void) { UART_EnableInt (HOST_UART, UART_INTEN_RDAIEN_Msk);  }
void BSP_UART_EnableTxInt (void) { UART_EnableInt (HOST_UART, UART_INTEN_THREIEN_Msk); }
void BSP_UART_DisableTxInt(void) { UART_DisableInt(HOST_UART, UART_INTEN_THREIEN_Msk); }

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
