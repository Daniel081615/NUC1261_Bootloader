/******************************************************************************
 * @file     ota_offset_patcher.c
 * @brief    OTA offset-based firmware patch 與 App 跳入
 *
 * 流程：
 *  1. 讀取並驗證 metadata page（存入 s_meta_buf，後續從 RAM 存取）
 *  2. 標記 target bank INCOMING（斷電冪等）
 *  3. 逐頁：只對含 offset 的 page 做 read-erase-write
 *  4. CalcCRC32(fwsize) 計算 post-patch final CRC
 *  5. UpdateBankMeta(VALID) → UpdateFWInfo → JumpToApp
 *  錯誤：erase target bank，返回負數，main.c 迴圈重試 OTA
 ******************************************************************************/

#include <string.h>
#include "ota_offset_patcher.h"
#include "ota_patch_meta.h"
#include "flash_service.h"
#include "hal_sys.h"

static __attribute__((aligned(4))) uint8_t s_page_buf[FLASH_SVC_PAGE_SIZE];
static __attribute__((aligned(4))) uint8_t s_meta_buf[FLASH_SVC_PAGE_SIZE];

int32_t OtaOffsetPatcher_Apply(OtaApplyCtx_t *ctx)
{
    uint32_t              bank_base;
    uint32_t              meta_page_addr;
    const OtaPatchMeta_t *meta_hdr;
    const uint32_t       *offsets;
    uint32_t              fw_page_count;
    uint32_t              off_idx;
    uint32_t              i;
    int32_t               ret;

    if (ctx == NULL || ctx->meta == NULL
        || ctx->payload_size == 0u
        || (ctx->payload_size % FLASH_SVC_PAGE_SIZE) != 0u)
        return -1;

    bank_base      = FlashService_GetBankBase(ctx->target_bank);
    meta_page_addr = bank_base + ctx->payload_size - FLASH_SVC_PAGE_SIZE;

    /*
     * ① 先讀 metadata 到 s_meta_buf（RAM）再標記 INCOMING。
     *    當 payload_size == FLASH_SVC_BANK_SIZE 時，metadata page 與 BankMeta page 共址；
     *    必須先讀入 RAM，否則 UpdateBankMeta 的 erase 會摧毀 metadata。
     */
    ret = OtaPatchMeta_ReadAndValidate(meta_page_addr, s_meta_buf, ctx->meta->fw_size);
    if (ret != OTA_META_OK)
        goto err_exit;

    meta_hdr      = OtaPatchMeta_Header(s_meta_buf);
    offsets       = OtaPatchMeta_Offsets(s_meta_buf);
    fw_page_count = (meta_hdr->fwsize + FLASH_SVC_PAGE_SIZE - 1u) / FLASH_SVC_PAGE_SIZE;

    /* ② 標記 INCOMING */
    ctx->meta->usage = (uint8_t)BANK_USAGE_INCOMING;
    FlashService_UpdateBankMeta(ctx->target_bank, ctx->meta);
    HAL_WDT_Feed();

    /* ③ 逐頁 patch（只處理含有 offset 的 page） */
    off_idx = 0u;
    for (i = 0u; i < fw_page_count; i++)
    {
        uint32_t  page_addr  = bank_base + (i * FLASH_SVC_PAGE_SIZE);
        uint32_t  page_start = i * FLASH_SVC_PAGE_SIZE;
        uint32_t  page_end   = page_start + FLASH_SVC_PAGE_SIZE;
        uint32_t *words;
        uint32_t  j;

        HAL_WDT_Feed();

        if (off_idx >= meta_hdr->offset_count || offsets[off_idx] >= page_end)
            continue;  /* 此 page 無 offset，fw bytes 已正確，跳過 */

        FlashService_ReadPage(page_addr, (uint32_t *)(void *)s_page_buf,
                              FLASH_SVC_PAGE_SIZE / sizeof(uint32_t));

        words = (uint32_t *)(void *)s_page_buf;
        for (j = off_idx; j < meta_hdr->offset_count && offsets[j] < page_end; j++)
        {
            uint32_t off      = offsets[j];
            uint32_t word_idx = (off - page_start) / sizeof(uint32_t);
            words[word_idx]  += bank_base;
        }
        off_idx = j;

        FlashService_EraseSinglePage(page_addr);
        FlashService_WritePageWords(page_addr, (uint32_t *)(void *)s_page_buf,
                                    FLASH_SVC_PAGE_SIZE / sizeof(uint32_t));
    }

    HAL_WDT_Feed();

    /* ④ post-patch CRC（fwsize bytes 只，不含 metadata page） */
    ctx->meta->fw_crc32 = FlashService_CalcCRC32(bank_base, meta_hdr->fwsize);
    ctx->meta->usage    = (uint8_t)BANK_USAGE_VALID;
    ctx->meta->health   = (uint8_t)FW_HEALTH_UNVERIFIED;
    FlashService_UpdateBankMeta(ctx->target_bank, ctx->meta);

    /* ⑤ 更新 active bank，跳入 App */
    {
        FW_Info_t fw;
        FlashService_ReadFWInfo(&fw);
        fw.active_bank = ctx->target_bank;
        fw.cmd         = (uint8_t)BTLD_CMD_NONE;
        FlashService_UpdateFWInfo(&fw);
    }

    FlashService_JumpToApp(bank_base);

    while (1) {}   /* safety sentinel */

err_exit:
    FlashService_EraseBank(ctx->target_bank);
    return ret;
}
