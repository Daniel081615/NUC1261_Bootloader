/******************************************************************************
 * @file     Select_fw.c
 * @brief    開機韌體選擇與跳轉邏輯（重構版）
 *           使用 FW_Info_t + FlashService API，修正 Bug B1/B2/B5
 ******************************************************************************/

#include "Select_fw.h"
#include "flash_service.h"
#include "fw_info.h"
#include "bsp_flash.h"

static uint32_t GetBankBase(uint8_t bank_index)
{
    return (bank_index == 0u) ? BSP_BANK0_BASE : BSP_BANK1_BASE;
}

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
        FlashService_JumpToApp(GetBankBase(target));   /* 不返回 */
    }

    /* ② App 觸發 OTA (Bug B5 修正: BTLD_UPDATE_METER=0xA1 取代舊 0x01) */
    if (fw.cmd == (uint8_t)BTLD_UPDATE_METER)
    {
        fw.cmd = (uint8_t)BTLD_CMD_NONE;
        FlashService_UpdateFWInfo(&fw);
        return;   /* 返回 → main.c 執行 BootloaderProcess() */
    }

    /* ③ 健康檢查：Active bank 空白或仍在接收 → 嘗試 Rollback */
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

    /* ④ 啟動計數保護：≥3 次未確認 → 嘗試切換到已確認的 Rollback Bank */
    if (meta.trial_counter >= 3u)
    {
        fb = (fw.active_bank == 0u) ? 1u : 0u;
        FlashService_ReadBankMeta(fb, &fb_meta);

        if (fb_meta.health == (uint8_t)FW_HEALTH_CONFIRMED)
        {
            fw.active_bank = fb;
            FlashService_UpdateFWInfo(&fw);
            meta = fb_meta;
            meta.trial_counter = 0u;
        }
        /* 若 Rollback Bank 也不健康：仍嘗試跳，讓 App 決定是否求救 */
				
				return;
    }

    /* ⑤ 遞增 trial_counter，App 確認健康後可將其歸零 */
    meta.trial_counter++;
    FlashService_UpdateBankMeta(fw.active_bank, &meta);

    /* ⑥ 跳入 App — 不返回 */
    FlashService_JumpToApp(GetBankBase(fw.active_bank));

    while (1) {}   /* 安全哨 */
}
