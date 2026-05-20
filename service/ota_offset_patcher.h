#ifndef OTA_OFFSET_PATCHER_H
#define OTA_OFFSET_PATCHER_H

#include <stdint.h>
#include "fw_info.h"

/*
 * OtaApplyCtx_t — 由 main.c（組合根）在每次 OTA_DONE 後填入並傳入
 * OtaOffsetPatcher_Apply()。
 * 所有頁緩衝與 flash 操作由 patcher 內部靜態資源管理。
 */
typedef struct {
    uint8_t          target_bank;   /* 0 or 1，由 BootloaderProcess HandleEnterReq 決定 */
    Bank_MetaInfo_t *meta;          /* patcher 更新 fw_crc32 / usage / health */
    uint32_t         payload_size;  /* page-aligned，用於定位 metadata page */
} OtaApplyCtx_t;

/*
 * 執行 offset-based patch 並跳入 App。成功時不返回。
 * 失敗時 erase target bank，返回負數錯誤碼。
 */
int32_t OtaOffsetPatcher_Apply(OtaApplyCtx_t *ctx);

#endif /* OTA_OFFSET_PATCHER_H */
