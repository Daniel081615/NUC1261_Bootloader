#ifndef __HOSTPROCESS_H__
#define __HOSTPROCESS_H__

#include "NUC1261.h"
#include "fw_info.h"
#include "ota_scheduler.h"
#include "bsp_flash.h"

/* ─── Thumb-2 Instruction Decode Constants ─── */
#define VectorTableSize         0xC0u
#define RegionTableSize         0x20u
#define PageInstructNum         (BSP_FLASH_PAGE_SIZE / sizeof(uint16_t))

/* LDR Rd,[PC,#xx] T1 encoding */
#define LDR_r0_sp_OPCODE        0x9800u
#define LDR_r0_INSTR            0x4800u
#define ADR_r0_INSTR            0xA000u
#define CMP_r0_INSTR            0x2800u

/* Jump-table detection pattern */
#define JmpTbl_LSLS_INSTR1      0x0081u
#define JmpTbl_ADR_INSTR2       0xA000u
#define JmpTbl_LDR_INSTR3       0x5840u
#define JmpTbl_MOV_INSTR4       0x4687u
#define MOV_r8_r8               0x46C0u

/* Instruction field masks */
#define LDR_r0_INSTR_Msk        0xFF00u
#define LDR_r0_OFFSET_Msk       0x00FFu
#define ADR_r0_INSTR_Msk        0xFFF0u
#define ADR_r0_OFFSET_Msk       0x000Fu
#define CMP_r0_INSTR_Msk        0xFF00u
#define CMP_r0_OFFSET_Msk       0x00FFu
#define ALIGN_4Byte_Msk         0xFFFFFFFCu

/* ─── Host Protocol Flags ─── */
#define FLAG_OTA_UPDATE         BIT7

/* ─── Legacy ALIVE/SYS_INFO (backward-compat with Center host) ─── */
#define METER_CMD_ALIVE         0x10u
#define METER_RSP_SYS_INFO      0x31u

/* ─── Variables Shared Between BootloaderProcess and patch_engine ─── */
extern uint8_t          BankID;
extern _Bool            _fgPatchEnable;
extern Bank_MetaInfo_t  NewBankMeta;
extern uint32_t         g_apromSize;
extern __attribute__((aligned(4))) uint8_t Aprom_Page_Buff[];
extern __attribute__((aligned(4))) uint8_t Next_Aprom_Page_Buff[];

extern void BootloaderProcess(void);

#endif /* __HOSTPROCESS_H__ */
