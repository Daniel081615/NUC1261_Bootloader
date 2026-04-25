#include "patch_engine.h"
#include "string.h"
#include "BootloaderProcess.h"
#include "Select_fw.h"
#include "fw_metadata.h"
#include "crc_user.h"
#include "fmc_user.h"
#include "ExternFunc.h"

static void VectorTable_Patcher(uint32_t *vectortable);
static void RegionTable_Patcher(uint32_t *regiontable, size_t FwSize, uint16_t PatchCount);
static void JumpTable_Patcher(uint8_t JmpAdrNum, size_t patch_byte_offset, uint8_t *page_buf, uint32_t fw_size, uint8_t i);

static void VectorTable_Patcher(uint32_t *vectortable)
{
    for (uint8_t i = 0; i < (VectorTableSize / 4); i++)
    {
        uint32_t VecValue = vectortable[i];
        if (VecValue != 0 && (VecValue < g_apromSize))
        {
            VecValue += Fw_BaseAddr[BankID][FW];
            vectortable[i] = VecValue;
        }
    }
}

static void RegionTable_Patcher(uint32_t *regiontable, size_t FwSize, uint16_t PatchCount)
{
    uint32_t RegnTblVal = 0;

    for (uint8_t i = 0; i < PatchCount; i++)
    {
        RegnTblVal = regiontable[i];
        if ((RegnTblVal > 0) && (RegnTblVal <= FwSize))
        {
            RegnTblVal += Fw_BaseAddr[BankID][FW];
            regiontable[i] = RegnTblVal;
        }
    }
}

static void JumpTable_Patcher(uint8_t JmpAdrNum, size_t patch_byte_offset, uint8_t *page_buf, uint32_t fw_size, uint8_t i)
{
    uint32_t NextPageAddr     = Fw_BaseAddr[BankID][FW] + ((i + 1) * FMC_FLASH_PAGE_SIZE);
    uint32_t Plus_2_Page_Base = NextPageAddr + FMC_FLASH_PAGE_SIZE;

    if (patch_byte_offset + (JmpAdrNum * 4) < FMC_FLASH_PAGE_SIZE)
    {
        for (uint8_t k = 0; k < JmpAdrNum; k++)
        {
            uint32_t *wp          = (uint32_t *)&page_buf[patch_byte_offset + k * 4];
            uint32_t  original_addr = *wp;
            if ((original_addr > 0) && (original_addr < fw_size))
                *wp = original_addr + Fw_BaseAddr[BankID][FW];
        }
    }
    else
    {
        uint32_t JmpTblExceedSize = (patch_byte_offset + (JmpAdrNum * 4)) - FMC_FLASH_PAGE_SIZE;

        // FIX: was VLA uint32_t ExceedData[JmpTblExceedSize/4] -- unsafe on embedded stack
        uint32_t ExceedData[FMC_FLASH_PAGE_SIZE / sizeof(uint32_t)];

        ReadData(NextPageAddr, NextPageAddr + JmpTblExceedSize, ExceedData);
        ReadData(NextPageAddr, Plus_2_Page_Base, (uint32_t *)Next_Aprom_Page_Buff);

        // Patch current-page portion of jump table
        for (uint8_t k = 0; k < (JmpAdrNum - (JmpTblExceedSize / 4)); k++)
        {
            uint32_t *wp          = (uint32_t *)&page_buf[patch_byte_offset + k * 4];
            uint32_t  original_addr = *wp;
            if ((original_addr > 0) && (original_addr < fw_size))
                *wp = original_addr + Fw_BaseAddr[BankID][FW];
        }

        // Patch overflow portion in next page
        // FIX: was &ExceedData[k*4] (wrong -- ExceedData is uint32_t[], use index directly)
        for (uint8_t k = 0; k < (JmpTblExceedSize / 4); k++)
        {
            uint32_t original_addr = ExceedData[k];
            if ((original_addr > 0) && (original_addr < fw_size))
                ExceedData[k] = original_addr + Fw_BaseAddr[BankID][FW];
        }

        memcpy(Next_Aprom_Page_Buff, ExceedData, JmpTblExceedSize);

        FMC_Erase_User(NextPageAddr);
        WriteData(NextPageAddr, Plus_2_Page_Base, (uint32_t *)Next_Aprom_Page_Buff);
    }
}

