/******************************************************************************
 * @file      isp_user.c
 * @brief     ISP 韌體更新範例程式
 * @version   0x31
 ******************************************************************************/

#include 	<stdio.h>
#include 	"isp_user.h"
#include 	"MyDef.h"
#include 	"fmc_user.h"
#include 	"Select_fw.h"
#include 	"crc_user.h"
#include 	"Patch_binfile.h"

/*----------------------------- 全域變數區 -----------------------------*/

volatile uint8_t bISPDataReady;
__attribute__((aligned(4))) uint8_t response_buff[100];
__attribute__((aligned(4))) static uint8_t aprom_buf[FMC_FLASH_PAGE_SIZE];

uint32_t Now_Page_Base, Next_Page_Base;
uint32_t VtorTbl[VectorTableSize/4], RegnTbl_Base;

size_t num_instructions = FMC_FLASH_PAGE_SIZE / sizeof(uint16_t);

FWMetadata meta;

__STATIC_INLINE uint8_t Checksum(unsigned char *buf, int len)
{
    int i;
    uint8_t c = 0;
    for (i = 1; i < len; i++) c += buf[i];
    return c;
}

/**
 * @brief 		解析 ISP 指令並執行對應操作
 * @param 		buffer 接收封包
 * @param 		len    封包長度
 * @return 		處理成功回傳 1，失敗回傳 
 * @version		simplify metadata cmd, no need to send host offset address
 */
