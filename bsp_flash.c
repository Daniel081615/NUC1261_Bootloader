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
    //FMC_ENABLE_DF_UPDATE();   /* Data Flash (FW_Info 0x1F800) 寫入需要獨立開啟 */
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

BSP_FLASH_Status BSP_Flash_WritePage(uint32_t page_addr,
                                     const uint32_t *data,
                                     uint32_t byte_count)
{
    uint32_t word_count;
    BSP_FLASH_Status ret;

    if (data == NULL)
        return BSP_FLASH_ERR_PARAM;

    if ((page_addr % BSP_FLASH_PAGE_SIZE) != 0U)
        return BSP_FLASH_ERR_ALIGN;

    if ((byte_count == 0U) || ((byte_count % 4U) != 0U))
        return BSP_FLASH_ERR_PARAM;

    if (BSP_Flash_IsValidAddr(page_addr, byte_count) != BSP_FLASH_OK)
        return BSP_FLASH_ERR_RANGE;

    word_count = byte_count / 4U;

    if (word_count > (BSP_FLASH_PAGE_SIZE / 4U))
        return BSP_FLASH_ERR_PARAM;

    ret = BSP_Flash_ErasePage(page_addr);
    if (ret != BSP_FLASH_OK)
        return ret;

    ret = BSP_Flash_IsBlank(page_addr, BSP_FLASH_PAGE_SIZE);
    if (ret != BSP_FLASH_OK)
        return ret;

    ret = BSP_Flash_WriteWords(page_addr, data, word_count);
    if (ret != BSP_FLASH_OK)
        return ret;

    ret = BSP_Flash_VerifyPage(page_addr, data, word_count);
    if (ret != BSP_FLASH_OK)
        return ret;

    return BSP_FLASH_OK;
}

BSP_FLASH_Status BSP_Flash_VerifyPage(uint32_t u32Addr,
                                      const uint32_t *u32Data,
                                      uint32_t u32Num)
{
    uint32_t i;
    uint32_t readback;

    if ((u32Data == NULL) || (u32Num == 0U))
        return BSP_FLASH_ERR_PARAM;

    if ((u32Addr % 4U) != 0U)
        return BSP_FLASH_ERR_ALIGN;

    if (BSP_Flash_IsValidAddr(u32Addr, u32Num * 4U) != BSP_FLASH_OK)
        return BSP_FLASH_ERR_RANGE;

    for (i = 0; i < u32Num; i++)
    {
        readback = FMC_Read(u32Addr + i * 4U);
        if (readback != u32Data[i])
            return BSP_FLASH_ERR_VERIFY;
    }

    return BSP_FLASH_OK;
}

BSP_FLASH_Status BSP_Flash_IsBlank(uint32_t u32Addr, uint32_t u32Len)
{
    uint32_t i;

    if ((u32Len == 0U) || ((u32Len % 4U) != 0U))
        return BSP_FLASH_ERR_PARAM;

    if ((u32Addr % 4U) != 0U)
        return BSP_FLASH_ERR_ALIGN;

    if (BSP_Flash_IsValidAddr(u32Addr, u32Len) != BSP_FLASH_OK)
        return BSP_FLASH_ERR_RANGE;

    for (i = 0; i < (u32Len / 4U); i++)
    {
        if (FMC_Read(u32Addr + i * 4U) != 0xFFFFFFFFUL)
            return BSP_FLASH_ERR_BLANK;
    }

    return BSP_FLASH_OK;
}

int32_t BSP_Flash_ReadDataFlashBase(void)
{
    /* Read Data Flash base address */
    return FMC_ReadDataFlashBaseAddr();
}

void BSP_Flash_JumpToApp(uint32_t app_base) {
    uint32_t msp_value = *((volatile uint32_t *)app_base);
    
    if ((msp_value & 0x2FFE0000) != 0x20000000) {
        return; 
    }

    /* NUC1261 專屬的 Vector 映射 */
    FMC_SetVectorPageAddr(app_base);

    uint32_t jump_address = *((volatile uint32_t *)(app_base + 4));
    void (*app_reset_handler)(void) = (void (*)(void))jump_address;

    __disable_irq();

    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    __set_MSP(msp_value);
    app_reset_handler();

    while (1) {}
}

uint32_t BSP_Flash_GetCRC32(uint32_t addr, uint32_t byte_len)
{
    if ((addr % 4U) != 0U)
        return 0xFFFFFFFFUL;

    if ((byte_len == 0U) || ((byte_len % 4U) != 0U))
        return 0xFFFFFFFFUL;

    if (BSP_Flash_IsValidAddr(addr, byte_len) != BSP_FLASH_OK)
        return 0xFFFFFFFFUL;

    return FMC_GetCheckSum(addr, byte_len / 4U);
}
