/* bsp_flash.c — M031LE3AE FMC flash implementation.
 * This is the ONLY file in the M031 BSP that includes M031Series.h for FMC.
 * Key difference from NUC1261: page size = 512 B, CRC function = FMC_GetChkSum(). */

#include "M031Series.h"
#include "bsp_flash.h"

void BSP_Flash_Init(void)
{
    SYS_UnlockReg();
    FMC_Open();
    FMC_ENABLE_AP_UPDATE();
}

void BSP_Flash_DeInit(void)
{
    FMC_DISABLE_AP_UPDATE();
    FMC_Close();
    SYS_LockReg();
}

BSP_FLASH_Status BSP_Flash_IsValidAddr(uint32_t u32Addr, uint32_t u32Len)
{
    uint32_t end;

    if (u32Len == 0U)
        return BSP_FLASH_ERR_PARAM;

    end = u32Addr + u32Len;
    if (end < u32Addr)
        return BSP_FLASH_ERR_RANGE;

    if (u32Addr >= BSP_APP_BASE && end <= BSP_APROM_END)
        return BSP_FLASH_OK;

    /* Data Flash region (FW_Info) */
    if (u32Addr >= BSP_FW_INFO_BASE && end <= BSP_APROM_END)
        return BSP_FLASH_OK;

    return BSP_FLASH_ERR_RANGE;
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
        FMC_Write(u32Addr + (i * 4U), pu32Data[i]);

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
        pu32Data[i] = FMC_Read(u32Addr + (i * 4U));
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

    /* M031 uses FMC_GetChkSum(), not FMC_GetCheckSum() as on NUC1261 */
    return FMC_GetChkSum(addr, byte_len);
}

void BSP_Flash_ConfigVerifyAndFix(void)
{
    uint32_t au32Config[2];

    FMC_Open();
    FMC_ENABLE_CFG_UPDATE();

    au32Config[0] = FMC_Read(FMC_CONFIG_BASE);
    au32Config[1] = FMC_Read(FMC_CONFIG_BASE + 4U);

    /* CBS[1:0] = 00b (APROM+IAP), DFBA = BSP_FW_INFO_BASE */
    if ((au32Config[0] & 0x3UL) == 0x0UL && au32Config[1] == BSP_FW_INFO_BASE)
    {
        FMC_DISABLE_CFG_UPDATE();
        FMC_Close();
        return;
    }

    au32Config[0] &= ~0x3UL;
    au32Config[1]  = BSP_FW_INFO_BASE;
    FMC_Erase(FMC_CONFIG_BASE);
    FMC_Write(FMC_CONFIG_BASE,      au32Config[0]);
    FMC_Write(FMC_CONFIG_BASE + 4U, au32Config[1]);

    SYS_ResetChip();
    while (1) {}
}
