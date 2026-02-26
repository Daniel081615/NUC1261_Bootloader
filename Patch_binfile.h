#ifndef __PATCH_BINFILE_H__
#define __PATCH_BINFILE_H__

#include "stdint.h"
#include "fw_metadata.h"

extern void PatchJumpTable(uint8_t JmpAdrNum, uint32_t patch_byte_offset, uint8_t *aprom_buf, uint32_t fw_size, uint8_t i);
extern void VtorTbl_Patcher(uint32_t *vectortable);
extern void RegnTbl_Patcher(uint32_t *regiontable, uint32_t FwSize, uint16_t PatchStartIdx, uint16_t PatchCount);

#endif