int Parsecmd(unsigned char *buffer, uint8_t len)
{

    static uint32_t StartAddress, TotalLen, LastDataLen;
    static uint8_t gcmd;
		static uint32_t g_packno;
    uint8_t *response = response_buff;
    uint8_t lcmd, lcksum;
    uint32_t srclen, i, VtorTbl[VectorTableSize/4];
    unsigned char *pSrc = buffer;
    srclen = len;

    uint8_t calc_cksum = Checksum(&buffer[0], 98);
    if (calc_cksum != buffer[98]) {
        response[0] = UART_BUFF_HEAD;
        response[3] = buffer[3];
        response[98] = 0xFF;
        response[99] = UART_BUFF_TAIL;
        return 0;
    }

    lcmd     = buffer[2];
    pSrc     = &buffer[4];
    srclen   = 92;
    
    if (lcmd == CMD_SYNC_PACKNO) g_packno = buffer[3];
//    if ((lcmd != 0) && (lcmd != CMD_RESEND_PACKET)) 
//				gcmd = lcmd;

		/***	Meter Fw has already fail	***/
		else if (lcmd == METER_OTA_UPDATE_CMD){
        g_packno = 0;
        goto out;				
		}
    else if (lcmd == CMD_CONNECT) {
        g_packno = 0;
        goto out;
    }
		else if (lcmd == CMD_GET_UPDATE_PACKNO) {
        goto out;
    }
		

    /***
		 *	@brief Always ask Host to send patched bin file at Base Addr FW1 (0x2000)
		 *	to determine which bank to update, Host don't need to know this
		 ***/
    else if (lcmd == CMD_UPDATE_META_INFO) {
        
        uint32_t UpdateMetaAddr;
			
				meta.Version 		= *(uint32_t *)pSrc;
				meta.fw_crc32 	 		= *(uint32_t *)(pSrc + 4);
				meta.fw_size 	 			= *(uint32_t *)(pSrc + 8);
				meta.RegnTbl_base		= *(uint32_t *)(pSrc + 12);	
				
				if (!bBank1_Valid && bBank2_Valid) {
						UpdateMetaAddr = BANK1_META_BASE;
						meta.fw_start_addr = BANK1_BASE;
				}	else if (bBank1_Valid && !bBank2_Valid) {
						UpdateMetaAddr = BANK2_META_BASE;
						meta.fw_start_addr = BANK2_BASE;
				}	else {
						UpdateMetaAddr = BANK2_META_BASE;
						meta.fw_start_addr = BANK2_BASE;
				}
				WriteMetadata(&meta, UpdateMetaAddr);
				goto out;
		}
		
		else if (lcmd == CMD_RESEND_PACKET) {
        uint32_t PageAddress;
        StartAddress -= LastDataLen;
        TotalLen += LastDataLen;
        PageAddress = StartAddress & ~(FMC_FLASH_PAGE_SIZE - 1);
        if (PageAddress >= Config0) goto out;
        ReadData(PageAddress, StartAddress, (uint32_t *)aprom_buf);
        FMC_Erase_User(PageAddress);
        WriteData(PageAddress, StartAddress, (uint32_t *)aprom_buf);
        if ((StartAddress % FMC_FLASH_PAGE_SIZE) >= (FMC_FLASH_PAGE_SIZE - LastDataLen)) {
            FMC_Erase_User(PageAddress + FMC_FLASH_PAGE_SIZE);
        }
				g_packno--;
        goto out;
    }
			// Erase Aprom update area
			else if (lcmd == CMD_ERASE_BANK) {
				StartAddress = meta.fw_start_addr;
				TotalLen = meta.fw_size;
			
//				if (StartAddress < FMC_APROM_BASE || MAX_BANK_SIZE > g_dataFlashAddr) goto out;
				FMC_ENABLE_ISP();
				EraseAP(StartAddress, MAX_BANK_SIZE);
				g_packno = 0;
    }
			else if ((lcmd == CMD_UPDATE_FW)) {
				if (TotalLen < srclen) 
						srclen = TotalLen;
        TotalLen -= srclen;
        WriteData(StartAddress, StartAddress + srclen, (uint32_t *)pSrc);
        memset(pSrc, 0, srclen);
        ReadData(StartAddress, StartAddress + srclen, (uint32_t *)pSrc);
        StartAddress += srclen;
        LastDataLen = srclen;
				++g_packno;

        if (TotalLen == 0) {
            FWstatus NowStatus;
            if (meta.fw_start_addr == BANK1_BASE) {
                NowStatus.Fw_Base = BANK1_BASE;
                NowStatus.Fw_Meta_Base = BANK1_META_BASE;
            } else if (meta.fw_start_addr == BANK2_BASE) {
                NowStatus.Fw_Base = BANK2_BASE;
                NowStatus.Fw_Meta_Base = BANK2_META_BASE;
            } else {
                goto out;
            }
						
            if (VerifyFirmware(&meta)) { 
								
								if (meta.fw_start_addr == BANK2_BASE)
								{
										/***
										2025.08.08 revised
										2025.09.25 revise region tbale pacth method

										# Patch Vector table offset
										# Patch Region table offset
										# Patch instruction "LDR r0, [PC #xx]
										# Patch JmpTbl entrance address by finding JmpTbl instruction sequences, 
										#	Make sure patching process integrity 
										
										 ***/
										uint32_t FwSize = meta.fw_size;
										uint8_t FwPages = FwSize / FMC_FLASH_PAGE_SIZE;
										
										uint32_t RegnTbl_StartPage = meta.RegnTbl_base / FMC_FLASH_PAGE_SIZE; 
										uint32_t RegnTbl_EndPage = FwPages;
									
										for (uint8_t i = 0; i <= FwPages; i++)
										{
												uint32_t Now_Page_Base 	= BANK2_BASE + (i * FMC_FLASH_PAGE_SIZE);
												uint32_t Next_Page_Base = Now_Page_Base + FMC_FLASH_PAGE_SIZE;
											
												ReadData(Now_Page_Base, Next_Page_Base, (uint32_t *)&aprom_buf);
												/*	if across the page... ,	memcpy... */
												// if()
												
												//	Patch Vector Table
												if (i == 0){
														VtorTbl_Patcher((uint32_t *)&aprom_buf);
												}
												
												//	Patch Region table

												
											 if (i >= (RegnTbl_StartPage) && i <= RegnTbl_EndPage) 
												{
														uint32_t PatchStartByteOffset = 0;
														uint16_t PatchCount = 0;
														
														if (i == RegnTbl_StartPage) {
																PatchStartByteOffset = meta.RegnTbl_base % FMC_FLASH_PAGE_SIZE;
														} else {
																PatchStartByteOffset = 0;
														}
														
														if (i == RegnTbl_StartPage && i == RegnTbl_EndPage) {
																// Start & End @ the same page.
																PatchCount = RegionTableSize / sizeof(uint32_t);
														} else if (i == RegnTbl_StartPage) {
																PatchCount = (FMC_FLASH_PAGE_SIZE - PatchStartByteOffset) / sizeof(uint32_t);
														} else if (i == RegnTbl_EndPage) {
																uint32_t RemainingBytes = FwSize - (i * FMC_FLASH_PAGE_SIZE) - PatchStartByteOffset;
																PatchCount = RemainingBytes / sizeof(uint32_t);
														}
														
														uint16_t PatchStartIdx = PatchStartByteOffset / sizeof(uint32_t); 

														if (PatchCount > 0) {
																RegnTbl_Patcher((uint32_t *)&aprom_buf, FwSize, PatchStartIdx, PatchCount);
														}
												}
												
												// Patch instruction "LDR r0, [PC #xx]
												uint16_t *instr_ptr = (uint16_t *)&aprom_buf;
												for (size_t j = 0; j < num_instructions; j++)
												{
														//	Patch LDR	addr
														if ( (instr_ptr[j] & LDR_r0_INSTR_Msk) == LDR_r0_INSTR)
														{			
																uint32_t pc_offset 	= (instr_ptr[j] & LDR_r0_OFFSET_Msk) * 4;	// #xx offset *4
																size_t patch_byte_offset = (j * 2) + pc_offset + 4;
																patch_byte_offset = patch_byte_offset & ALIGN_4Byte_Msk;				// For 4Byte align 
															
																if (patch_byte_offset < FMC_FLASH_PAGE_SIZE) 
																{		
																		uint32_t *word_ptr_in_buffer = (uint32_t *)&aprom_buf[patch_byte_offset];
																		uint32_t original_addr = *word_ptr_in_buffer;
																
																		if ((original_addr < BANK1_BASE + FwSize) && (original_addr > BANK1_BASE) && original_addr != WDT_RESET_COUNTER_KEYWORD)
																		{
																				original_addr += FW_PATCH_OFFSET;
																				*word_ptr_in_buffer =original_addr;
																		}
																}
														}

														/***
															@brief	Finding Jump table with 4 fixed instructions and patch the jump address according to the table
															
															#CMP r0,#0xXX 		0x28XX, CMP_r0_INSTR, @XX+1 equal to jump entrance numbers
															.															 .
															.		SearchUp 2-3 Instrustions  .
															.      												 .

															#LSLS r1,r0,#2 		0x0081, JmpTbl_LSLS_INSTR1
															#ADR r0,{pc}+8		0xA001, JmpTbl_ADR_INSTR2
															#LDR r0,[r0,r1]		0x5840, JmpTbl_LDR_INSTR3
															#MOV pc,r0 				0x4687, JmpTbl_MOV_INSTR4
															
														 ***/

															// ... inside your for loop iterating through instructions ...
															if ((instr_ptr[j] == JmpTbl_MOV_INSTR4) &&
																	(instr_ptr[j-1] == JmpTbl_LDR_INSTR3) &&
																	((instr_ptr[j-2] & ADR_r0_INSTR_Msk) == JmpTbl_ADR_INSTR2) && // Masked comparison for ADR r0
																	(instr_ptr[j-3] == JmpTbl_LSLS_INSTR1))
															{
																	// Check for LDR r0,[sp, #XX] preceding LSLS at j-4
																	if ((instr_ptr[j-4] & LDR_r0_sp_OPCODE) == LDR_r0_sp_OPCODE)
																	{
																			// find the relative index of CMP r0,#0xXX
																			
																			int cmp_idx = -1;
																		
																				//	Also needs to consider the instruction set between psges()
//																			for (uint8_t i = j-5; i < 0x10; i--)
//																			{
//																					if ((instr_ptr[i] & CMP_r0_INSTR_Msk) == CMP_r0_INSTR)
//																					{
//																							cmp_idx = i;
//																							break;
//																					}
//																			}
																		
																			// Check j-6 (Typical position after skipping LDR, BLS, and BL.W (32-bit))
																			if ((instr_ptr[j-6] & CMP_r0_INSTR_Msk) == CMP_r0_INSTR)
																					cmp_idx = j-6;
																			
																			// Check j-7 (Typical position after skipping LDR, BLS, and BL.W (32-bit))
																			if ((instr_ptr[j-7] & CMP_r0_INSTR_Msk) == CMP_r0_INSTR)
																					cmp_idx = j-7;
																			
																			// Check j-8 (Typical position after skipping LDR, BLS, and BL.W (32-bit))
																			if ((instr_ptr[j-8] & CMP_r0_INSTR_Msk) == CMP_r0_INSTR)
																					cmp_idx = j-8;
																			
																			// Check j-9 (For robustness against potential preceding instructions)
																			else if ((instr_ptr[j-9] & CMP_r0_INSTR_Msk) == CMP_r0_INSTR)
																					cmp_idx = j-9;

																			if (cmp_idx != -1)
																			{
																					// Get Jump Table entrance number (JmpAdrNum = CMP_val + 1)
																					uint8_t JmpAdrNum = (instr_ptr[cmp_idx] & CMP_r0_OFFSET_Msk) + 1;
																					
																					// Calculate Jump Table data start address offset in buffer
																					// ADR instruction at j-2 holds the offset to the jump table data
																					uint32_t pc_offset = (instr_ptr[j-2] & ADR_r0_OFFSET_Msk) * 4;
																					size_t patch_byte_offset = ((j-2) * 2) + pc_offset + 4;
																				
//																					if (instr_ptr[j+1] == MOV_r8_r8)
//																							patch_byte_offset += 2;
																					
																					patch_byte_offset = patch_byte_offset & ALIGN_4Byte_Msk; // Ensure 4-byte alignment
																					// Call the function to patch the addresses in the Jump Table data block
																					PatchJumpTable(JmpAdrNum, patch_byte_offset, aprom_buf, FwSize, i);
																			}
																	}
															}
												}
												/*	Erase & Write flash	*/
												FMC_Erase_User(Now_Page_Base);
												WriteData(Now_Page_Base, Next_Page_Base, (uint32_t *)&aprom_buf);
										}
										/*	Calculate & write Bank2 crc32 & Metadata	*/
										meta.fw_crc32 			= Calculate_CRC32(BANK2_BASE, FwSize);
										meta.WDTRst_counter = 0;
								}
								
								NowStatus.Version = meta.Version;
								WriteMetadata(&meta, NowStatus.Fw_Meta_Base);
								WriteStatus(&NowStatus);
								JumpToFirmware(NowStatus.Fw_Base);
            } else {
                meta.flags = FW_INVALID_FLAG;
                WriteMetadata(&meta, NowStatus.Fw_Meta_Base);
                FMC_SetVectorPageAddr(FMC_APROM_BASE);
								NVIC_SystemReset();
            }
        }
    }

out:
    // 組裝回應封包
    response[0] = UART_BUFF_HEAD;
    response[1] = MyDeviceID;
    response[2] = lcmd;
    response[3] = g_packno;
    lcksum = Checksum(&response[0], 98);
    response[98] = lcksum;
    response[99] = UART_BUFF_TAIL;
    return 0;
}