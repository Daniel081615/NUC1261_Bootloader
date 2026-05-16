/******************************************************************************
 * @file     patch_engine.c
 * @brief    韌體重定位 Patch 引擎（重構版）
 *           使用 BSP_Flash + FlashService API，移除 fmc_user/crc_user 依賴
 ******************************************************************************/

#include "patch_engine.h"
#include "BootloaderProcess.h"
#include "bsp_flash.h"
#include "flash_service.h"
#include "ExternFunc.h"

static uint32_t GetBankBase(uint8_t bank)
{
    return (bank == 0u) ? BSP_BANK0_BASE : BSP_BANK1_BASE;
}

static void VectorTable_Patcher(uint32_t *vectortable)
{
    uint32_t bank_base = GetBankBase(BankID);
    uint8_t  i;

    for (i = 0u; i < (uint8_t)(VectorTableSize / 4u); i++)
    {
        uint32_t VecValue = vectortable[i];
        if (VecValue != 0u && VecValue < g_apromSize)
            vectortable[i] = VecValue + bank_base;
    }
}

static void RegionTable_Patcher(uint32_t *regiontable, uint32_t FwSize, uint16_t PatchCount)
{
    uint32_t bank_base   = GetBankBase(BankID);
    uint16_t num_regions = PatchCount / 4u;  /* 每個 descriptor = {src, dst, size, handler} */
    uint16_t i;

    for (i = 0u; i < num_regions; i++)
    {
        uint32_t *desc = &regiontable[i * 4u];

        /* word[0]: ROM source address — patch */
        if (desc[0] > 0u && desc[0] <= FwSize)
            desc[0] += bank_base;

        /* word[1]: RAM destination — SKIP (SRAM 地址 > FwSize，其實不會被誤 patch，但明確跳過) */

        /* word[2]: byte count — NEVER PATCH，這是最關鍵的 bug fix */
        /* (保持不動，原值不動) */

        /* word[3]: handler function pointer — patch */
        if (desc[3] > 0u && desc[3] <= FwSize)
            desc[3] += bank_base;
    }
}

static void JumpTable_Patcher(uint8_t JmpAdrNum, size_t patch_byte_offset,
                               uint8_t *page_buf, uint32_t fw_size, uint8_t page_idx)
{
    uint32_t bank_base        = GetBankBase(BankID);
    uint32_t NextPageAddr     = bank_base + ((uint32_t)(page_idx + 1u) * BSP_FLASH_PAGE_SIZE);
    uint32_t Plus_2_Page_Base = NextPageAddr + BSP_FLASH_PAGE_SIZE;
    uint8_t  k;

    if (patch_byte_offset + ((uint32_t)JmpAdrNum * 4u) < BSP_FLASH_PAGE_SIZE)
    {
        for (k = 0u; k < JmpAdrNum; k++)
        {
            uint32_t *wp   = (uint32_t *)&page_buf[patch_byte_offset + ((uint32_t)k * 4u)];
            uint32_t  orig = *wp;
            if (orig > 0u && orig < fw_size)
                *wp = orig + bank_base;
        }
    }
    else
    {
        uint32_t JmpTblExceedSize = (uint32_t)(patch_byte_offset + ((uint32_t)JmpAdrNum * 4u))
                                    - BSP_FLASH_PAGE_SIZE;
        uint8_t  in_page_cnt     = JmpAdrNum - (uint8_t)(JmpTblExceedSize / 4u);
        uint8_t  exceed_cnt      = (uint8_t)(JmpTblExceedSize / 4u);

        /* Read the full next page once — no extra stack buffer needed */
        BSP_Flash_ReadWords(NextPageAddr, (uint32_t *)Next_Aprom_Page_Buff,
                            BSP_FLASH_PAGE_SIZE / sizeof(uint32_t));

        /* Patch current-page portion */
        for (k = 0u; k < in_page_cnt; k++)
        {
            uint32_t *wp   = (uint32_t *)&page_buf[patch_byte_offset + ((uint32_t)k * 4u)];
            uint32_t  orig = *wp;
            if (orig > 0u && orig < fw_size)
                *wp = orig + bank_base;
        }

        /* Patch overflow portion directly in Next_Aprom_Page_Buff */
        for (k = 0u; k < exceed_cnt; k++)
        {
            uint32_t *wp = &((uint32_t *)(void *)Next_Aprom_Page_Buff)[k];
            if (*wp > 0u && *wp < fw_size)
                *wp += bank_base;
        }

        BSP_Flash_ErasePage(NextPageAddr);
        BSP_Flash_WriteWords(NextPageAddr, (uint32_t *)Next_Aprom_Page_Buff,
                             BSP_FLASH_PAGE_SIZE / sizeof(uint32_t));
    }

    (void)Plus_2_Page_Base;
}

