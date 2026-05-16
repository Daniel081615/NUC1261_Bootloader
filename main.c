/**************************************************************************//**
 * @file      main.c
 * @brief     NUC1261 Meter Bootloader
 ******************************************************************************/

#include "MyDef.h"
#include "NUC1261.h"
#include "MeterV52PinConfig.h"
#include "uart_drv.h"
#include "BootloaderProcess.h"
#include "patch_engine.h"
#include "Select_fw.h"
#include "bl_fmc_adapter.h"
#include "flash_service.h"
#include "fw_info.h"
#include "bsp_flash.h"

#define PLLCTL_SETTING      CLK_PLLCTL_72MHz_HIRC
#define PLL_CLOCK           71884800

void ReadMyDeviceID(void);

uint8_t  MyDeviceID;
uint32_t g_apromSize;

void SYS_Init(void)
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

void WDT_Init(void)
{
    WDT_Close();
    WDT_Open(WDT_TIMEOUT_2POW16, WDT_RESET_DELAY_18CLK, TRUE, FALSE);
    WDT_EnableInt();
    NVIC_EnableIRQ(WDT_IRQn);
    WDT_RESET_COUNTER();
}

int main(void)
{
    MeterV52PinConfig_init();
    SYS_UnlockReg();

    SYS_Init();

    UART1_Init();
    BL_SysTickInit();

    ReadMyDeviceID();

    FlashService_Init(&g_bl_fmc_driver);

    g_apromSize = APROM_SIZE;

    Boot_SelectFW();

    while (1)
    {
        BootloaderProcess();
        PatchProcess();
    }
}

void ReadMyDeviceID(void)
{
    if (PB2) MyDeviceID |= BIT0;
    if (PB3) MyDeviceID |= BIT1;
    if (PB4) MyDeviceID |= BIT2;
    if (PB5) MyDeviceID |= BIT3;
    if (PB6) MyDeviceID |= BIT4;
    if (PB7) MyDeviceID |= BIT5;
}
