/* bsp_init.c — M031LE3AE board-level system initialisation.
 *
 * Clock: HIRC 48 MHz (no HXT/PLL needed for bootloader).
 * WDT:   LIRC 38.4 kHz, 2^18/38400 ≈ 6.8 s timeout.
 * UART0: clock from HIRC/1 = 48 MHz → 57600 baud error < 0.2%.
 *
 * DeviceID GPIO: defaults to PB2-PB7 (same as NUC1261).
 * *** If these pins conflict with board hardware, update BSP_SampleDeviceID(). *** */

#include "M031Series.h"
#include "bsp_config.h"
#include "bsp_init.h"

static uint8_t s_device_id = 0u;

static void BSP_SystemInit(void)
{
    /* 1. 先不要在這裡開中斷，確保硬體初始化不被打斷 */
	
    /* 2. 解鎖暫存器保護 */
    SYS_UnlockReg();

    /* 3. 開啟 HIRC (48 MHz) */
    CLK_EnableXtalRC(CLK_PWRCTL_HIRCEN_Msk);
    CLK_WaitClockReady(CLK_STATUS_HIRCSTB_Msk);

    /* 4. 將 HCLK 直接設為 HIRC / 1 = 48 MHz */
    CLK_SetHCLK(CLK_CLKSEL0_HCLKSEL_HIRC, CLK_CLKDIV0_HCLK(1));

    /* 5. 更新系統核心時脈變數 */
    SystemCoreClockUpdate();

    /* 6. 周邊時脈設定 */
    CLK_EnableModuleClock(UART0_MODULE);
    CLK_EnableModuleClock(WDT_MODULE);
    
    /* UART0 時脈源使用 HIRC */
    CLK_SetModuleClock(UART0_MODULE, CLK_CLKSEL1_UART0SEL_HIRC, CLK_CLKDIV0_UART0(1));
    /* 【安全修正】看門狗時脈源改回完全獨立的 LIRC (38.4 kHz)，確保省電與死機時的強制重置能力 */
    CLK_SetModuleClock(WDT_MODULE,   CLK_CLKSEL1_WDTSEL_LIRC,   0u);

    /* 7. 管腳多功能設定 (UART0 Pin-Mux) */
    SYS->GPA_MFPH = (SYS->GPA_MFPH & ~SYS_GPA_MFPH_PA15MFP_Msk) | SYS_GPA_MFPH_PA15MFP_UART0_RXD;
    SYS->GPA_MFPH = (SYS->GPA_MFPH & ~SYS_GPA_MFPH_PA14MFP_Msk) | SYS_GPA_MFPH_PA14MFP_UART0_TXD;

    /* 8. LED 與 GPIO 設定 */
    GPIO_SetMode(PF, BIT5, GPIO_MODE_OUTPUT);
    GPIO_SetMode(PB, BIT1, GPIO_MODE_OUTPUT);
    GPIO_SetMode(PB, BIT2 | BIT3 | BIT4 | BIT5 | BIT6 | BIT7, GPIO_MODE_QUASI);

    /* 9. 【安全修正】重新鎖定暫存器保護，防止關鍵設定被跑飛的程式誤寫 */
    SYS_LockReg();

    /* 10. 【關鍵調整】硬體周邊與時脈全部配置安全、穩定後，再解開全域中斷總開關 */
    __enable_irq();
}

static void BSP_WDT_Init(void)
{
    WDT_Close();
    /* LIRC 38.4 kHz, 2^18 / 38400 ≈ 6.8 s, reset on timeout */
    WDT_Open(WDT_TIMEOUT_2POW18, WDT_RESET_DELAY_18CLK, 1u, 0u);
    WDT_EnableInt();
    NVIC_EnableIRQ(WDT_IRQn);
    WDT_RESET_COUNTER();
}

static void BSP_SampleDeviceID(void)
{
    s_device_id = 0u;
    if (PB2) s_device_id |= (uint8_t)BIT0;
    if (PB3) s_device_id |= (uint8_t)BIT1;
    if (PB4) s_device_id |= (uint8_t)BIT2;
    if (PB5) s_device_id |= (uint8_t)BIT3;
    if (PB6) s_device_id |= (uint8_t)BIT4;
    if (PB7) s_device_id |= (uint8_t)BIT5;
}

void BSP_Init(void)
{
    SYS_UnlockReg();
    BSP_SystemInit();
    BSP_WDT_Init();
    BSP_SampleDeviceID();
    PF5 = 1u;   /* LED_G off (active-low) */
    PB1 = 1u;   /* LED_R off (active-low) */
    SYS_LockReg();
}

uint8_t BSP_GetDeviceID(void)
{
    return s_device_id;
}

void BSP_WDT_Feed(void)
{
    WDT_RESET_COUNTER();
}