void PatchProcess(void)
{
    uint32_t  FwSize;
    uint8_t   FwPages;
    uint32_t  bank_base;
    uint32_t  RegionTablePage;
    uint32_t  LastPage;
    uint8_t   i;

    if (!_fgPatchEnable)
        return;

    FwSize    = NewBankMeta.fw_size;
    FwPages   = (uint8_t)((FwSize + BSP_FLASH_PAGE_SIZE - 1u) / BSP_FLASH_PAGE_SIZE);
    bank_base = GetBankBase(BankID);
    LastPage  = (uint32_t)FwPages - 1u;

    /* sentinel disables region-table patching when region_table_addr == 0 */
    RegionTablePage = (NewBankMeta.region_table_addr != 0u)
                      ? (NewBankMeta.region_table_addr / BSP_FLASH_PAGE_SIZE)
                      : 0xFFFFFFFFu;

    /* Mark INCOMING before touching pages — power-loss safe */
    NewBankMeta.usage = (uint8_t)BANK_USAGE_INCOMING;
    FlashService_UpdateBankMeta(BankID, &NewBankMeta);
    WDT_RESET_COUNTER();

    for (i = 0u; i < FwPages; i++)
    {
        uint32_t NowPageAddr = bank_base + ((uint32_t)i * BSP_FLASH_PAGE_SIZE);

        WDT_RESET_COUNTER();
        BSP_Flash_ReadWords(NowPageAddr, (uint32_t *)Aprom_Page_Buff,
                            BSP_FLASH_PAGE_SIZE / sizeof(uint32_t));

        /* ① Vector table (page 0 only) */
        if (i == 0u)
            VectorTable_Patcher((uint32_t *)Aprom_Page_Buff);

        /* ② Region table (skipped when region_table_addr == 0) */
        if ((uint32_t)i >= RegionTablePage && (uint32_t)i <= LastPage)
        {
            uint32_t PatchStartByteOffset = 0u;
            uint16_t PatchCount           = 0u;

            if ((uint32_t)i == RegionTablePage)
                PatchStartByteOffset = NewBankMeta.region_table_addr % BSP_FLASH_PAGE_SIZE;

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
                RegionTable_Patcher((uint32_t *)&Aprom_Page_Buff[idx * sizeof(uint32_t)],
                                    FwSize, PatchCount);
            }
        }

        /* ③ Thumb LDR Rd,[PC,#xx] scan + jump-table detection */
        {
            uint16_t *instr_ptr = (uint16_t *)Aprom_Page_Buff;
            size_t    j;

            for (j = 9u; j < PageInstructNum; j++)
            {
                /* LDR Rd,[PC,#imm8] T1 encoding */
                if ((instr_ptr[j] & 0xF800u) == LDR_r0_INSTR)
                {
                    uint32_t pc_off   = (uint32_t)(instr_ptr[j] & LDR_r0_OFFSET_Msk) * 4u;
                    size_t   byte_off = ((j * 2u) + pc_off + 4u) & (size_t)ALIGN_4Byte_Msk;

                    if (byte_off < BSP_FLASH_PAGE_SIZE)
                    {
                        uint32_t *wp   = (uint32_t *)&Aprom_Page_Buff[byte_off];
                        uint32_t  orig = *wp;
                        if (orig > 0u && orig < FwSize && orig != WDT_RESET_COUNTER_KEYWORD)
                            *wp = orig + bank_base;
                    }
                }

                /* Jump-table: LSLS r1,r0,#2 / ADR r0,{pc}+N / LDR r0,[r0,r1] / MOV pc,r0 */
                if ((instr_ptr[j]     == JmpTbl_MOV_INSTR4)                           &&
                    (instr_ptr[j - 1] == JmpTbl_LDR_INSTR3)                           &&
                    ((instr_ptr[j - 2] & ADR_r0_INSTR_Msk) == JmpTbl_ADR_INSTR2)     &&
                    (instr_ptr[j - 3] == JmpTbl_LSLS_INSTR1))
                {
                    if ((instr_ptr[j - 4] & LDR_r0_sp_OPCODE) == LDR_r0_sp_OPCODE)
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
                            JumpTable_Patcher(JmpAdrNum, byte_off, Aprom_Page_Buff, FwSize, i);
                        }
                    }
                }
            }
        }

        BSP_Flash_ErasePage(NowPageAddr);
        BSP_Flash_WriteWords(NowPageAddr, (uint32_t *)Aprom_Page_Buff,
                             BSP_FLASH_PAGE_SIZE / sizeof(uint32_t));
    }

    WDT_RESET_COUNTER();

    /* Recalculate CRC after patching, then mark VALID */
    NewBankMeta.fw_crc32 = BSP_Flash_GetCRC32(bank_base, FwSize);
    NewBankMeta.usage    = (uint8_t)BANK_USAGE_VALID;
    NewBankMeta.health   = (uint8_t)FW_HEALTH_UNVERIFIED;
    FlashService_UpdateBankMeta(BankID, &NewBankMeta);

    /* Update FW_Info to point to the newly patched bank */
    {
        FW_Info_t fw;
        FlashService_ReadFWInfo(&fw);
        fw.active_bank = BankID;
        fw.cmd         = (uint8_t)BTLD_CMD_NONE;
        FlashService_UpdateFWInfo(&fw);
    }

    FlashService_JumpToApp(bank_base);

    while (1) {}   /* safety sentinel — never reached */
}
