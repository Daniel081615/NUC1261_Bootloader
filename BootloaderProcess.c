#include	"NUC1261.h"
#include	"MyDef.h"
#include	"ExternFunc.h"
#include	"BootloaderProcess.h"
#include  "stdlib.h"
#include  "string.h"
#include	"Select_fw.h"
#include  "fmc_user.h"

//	Functions
void BootloaderProcess(void);

//	Parse Func
void Host_AliveProcess(void);
void Host_OTAUpdateProcess(void);
// RSP Host Func
void SendHost_SystemInformation(void);
void SendHost_OTAPackNo(void);

//	FMC Functions
void BankMetaUpdate(Bank_MetaInfo *meta);

//  Crc Calculator
uint32_t CRC32_Calculator(const uint32_t *pData, uint32_t len);

//	Patch Func
void VectorTable_Patcher(uint32_t *vectortable);
void RegionTable_Patcher(uint32_t *regiontable, size_t FwSize, uint16_t PatchCount);
void JumpTable_Patcher(uint8_t JmpAdrNum, size_t patch_byte_offset, uint8_t *Aprom_Page_Buff, uint32_t fw_size, uint8_t i);

//	Variables
_Bool bDelaySendHostCMD, _fgPatchEnable;
uint8_t Host_MeterID;
uint8_t BankID;
__attribute__((aligned(4)))	uint8_t Aprom_Page_Buff[FMC_FLASH_PAGE_SIZE];
__attribute__((aligned(4))) uint8_t Next_Aprom_Page_Buff[FMC_FLASH_PAGE_SIZE];
uint32_t StartAddress; 
uint32_t TotalLen, PktPayloadLen, LastDataLen;

const uint32_t Fw_BaseAddr[DUAL_BANK][2] =
{
    { BANK1_BASE,	BANK1_META_BASE },
    { BANK2_BASE, BANK2_META_BASE }
};

Bank_MetaInfo	NewBankMeta;
FW_Info				NowFwInfo;

//	Tick
uint8_t TickHost;
uint8_t WaitTime;
uint8_t iTickDelaySendHostCMD;

//	Host status flag
uint8_t cmdType, UpdatecmdType ;
uint8_t fgFromHostFlag, fgToHostFlag, fgToHostRSPFlag;

//	Update PakcNo
uint8_t g_packno, TotalPackNo;

void ClearRespDelayTimer(void)
{
	bDelaySendHostCMD 		= TRUE;
	iTickDelaySendHostCMD = 0;	
	WaitTime							= 5;
}

void BootloaderProcess(void)
{
    uint8_t i,checksum,fgDataReady,fgFromHostRSPFlag;

    if ( TickHost == 49 )
    {
        TickHost++;
        ResetHostUART();	
    }
    if( HostTokenReady )
    {
        HostTokenReady = 0 ;
        fgDataReady = 0 ;
        checksum = 0 ;

        for(i=1; i<(MAX_UART_PACKET_LENGTH-2); i++)
        {
            checksum += HostToken[i];				
        }
				
        if (HostToken[MAX_UART_PACKET_LENGTH-2] == checksum)
						fgDataReady = 1 ;
				else {
						//	CRC Cal Error
						if (HostToken[2] == METER_CMD_OTA_UPDATE)
						{
								if (!fgDataReady)
								{
										HostTxBuffer[2]   = 0;          	// 可視為錯誤 CMD
										HostTxBuffer[3]   = HostToken[3]; // 原封回傳 packno
										CalChecksumH();
										return;
								}
						}
				}
				
        // 0x55,CenterID,CMD,....,Checksum,\n
        if (fgDataReady)
        {						
            if ( HostToken[1] == MyDeviceID)
            {
								//RestartUart0Cnt = 0;
                Host_MeterID = HostToken[1]-1 ;	
                fgFromHostFlag = HostToken[3];
                fgFromHostRSPFlag = HostToken[4];
                fgToHostFlag &= fgFromHostRSPFlag;
                LED_R_TOGGLE();
                fgToHostRSPFlag = 0xFF ;
							
								
                
								// Token ready
								switch( HostToken[2])
								{
										case METER_CMD_ALIVE :
											Host_AliveProcess();						
											cmdType = METER_RSP_SYS_INFO ;
											ClearRespDelayTimer() ;	
											break;
								}
								
								
								if (HostToken[2] & METER_CMD_OTA_UPDATE)
								{
										cmdType = METER_RSP_OTA_INFO;
										UpdatecmdType = HostToken[2] & (~BIT5);
										Host_OTAUpdateProcess();
										
								}
						}
				} else {			
						ResetHostUART();
				}
		}

    if ( bDelaySendHostCMD )
    {
        if ( iTickDelaySendHostCMD > WaitTime)
        {
            bDelaySendHostCMD = 0 ;
            if ( Host_MeterID == MyDeviceID )
            {
                switch (cmdType)
                {
                    case METER_RSP_SYS_INFO :
                        SendHost_SystemInformation();
                        break;		
										case METER_RSP_OTA_INFO:
												SendHost_OTAPackNo();
                    default :
                        break;
                }            
            }
        }
    }
}

