#ifndef OTA_OFFSET_PATCHER_H
#define OTA_OFFSET_PATCHER_H

#include <stdint.h>
#include "fw_info.h"
#include "flash_service.h"

/*
 * OtaApplyCtx_t — 依賴注入容器，由 main.c 組裝後傳入 OtaOffsetPatcher_Apply()。
 *
 * 靜態欄位（迴圈外一次設定）：page_buf, meta_buf, flash
 * 動態欄位（每次 OTA_DONE 後填入）：target_bank, meta, payload_size
 */
typedef struct {
    uint8_t             target_bank;   /* 0 or 1，由 BootloaderProcess HandleEnterReq 決定 */
    Bank_MetaInfo_t    *meta;          /* patcher 更新 fw_crc32 / usage / health */
    uint32_t            payload_size;  /* page-aligned，用於定位 metadata page */
    uint8_t            *page_buf;      /* BSP_FLASH_PAGE_SIZE bytes，4-byte aligned */
    uint8_t            *meta_buf;      /* BSP_FLASH_PAGE_SIZE bytes，4-byte aligned */
    const IFmcDriver_t *flash;
} OtaApplyCtx_t;

/*
 * 執行 offset-based patch 並跳入 App。成功時不返回。
 * 失敗時 erase target bank，返回負數錯誤碼。
 */
int32_t OtaOffsetPatcher_Apply(OtaApplyCtx_t *ctx);

#endif /* OTA_OFFSET_PATCHER_H */
