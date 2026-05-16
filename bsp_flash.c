#include "NUC1261.h"
#include "bsp_flash.h"
#include "fmc.h"

int32_t g_FMC_i32ErrCode;

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


void BSP_Flash_JumpToApp(uint32_t app_base) {
    // 1. 取得 Application 的初始堆疊指標 (Initial Stack Pointer, MSP)
    uint32_t msp_value = *((volatile uint32_t *)app_base);
    
    // 檢查堆疊指標 (MSP) 是否落在合法的內部記憶體 (SRAM) 範圍內
    if ((msp_value & 0x2FFE0000) != 0x20000000) {
        return; 
    }

    // 2. 解鎖系統暫存器與開啟 FMC (System Unlock & FMC Open)
    // 這是 Nuvoton 晶片的關鍵：必須解鎖才能操作快閃記憶體控制器 (FMC)
    SYS_UnlockReg();
    FMC_Open();

    // 3. 設定硬體向量映射 (Vector Mapping)
    // 將 0x00000000 的存取硬體重新導向至您的 app_base
    FMC_SetVectorPageAddr(app_base);

    // 4. 取得 Application 的程式進入點 (Reset Handler)
    uint32_t jump_address = *((volatile uint32_t *)(app_base + 4));
    void (*app_reset_handler)(void) = (void (*)(void))jump_address;

    // 5. 關閉全域中斷 (Global Interrupts)
    __disable_irq();

    // 6. 關閉並清除系統滴答定時器 (SysTick)
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    // 7. 徹底禁用並清除巢狀向量中斷控制器 (NVIC) 的所有狀態
    // 確保不會有任何 UART 或 Timer 的殘留中斷在 App 開機時誤觸發
    // Cortex-M 家族通常有最多 8 個 NVIC 暫存器 (視支援的中斷數量而定，寫入 0xFFFFFFFF 會清除全部)
    for (int i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF; // 禁用中斷 (Interrupt Clear-Enable Register)
        NVIC->ICPR[i] = 0xFFFFFFFF; // 清除待處理標誌 (Interrupt Clear-Pending Register)
    }

    // 8. 重新鎖定系統暫存器 (System Lock) - 安全考量
    SYS_LockReg();

    // 9. 設定主堆疊指標 (Main Stack Pointer) 至 App 的設定值
    __set_MSP(msp_value);

    // 10. 執行跳躍 (Jump)
    app_reset_handler();

    // 程式永遠不該執行到這裡，若發生異常則卡在死迴圈 (Infinite Loop)
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
