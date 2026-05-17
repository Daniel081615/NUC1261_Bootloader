#ifndef FLASH_SERVICE_H
#define FLASH_SERVICE_H

#include <stdint.h>
#include "fw_info.h"
#include "bsp_flash.h"

/*
 * IFmcDriver_t — FMC 驅動介面
 * bl_fmc_adapter.c 實作此介面，將 BSP_Flash_* 橋接進來。
 */
typedef struct {
    int32_t  (*Init)(void);
    int32_t  (*ErasePage)(uint32_t addr);
    int32_t  (*WriteWords)(uint32_t addr, const uint32_t *data, uint32_t word_cnt);
    void     (*ReadWords)(uint32_t addr, uint32_t *data, uint32_t word_cnt);
    uint32_t (*GetCRC32)(uint32_t addr, uint32_t byte_len);
    void     (*JumpToApp)(uint32_t app_base);
} IFmcDriver_t;

/* ─── 初始化 ─── */
void FlashService_Init(const IFmcDriver_t *drv);

/* ─── FW_Info 讀寫 (Data Flash at BSP_FW_INFO_BASE) ─── */
void    FlashService_ReadFWInfo(FW_Info_t *fw);
int32_t FlashService_UpdateFWInfo(const FW_Info_t *fw);

/* ─── Bank Meta 讀寫 (at BSP_BANK0/1_META_BASE) ─── */
void    FlashService_ReadBankMeta(uint8_t bank, Bank_MetaInfo_t *meta);
int32_t FlashService_UpdateBankMeta(uint8_t bank, const Bank_MetaInfo_t *meta);

/* ─── OTA 操作 ─── */
int32_t FlashService_EraseBank(uint8_t bank);
int32_t FlashService_WriteFirmware(uint8_t bank, uint32_t offset,
                                   const uint8_t *data, uint32_t byte_len);
_Bool   FlashService_VerifyBankCRC(uint8_t bank, uint32_t fw_size, uint32_t expected_crc);

/* ─── 跳入 App ─── */
void FlashService_JumpToApp(uint32_t app_base);

#endif /* FLASH_SERVICE_H */