void Host_AliveProcess(void)
{
		//	Request OTA Update
		fgToHostFlag |= FLAG_OTA_UPDATE;
}

/***	OTA Parser	***/
void Host_OTAUpdateProcess(void)
{ 
		uint8_t *pSrc = HostToken;
		uint32_t PageAddress;
	
		pSrc = &HostToken[4];	
		PktPayloadLen = UART_PACKET_PAYLOAD_LEN;	//92 Byte
	
		switch(UpdatecmdType)
		{
				case 	CMD_UPDATE_META_INFO :
					
						NewBankMeta.Version 					= *(uint32_t *)pSrc;
						NewBankMeta.Size 	 						= *(uint32_t *)(pSrc + 4);
						NewBankMeta.RegionTable_Addr	= *(uint32_t *)(pSrc + 8);
						NewBankMeta.fw_crc32 					= *(uint32_t *)(pSrc + 12);
				
						TotalLen = NewBankMeta.Size;
						TotalPackNo = (TotalLen / UART_PACKET_PAYLOAD_LEN) +1;
						g_packno = 0;
						
						if (!bBank1_Valid && bBank2_Valid) {
								BankID = BANK1;
						}	else if (bBank1_Valid && !bBank2_Valid) {
								BankID = BANK2;
						}	else {
								BankID = BANK1;
						}
						StartAddress = Fw_BaseAddr[BankID][FW];
						/***	ERASE APROM		***/
						EraseAP(Fw_BaseAddr[BankID][FW], BANK_SIZE);
					break;
						
				case	CMD_UPDATE_FW	:
						if (TotalLen <= UART_PACKET_PAYLOAD_LEN) 
								PktPayloadLen = TotalLen;
						
						TotalLen -= PktPayloadLen;
						/***	Write APROM		***/
						WriteData(StartAddress, StartAddress + PktPayloadLen, (uint32_t *)pSrc);
						StartAddress += PktPayloadLen;
						
						if (g_packno < TotalPackNo)
						{
								g_packno++;
						}	else {
								uint32_t u32Crc =	CRC32_Calculator(&Fw_BaseAddr[BankID][FW], NewBankMeta.Size);
								if (u32Crc == NewBankMeta.fw_crc32)
								{
										_fgPatchEnable = TRUE; 							
								} else {
										EraseAP(Fw_BaseAddr[BankID][FW], BANK_SIZE);
										g_packno = 0;
								}

						}
								
					break;
				
				case	CMD_RESEND_PACKET :
						
						PageAddress = StartAddress & (~(FMC_FLASH_PAGE_SIZE - 1));
						//	Read APROM
						ReadData(PageAddress, StartAddress, (uint32_t *)Aprom_Page_Buff);
						//	Erase APROM
						EraseAP(PageAddress, StartAddress);
						
						StartAddress -= LastDataLen;
						TotalLen += LastDataLen;
						//	Write APROM Back
						WriteData(PageAddress, StartAddress, (uint32_t *)Aprom_Page_Buff);
					
						g_packno--;
					break;
		}
}

