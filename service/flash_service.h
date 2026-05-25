#ifndef FLASH_SERVICE_H
#define FLASH_SERVICE_H

#include <stdint.h>
#include "fw_info.h"

/* Flash geometry constants — aliased from HAL so BSP is the single source of truth. */
#include "hal_flash.h"
#define FLASH_SVC_PAGE_SIZE  HAL_FLASH_PAGE_SIZE
#define FLASH_SVC_BANK_SIZE  HAL_FLASH_BANK_SIZE

/* ─── 初始化 ─── */
void FlashService_Init(void);

/* ─── FW_Info 讀寫 (Data Flash at BSP_FW_INFO_BASE) ─── */
void    FlashService_ReadFWInfo(FW_Info_t *fw);
int32_t FlashService_UpdateFWInfo(const FW_Info_t *fw);

/* ─── Bank Meta 讀寫 ─── */
void    FlashService_ReadBankMeta(uint8_t bank, Bank_MetaInfo_t *meta);
int32_t FlashService_UpdateBankMeta(uint8_t bank, const Bank_MetaInfo_t *meta);

/* ─── OTA 操作 ─── */
int32_t FlashService_EraseBank(uint8_t bank);
int32_t FlashService_WriteFirmware(uint8_t bank, uint32_t offset,
                                   const uint8_t *data, uint32_t byte_len);
_Bool   FlashService_VerifyBankCRC(uint8_t bank, uint32_t fw_size, uint32_t expected_crc);

/* ─── 低層 flash 操作（供 service 層內部模組使用） ─── */
uint32_t FlashService_GetBankBase(uint8_t bank);
void     FlashService_ReadPage(uint32_t addr, uint32_t *buf, uint32_t word_cnt);
int32_t  FlashService_EraseSinglePage(uint32_t addr);
int32_t  FlashService_WritePageWords(uint32_t addr, const uint32_t *buf, uint32_t word_cnt);
uint32_t FlashService_CalcCRC32(uint32_t addr, uint32_t byte_len);

/* ─── 跳入 App ─── */
void FlashService_JumpToApp(uint32_t app_base);
void FlashService_JumpToBank(uint8_t bank);

#endif /* FLASH_SERVICE_H */
