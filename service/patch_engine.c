/******************************************************************************
 * @file     patch_engine.c
 * @brief    韌體重定位 Patch 引擎
 *           所有狀態透過 PatchCtx_t 注入，不依賴任何全域變數。
 *           Flash 操作統一走 ctx->flash（IFmcDriver_t），不直接呼叫 BSP。
 ******************************************************************************/

#include "patch_engine.h"
#include "flash_service.h"
#include "MyDef.h"           /* NUC1261.h → WDT_RESET_COUNTER(), BYTE0/1_Msk */
#include <string.h>          /* memcpy */

/* ─── 內部輔助 ─────────────────────────────────────────────────────────── */

static uint32_t GetBankBase(uint8_t bank_id)
{
    return (bank_id == 0u) ? BSP_BANK0_BASE : BSP_BANK1_BASE;
}

/* ─── VectorTable_Patcher ──────────────────────────────────────────────── */

static void VectorTable_Patcher(uint32_t *vectortable,
                                uint32_t  aprom_size,
                                uint32_t  bank_base)
{
    uint8_t i;

    for (i = 0u; i < (uint8_t)(VectorTableSize / 4u); i++)
    {
        uint32_t VecValue = vectortable[i];
        if (VecValue != 0u && VecValue < aprom_size)
            vectortable[i] = VecValue + bank_base;
    }
}

/* ─── RegionTable_Patcher ──────────────────────────────────────────────── */

static void RegionTable_Patcher(uint32_t *regiontable,
                                uint32_t  FwSize,
                                uint16_t  PatchCount,
                                uint32_t  bank_base)
{
    uint16_t num_regions = PatchCount / 4u;
    uint16_t i;

    for (i = 0u; i < num_regions; i++)
    {
        uint32_t *desc = &regiontable[i * 4u];

        /* word[0]: ROM source address */
        if (desc[0] > 0u && desc[0] <= FwSize)
            desc[0] += bank_base;

        /* word[1]: RAM destination — SKIP */

        /* word[2]: byte count — NEVER PATCH */

        /* word[3]: handler function pointer */
        if (desc[3] > 0u && desc[3] <= FwSize)
            desc[3] += bank_base;
    }
}

/* ─── JumpTable_Patcher ────────────────────────────────────────────────── */

/* Returns 1u if jump table overflows into the next page (next_page_buf was updated).
 * next_already_staged: pass current next_pre_staged so flash is not re-read when
 * multiple jump tables on the same page both overflow into the same next page. */
static _Bool JumpTable_Patcher(uint8_t             JmpAdrNum,
                               size_t               patch_byte_offset,
                               uint8_t             *page_buf,
                               uint32_t             fw_size,
                               uint8_t              page_idx,
                               uint32_t             bank_base,
                               const IFmcDriver_t  *flash,
                               uint8_t             *next_page_buf,
                               _Bool                next_already_staged)
{
    uint32_t NextPageAddr = bank_base + ((uint32_t)(page_idx + 1u) * BSP_FLASH_PAGE_SIZE);
    uint8_t  k;

    if (patch_byte_offset + ((uint32_t)JmpAdrNum * 4u) < BSP_FLASH_PAGE_SIZE)
    {
        for (k = 0u; k < JmpAdrNum; k++)
        {
            uint32_t *wp      = (uint32_t *)&page_buf[patch_byte_offset + ((uint32_t)k * 4u)];
            uint32_t  orig    = *wp;
            uint32_t  stripped = orig & ~1u;
            /* Jump table entries must be Thumb pointers (LSB=1) on Cortex-M0 */
            if ((orig & 1u) != 0u && stripped > 0u && stripped < fw_size)
                *wp = orig + bank_base;
        }
        return 0u;
    }
    else
    {
        uint32_t JmpTblExceedSize = (uint32_t)(patch_byte_offset + ((uint32_t)JmpAdrNum * 4u))
                                    - BSP_FLASH_PAGE_SIZE;
        uint8_t  in_page_cnt     = JmpAdrNum - (uint8_t)(JmpTblExceedSize / 4u);
        uint8_t  exceed_cnt      = (uint8_t)(JmpTblExceedSize / 4u);

        /* Only read from flash if the next page hasn't already been staged by a
         * previous jump table on this same page iteration. */
        if (!next_already_staged)
            flash->ReadWords(NextPageAddr, (uint32_t *)next_page_buf,
                             BSP_FLASH_PAGE_SIZE / sizeof(uint32_t));

        for (k = 0u; k < in_page_cnt; k++)
        {
            uint32_t *wp      = (uint32_t *)&page_buf[patch_byte_offset + ((uint32_t)k * 4u)];
            uint32_t  orig    = *wp;
            uint32_t  stripped = orig & ~1u;
            if ((orig & 1u) != 0u && stripped > 0u && stripped < fw_size)
                *wp = orig + bank_base;
        }

        for (k = 0u; k < exceed_cnt; k++)
        {
            uint32_t *wp      = &((uint32_t *)(void *)next_page_buf)[k];
            uint32_t  orig    = *wp;
            uint32_t  stripped = orig & ~1u;
            if ((orig & 1u) != 0u && stripped > 0u && stripped < fw_size)
                *wp = orig + bank_base;
        }

        /* Flash write deferred: caller writes next_page_buf when the loop
         * reaches that page, ensuring all patchers run on it exactly once. */
        return 1u;
    }
}

