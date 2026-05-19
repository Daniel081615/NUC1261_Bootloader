#ifndef OTA_PATCH_META_H
#define OTA_PATCH_META_H

#include <stdint.h>
#include "bsp_flash.h"
#include "flash_service.h"  /* IFmcDriver_t */

/* ─── Magic / Version ─── */
#define OTA_META_MAGIC        0x50415441 	  /*'ATAP' LE */
#define OTA_META_VERSION      1u
#define OTA_META_SOURCE_BASE  0x00000000UL  /* fw 固定編譯於 base 0 */

/* ─── Layout constants ─── */
#define OTA_META_HDR_SIZE     20u
#define OTA_META_MAX_OFFSETS  ((BSP_FLASH_PAGE_SIZE - OTA_META_HDR_SIZE) / sizeof(uint32_t))
/* = (2048 - 20) / 4 = 507 */

/* ─── Error codes ─── */
typedef enum {
    OTA_META_OK            =  0,
    OTA_META_ERR_PARAM     = -1,
    OTA_META_ERR_MAGIC     = -2,
    OTA_META_ERR_VERSION   = -3,
    OTA_META_ERR_FWSIZE    = -4,
    OTA_META_ERR_COUNT     = -5,
    OTA_META_ERR_OFFSET    = -6,
} OtaMetaStatus_t;

/*
 * OtaPatchMeta_t — metadata page header (20 bytes)
 * 緊跟其後：uint32_t offsets[offset_count]
 * 整個 metadata page = BSP_FLASH_PAGE_SIZE，多餘空間填 0xFF
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t magic;         /* OTA_META_MAGIC */
    uint32_t meta_version;  /* OTA_META_VERSION */
    uint32_t fwsize;        /* raw fw image 大小（bytes），不含 metadata page */
    uint32_t fwCrc32;   		/* OTA_META_SOURCE_BASE = 0x00000000 */
    uint32_t offset_count;  /* offsets 陣列長度 */
} OtaPatchMeta_t;           /* 20 bytes */
#pragma pack(pop)

/*
 * 從 flash 讀取並驗證 metadata page。
 * meta_buf 必須為 BSP_FLASH_PAGE_SIZE bytes，4-byte aligned。
 * 成功後可用 OtaPatchMeta_Header / OtaPatchMeta_Offsets 存取內容。
 */
int32_t OtaPatchMeta_ReadAndValidate(const IFmcDriver_t *flash,
                                      uint32_t            meta_page_addr,
                                      uint8_t            *meta_buf,
                                      uint32_t            fw_image_size);

static inline const OtaPatchMeta_t *OtaPatchMeta_Header(const uint8_t *meta_buf)
{
    return (const OtaPatchMeta_t *)(const void *)meta_buf;
}

static inline const uint32_t *OtaPatchMeta_Offsets(const uint8_t *meta_buf)
{
    return (const uint32_t *)(const void *)(meta_buf + OTA_META_HDR_SIZE);
}

#endif /* OTA_PATCH_META_H */
