/* bsp_flash.h — M031LE3AE flash layout constants and API.
 *
 * Interface is identical to bsp/bsp_flash.h (NUC1261) so that the HAL and
 * service layers compile against either BSP without modification.
 * Only the page size and meta-page addresses differ from NUC1261. */

#ifndef __BSP_FLASH_H__
#define __BSP_FLASH_H__

#include <stdint.h>
#include <stddef.h>

/* ─── Flash 配置 ─── */
#define BSP_FLASH_PAGE_SIZE         (0x0200UL)          /* M031: 512 B per page */
#define BSP_APROM_BASE              (0x00000000UL)
#define BSP_APP_BASE                (0x00002000UL)
#define BSP_APROM_END               (0x00020000UL)

/* OTA 雙 Bank 位址規劃 — base/size same as NUC1261 */
#define BSP_BANK0_BASE              (0x00002000UL)      /* Bank 0 FW 起始 */
#define BSP_BANK1_BASE              (0x00010000UL)      /* Bank 1 FW 起始 */
#define BSP_BANK_SIZE               (0x0000E000UL)      /* 每個 Bank 大小 (56 KB) */

/* Bank Meta 頁位址：Bank 末端倒數第一個 Flash page (512 B on M031) */
#define BSP_BANK0_META_BASE         (BSP_BANK1_BASE - BSP_FLASH_PAGE_SIZE)      /* 0x0000FE00 */
#define BSP_BANK1_META_BASE         (BSP_BANK1_BASE + BSP_BANK_SIZE - BSP_FLASH_PAGE_SIZE) /* 0x0001DC00 */

/* FW_Info 位址：Data Flash (CONFIG1 DFBA = 0x1F800) */
#define BSP_FW_INFO_BASE            (0x0001F800UL)

#define BSP_DATA_FLASH_FLAG_ADDR    (BSP_FW_INFO_BASE)

/* ─── 錯誤碼 ─── */
typedef enum {
    BSP_FLASH_OK          =  0,
    BSP_FLASH_ERR_ALIGN   = -1,
    BSP_FLASH_ERR_RANGE   = -2,
    BSP_FLASH_ERR_ERASE   = -3,
    BSP_FLASH_ERR_WRITE   = -4,
    BSP_FLASH_ERR_VERIFY  = -5,
    BSP_FLASH_ERR_BLANK   = -6,
    BSP_FLASH_ERR_PARAM   = -7,
} BSP_FLASH_Status;

/* ─── 初始化 / 去初始化 ─── */
void             BSP_Flash_Init(void);
void             BSP_Flash_DeInit(void);

/* ─── 位址合法性檢查 ─── */
BSP_FLASH_Status BSP_Flash_IsValidAddr(uint32_t addr, uint32_t len);

/* ─── Page 操作 ─── */
BSP_FLASH_Status BSP_Flash_ErasePage(uint32_t page_addr);

/* ─── Word 讀寫 ─── */
BSP_FLASH_Status BSP_Flash_WriteWords(uint32_t addr,
                                      const uint32_t *data,
                                      uint32_t word_count);
void             BSP_Flash_ReadWords(uint32_t addr,
                                     uint32_t *data,
                                     uint32_t word_count);

/* ─── 其他功能 ─── */
void             BSP_Flash_JumpToApp(uint32_t app_base);
uint32_t         BSP_Flash_GetCRC32(uint32_t addr, uint32_t byte_len);
void             BSP_Flash_ConfigVerifyAndFix(void);

#endif /* __BSP_FLASH_H__ */
