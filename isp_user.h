/**************************************************************************//**
 * @file     isp_user.h
 * @brief    ISP Command header file
 * @version  0x32
 *
 * @note
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @copyright Copyright (C) 2016 Nuvoton Technology Corp. All rights reserved.
 ******************************************************************************/
#ifndef ISP_USER_H
#define ISP_USER_H

#define FW_VERSION 0x34

#define	VectorTableSize	 0xC0		// 192byte
#define	RegionTableSize	 0x20		// 32byte

#include "fmc_user.h"
#include "fw_metadata.h"
#include "crc_user.h"
#include "stdbool.h"
#include <string.h>

typedef enum {
    METER_OTA_UPDATE_CMD    = 0x50,
	
    CMD_UPDATE_FW        = 0xA0,
		CMD_GET_UPDATE_PACKNO					= 0xA1,
    CMD_ERASE_BANK          = 0xA3,
    CMD_SYNC_PACKNO         = 0xA4,
    CMD_UPDATE_META_INFO     = 0xA5,
	
    CMD_CONNECT             = 0xAE,
    CMD_RESEND_PACKET       = 0xFF,
} MeterOtaUpdatecmd;

#define V6M_AIRCR_VECTKEY_DATA    0x05FA0000UL
#define V6M_AIRCR_SYSRESETREQ     0x00000004UL


//@LSLS 	0x0081	=>	Size of JmpAdrNum *4
//@ADR		0xa001	=>	JmpTbl StartAddr
//@LDR		0x5840	=>	Load JmpTbl Addr
//@MOV		0x4687
#define LDR_r0_sp_OPCODE			0x9800
#define	LDR_r0_INSTR					0x4800
#define LDR_r3_INSTR					0x68E0
#define ADR_r0_INSTR					0xA000
#define CMP_r0_INSTR					0x2800

#define JmpTbl_LSLS_INSTR1		0x0081
#define JmpTbl_ADR_INSTR2			0xA000
#define	JmpTbl_LDR_INSTR3			0x5840
#define	JmpTbl_MOV_INSTR4			0x4687
#define MOV_r8_r8							0x46c0

#define LDR_r0_INSTR_Msk			0xff00
#define LDR_r0_OFFSET_Msk			0x00ff

#define LDR_r3_INSTR_Msk			0xfff0
#define LDR_r3_OFFSET_Msk			0x000f

#define ADR_r0_INSTR_Msk			0xfff0
#define ADR_r0_OFFSET_Msk			0x000f

#define CMP_r0_INSTR_Msk			0xff00
#define CMP_r0_OFFSET_Msk			0x00ff

#define ALIGN_4Byte_Msk				0xfffffffc

#define LOOKAHEAD_INSTR_COUNT 10
#define LOOKAHEAD_BYTES (LOOKAHEAD_INSTR_COUNT * 2) // 20 bytes

// targetdev.c
extern void GetDataFlashInfo(uint32_t *addr, uint32_t *size);
extern uint32_t GetApromSize(void);

// isp_user.c

extern int Parsecmd(unsigned char *buffer, uint8_t len);

extern uint32_t g_apromSize, g_dataFlashAddr, g_dataFlashSize;
extern __attribute__((aligned(4))) uint8_t response_buff[100];
extern volatile uint8_t bISPDataReady;
extern __attribute__((aligned(4))) uint8_t usb_rcvbuf[];

// main.c 
extern uint8_t MyDeviceID;
void ReadMyDeviceID(void);

#endif	// #ifndef ISP_USER_H

/*** (C) COPYRIGHT 2019 Nuvoton Technology Corp. ***/
