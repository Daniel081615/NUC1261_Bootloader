/***************************************************************************//**
 * @file     fw_metadata.h
 * @version  V1.00 
 * @brief    metadata source file
 *
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @copyright Copyright (C) 2016 Nuvoton Technology Corp. All rights reserved.
 ******************************************************************************/
#ifndef __METADATA_H__
#define __METADATA_H__

#include "fmc.h"
#include "fmc_user.h"
#include "stdbool.h"

#define FW_INVALID_FLAG         BIT0
#define FW_VALID_FLAG           BIT1
#define FW_ACTIVE_FLAG          BIT2


#define BANK1_BASE					0x00002000
#define BANK2_BASE					0x00010000
#define BANK1_META_BASE  		0x0001F000  
#define BANK2_META_BASE			0x0001F800


#define MAX_BANK_SIZE						BANK2_BASE - BANK1_BASE
#define	FW_PATCH_OFFSET					BANK2_BASE - BANK1_BASE

#define	MAX_Meter_FwVer_ID			0x10000000

//	FWStatus
typedef struct {
    uint32_t Fw_Base;
		uint32_t Fw_Meta_Base;
    uint16_t status;
		uint16_t cmd;
		uint32_t Version;
} FWstatus;

//	FWMetadata
typedef struct {
    uint32_t flags;
    uint32_t fw_crc32;
    uint32_t Version;
    uint32_t fw_start_addr;
    uint32_t fw_size;
    uint16_t trial_counter;
    uint16_t WDTRst_counter;
		uint32_t RegnTbl_base;
    uint32_t meta_crc;
} FWMetadata;

extern FWMetadata meta;
int WriteMetadata(FWMetadata *meta, uint32_t meta_base);
int WriteStatus(FWstatus *meta);

#endif