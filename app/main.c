/**************************************************************************//**
 * @file      main.c
 * @brief     NUC1261 Meter Bootloader
 ******************************************************************************/

#include "MyDef.h"
#include "NUC1261.h"
#include "MeterV52PinConfig.h"
#include "fmc.h"
#include "uart_drv.h"
#include "BootloaderProcess.h"
#include "ota_offset_patcher.h"
#include "Select_fw.h"
#include "bl_fmc_adapter.h"
#include "flash_service.h"
#include "fw_info.h"
#include "bsp_flash.h"

#define PLLCTL_SETTING      CLK_PLLCTL_72MHz_HIRC
#define PLL_CLOCK           71884800

void ReadMyDeviceID(void);
static void DataFlashConfig(void);

uint8_t  MyDeviceID;

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
    OtaApplyCtx_t apply_ctx;

    MeterV52PinConfig_init();
    SYS_UnlockReg();

    //DataFlashConfig();

    SYS_Init();
    WDT_Init();

    ReadMyDeviceID();

    UART1_Init(MyDeviceID);
    BL_SysTickInit();
    BootloaderProcess_Init(MyDeviceID);

    FlashService_Init(&g_bl_fmc_driver);

    LED_G_Off();
    LED_R_Off();

    Boot_SelectFW();

    /* 靜態欄位：迴圈外一次設定 */
    apply_ctx.page_buf = Aprom_Page_Buff;
    apply_ctx.meta_buf = Next_Aprom_Page_Buff;
    apply_ctx.flash    = &g_bl_fmc_driver;

    while (1)
    {
        BootloaderProcess();

        /* BootloaderProcess() 僅在 OTA_DONE 後返回；BankID/NewBankMeta/OtaPayloadSize 此時有效 */
        apply_ctx.target_bank  = BankID;
        apply_ctx.meta         = &NewBankMeta;
        apply_ctx.payload_size = OtaPayloadSize;

        /* 成功：跳入 App，不返回。失敗：erase bank 後返回，迴圈重進 BootloaderProcess() */
        OtaOffsetPatcher_Apply(&apply_ctx);
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

/* CONFIG0[1:0] = CBS (Config Boot Select):
 *   00b = APROM + new IAP — required for FMC_SetVectorPageAddr (VECMAP).
 * CONFIG1 = DFBA (Data Flash Base Address): must equal BSP_FW_INFO_BASE.
 * If either setting is wrong this function self-corrects once and resets.
 * Protected registers must already be unlocked by the caller. */
static void DataFlashConfig(void)
{
    uint32_t au32Config[2];

    FMC_Open();
    FMC_ENABLE_CFG_UPDATE();

    FMC_ReadConfig(au32Config, 2);

    if ((au32Config[0] & 0x3UL) == 0x0UL && au32Config[1] == BSP_FW_INFO_BASE)
    {
        FMC_DISABLE_CFG_UPDATE();
        FMC_Close();
        return;
    }

    au32Config[0] &= ~0x3UL;           /* CBS → 00b: APROM + new IAP */
    au32Config[1]  = BSP_FW_INFO_BASE; /* DFBA → 0x0001F800 */
    FMC_Erase(FMC_CONFIG_BASE);
    FMC_WriteConfig(au32Config, 2);

    SYS_ResetChip();
    while (1);                          /* unreachable — wait for reset */
}
