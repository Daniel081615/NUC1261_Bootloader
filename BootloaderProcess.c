#include	"NUC1261.h"
#include	"MyDef.h"
#include	"ExternFunc.h"
#include	"BootloaderProcess.h"
#include	"Select_fw.h"
#include	"fmc_user.h"
#include	"crc_user.h"
#include	"fw_metadata.h"
#include	"patch_engine.h"

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
										HostTxBuffer[2]   = 0;          	// 鍙鐐洪尟瑾?CMD
										HostTxBuffer[3]   = HostToken[3]; // 鍘熷皝鍥炲偝 packno
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
												break;
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
						
						TotalLen 		-= PktPayloadLen;
						LastDataLen  = PktPayloadLen;
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
						//	Read APROM 鈥?preserve data in this page up to current write position
						ReadData(PageAddress, PageAddress + FMC_FLASH_PAGE_SIZE, (uint32_t *)Aprom_Page_Buff);
						//	Erase one flash page only
						EraseAP(PageAddress, FMC_FLASH_PAGE_SIZE);

						StartAddress -= LastDataLen;
						TotalLen += LastDataLen;
						//	Restore the valid portion of the page before the rolled-back packet
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
