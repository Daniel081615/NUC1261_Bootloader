/**************************************************************************//**
 * @file      main.c
 * @brief     NUC1261 Meter Bootloader — 重構版
 *            依照重構計畫 v2: bsp_flash + flash_service
 ******************************************************************************/

#include "stdio.h"
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
void DataFlashConfig(void);
static void MigrateFWInfoIfNeeded(void);

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
    CLK_EnableModuleClock(UART2_MODULE);
    CLK_EnableModuleClock(WDT_MODULE);

    CLK_SetModuleClock(UART0_MODULE, CLK_CLKSEL1_UARTSEL_HXT, CLK_CLKDIV0_UART(1));
    CLK_SetModuleClock(UART1_MODULE, CLK_CLKSEL1_UARTSEL_HXT, CLK_CLKDIV0_UART(1));
    CLK_SetModuleClock(UART2_MODULE, CLK_CLKSEL1_UARTSEL_HXT, CLK_CLKDIV0_UART(1));
    CLK_SetModuleClock(WDT_MODULE, CLK_CLKSEL1_WDTSEL_LIRC, 0);

    SYS->GPD_MFPL = (SYS->GPD_MFPL & (~SYS_GPD_MFPL_PD0MFP_Msk)) | SYS_GPD_MFPL_PD0MFP_UART0_RXD;
    SYS->GPD_MFPL = (SYS->GPD_MFPL & (~SYS_GPD_MFPL_PD1MFP_Msk)) | SYS_GPD_MFPL_PD1MFP_UART0_TXD;
    SYS->GPE_MFPH = (SYS->GPE_MFPH & (~SYS_GPE_MFPH_PE13MFP_Msk)) | SYS_GPE_MFPH_PE13MFP_UART1_RXD;
    SYS->GPE_MFPH = (SYS->GPE_MFPH & (~SYS_GPE_MFPH_PE12MFP_Msk)) | SYS_GPE_MFPH_PE12MFP_UART1_TXD;
    SYS->GPE_MFPH = (SYS->GPE_MFPH & (~SYS_GPE_MFPH_PE11MFP_Msk)) | SYS_GPE_MFPH_PE11MFP_UART1_nRTS;
    SYS->GPC_MFPL = (SYS->GPC_MFPL & (~SYS_GPC_MFPL_PC3MFP_Msk)) | SYS_GPC_MFPL_PC3MFP_UART2_RXD;
    SYS->GPC_MFPL = (SYS->GPC_MFPL & (~SYS_GPC_MFPL_PC2MFP_Msk)) | SYS_GPC_MFPL_PC2MFP_UART2_TXD;
    SYS->GPC_MFPL = (SYS->GPC_MFPL & (~SYS_GPC_MFPL_PC1MFP_Msk)) | SYS_GPC_MFPL_PC1MFP_UART2_nRTS;
}

void WDT_Init(void)
{
    WDT_Close();
    WDT_Open(WDT_TIMEOUT_2POW16, WDT_RESET_DELAY_18CLK, TRUE, FALSE);
    WDT_EnableInt();
    NVIC_EnableIRQ(WDT_IRQn);
    WDT_RESET_COUNTER();
}

/* 確保 Data Flash 位於 BSP_FW_INFO_BASE；若 CONFIG 不符則更新並 Reset */
void DataFlashConfig(void)
{
    uint32_t au32Config[2], u32BaseAddr;

    FMC_Open();
    FMC_ReadConfig(au32Config, 2);
    u32BaseAddr = FMC_ReadDataFlashBaseAddr();

    if (u32BaseAddr == BSP_FW_INFO_BASE) return;

    au32Config[0] &= ~(0x3u << 6u);
    au32Config[0] |=  (0x2u << 6u);
    au32Config[0] &= ~0x1u;
    au32Config[1]  = BSP_FW_INFO_BASE;

    FMC_ENABLE_CFG_UPDATE();
    FMC_WriteConfig(au32Config, 2);
    NVIC_SystemReset();
}

/* 首次燒錄新 BL 後，將舊 2-byte FW_Info 格式遷移至新 8-byte FW_Info_t (Bug B3/B4) */
static void MigrateFWInfoIfNeeded(void)
{
    FW_Info_t fw;
    FlashService_ReadFWInfo(&fw);

    /* 舊格式: active_bank > 1 (舊版 1-indexed) 或 cmd == 0x01 (舊 BTLD_CMD_OTA_UPDATE) */
    if (fw.active_bank > 1u || fw.cmd == 0x01u || fw.cmd == 0x00u)
    {
        fw.active_bank   = 0u;
        fw.cmd           = (uint8_t)BTLD_CMD_NONE;
        fw.bank0_usage   = (uint8_t)BANK_USAGE_EMPTY;
        fw.bank1_usage   = (uint8_t)BANK_USAGE_EMPTY;
        fw.health        = (uint8_t)FW_HEALTH_UNVERIFIED;
        fw.trial_counter = 0u;
        fw.reserved[0]   = 0u;
        fw.reserved[1]   = 0u;
        FlashService_UpdateFWInfo(&fw);
    }
}

int main(void)
{
    MeterV52PinConfig_init();
    SYS_UnlockReg();

    SYS_Init();
    //WDT_Init();
    //SYS_UnlockReg();

    //UART0_Init();
    UART1_Init();
    BL_SysTickInit();

    ReadMyDeviceID();

    /* Data Flash 位址確認（可能 Reset 後重開機） */
    //DataFlashConfig();

    /* ① Flash 服務初始化 — 後續所有 FlashService_* 依賴此呼叫 */
    FlashService_Init(&g_bl_fmc_driver);

    g_apromSize = APROM_SIZE;

    /* ② 首次燒錄格式遷移 */
    //MigrateFWInfoIfNeeded();

    /* ③ 判斷啟動路徑：若 cmd=BTLD_UPDATE_METER → 返回繼續往下；否則跳 App（不返回） */
    Boot_SelectFW();

    /* ④ OTA 接收主迴圈 + Patch（只有進 OTA 模式才執行到這裡） */
    while (1)
    {
        BootloaderProcess();  /* 阻塞直到 OTA DONE / ERROR */
        PatchProcess();       /* _fgPatchEnable=TRUE 時 patch + JumpToApp（不返回） */
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
