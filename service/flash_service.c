#include "flash_service.h"
#include "bsp_flash.h"
#include <string.h>

/* ─── 內部輔助 ─── */

static uint32_t BankBase(uint8_t bank)
{
    return (bank == 0u) ? BSP_BANK0_BASE : BSP_BANK1_BASE;
}

static uint32_t BankMetaBase(uint8_t bank)
{
    return (bank == 0u) ? BSP_BANK0_META_BASE : BSP_BANK1_META_BASE;
}

/* ─── 初始化 ─── */

void FlashService_Init(void)
{
    BSP_Flash_Init();
}

/* ─── FW_Info 讀寫 (Data Flash at BSP_FW_INFO_BASE) ─── */

void FlashService_ReadFWInfo(FW_Info_t *fw)
{
    if (fw == NULL) return;
    memset(fw, 0xFF, sizeof(FW_Info_t));
    BSP_Flash_ReadWords(BSP_FW_INFO_BASE, (uint32_t *)fw,
                        (sizeof(FW_Info_t) + 3u) / 4u);
}

int32_t FlashService_UpdateFWInfo(const FW_Info_t *fw)
{
    int32_t ret;
    if (fw == NULL) return -1;

    ret = (int32_t)BSP_Flash_ErasePage(BSP_FW_INFO_BASE);
    if (ret != 0) return ret;

    return (int32_t)BSP_Flash_WriteWords(BSP_FW_INFO_BASE, (const uint32_t *)fw,
                                         (sizeof(FW_Info_t) + 3u) / 4u);
}

/* ─── Bank Meta 讀寫 ─── */

void FlashService_ReadBankMeta(uint8_t bank, Bank_MetaInfo_t *meta)
{
    if (meta == NULL) return;
    memset(meta, 0xFF, sizeof(Bank_MetaInfo_t));
    BSP_Flash_ReadWords(BankMetaBase(bank), (uint32_t *)meta,
                        (sizeof(Bank_MetaInfo_t) + 3u) / 4u);
}

int32_t FlashService_UpdateBankMeta(uint8_t bank, const Bank_MetaInfo_t *meta)
{
    uint32_t meta_base;
    int32_t  ret;

    if (meta == NULL) return -1;
    meta_base = BankMetaBase(bank);

    ret = (int32_t)BSP_Flash_ErasePage(meta_base);
    if (ret != 0) return ret;

    return (int32_t)BSP_Flash_WriteWords(meta_base, (const uint32_t *)meta,
                                         (sizeof(Bank_MetaInfo_t) + 3u) / 4u);
}

/* ─── OTA 操作 ─── */

int32_t FlashService_EraseBank(uint8_t bank)
{
    uint32_t base = BankBase(bank);
    uint32_t end  = base + BSP_BANK_SIZE;
    uint32_t addr;
    int32_t  ret;

    for (addr = base; addr < end; addr += BSP_FLASH_PAGE_SIZE)
    {
        ret = (int32_t)BSP_Flash_ErasePage(addr);
        if (ret != 0) return ret;
    }
    return 0;
}

int32_t FlashService_WriteFirmware(uint8_t bank, uint32_t offset,
                                   const uint8_t *data, uint32_t byte_len)
{
    uint32_t dest_addr;

    if (data == NULL)          return -1;
    if ((byte_len % 4u) != 0u) return -1;

    dest_addr = BankBase(bank) + offset;
    return (int32_t)BSP_Flash_WriteWords(dest_addr, (const uint32_t *)data,
                                         byte_len / 4u);
}

_Bool FlashService_VerifyBankCRC(uint8_t bank, uint32_t fw_size, uint32_t expected_crc)
{
    uint32_t calc;
    if (fw_size == 0u) return 0;

    calc = BSP_Flash_GetCRC32(BankBase(bank), fw_size);
    return (calc == expected_crc);
}

/* ─── 跳入 App ─── */

void FlashService_JumpToApp(uint32_t app_base)
{
    BSP_Flash_JumpToApp(app_base);
    while (1) {}    /* safety sentinel */
}

void FlashService_JumpToBank(uint8_t bank)
{
    BSP_Flash_JumpToApp(BankBase(bank));
    while (1) {}    /* safety sentinel */
}

/* ─── 低層 flash 操作（供 service 層內部模組使用） ─── */

uint32_t FlashService_GetBankBase(uint8_t bank)
{
    return BankBase(bank);
}

void FlashService_ReadPage(uint32_t addr, uint32_t *buf, uint32_t word_cnt)
{
    BSP_Flash_ReadWords(addr, buf, word_cnt);
}

int32_t FlashService_EraseSinglePage(uint32_t addr)
{
    return (int32_t)BSP_Flash_ErasePage(addr);
}

int32_t FlashService_WritePageWords(uint32_t addr, const uint32_t *buf, uint32_t word_cnt)
{
    return (int32_t)BSP_Flash_WriteWords(addr, buf, word_cnt);
}

uint32_t FlashService_CalcCRC32(uint32_t addr, uint32_t byte_len)
{
    return BSP_Flash_GetCRC32(addr, byte_len);
}
