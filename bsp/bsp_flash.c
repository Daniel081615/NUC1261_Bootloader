#include "NUC1261.h"
#include "bsp_flash.h"
#include "fmc.h"

//int32_t g_FMC_i32ErrCode;

void BSP_Flash_Init(void)
{
    /* Unlock protected registers */
    SYS_UnlockReg();

    /* Enable FMC ISP — both APROM and Data Flash must be writable because
     * FW_Info (FW_INFO_BASE 0x1F800) lives in Data Flash. */
    FMC_Open();
    FMC_ENABLE_AP_UPDATE();
}

void BSP_Flash_DeInit(void)
{
    /* Disable FMC ISP function */
		FMC_DISABLE_AP_UPDATE();
		FMC_Close();
	
    /* Lock protected registers */
    SYS_LockReg();
}

BSP_FLASH_Status BSP_Flash_IsValidAddr(uint32_t u32Addr, uint32_t u32Len)
{
    uint32_t end;

    if (u32Len == 0U)
        return BSP_FLASH_ERR_PARAM;

    end = u32Addr + u32Len;
    if (end < u32Addr)                  /* 32-bit 溢位保護 */
        return BSP_FLASH_ERR_RANGE;

    /* APROM 應用程式區：[BSP_APP_BASE, BSP_APROM_END) */
    if (u32Addr >= BSP_APP_BASE && end <= BSP_APROM_END)
        return BSP_FLASH_OK;

    /* Data Flash 區（FW_Info / Bank Meta）：[0x1F800, BSP_APROM_END)
     * 舊版只允許 APROM 範圍，導致 0x1F8xx 位址被靜默拒絕、讀出 stack 垃圾。 */
    if (u32Addr >= 0x1F800UL && end <= BSP_APROM_END)
        return BSP_FLASH_OK;

    return BSP_FLASH_ERR_RANGE;
}

BSP_FLASH_Status BSP_Flash_WriteWords(uint32_t u32Addr,
                                      const uint32_t *pu32Data,
                                      uint32_t u32Count)
{
    uint32_t i;

    if ((pu32Data == NULL) || (u32Count == 0U))
        return BSP_FLASH_ERR_PARAM;

    if ((u32Addr % 4U) != 0U)
        return BSP_FLASH_ERR_ALIGN;

    if (BSP_Flash_IsValidAddr(u32Addr, u32Count * 4U) != BSP_FLASH_OK)
        return BSP_FLASH_ERR_RANGE;

    for (i = 0; i < u32Count; i++)
    {
        FMC_Write(u32Addr + (i * 4U), pu32Data[i]);
    }

    return BSP_FLASH_OK;
}

void BSP_Flash_ReadWords(uint32_t u32Addr, uint32_t *pu32Data, uint32_t u32Count)
{
    uint32_t i;

    if ((pu32Data == NULL) || (u32Count == 0U))
        return;

    if ((u32Addr % 4U) != 0U)
        return;

    if (BSP_Flash_IsValidAddr(u32Addr, u32Count * 4U) != BSP_FLASH_OK)
        return;

    for (i = 0; i < u32Count; i++)
    {
        pu32Data[i] = FMC_Read(u32Addr + (i * 4U));
    }
}

BSP_FLASH_Status BSP_Flash_ErasePage(uint32_t u32Addr)
{
    if ((u32Addr % BSP_FLASH_PAGE_SIZE) != 0U)
        return BSP_FLASH_ERR_ALIGN;

    if (BSP_Flash_IsValidAddr(u32Addr, BSP_FLASH_PAGE_SIZE) != BSP_FLASH_OK)
        return BSP_FLASH_ERR_RANGE;

    if (FMC_Erase(u32Addr) != 0)
        return BSP_FLASH_ERR_ERASE;

    return BSP_FLASH_OK;
}


void BSP_Flash_JumpToApp(uint32_t app_base) 
{
    SYS_UnlockReg();
    FMC_Open();
    FMC_SetVectorPageAddr(app_base);

    NVIC_SystemReset();
}

uint32_t BSP_Flash_GetCRC32(uint32_t addr, uint32_t byte_len)
{
    if ((addr % 4U) != 0U)
        return 0xFFFFFFFFUL;

    if ((byte_len == 0U) || ((byte_len % 4U) != 0U))
        return 0xFFFFFFFFUL;

    if (BSP_Flash_IsValidAddr(addr, byte_len) != BSP_FLASH_OK)
        return 0xFFFFFFFFUL;

    return FMC_GetCheckSum(addr, (int32_t)byte_len);
}

/* Logic extracted verbatim from DataFlashConfig() in original main.c. */
void BSP_Flash_ConfigVerifyAndFix(void)
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

    au32Config[0] &= ~0x3UL;           /* CBS -> 00b: APROM + new IAP */
    au32Config[1]  = BSP_FW_INFO_BASE; /* DFBA -> 0x0001F800 */
    FMC_Erase(FMC_CONFIG_BASE);
    FMC_WriteConfig(au32Config, 2);

    SYS_ResetChip();
    while (1) {}                        /* unreachable — wait for reset */
}
