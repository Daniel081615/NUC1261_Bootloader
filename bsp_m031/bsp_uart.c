/* bsp_uart.c — M031LE3AE host-UART (UART0) + SysTick implementation.
 * This is the ONLY file in the M031 BSP that includes M031Series.h for UART.
 * UART0: PA15(RXD) / PA14(TXD), RS485 AUD, 57600 baud.
 * M031 UART0 and UART2 share UART02_IRQHandler (same ISR name as NUC1261). */

#include "M031Series.h"
#include "bsp_config.h"
#include "bsp_uart.h"

/* M031 bootloader always uses UART0 (BTLD_HOST_UART_CH == 0) */
#define HOST_UART         UART0
#define HOST_UART_RST     UART0_RST
#define HOST_UART_MODULE  UART0_MODULE
#define HOST_IRQn					UART02_IRQn

/* TX pin: PA14 in GPA_MFPH (pins 8-15) */
#define HOST_TXD_REG      SYS->GPA_MFPH
#define HOST_TXD_MASK     SYS_GPA_MFPH_PA14MFP_Msk
#define HOST_TXD_FUNC     SYS_GPA_MFPH_PA14MFP_UART0_TXD


/* ================================================================
 *  IRQ callback
 * ============================================================== */
static BSP_UART_IRQ_t s_irq_cb = NULL;

void BSP_UART_RegisterCallback(BSP_UART_IRQ_t cb) { s_irq_cb = cb; }

/* ================================================================
 *  IRQ entry point — M031 UART0+UART2 share this handler
 * ============================================================== */
void UART02_IRQHandler(void)
{
    if (s_irq_cb != NULL) s_irq_cb();
}

/* ================================================================
 *  Hardware init
 * ============================================================== */
void BSP_UART_HW_Init(uint32_t baud_rate)
{
    /* 重置 UART 模組 */
    SYS_ResetModule(HOST_UART_RST);
    
    /* 開啟 UART 硬體並設定鮑率 */
    UART_Open(HOST_UART, baud_rate);
    BSP_UART_RS485_AUD();

    /* 1. 開啟 UART 內部的接收中斷 */
    BSP_UART_EnableRxInt();
    
    /* 2. 【關鍵】開啟 Cortex-M0 核心的 NVIC 中斷控制器 */
    NVIC_EnableIRQ(HOST_IRQn); 
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
_Bool   BSP_UART_TxEmpty(void)       { return (_Bool)UART_IS_TX_EMPTY(HOST_UART); }

/* ================================================================
 *  TX pin MFP control (RS485 direction via PA14 GPIO/UART toggle)
 * ============================================================== */
void BSP_UART_TxPinDisable(void)
{
    /* Release TXD MFP first, then set QUASI (open-drain + pull-up = HIGH).
     * UT2201 auto-direction module uses TXD level to control DE/nRE:
     * HIGH = idle = receiver enabled; LOW = start bit = transmitter enabled.
     * OUTPUT LOW would permanently disable the receiver. */
    HOST_TXD_REG &= ~HOST_TXD_MASK;
    PA->MODE = (PA->MODE & ~GPIO_MODE_MODE14_Msk)
             | (GPIO_MODE_QUASI << GPIO_MODE_MODE14_Pos);
}

void BSP_UART_TxPinEnable(void)
{
    HOST_TXD_REG = (HOST_TXD_REG & ~HOST_TXD_MASK) | HOST_TXD_FUNC;
}

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
