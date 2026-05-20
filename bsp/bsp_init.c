/* bsp_init.c — Board-level system initialisation (NUC1261 / MeterV5.2)
 * Logic extracted verbatim from app/main.c; no behaviour change. */

#include "NUC1261.h"
#include "bsp_config.h"
#include "MeterV52PinConfig.h"
#include "bsp_init.h"

#define PLLCTL_SETTING  CLK_PLLCTL_72MHz_HIRC
#define PLL_CLOCK       71884800

static uint8_t s_device_id = 0u;

/* ── Clock + pin-mux (was SYS_Init in main.c) ── */
static void BSP_SystemInit(void)
{
#ifdef MeterV5_2
    SYS->GPF_MFPL = (SYS->GPF_MFPL & (~SYS_GPF_MFPL_PF3MFP_Msk)) | SYS_GPF_MFPL_PF3MFP_XT1_OUT;
    SYS->GPF_MFPL = (SYS->GPF_MFPL & (~SYS_GPF_MFPL_PF4MFP_Msk)) | SYS_GPF_MFPL_PF4MFP_XT1_IN;
#else
    SYS->GPF_MFPL = (SYS->GPF_MFPL & (~SYS_GPF_MFPL_PF0MFP_Msk)) | SYS_GPF_MFPL_PF0MFP_X32_OUT;
    SYS->GPF_MFPL = (SYS->GPF_MFPL & (~SYS_GPF_MFPL_PF1MFP_Msk)) | SYS_GPF_MFPL_PF1MFP_X32_IN;
#endif

    CLK_EnableXtalRC(CLK_PWRCTL_HIRCEN_Msk | CLK_PWRCTL_HXTEN_Msk | CLK_PWRCTL_LIRCEN_Msk);
    CLK_WaitClockReady(CLK_STATUS_HIRCSTB_Msk | CLK_STATUS_HXTSTB_Msk);
    CLK_SetHCLK(CLK_CLKSEL0_HCLKSEL_HIRC, CLK_CLKDIV0_HCLK(1));
    CLK_SetCoreClock(PLL_CLOCK);
    CLK_SetSysTickClockSrc(CLK_CLKSEL0_STCLKSEL_HCLK_DIV2);

    CLK_EnableModuleClock(UART0_MODULE);
    CLK_EnableModuleClock(UART1_MODULE);
    CLK_EnableModuleClock(WDT_MODULE);

    CLK_SetModuleClock(UART0_MODULE, CLK_CLKSEL1_UARTSEL_HXT, CLK_CLKDIV0_UART(1));
    CLK_SetModuleClock(UART1_MODULE, CLK_CLKSEL1_UARTSEL_HXT, CLK_CLKDIV0_UART(1));
    CLK_SetModuleClock(WDT_MODULE, CLK_CLKSEL1_WDTSEL_LIRC, 0);

    SYS->GPD_MFPL = (SYS->GPD_MFPL & (~SYS_GPD_MFPL_PD0MFP_Msk)) | SYS_GPD_MFPL_PD0MFP_UART0_RXD;
    SYS->GPD_MFPL = (SYS->GPD_MFPL & (~SYS_GPD_MFPL_PD1MFP_Msk)) | SYS_GPD_MFPL_PD1MFP_UART0_TXD;
    SYS->GPE_MFPH = (SYS->GPE_MFPH & (~SYS_GPE_MFPH_PE13MFP_Msk)) | SYS_GPE_MFPH_PE13MFP_UART1_RXD;
    SYS->GPE_MFPH = (SYS->GPE_MFPH & (~SYS_GPE_MFPH_PE12MFP_Msk)) | SYS_GPE_MFPH_PE12MFP_UART1_TXD;
    SYS->GPE_MFPH = (SYS->GPE_MFPH & (~SYS_GPE_MFPH_PE11MFP_Msk)) | SYS_GPE_MFPH_PE11MFP_UART1_nRTS;
}

/* ── WDT (was WDT_Init in main.c) ── */
static void BSP_WDT_Init(void)
{
    WDT_Close();
    WDT_Open(WDT_TIMEOUT_2POW16, WDT_RESET_DELAY_18CLK, 1u, 0u);
    WDT_EnableInt();
    NVIC_EnableIRQ(WDT_IRQn);
    WDT_RESET_COUNTER();
}

/* ── Device ID sampling (was ReadMyDeviceID in main.c) ── */
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

/* ── Public API ── */

void BSP_Init(void)
{
    MeterV52PinConfig_init();
    SYS_UnlockReg();
    BSP_SystemInit();
    BSP_WDT_Init();
    BSP_SampleDeviceID();
    PD7 = 1u;   /* LED_G off (active-low) */
    PF2 = 1u;   /* LED_R off (active-low) */
    /* BSP_Flash_ConfigVerifyAndFix() intentionally not called here —
     * matches the commented-out DataFlashConfig() in the original main.c */
}

uint8_t BSP_GetDeviceID(void)
{
    return s_device_id;
}

void BSP_WDT_Feed(void)
{
    WDT_RESET_COUNTER();
}
