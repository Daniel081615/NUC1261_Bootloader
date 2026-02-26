/***************************************************************************//**
 * @file     fw_metadata.c
 * @brief    韌體 Metadata 寫入與狀態管理
 ******************************************************************************/

#include "fw_metadata.h"
#include <stdint.h>
#include "fmc.h"
#include "crc_user.h"
#include "BootloaderProcess.h"

#define MAX_FLASH_SIZE      0x001FFFFF          // 最大 Flash 容量
#define METADATA_SIZE       sizeof(FWMetadata)  // Metadata 結構大小

extern int32_t g_FMC_i32ErrCode;

/**
 * @brief  將 Metadata 寫入指定 Flash 位址
 * @param  meta      Metadata 指標
 * @param  meta_base 寫入起始位址
 * @return 0 成功，負值失敗
 */
int WriteMetadata(FWMetadata *meta, uint32_t meta_base)
{
		g_FMC_i32ErrCode = 0;
		
    meta->meta_crc = CRC32_Calc((uint32_t *)meta, sizeof(FWMetadata) - sizeof(uint32_t));

    FMC_Open();
    FMC_ENABLE_ISP();

    if (FMC_Proc(FMC_ISPCMD_PAGE_ERASE, meta_base, meta_base + FMC_FLASH_PAGE_SIZE, 0) != 0) {
				goto exit_proc;  
				g_FMC_i32ErrCode = -1;
    }

    if (FMC_Proc(FMC_ISPCMD_PROGRAM, meta_base, meta_base + sizeof(FWMetadata), (uint32_t *)meta) != 0) {
        goto exit_proc;
				g_FMC_i32ErrCode = -2;
    }

exit_proc:
    FMC_DISABLE_ISP();
    FMC_Close();
    return g_FMC_i32ErrCode;
}

/**
 * @brief  寫入韌體狀態資訊
 * @param  meta 狀態結構指標
 * @return 0 成功，負值失敗
 */
int WriteStatus(FWstatus *meta)
{
    FMC_Open();
    FMC_ENABLE_ISP();
	
		g_FMC_i32ErrCode = 0;

    if (FMC_Proc(FMC_ISPCMD_PAGE_ERASE, FW_INFO_BASE, FW_INFO_BASE + FMC_FLASH_PAGE_SIZE, 0) != 0) {
        g_FMC_i32ErrCode = -1;
        goto exit_proc;
    }

    if (FMC_Proc(FMC_ISPCMD_PROGRAM, FW_INFO_BASE, FW_INFO_BASE + sizeof(FWstatus), (uint32_t *)meta) != 0) {
        g_FMC_i32ErrCode = -2;
        goto exit_proc;
    }

exit_proc:
    FMC_DISABLE_ISP();
    FMC_Close();
    return g_FMC_i32ErrCode;
}