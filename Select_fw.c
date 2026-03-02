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
#include "fmc_user.h"


uint8_t bBank1_Valid, bBank2_Valid ;
Bank_MetaInfo	  Bank1Meta = {0}, Bank2Meta = {0};

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
		//	Read BankMeta
		ReadData(FW_INFO_BASE, FW_INFO_BASE + sizeof(FW_Info), (uint32_t *)&NowFwInfo);
		
		BankValidateProcess();
		
		if (NowFwInfo.cmd == BTLD_UPDATE_METER) 
		{
        FMC_Erase(FW_INFO_BASE);
        return;
    }

    if ((bBank1_Valid + bBank2_Valid) <= 0) {
				BankID = 0xff;
        return;
		} else {
        if (bBank1_Valid && bBank2_Valid){
            if (bBank1_Valid > bBank2_Valid)
                BankID = BANK1;
            else 
                BankID = BANK2;
        } else {
            if (bBank1_Valid > 0) 
                BankID = BANK1;
             else 
                BankID = BANK2;
        } 
    }
    NowFwInfo.Bank = BankID;
    FwInfoUpdate(&NowFwInfo);
    JumpToFirmware(Fw_BaseAddr[BankID][FW]);
}

void JumpToFirmware(uint32_t fwAddr)
{
		SYS_UnlockReg();
		
    WDT_Open(WDT_TIMEOUT_2POW18, WDT_RESET_DELAY_3CLK, TRUE, TRUE);
    WDT_RESET_COUNTER();
		WDT_CLEAR_RESET_FLAG();
	
    FMC_SetVectorPageAddr(fwAddr);
    NVIC_SystemReset();
}

