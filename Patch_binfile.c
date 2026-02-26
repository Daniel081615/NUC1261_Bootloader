#include "NUC1261.h"
#include "Patch_binfile.h"

#include "ExternFunc.h"
#include "stdlib.h"
#include "string.h"

/***
 @brief	Patch JmpTbl
 ***/
 void VtorTbl_Patcher(uint32_t *vectortable);
void PatchJumpTable(uint8_t JmpAdrNum, size_t patch_byte_offset, uint8_t *aprom_buf, uint32_t fw_size, uint8_t i);
void RegnTbl_Patcher(uint32_t *regiontable, size_t FwSize, uint16_t PatchStartIdx, uint16_t PatchCount);
 
__attribute__((aligned(4))) static uint8_t next_aprom_buf[FMC_FLASH_PAGE_SIZE];

void PatchJumpTable(uint8_t JmpAdrNum, size_t patch_byte_offset, uint8_t *aprom_buf, uint32_t fw_size, uint8_t i)
{
		
		uint32_t Next_Page_Base = BANK2_BASE + ((i+1) * FMC_FLASH_PAGE_SIZE);
		uint32_t Plus_2_Page_Base = Next_Page_Base + FMC_FLASH_PAGE_SIZE;
	
		//	Patch JumTbl 
    if (patch_byte_offset + (JmpAdrNum * 4) < FMC_FLASH_PAGE_SIZE) 
    {		
        for (uint8_t k = 0; k < JmpAdrNum; k++)
        {																										
            uint32_t *word_ptr_in_buffer = (uint32_t *)&aprom_buf[patch_byte_offset + k*4];
            uint32_t original_addr = *word_ptr_in_buffer;
            
            if ((original_addr < BANK1_BASE + fw_size) && (original_addr > BANK1_BASE))
            {
                *word_ptr_in_buffer = original_addr + FW_PATCH_OFFSET;
            }
        }
    } 
			else {
				
        // Patch JmpTbl cut by Flash page
        uint32_t JmpTblExceedSize = (patch_byte_offset + (JmpAdrNum * 4) - FMC_FLASH_PAGE_SIZE);
        uint32_t ExceedData[JmpTblExceedSize/4];

        ReadData(Next_Page_Base, Next_Page_Base + JmpTblExceedSize, (uint32_t *)&ExceedData);
				ReadData(Next_Page_Base, Plus_2_Page_Base, (uint32_t *)&next_aprom_buf);

        // Patch current page JmpTbl
        for (uint8_t k = 0; k < (JmpAdrNum - (JmpTblExceedSize/4)); k++)
        {
            uint32_t *word_ptr_in_buffer = (uint32_t *)&aprom_buf[patch_byte_offset + k*4];
            uint32_t original_addr = *word_ptr_in_buffer;
            if ((original_addr < BANK1_BASE + fw_size) && (original_addr > BANK1_BASE))
            {
                *word_ptr_in_buffer = original_addr + FW_PATCH_OFFSET;
            }
        }

        // Patch JmpTbl overflow
        for (uint8_t k = 0; k < (JmpTblExceedSize/4); k++)
        {
            uint32_t *word_ptr_in_buffer = (uint32_t *)&ExceedData[k*4];
            uint32_t original_addr = *word_ptr_in_buffer;
            if ((original_addr < BANK1_BASE + fw_size) && (original_addr > BANK1_BASE))
            {
                *word_ptr_in_buffer = original_addr + FW_PATCH_OFFSET;
            }
        }
				
				memcpy(next_aprom_buf, ExceedData, sizeof(ExceedData));

				FMC_Erase_User(Next_Page_Base);
        WriteData(Next_Page_Base, Plus_2_Page_Base, (uint32_t *)&next_aprom_buf);
    }
}

/***
 @brief	Patch Vector table
 ***/
void VtorTbl_Patcher(uint32_t *vectortable)
{
		for (uint8_t i = 0; i < (VectorTableSize/4); i++)
		{
				uint32_t VecValue = vectortable[i];
				if (VecValue !=0  && (VecValue < g_apromSize))
				{
						VecValue += FW_PATCH_OFFSET;
						vectortable[i] = VecValue;
				}
		}
}

/***
 @brief	Patch Region table
 ***/
void RegnTbl_Patcher(uint32_t *regiontable, size_t FwSize, uint16_t PatchStartIdx, uint16_t PatchCount)
{
		uint32_t RegnTblVal;
				
		for (uint8_t i = 0; i < PatchCount; i++)
		{
				RegnTblVal = regiontable[PatchStartIdx +i];
			
				if (RegnTblVal!=0 && (RegnTblVal > BANK1_BASE) && (RegnTblVal <= BANK1_BASE + FwSize)) 
				{
						RegnTblVal += FW_PATCH_OFFSET;
						regiontable[PatchStartIdx +i] = RegnTblVal;
				}
		}
}