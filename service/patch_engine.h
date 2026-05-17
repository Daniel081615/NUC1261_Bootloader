#ifndef __PATCH_ENGINE_H__
#define __PATCH_ENGINE_H__

#include <stdint.h>
#include "fw_info.h"
#include "flash_service.h"   /* IFmcDriver_t */

/* ─── Thumb-2 Instruction Decode Constants ─── */
#define VectorTableSize         0xC0u
#define RegionTableSize         0x20u
#define PageInstructNum         (BSP_FLASH_PAGE_SIZE / sizeof(uint16_t))

/* LDR Rd,[PC,#xx] T1 encoding */
#define LDR_r0_INSTR            0x4800u
#define ADR_r0_INSTR            0xA000u
#define CMP_r0_INSTR            0x2800u

/* Jump-table detection pattern */
#define JmpTbl_LSLS_INSTR1      0x0081u
#define JmpTbl_ADR_INSTR2       0xA000u
#define JmpTbl_LDR_INSTR3       0x5840u
#define JmpTbl_MOV_INSTR4       0x4687u   /* MOV pc, r0 */
#define JmpTbl_BX_INSTR4        0x4700u   /* BX r0 — O2/O3 alternative to MOV pc,r0 */
#define MOV_r8_r8               0x46C0u

/* Instruction field masks */
#define LDR_r0_INSTR_Msk        0xFF00u
#define LDR_r0_OFFSET_Msk       0x00FFu
#define ADR_r0_INSTR_Msk        0xFFF0u
#define ADR_r0_OFFSET_Msk       0x000Fu
#define CMP_r0_INSTR_Msk        0xFF00u
#define CMP_r0_OFFSET_Msk       0x00FFu
#define ALIGN_4Byte_Msk         0xFFFFFFFCu

/*
 * PatchCtx_t — 依賴注入容器，由 main.c 組裝後傳入 PatchProcess()。
 * patch_engine 不再存取任何全域變數。
 */
typedef struct {
    uint8_t             bank_id;        /* 目標 bank (0 or 1) */
    Bank_MetaInfo_t    *meta;           /* 可寫：engine 更新 fw_crc32 / usage / health */
    uint32_t            aprom_size;     /* APROM 大小，用於地址合法性判斷 */
    uint8_t            *page_buf;       /* BSP_FLASH_PAGE_SIZE bytes，4-byte aligned */
    uint8_t            *next_page_buf;  /* BSP_FLASH_PAGE_SIZE bytes，4-byte aligned */
    const IFmcDriver_t *flash;          /* 注入的 Flash 驅動（ErasePage/WriteWords/…） */
} PatchCtx_t;

void PatchProcess(PatchCtx_t *ctx);

#endif /* __PATCH_ENGINE_H__ */
