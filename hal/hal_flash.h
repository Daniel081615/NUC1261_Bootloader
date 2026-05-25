#ifndef HAL_FLASH_H
#define HAL_FLASH_H

#include <stdint.h>

/* ─── Flash layout constants ─────────────────────────────────────────────
 * Aliased from bsp_flash.h so that BSP is the single source of truth.
 * All upper layers (service, app) must use HAL_FLASH_* names. */
#include "bsp_flash.h"
#define HAL_FLASH_PAGE_SIZE         BSP_FLASH_PAGE_SIZE
#define HAL_FLASH_BANK0_BASE        BSP_BANK0_BASE
#define HAL_FLASH_BANK1_BASE        BSP_BANK1_BASE
#define HAL_FLASH_BANK_SIZE         BSP_BANK_SIZE
#define HAL_FLASH_BANK0_META_BASE   BSP_BANK0_META_BASE
#define HAL_FLASH_BANK1_META_BASE   BSP_BANK1_META_BASE
#define HAL_FLASH_FW_INFO_BASE      BSP_FW_INFO_BASE

/* ─── Status codes ───────────────────────────────────────────────────── */
typedef enum {
    HAL_FLASH_OK          =  0,
    HAL_FLASH_ERR_ALIGN   = -1,
    HAL_FLASH_ERR_RANGE   = -2,
    HAL_FLASH_ERR_ERASE   = -3,
    HAL_FLASH_ERR_WRITE   = -4,
    HAL_FLASH_ERR_PARAM   = -7,
} HAL_Flash_Status;

/* ─── API ────────────────────────────────────────────────────────────── */
void             HAL_Flash_Init(void);
HAL_Flash_Status HAL_Flash_ErasePage(uint32_t addr);
HAL_Flash_Status HAL_Flash_WriteWords(uint32_t addr, const uint32_t *data,
                                      uint32_t word_count);
void             HAL_Flash_ReadWords(uint32_t addr, uint32_t *data,
                                     uint32_t word_count);
uint32_t         HAL_Flash_GetCRC32(uint32_t addr, uint32_t byte_len);
void             HAL_Flash_JumpToApp(uint32_t app_base);

#endif /* HAL_FLASH_H */
