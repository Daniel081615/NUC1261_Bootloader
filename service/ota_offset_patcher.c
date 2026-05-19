/******************************************************************************
 * @file     ota_offset_patcher.c
 * @brief    OTA offset-based firmware patch 與 App 跳入
 *
 * 流程：
 *  1. 讀取並驗證 metadata page（存入 meta_buf，後續從 RAM 存取）
 *  2. 標記 target bank INCOMING（斷電冪等）
 *  3. 逐頁：只對含 offset 的 page 做 read-erase-write
 *  4. GetCRC32(fwsize) 計算 post-patch final CRC
 *  5. UpdateBankMeta(VALID) → UpdateFWInfo → JumpToApp
 *  錯誤：erase target bank，返回負數，main.c 迴圈重試 OTA
 ******************************************************************************/

#include "ota_offset_patcher.h"
#include "ota_patch_meta.h"
#include "flash_service.h"
#include "MyDef.h"    /* WDT_RESET_COUNTER() */
#include "bsp_flash.h"

static uint32_t BankBase(uint8_t bank)
{
    return (bank == 0u) ? BSP_BANK0_BASE : BSP_BANK1_BASE;
}

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

    if (ctx == NULL || ctx->meta == NULL || ctx->flash == NULL
        || ctx->page_buf == NULL || ctx->meta_buf == NULL
        || ctx->payload_size == 0u
        || (ctx->payload_size % BSP_FLASH_PAGE_SIZE) != 0u)
        return -1;

    bank_base      = BankBase(ctx->target_bank);
    meta_page_addr = bank_base + ctx->payload_size - BSP_FLASH_PAGE_SIZE;

    /*
     * ① 先讀 metadata 到 meta_buf（RAM）再標記 INCOMING。
     *    當 payload_size == BSP_BANK_SIZE 時，metadata page 與 BankMeta page 共址；
     *    必須先讀入 RAM，否則 UpdateBankMeta 的 erase 會摧毀 metadata。
     */
    ret = OtaPatchMeta_ReadAndValidate(ctx->flash, meta_page_addr,
                                        ctx->meta_buf, ctx->meta->fw_size);
    if (ret != OTA_META_OK)
        goto err_exit;

    meta_hdr      = OtaPatchMeta_Header(ctx->meta_buf);
    offsets       = OtaPatchMeta_Offsets(ctx->meta_buf);
    fw_page_count = (meta_hdr->fwsize + BSP_FLASH_PAGE_SIZE - 1u) / BSP_FLASH_PAGE_SIZE;

    /* ② 標記 INCOMING */
    ctx->meta->usage = (uint8_t)BANK_USAGE_INCOMING;
    FlashService_UpdateBankMeta(ctx->target_bank, ctx->meta);
    WDT_RESET_COUNTER();

    /* ③ 逐頁 patch（只處理含有 offset 的 page） */
    off_idx = 0u;
    for (i = 0u; i < fw_page_count; i++)
    {
        uint32_t  page_addr  = bank_base + (i * BSP_FLASH_PAGE_SIZE);
        uint32_t  page_start = i * BSP_FLASH_PAGE_SIZE;
        uint32_t  page_end   = page_start + BSP_FLASH_PAGE_SIZE;
        uint32_t *words;
        uint32_t  j;

        WDT_RESET_COUNTER();

        if (off_idx >= meta_hdr->offset_count || offsets[off_idx] >= page_end)
            continue;  /* 此 page 無 offset，fw bytes 已正確，跳過 */

        ctx->flash->ReadWords(page_addr, (uint32_t *)(void *)ctx->page_buf,
                              BSP_FLASH_PAGE_SIZE / sizeof(uint32_t));

        words = (uint32_t *)(void *)ctx->page_buf;
        for (j = off_idx; j < meta_hdr->offset_count && offsets[j] < page_end; j++)
        {
            uint32_t off      = offsets[j];
            uint32_t word_idx = (off - page_start) / sizeof(uint32_t);
            words[word_idx]  += bank_base;
        }
        off_idx = j;

        ctx->flash->ErasePage(page_addr);
        ctx->flash->WriteWords(page_addr, (uint32_t *)(void *)ctx->page_buf,
                               BSP_FLASH_PAGE_SIZE / sizeof(uint32_t));
    }

    WDT_RESET_COUNTER();

    /* ④ post-patch CRC（fwsize bytes 只，不含 metadata page） */
    ctx->meta->fw_crc32 = ctx->flash->GetCRC32(bank_base, meta_hdr->fwsize);
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
