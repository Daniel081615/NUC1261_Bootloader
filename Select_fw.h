#ifndef __SELECT_FW_H__
#define __SELECT_FW_H__

#include "stdbool.h"
#include "fw_metadata.h"
#include "BootloaderProcess.h"
#include "fmc.h"
#include "wdt.h"

//	BankMeta Flag
#define FW_INVALID_FLAG         BIT0
#define FW_VALID_FLAG           BIT1
#define FW_ACTIVE_FLAG          BIT2

/** @def FWstatus **/
 enum {
	 //Bootloader cmds
	 BTLD_CLEAR_CMD = 0,
	 BTLD_UPDATE_CENTER,
	 BTLD_UPDATE_METER,	 
	 
	 //	FwBank status
/*	 DUAL_FW_BANK_VALID = 0x10,
	 ACTIVE_FW_BANK_VALID,
	 BACKUP_FW_BANK_VALID,
	 METER_OTA_UPDATEING,
	 ALL_FW_BANK_INVALID
*/
 } ;
 
extern uint8_t	bBank1_Valid, bBank2_Valid;

extern void JumpToFirmware(uint32_t fwAddr);
extern void BankSelectProcess(void);
 
#endif // __SELECT_FW_H__