void SendHost_SystemInformation(void)
{
    HostTxBuffer[2] = METER_RSP_SYS_INFO ;	
    HostTxBuffer[3] = fgToHostFlag; 	
    HostTxBuffer[4] = fgToHostRSPFlag;
	
		CalChecksumH();
}

void SendHost_OTAPackNo(void)
{
		HostTxBuffer[2] = UpdatecmdType + METER_RSP_OTA_INFO ;	
    HostTxBuffer[3] = g_packno;
	
		CalChecksumH();
}

//	Write Fw Meta
void BankMetaUpdate(Bank_MetaInfo *metainfo)
{
		uint32_t MetaBase = Fw_BaseAddr[BankID][META];
		//	Erase MetaInfo
		FMC_Erase(MetaBase);
		//	Write MetaInfo
		FMC_Proc(FMC_ISPCMD_PROGRAM		, MetaBase, MetaBase + sizeof(Bank_MetaInfo), (uint32_t *)metainfo);
}

void FwInfoUpdate(FW_Info *fwinfo)
{
		uint32_t FwInfoBase = FW_INFO_BASE;
		//	Erase FwInfo
    FMC_Erase(FwInfoBase);
		//	Write FwInfo
    FMC_Proc(FMC_ISPCMD_PROGRAM	, FwInfoBase, FwInfoBase + sizeof(FW_Info), (uint32_t *)fwinfo);
}

//	Patch Function

