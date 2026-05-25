#ifndef HAL_FLASH_H
#define HAL_FLASH_H

#include <stdint.h>

/* ─── Flash layout constants ─────────────────────────────────────────────
 * These mirror bsp_flash.h.  Only hal_flash.c may include bsp_flash.h;
 * all upper layers (service, app) must use HAL_FLASH_* names. */
#define HAL_FLASH_PAGE_SIZE         (2048UL)
#define HAL_FLASH_BANK0_BASE        (0x00002000UL)
#define HAL_FLASH_BANK1_BASE        (0x00010000UL)
#define HAL_FLASH_BANK_SIZE         (0x0000E000UL)
#define HAL_FLASH_BANK0_META_BASE   (HAL_FLASH_BANK1_BASE - HAL_FLASH_PAGE_SIZE)
#define HAL_FLASH_BANK1_META_BASE   (HAL_FLASH_BANK1_BASE + HAL_FLASH_BANK_SIZE - HAL_FLASH_PAGE_SIZE)
#define HAL_FLASH_FW_INFO_BASE      (0x0001F800UL)

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