/* ─── PatchProcess ─────────────────────────────────────────────────────── */

void PatchProcess(PatchCtx_t *ctx)
{
    uint32_t  FwSize;
    uint8_t   FwPages;
    uint32_t  bank_base;
    uint32_t  RegionTablePage;
    uint32_t  LastPage;
    uint8_t   i;
    _Bool     next_pre_staged;

    if (ctx == NULL || ctx->meta == NULL || ctx->flash == NULL)
        return;

    FwSize    = ctx->meta->fw_size;
    FwPages   = (uint8_t)((FwSize + BSP_FLASH_PAGE_SIZE - 1u) / BSP_FLASH_PAGE_SIZE);
    bank_base = GetBankBase(ctx->bank_id);
    LastPage  = (uint32_t)FwPages - 1u;

    RegionTablePage = (ctx->meta->region_table_addr != 0u)
                      ? (ctx->meta->region_table_addr / BSP_FLASH_PAGE_SIZE)
                      : 0xFFFFFFFFu;

    /* Mark INCOMING before touching pages — power-loss safe */
    ctx->meta->usage = (uint8_t)BANK_USAGE_INCOMING;
    FlashService_UpdateBankMeta(ctx->bank_id, ctx->meta);
    WDT_RESET_COUNTER();

    next_pre_staged = 0u;

    for (i = 0u; i < FwPages; i++)
    {
        uint32_t NowPageAddr = bank_base + ((uint32_t)i * BSP_FLASH_PAGE_SIZE);

        WDT_RESET_COUNTER();
        if (next_pre_staged) {
            memcpy(ctx->page_buf, ctx->next_page_buf, BSP_FLASH_PAGE_SIZE);
            next_pre_staged = 0u;
        } else {
            ctx->flash->ReadWords(NowPageAddr, (uint32_t *)ctx->page_buf,
                                  BSP_FLASH_PAGE_SIZE / sizeof(uint32_t));
        }

        /* ① Vector table (page 0 only) */
        if (i == 0u)
            VectorTable_Patcher((uint32_t *)ctx->page_buf, ctx->aprom_size, bank_base);

        /* ② Region table */
        if ((uint32_t)i >= RegionTablePage && (uint32_t)i <= LastPage)
        {
            uint32_t PatchStartByteOffset = 0u;
            uint16_t PatchCount           = 0u;

            if ((uint32_t)i == RegionTablePage)
                PatchStartByteOffset = ctx->meta->region_table_addr % BSP_FLASH_PAGE_SIZE;

            if ((uint32_t)i == RegionTablePage && (uint32_t)i == LastPage)
                PatchCount = (uint16_t)(RegionTableSize / sizeof(uint32_t));
            else if ((uint32_t)i == RegionTablePage)
                PatchCount = (uint16_t)((BSP_FLASH_PAGE_SIZE - PatchStartByteOffset)
                                        / sizeof(uint32_t));
            else if ((uint32_t)i == LastPage)
            {
                uint32_t rem = FwSize - ((uint32_t)i * BSP_FLASH_PAGE_SIZE) - PatchStartByteOffset;
                PatchCount   = (uint16_t)(rem / sizeof(uint32_t));
            }

            if (PatchCount > 0u)
            {
                uint32_t idx = PatchStartByteOffset / (uint32_t)sizeof(uint32_t);
                RegionTable_Patcher((uint32_t *)&ctx->page_buf[idx * sizeof(uint32_t)],
                                    FwSize, PatchCount, bank_base);
            }
        }

        /* ③ Thumb LDR Rd,[PC,#xx] scan + jump-table detection */
        {
            uint16_t *instr_ptr = (uint16_t *)ctx->page_buf;
            size_t    j;

            for (j = 9u; j < PageInstructNum; j++)
            {
                if ((instr_ptr[j] & 0xF800u) == LDR_r0_INSTR)
                {
                    uint32_t pc_off   = (uint32_t)(instr_ptr[j] & LDR_r0_OFFSET_Msk) * 4u;
                    size_t   byte_off = ((j * 2u) + pc_off + 4u) & (size_t)ALIGN_4Byte_Msk;

                    if (byte_off < BSP_FLASH_PAGE_SIZE)
                    {
                        uint32_t *wp      = (uint32_t *)&ctx->page_buf[byte_off];
                        uint32_t  orig    = *wp;
                        uint32_t  stripped = orig & ~1u;
                        /* Thumb ptr: LSB=1, stripped addr in [VectorTableSize, FwSize)
                         * Data ptr:  4-byte aligned, addr in [VectorTableSize, FwSize) */
                        if (orig != WDT_RESET_COUNTER_KEYWORD &&
                            (((orig & 1u) != 0u && stripped >= VectorTableSize && stripped < FwSize) ||
                             ((orig & 3u) == 0u && orig     >= VectorTableSize && orig     < FwSize)))
                            *wp = orig + bank_base;
                    }
                }

                /* Accept both MOV pc,r0 (O0/O1) and BX r0 (O2/O3) as jump dispatch */
                if (((instr_ptr[j] == JmpTbl_MOV_INSTR4) || (instr_ptr[j] == JmpTbl_BX_INSTR4)) &&
                    (instr_ptr[j - 1] == JmpTbl_LDR_INSTR3)                                       &&
                    ((instr_ptr[j - 2] & ADR_r0_INSTR_Msk) == JmpTbl_ADR_INSTR2)                 &&
                    (instr_ptr[j - 3] == JmpTbl_LSLS_INSTR1))
                {
                    int cmp_idx = -1;
                    if ((instr_ptr[j - 6] & BYTE1_Msk) == CMP_r0_INSTR) cmp_idx = (int)(j - 6u);
                    if ((instr_ptr[j - 7] & BYTE1_Msk) == CMP_r0_INSTR) cmp_idx = (int)(j - 7u);
                    if ((instr_ptr[j - 8] & BYTE1_Msk) == CMP_r0_INSTR) cmp_idx = (int)(j - 8u);
                    if ((instr_ptr[j - 9] & BYTE1_Msk) == CMP_r0_INSTR) cmp_idx = (int)(j - 9u);

                    if (cmp_idx != -1)
                    {
                        uint8_t  JmpAdrNum = (uint8_t)(instr_ptr[cmp_idx] & BYTE0_Msk) + 1u;
                        uint32_t pc_off    = (uint32_t)(instr_ptr[j - 2] & ADR_r0_OFFSET_Msk) * 4u;
                        size_t   byte_off  = (((j - 2u) * 2u) + pc_off + 4u)
                                             & (size_t)ALIGN_4Byte_Msk;
                        if (JumpTable_Patcher(JmpAdrNum, byte_off,
                                              ctx->page_buf, FwSize, i,
                                              bank_base, ctx->flash, ctx->next_page_buf,
                                              next_pre_staged))
                            next_pre_staged = 1u;
                    }
                }
            }
        }

        ctx->flash->ErasePage(NowPageAddr);
        ctx->flash->WriteWords(NowPageAddr, (uint32_t *)ctx->page_buf,
                               BSP_FLASH_PAGE_SIZE / sizeof(uint32_t));
    }

    WDT_RESET_COUNTER();

    /* Recalculate CRC after patching, then mark VALID */
    ctx->meta->fw_crc32 = ctx->flash->GetCRC32(bank_base, FwSize);
    ctx->meta->usage    = (uint8_t)BANK_USAGE_VALID;
    ctx->meta->health   = (uint8_t)FW_HEALTH_UNVERIFIED;
    FlashService_UpdateBankMeta(ctx->bank_id, ctx->meta);

    {
        FW_Info_t fw;
        FlashService_ReadFWInfo(&fw);
        fw.active_bank = ctx->bank_id;
        fw.cmd         = (uint8_t)BTLD_CMD_NONE;
        FlashService_UpdateFWInfo(&fw);
    }

    FlashService_JumpToApp(bank_base);

    while (1) {}   /* safety sentinel — never reached */
}
