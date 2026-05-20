/* bsp_flash.h */

#ifndef __BSP_FLASH_H__
#define __BSP_FLASH_H__

#include <stdint.h>
#include <stddef.h>

/* ─── Flash 配置 ─── */
#define BSP_FLASH_PAGE_SIZE         (2048UL)            /* NUC126: 2 KB per page */
#define BSP_APROM_BASE              (0x00000000UL)
#define BSP_APP_BASE								(0x00002000UL)
#define BSP_APROM_END								(0x00020000UL)

/* OTA 雙 Bank 位址規劃 */
#define BSP_BANK0_BASE              (0x00002000UL)      /* Bank 0 FW 起始 */
#define BSP_BANK1_BASE              (0x00010000UL)      /* Bank 1 FW 起始 */
#define BSP_BANK_SIZE               (0x0000E000UL)      /* 每個 Bank 大小 (56 KB) */

/* Bank Meta 頁位址：Bank 末端倒數第一個 Flash page */
#define BSP_BANK0_META_BASE         (BSP_BANK1_BASE - BSP_FLASH_PAGE_SIZE)
#define BSP_BANK1_META_BASE         (BSP_BANK1_BASE + BSP_BANK_SIZE - BSP_FLASH_PAGE_SIZE)

/* FW_Info 位址：緊接 Bank1 末端 (Data Flash 起始) */
#define BSP_FW_INFO_BASE            (0x0001F800UL)

#define BSP_DATA_FLASH_FLAG_ADDR    (BSP_FW_INFO_BASE)

/* ─── 錯誤碼 ─── */
typedef enum {
    BSP_FLASH_OK          =  0,
    BSP_FLASH_ERR_ALIGN   = -1,   /* 位址未對齊 */
    BSP_FLASH_ERR_RANGE   = -2,   /* 超出區域邊界 */
    BSP_FLASH_ERR_ERASE   = -3,   /* 擦除失敗 */
    BSP_FLASH_ERR_WRITE   = -4,   /* 寫入失敗 */
    BSP_FLASH_ERR_VERIFY  = -5,   /* 資料比對不符 */
    BSP_FLASH_ERR_BLANK   = -6,   /* 擦除後不是全 0xFF */
    BSP_FLASH_ERR_PARAM   = -7,   /* 參數錯誤 */
} BSP_FLASH_Status;

/* ─── 初始化 / 去初始化 ─── */
void             BSP_Flash_Init(void);
void             BSP_Flash_DeInit(void);

/* ─── 位址合法性檢查 ─── */
BSP_FLASH_Status BSP_Flash_IsValidAddr(uint32_t addr, uint32_t len);

/* ─── Page 操作 ─── */
BSP_FLASH_Status BSP_Flash_ErasePage(uint32_t page_addr);

/* ─── Word 讀寫（帶 Verify） ─── */
BSP_FLASH_Status BSP_Flash_WriteWords(uint32_t addr,
                                      const uint32_t *data,
                                      uint32_t word_count);
void             BSP_Flash_ReadWords(uint32_t addr,
                                     uint32_t *data,
                                     uint32_t word_count);

/* ─── 其他功能 ─── */
void             BSP_Flash_JumpToApp(uint32_t app_base);
uint32_t         BSP_Flash_GetCRC32(uint32_t addr, uint32_t byte_len);

/*
 * Verify CONFIG0 (CBS=APROM+IAP) and CONFIG1 (DFBA=BSP_FW_INFO_BASE);
 * rewrite and chip-reset if either is wrong. Caller must have called
 * SYS_UnlockReg() first. Mirrors the original DataFlashConfig() in main.c.
 */
void             BSP_Flash_ConfigVerifyAndFix(void);

#endif