void PatchProcess(void)
{
    if (!_fgPatchEnable)
        return;

    uint32_t FwSize  = NewBankMeta.Size;
    uint8_t  FwPages = (FwSize + FMC_FLASH_PAGE_SIZE - 1) / FMC_FLASH_PAGE_SIZE;

    uint32_t RegionTablePage = NewBankMeta.RegionTable_Addr / FMC_FLASH_PAGE_SIZE;
    uint32_t LastPage        = FwPages - 1;

    /* Mark bank INVALID before touching pages -- power-loss safe */
    NewBankMeta.flags = FW_INVALID_FLAG;
    BankMetaUpdate(&NewBankMeta);
    WDT_RESET_COUNTER();

    for (uint8_t i = 0; i < FwPages; i++)
    {
        uint32_t NowPageAddr  = Fw_BaseAddr[BankID][FW] + (i * FMC_FLASH_PAGE_SIZE);
        uint32_t NextPageAddr = NowPageAddr + FMC_FLASH_PAGE_SIZE;

        WDT_RESET_COUNTER();
        ReadData(NowPageAddr, NextPageAddr, (uint32_t *)Aprom_Page_Buff);

        if (i == 0)
            VectorTable_Patcher((uint32_t *)Aprom_Page_Buff);

        if (i >= RegionTablePage && i <= LastPage)
        {
            uint32_t PatchStartByteOffset = 0;
            uint16_t PatchCount = 0;

            if (i == RegionTablePage)
                PatchStartByteOffset = NewBankMeta.RegionTable_Addr % FMC_FLASH_PAGE_SIZE;

            if (i == RegionTablePage && i == LastPage)
                PatchCount = RegionTableSize / sizeof(uint32_t);
            else if (i == RegionTablePage)
                PatchCount = (FMC_FLASH_PAGE_SIZE - PatchStartByteOffset) / sizeof(uint32_t);
            else if (i == LastPage)
            {
                uint32_t RemainingBytes = FwSize - (i * FMC_FLASH_PAGE_SIZE) - PatchStartByteOffset;
                PatchCount = RemainingBytes / sizeof(uint32_t);
            }

            uint16_t PatchStartIdx = PatchStartByteOffset / sizeof(uint32_t);
            if (PatchCount > 0)
                RegionTable_Patcher((uint32_t *)&Aprom_Page_Buff[PatchStartIdx], FwSize, PatchCount);
        }

        // Scan Thumb LDR Rd,[PC,#xx] (T1 encoding: mask 0xF800 == 0x4800, Rd = r0..r7)
        uint16_t *instr_ptr = (uint16_t *)Aprom_Page_Buff;
        for (size_t j = 9; j < PageInstructNum; j++)
        {
            if ((instr_ptr[j] & 0xF800) == LDR_r0_INSTR)
            {
                uint32_t pc_offset        = (instr_ptr[j] & BYTE0_Msk) * 4;
                size_t   patch_byte_offset = ((j * 2) + pc_offset + 4) & ALIGN_4Byte_Msk;

                if (patch_byte_offset < FMC_FLASH_PAGE_SIZE)
                {
                    uint32_t *wp          = (uint32_t *)&Aprom_Page_Buff[patch_byte_offset];
                    uint32_t  original_addr = *wp;

                    if ((original_addr > 0) && (original_addr < FwSize) &&
                        (original_addr != WDT_RESET_COUNTER_KEYWORD))
                    {
                        *wp = original_addr + Fw_BaseAddr[BankID][FW];
                    }
                }
            }

            // Detect: LSLS r1,r0,#2 / ADR r0,{pc}+N / LDR r0,[r0,r1] / MOV pc,r0
            if ((instr_ptr[j]   == JmpTbl_MOV_INSTR4)                         &&
                (instr_ptr[j-1] == JmpTbl_LDR_INSTR3)                         &&
                ((instr_ptr[j-2] & ADR_r0_INSTR_Msk) == JmpTbl_ADR_INSTR2)   &&
                (instr_ptr[j-3] == JmpTbl_LSLS_INSTR1))
            {
                if ((instr_ptr[j-4] & LDR_r0_sp_OPCODE) == LDR_r0_sp_OPCODE)
                {
                    int cmp_idx = -1;
                    if ((instr_ptr[j-6] & BYTE1_Msk) == CMP_r0_INSTR) cmp_idx = j-6;
                    if ((instr_ptr[j-7] & BYTE1_Msk) == CMP_r0_INSTR) cmp_idx = j-7;
                    if ((instr_ptr[j-8] & BYTE1_Msk) == CMP_r0_INSTR) cmp_idx = j-8;
                    if ((instr_ptr[j-9] & BYTE1_Msk) == CMP_r0_INSTR) cmp_idx = j-9;

                    if (cmp_idx != -1)
                    {
                        uint8_t  JmpAdrNum        = (instr_ptr[cmp_idx] & BYTE0_Msk) + 1;
                        uint32_t pc_offset        = (instr_ptr[j-2] & ADR_r0_OFFSET_Msk) * 4;
                        size_t   patch_byte_offset = (((j-2) * 2) + pc_offset + 4) & ALIGN_4Byte_Msk;
                        JumpTable_Patcher(JmpAdrNum, patch_byte_offset, Aprom_Page_Buff, FwSize, i);
                    }
                }
            }
        }

        FMC_Erase_User(NowPageAddr);
        WriteData(NowPageAddr, NextPageAddr, (uint32_t *)Aprom_Page_Buff);
    }

    WDT_RESET_COUNTER();

    NewBankMeta.fw_crc32 = CRC32_Calculator((uint32_t *)Fw_BaseAddr[BankID][FW], FwSize);
    NewBankMeta.flags    = FW_VALID_FLAG | FW_ACTIVE_FLAG;
    BankMetaUpdate(&NewBankMeta);

    NowFwInfo.Bank = BankID;
    NowFwInfo.cmd  = BTLD_CLEAR_CMD;
    FwInfoUpdate(&NowFwInfo);

    JumpToFirmware(Fw_BaseAddr[BankID][FW]);
}
