/******************************************************************************
 * @file     Select_fw.c
 * @brief    開機韌體選擇與跳轉邏輯
 *           使用 FW_Info_t + FlashService API，修正 Bug B1/B2/B5
 ******************************************************************************/

#include "Select_fw.h"
#include "flash_service.h"
#include "ota_offset_patcher.h"
#include "fw_info.h"

void Boot_SelectFW(void)
{
    FW_Info_t       fw;
    Bank_MetaInfo_t meta;
    uint8_t         fb;
    Bank_MetaInfo_t fb_meta;

    FlashService_ReadFWInfo(&fw);

    /* ① 維修強制跳轉 */
    if (fw.cmd == (uint8_t)BTLD_FORCE_BANK1 ||
        fw.cmd == (uint8_t)BTLD_FORCE_BANK2)
    {
        uint8_t target = (fw.cmd == (uint8_t)BTLD_FORCE_BANK1) ? 0u : 1u;
        fw.cmd = (uint8_t)BTLD_CMD_NONE;
        FlashService_UpdateFWInfo(&fw);
        FlashService_JumpToApp(FlashService_GetBankBase(target));   /* 不返回 */
    }

    /* ② App 觸發 OTA */
    if (fw.cmd == (uint8_t)BTLD_UPDATE_METER)
    {
        fw.cmd = (uint8_t)BTLD_CMD_NONE;
        FlashService_UpdateFWInfo(&fw);
        return;
    }

    /* ③ 跳過接收，直接 patch INCOMING bank（OTA 後斷電繼續 patch） */
    if (fw.cmd == (uint8_t)BTLD_PATCH)
    {
        static Bank_MetaInfo_t s_patch_meta;
        OtaApplyCtx_t          apply_ctx;
        uint8_t                bank;

        fw.cmd = (uint8_t)BTLD_CMD_NONE;
        FlashService_UpdateFWInfo(&fw);

        for (bank = 0u; bank < 2u; bank++)
        {
            FlashService_ReadBankMeta(bank, &s_patch_meta);
            if (s_patch_meta.usage == (uint8_t)BANK_USAGE_INCOMING &&
                s_patch_meta.fw_size > 0u)
            {
                uint32_t fw_pages        = (s_patch_meta.fw_size + FLASH_SVC_PAGE_SIZE - 1u)
                                           / FLASH_SVC_PAGE_SIZE;
                apply_ctx.target_bank  = bank;
                apply_ctx.meta         = &s_patch_meta;
                apply_ctx.payload_size = (fw_pages + 1u) * FLASH_SVC_PAGE_SIZE;
                OtaOffsetPatcher_Apply(&apply_ctx);   /* 成功不返回；失敗繼續 */
                break;
            }
        }
        return;   /* 無 INCOMING bank 或 patch 失敗 → 退回 OTA 接收 */
    }

    /* ④ 健康檢查：Active bank 空白或仍在接收 → 嘗試 Rollback */
    FlashService_ReadBankMeta(fw.active_bank, &meta);

    if (meta.usage == (uint8_t)BANK_USAGE_EMPTY ||
        meta.usage == (uint8_t)BANK_USAGE_INCOMING)
    {
        fb = (fw.active_bank == 0u) ? 1u : 0u;
        FlashService_ReadBankMeta(fb, &fb_meta);

        if (fb_meta.usage == (uint8_t)BANK_USAGE_VALID ||
            fb_meta.usage == (uint8_t)BANK_USAGE_ACTIVE)
        {
            fw.active_bank = fb;
            FlashService_UpdateFWInfo(&fw);
            meta = fb_meta;
        }
    }

    /* ⑤ 啟動計數保護：≥3 次未確認 → 嘗試切換到已確認的 Rollback Bank */
    if (meta.trial_counter >= 3u)
    {
        fb = (fw.active_bank == 0u) ? 1u : 0u;
        FlashService_ReadBankMeta(fb, &fb_meta);

        if (fb_meta.health == (uint8_t)FW_HEALTH_CONFIRMED)
        {
            fw.active_bank = fb;
            FlashService_UpdateFWInfo(&fw);
            fb_meta.trial_counter = 0u;
            FlashService_UpdateBankMeta(fb, &fb_meta);
            FlashService_JumpToApp(FlashService_GetBankBase(fb));   /* 不返回 */
        }
        /* 若 Rollback Bank 也不健康：進入 OTA 接收等待新韌體 */
    }

    /* ⑥ 遞增 trial_counter，App 確認健康後可將其歸零 */
    meta.trial_counter++;
    FlashService_UpdateBankMeta(fw.active_bank, &meta);

    /* ⑦ CRC 驗證：post-patch CRC 不通過 → 嘗試切換對岸 bank，否則進入 OTA 接收 */
    if (meta.fw_size == 0u || meta.fw_crc32 == 0xFFFFFFFFu ||
        !FlashService_VerifyBankCRC(fw.active_bank, meta.fw_size, meta.fw_crc32))
    {
        fb = (fw.active_bank == 0u) ? 1u : 0u;
        FlashService_ReadBankMeta(fb, &fb_meta);

        if ((fb_meta.usage == (uint8_t)BANK_USAGE_VALID ||
             fb_meta.usage == (uint8_t)BANK_USAGE_ACTIVE) &&
            fb_meta.fw_size > 0u && fb_meta.fw_crc32 != 0xFFFFFFFFu &&
            FlashService_VerifyBankCRC(fb, fb_meta.fw_size, fb_meta.fw_crc32))
        {
            fw.active_bank = fb;
            FlashService_UpdateFWInfo(&fw);
            fb_meta.trial_counter = 0u;
            FlashService_UpdateBankMeta(fb, &fb_meta);
            FlashService_JumpToApp(FlashService_GetBankBase(fb));   /* 不返回 */
        }
        return;   /* 兩 bank 均 CRC 失敗 → 進入 OTA 接收等待新韌體 */
    }

    /* ⑧ 跳入 App — 不返回 */
    FlashService_JumpToApp(FlashService_GetBankBase(fw.active_bank));

    while (1) {}   /* 安全哨 */
}
