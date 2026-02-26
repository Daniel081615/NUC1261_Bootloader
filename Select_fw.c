/******************************************************************************
 * @file     Select_fw.c
 * @version  V1.00
 * @brief    開機韌體選擇與跳轉邏輯
 * 1. 根據 Metadata 判斷啟動哪個韌體或進入 ISP
 * 2. 驗證韌體與 Metadata CRC
 * 3. 跳轉指定韌體
 * 4. WDT 初始化與餵狗
 * 5. LED 狀態顯示
 ******************************************************************************/

#include "MyDef.h"
#include "ExternFunc.h"
#include "Select_fw.h"
#include "BootloaderProcess.h"


uint8_t bBank1_Valid, bBank2_Valid ;
Bank_MetaInfo	Bank1Meta = {0}, Bank2Meta = {0};

extern void LED_G_Toggle(void);

bool is_valid_active(FWMetadata *meta) {
    if (meta->flags == 0xFFFFFFFF || 
				meta->fw_start_addr == 0xFFFFFFFF ||
				meta->meta_crc == 0xFFFFFFFF
		)	return false;
    return (meta->flags  == (FW_ACTIVE_FLAG | FW_VALID_FLAG));
}

void BankValidateProcess(void) 
{
		ReadData(Fw_BaseAddr[BANK1][META], Fw_BaseAddr[BANK1][META] + sizeof(Bank_MetaInfo), (uint32_t *)&Bank1Meta);
    ReadData(Fw_BaseAddr[BANK2][META], Fw_BaseAddr[BANK2][META] + sizeof(Bank_MetaInfo), (uint32_t *)&Bank2Meta);
	
    if (Bank1Meta.flags == FW_VALID_FLAG) 
			bBank1_Valid = 1;
		else if (Bank1Meta.flags == (FW_ACTIVE_FLAG | FW_VALID_FLAG))
			bBank1_Valid = 2;
		else 
			bBank1_Valid = 0;
			
		if (Bank2Meta.flags == FW_VALID_FLAG) 
			bBank2_Valid = 1;
		else if (Bank2Meta.flags == (FW_ACTIVE_FLAG | FW_VALID_FLAG))
			bBank2_Valid = 2;
		else 
			bBank2_Valid = 0;
}

void BankSelectProcess(void)
{
	
		Bank_MetaInfo	Bank1Meta = {0}, Bank2Meta = {0};
		
		//	Read BankMeta
		ReadData(FW_INFO_BASE, FW_INFO_BASE + sizeof(FW_Info), (uint32_t *)&NowFwInfo);
		
		BankValidateProcess();
		
		if (NowFwInfo.cmd == BTLD_UPDATE_METER) 
		{
        FMC_Erase(FW_INFO_BASE);
        return;
    }
		
		if (bBank1_Valid && bBank2_Valid)
		{
				if (bBank1_Valid > bBank2_Valid)
				{
						BankID = BANK1;
				} else {
						BankID = BANK2;
				}	
				NowFwInfo.Bank = BankID;
				FwInfoUpdate(&NowFwInfo);
				JumpToFirmware(Fw_BaseAddr[BankID][FW]);
		}	else if ((bBank1_Valid + bBank2_Valid) == 0) {
				BankID = 0xff;
		} else if (bBank1_Valid > 0) {
				if (bBank1_Valid > 0)
				{
						BankID = BANK1;
						NowFwInfo.Bank = BankID;
						FwInfoUpdate(&NowFwInfo);
						JumpToFirmware(Fw_BaseAddr[BankID][FW]);
				}
		} else {
				if (bBank2_Valid > 0)
				{
						BankID = BANK2;
						NowFwInfo.Bank = BankID;
						FwInfoUpdate(&NowFwInfo);
						JumpToFirmware(Fw_BaseAddr[BankID][FW]);
				}			
		}
}

void JumpToFirmware(uint32_t fwAddr)
{
    BlinkStatusLED(PD, 7, PF, 2, 5, 2000);
		
		SYS_UnlockReg();
		
    WDT_Open(WDT_TIMEOUT_2POW18, WDT_RESET_DELAY_3CLK, TRUE, TRUE);
    WDT_RESET_COUNTER();
		WDT_CLEAR_RESET_FLAG();
	
    FMC_Open();
    FMC_SetVectorPageAddr(fwAddr);
		FMC_Close();
    NVIC_SystemReset();
}

bool VerifyFirmware(FWMetadata *meta)
{
    if ((meta->fw_start_addr == 0xFFFFFFFF) ||
        (meta->fw_size       == 0xFFFFFFFF) ||
        (meta->fw_crc32      == 0xFFFFFFFF))
        return false;
    uint32_t crc = Calculate_CRC32(meta->fw_start_addr, meta->fw_size);
    return (crc == meta->fw_crc32);
}

static bool IsValidAddress(uint32_t addr)
{
    return ((addr == 0x2000 || addr == 0x10000) &&
            (addr % 4 == 0) &&
            (addr >= BANK1_BASE && addr < g_apromSize));
}


bool VerifyMetadataCRC(FWMetadata *meta)
{
    uint32_t calc_crc = Calculate_CRC32((uint32_t)meta, sizeof(FWMetadata) - sizeof(uint32_t));
    return (calc_crc == meta->meta_crc);
}

void BlinkStatusLED(GPIO_T *port, uint32_t pin, GPIO_T *port2, uint32_t pin2, uint8_t times, uint32_t delay_ms)
{
    for (uint8_t i = 0; i < times; i++) {
        port->DOUT ^= (1 << pin);
        port2->DOUT ^= (1 << pin2);
        CLK_SysTickDelay(delay_ms * 1000);
        port->DOUT ^= (1 << pin);
        port2->DOUT ^= (1 << pin2);
        CLK_SysTickDelay(delay_ms * 1000);
    }
}

