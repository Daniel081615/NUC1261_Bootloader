#include "flash_service.h"
#include <string.h>

static const IFmcDriver_t *g_drv = NULL;

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

void FlashService_Init(const IFmcDriver_t *drv)
{
    g_drv = drv;
    if (g_drv != NULL && g_drv->Init != NULL)
        g_drv->Init();
}

/* ─── FW_Info 讀寫 (Data Flash at BSP_FW_INFO_BASE) ─── */

void FlashService_ReadFWInfo(FW_Info_t *fw)
{
    if (fw == NULL || g_drv == NULL) return;
    memset(fw, 0xFF, sizeof(FW_Info_t));
    g_drv->ReadWords(BSP_FW_INFO_BASE, (uint32_t *)fw,
                     (sizeof(FW_Info_t) + 3u) / 4u);
}

int32_t FlashService_UpdateFWInfo(const FW_Info_t *fw)
{
    int32_t ret;
    if (fw == NULL || g_drv == NULL) return -1;

    ret = g_drv->ErasePage(BSP_FW_INFO_BASE);
    if (ret != 0) return ret;

    return g_drv->WriteWords(BSP_FW_INFO_BASE, (const uint32_t *)fw,
                             (sizeof(FW_Info_t) + 3u) / 4u);
}

/* ─── Bank Meta 讀寫 ─── */

void FlashService_ReadBankMeta(uint8_t bank, Bank_MetaInfo_t *meta)
{
    if (meta == NULL || g_drv == NULL) return;
    memset(meta, 0xFF, sizeof(Bank_MetaInfo_t));
    g_drv->ReadWords(BankMetaBase(bank), (uint32_t *)meta,
                     (sizeof(Bank_MetaInfo_t) + 3u) / 4u);
}

int32_t FlashService_UpdateBankMeta(uint8_t bank, const Bank_MetaInfo_t *meta)
{
    uint32_t meta_base;
    int32_t  ret;

    if (meta == NULL || g_drv == NULL) return -1;
    meta_base = BankMetaBase(bank);

    ret = g_drv->ErasePage(meta_base);
    if (ret != 0) return ret;

    return g_drv->WriteWords(meta_base, (const uint32_t *)meta,
                             (sizeof(Bank_MetaInfo_t) + 3u) / 4u);
}

/* ─── OTA 操作 ─── */

int32_t FlashService_EraseBank(uint8_t bank)
{
    uint32_t base = BankBase(bank);
    uint32_t end  = base + BSP_BANK_SIZE;
    uint32_t addr;
    int32_t  ret;

    if (g_drv == NULL) return -1;

    for (addr = base; addr < end; addr += BSP_FLASH_PAGE_SIZE)
    {
        ret = g_drv->ErasePage(addr);
        if (ret != 0) return ret;
    }
    return 0;
}

int32_t FlashService_WriteFirmware(uint8_t bank, uint32_t offset,
                                   const uint8_t *data, uint32_t byte_len)
{
    uint32_t dest_addr;

    if (data == NULL || g_drv == NULL) return -1;
    if ((byte_len % 4u) != 0u)         return -1;

    dest_addr = BankBase(bank) + offset;
    return g_drv->WriteWords(dest_addr, (const uint32_t *)data, byte_len / 4u);
}

_Bool FlashService_VerifyBankCRC(uint8_t bank, uint32_t fw_size, uint32_t expected_crc)
{
    uint32_t calc;
    if (g_drv == NULL || fw_size == 0u) return 0;

    calc = g_drv->GetCRC32(BankBase(bank), fw_size);
    return (calc == expected_crc);
}

/* ─── 跳入 App ─── */

void FlashService_JumpToApp(uint32_t app_base)
{
    if (g_drv != NULL && g_drv->JumpToApp != NULL)
        g_drv->JumpToApp(app_base);
    while (1) {}
}
