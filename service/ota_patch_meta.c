/******************************************************************************
 * @file     ota_patch_meta.c
 * @brief    OTA metadata page 讀取與驗證
 ******************************************************************************/

#include "ota_patch_meta.h"

int32_t OtaPatchMeta_ReadAndValidate(const IFmcDriver_t *flash,
                                      uint32_t            meta_page_addr,
                                      uint8_t            *meta_buf,
                                      uint32_t            fw_image_size)
{
    const OtaPatchMeta_t *hdr;
    const uint32_t       *offsets;
    uint32_t              i;
    uint32_t              prev;

    if (flash == NULL || meta_buf == NULL || fw_image_size == 0u)
        return OTA_META_ERR_PARAM;

    flash->ReadWords(meta_page_addr, (uint32_t *)(void *)meta_buf,
                     BSP_FLASH_PAGE_SIZE / sizeof(uint32_t));

    hdr = OtaPatchMeta_Header(meta_buf);

    if (hdr->magic != OTA_META_MAGIC)
        return OTA_META_ERR_MAGIC;

    if (hdr->meta_version != OTA_META_VERSION)
        return OTA_META_ERR_VERSION;

    if (hdr->fwsize != fw_image_size)
        return OTA_META_ERR_FWSIZE;

    if (hdr->offset_count > OTA_META_MAX_OFFSETS)
        return OTA_META_ERR_COUNT;

    /* Validate each offset: 4-byte aligned, within fw image, strictly ascending */
    offsets = OtaPatchMeta_Offsets(meta_buf);
    prev    = 0u;
    for (i = 0u; i < hdr->offset_count; i++)
    {
        uint32_t off = offsets[i];

        if ((off & 3u) != 0u)
            return OTA_META_ERR_OFFSET;
        if (off >= fw_image_size)
            return OTA_META_ERR_OFFSET;
        if (i > 0u && off <= prev)
            return OTA_META_ERR_OFFSET;

        prev = off;
    }

    return OTA_META_OK;
}