void PatchProcess(void)
{
		if (_fgPatchEnable)
		{
				uint32_t FwSize = NewBankMeta.Size;
				uint8_t FwPages = (FwSize + FMC_FLASH_PAGE_SIZE -1) / FMC_FLASH_PAGE_SIZE;
			
				uint32_t RegionTablePage = NewBankMeta.RegionTable_Addr / FMC_FLASH_PAGE_SIZE; 
				uint32_t LastPage = FwPages -1;
			
				//	Patch By Page
				for (uint8_t i = 0; i <= FwPages; i++)
				{
						uint32_t NowPageAddr 	= Fw_BaseAddr[BankID][FW] + (i * FMC_FLASH_PAGE_SIZE);
						uint32_t NextPageAddr = NowPageAddr + FMC_FLASH_PAGE_SIZE;
					
						ReadData(NowPageAddr, NextPageAddr, (uint32_t *)&Aprom_Page_Buff);
												
						//	Patch Vector Table
						if (i == 0){
								VectorTable_Patcher((uint32_t *)&Aprom_Page_Buff);
						}
												
												
						//	Patch Region table
						if (i >= (RegionTablePage) && i <= LastPage) 
						{
									uint32_t PatchStartByteOffset = 0;
									uint16_t PatchCount = 0;
									
									if (i == RegionTablePage) {
											PatchStartByteOffset = NewBankMeta.RegionTable_Addr % FMC_FLASH_PAGE_SIZE;
									} else {
											PatchStartByteOffset = 0;
									}
														
									if (i == RegionTablePage && i == LastPage) {
											// Start & End @ the same page.
											PatchCount = RegionTableSize / sizeof(uint32_t);
									} else if (i == RegionTablePage) {
											PatchCount = (FMC_FLASH_PAGE_SIZE - PatchStartByteOffset) / sizeof(uint32_t);
									} else if (i == LastPage) {
											uint32_t RemainingBytes = FwSize - (i * FMC_FLASH_PAGE_SIZE) - PatchStartByteOffset;
											PatchCount = RemainingBytes / sizeof(uint32_t);
									}
									
									uint16_t PatchStartIdx = PatchStartByteOffset / sizeof(uint32_t); 

									if (PatchCount > 0) {
											RegionTable_Patcher((uint32_t *)&Aprom_Page_Buff[PatchStartIdx], FwSize, PatchCount);
									}
							}
												
							// Patch instruction "LDR r0, [PC #xx]
							uint16_t *instr_ptr = (uint16_t *)&Aprom_Page_Buff;
							for (size_t j = 9; j < PageInstructNum; j++)
							{
									//	Patch LDR	addr
									if ( (instr_ptr[j] & BYTE1_Msk) == LDR_r0_INSTR)
									{			
											uint32_t pc_offset 	= (instr_ptr[j] & BYTE0_Msk) * 4;	// #xx offset *4
											size_t patch_byte_offset = (j * 2) + pc_offset + 4;
											patch_byte_offset = patch_byte_offset & ALIGN_4Byte_Msk;				// For 4Byte align 
										
											if (patch_byte_offset < FMC_FLASH_PAGE_SIZE) 
											{		
													uint32_t *word_ptr_in_buffer = (uint32_t *)&Aprom_Page_Buff[patch_byte_offset];
													uint32_t original_addr = *word_ptr_in_buffer;
											
													if ( ((original_addr > 0 ) && (original_addr < FwSize)) && 
															//	Keep WDT Keyword 0x5AA5 from Patching
																(original_addr != WDT_RESET_COUNTER_KEYWORD))
													{
															original_addr += Fw_BaseAddr[BankID][FW];
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
												
													// Check j-6 (Typical position after skipping LDR, BLS, and BL.W (32-bit))
													if ((instr_ptr[j-6] & BYTE1_Msk) == CMP_r0_INSTR)
															cmp_idx = j-6;
													
													// Check j-7 (Typical position after skipping LDR, BLS, and BL.W (32-bit))
													if ((instr_ptr[j-7] & BYTE1_Msk) == CMP_r0_INSTR)
															cmp_idx = j-7;
													
													// Check j-8 (Typical position after skipping LDR, BLS, and BL.W (32-bit))
													if ((instr_ptr[j-8] & BYTE1_Msk) == CMP_r0_INSTR)
															cmp_idx = j-8;
													
													// Check j-9 (For robustness against potential preceding instructions)
													if ((instr_ptr[j-9] & BYTE1_Msk) == CMP_r0_INSTR)
															cmp_idx = j-9;

													if (cmp_idx != -1)
													{
															// Get Jump Table entrance number (JmpAdrNum = CMP_val + 1)
															uint8_t JmpAdrNum = (instr_ptr[cmp_idx] & BYTE0_Msk) + 1;
															
															// Calculate Jump Table data start address offset in buffer
															// ADR instruction at j-2 holds the offset to the jump table data
															uint32_t pc_offset = (instr_ptr[j-2] & ADR_r0_OFFSET_Msk) * 4;
															size_t patch_byte_offset = ((j-2) * 2) + pc_offset + 4;
															
															patch_byte_offset = patch_byte_offset & ALIGN_4Byte_Msk; // Ensure 4-byte alignment
															// Call the function to patch the addresses in the Jump Table data block
															JumpTable_Patcher(JmpAdrNum, patch_byte_offset, Aprom_Page_Buff, FwSize, i);
													}
											}
									}
						}
						/*	Erase & Write flash	*/
						FMC_Erase_User(NowPageAddr);
						WriteData(NowPageAddr, NextPageAddr, (uint32_t *)&Aprom_Page_Buff);
				}
				//	Cal CRC
				NewBankMeta.fw_crc32 	= CRC32_Calculator(&Fw_BaseAddr[BankID][FW], FwSize);

				BankMetaUpdate(&NewBankMeta);
				FwInfoUpdate(&NowFwInfo);
				JumpToFirmware(Fw_BaseAddr[BankID][FW]);
		}
}

/**
 * @brief 通用 CRC32 計算（支援非 4-byte 對齊）
 * @param pData 資料指標
 * @param len   資料長度（位元組）
 * @return CRC32 結果
 */
uint32_t CRC32_Calculator(const uint32_t *pData, uint32_t len)
{
    uint32_t i, crc_result;

    CRC_Open(CRC_32, CRC_CHECKSUM_COM | CRC_CHECKSUM_RVS | CRC_WDATA_RVS,
             0xFFFFFFFF, CRC_CPU_WDATA_32);

    // 處理完整 4-byte 區塊
    for (i = 0; i <= (len/4)+1; i++) {
        CRC->DAT = pData[i];
    }

    crc_result = CRC_GetChecksum();
    return crc_result;
}


/**************************************/
/***	***		@Patcher Functions	***	***/
/**************************************/
/***
 @brief	Patch Vector table
 ***/
void VectorTable_Patcher(uint32_t *vectortable)
{
		for (uint8_t i = 0; i < (VectorTableSize/4); i++)
		{
				uint32_t VecValue = vectortable[i];
				if (VecValue !=0  && (VecValue < g_apromSize))
				{
						VecValue += Fw_BaseAddr[BankID][FW];
						vectortable[i] = VecValue;
				}
		}
}

/***
 @brief	Patch Region table
 ***/
void RegionTable_Patcher(uint32_t *regiontable, size_t FwSize, uint16_t PatchCount)
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

/***
 @brief	Patch Jump table
 ***/
void JumpTable_Patcher(uint8_t JmpAdrNum, size_t patch_byte_offset, uint8_t *Aprom_Page_Buff, uint32_t fw_size, uint8_t i)
{
		
		uint32_t NextPageAddr = Fw_BaseAddr[BankID][FW] + ((i+1) * FMC_FLASH_PAGE_SIZE);
		uint32_t Plus_2_Page_Base = NextPageAddr + FMC_FLASH_PAGE_SIZE;
	
		//	Patch JumTbl 
    if (patch_byte_offset + (JmpAdrNum * 4) < FMC_FLASH_PAGE_SIZE) 
    {		
        for (uint8_t k = 0; k < JmpAdrNum; k++)
        {																										
            uint32_t *word_ptr_in_buffer = (uint32_t *)&Aprom_Page_Buff[patch_byte_offset + k*4];
            uint32_t original_addr = *word_ptr_in_buffer;
            
            if ((original_addr > 0) && (original_addr < fw_size))
            {
								original_addr += Fw_BaseAddr[BankID][FW];
                *word_ptr_in_buffer = original_addr;
            }
        }
    } 
			else {
				
        // Patch JmpTbl cut by Flash page
        uint32_t JmpTblExceedSize = (patch_byte_offset + (JmpAdrNum * 4) - FMC_FLASH_PAGE_SIZE);
        uint32_t ExceedData[JmpTblExceedSize/4];

        ReadData(NextPageAddr, NextPageAddr + JmpTblExceedSize, (uint32_t *)&ExceedData);
				ReadData(NextPageAddr, Plus_2_Page_Base, (uint32_t *)&Next_Aprom_Page_Buff);

        // Patch current page JmpTbl
        for (uint8_t k = 0; k < (JmpAdrNum - (JmpTblExceedSize/4)); k++)
        {
            uint32_t *word_ptr_in_buffer = (uint32_t *)&Aprom_Page_Buff[patch_byte_offset + k*4];
            uint32_t original_addr = *word_ptr_in_buffer;
            if ((original_addr > 0) && (original_addr < fw_size))
            {
								original_addr += Fw_BaseAddr[BankID][FW];
                *word_ptr_in_buffer = original_addr;
            }
        }

        // Patch JmpTbl overflow
        for (uint8_t k = 0; k < (JmpTblExceedSize/4); k++)
        {
            uint32_t *word_ptr_in_buffer = (uint32_t *)&ExceedData[k*4];
            uint32_t original_addr = *word_ptr_in_buffer;
            if ((original_addr > 0) && (original_addr < fw_size))
            {
								original_addr += Fw_BaseAddr[BankID][FW];
                *word_ptr_in_buffer = original_addr;
            }
        }
				
				memcpy(Next_Aprom_Page_Buff, ExceedData, sizeof(ExceedData));

				FMC_Erase_User(NextPageAddr);
        WriteData(NextPageAddr, Plus_2_Page_Base, (uint32_t *)&Next_Aprom_Page_Buff);
    }
}