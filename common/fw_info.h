#ifndef FW_INFO_H
#define FW_INFO_H

#include <stdint.h>

/*
 * Shared firmware info structures between Bootloader and Application.
 * Stored in Data Flash at FW_INFO_BASE.
 *
 * 注意：
 * 1. 此檔案會同時被 Bootloader / CENTER App / METER App include。
 * 2. 結構需 #pragma pack，避免 Keil 自動插入 padding。
 */

/* Bank ID: 0-indexed to match BSP_BANK0_BASE / BSP_BANK1_BASE */
typedef enum {
    FW_BANK_0       = 0,    /* Bank 0 = APROM Bank1 (0x2000 base) */
    FW_BANK_1       = 1,    /* Bank 1 = APROM Bank2 (0x10000 base) */
    FW_BANK_INVALID = 0xFF,
} FW_BankId_t;

/* Bootloader command: 控制下一次 reset 時 Bootloader 行為 */
typedef enum {
    BTLD_CMD_NONE        = 0xFF, /* 正常開機：依 active_bank / bank_usage 選擇 Bank 跳入 */
    BTLD_UPDATE_METER    = 0xA1, /* App 觸發 OTA：BL 進入 OTA 接收主迴圈 (Bug B5 修正) */
    BTLD_FORCE_BANK1     = 0x11, /* 強制跳入 Bank 0 (維修用) */
    BTLD_FORCE_BANK2     = 0x12, /* 強制跳入 Bank 1 (維修用) */
} FW_BtldCmd_t;

/* Bank 用途（Usage） */
typedef enum {
    BANK_USAGE_EMPTY        = 0xFF, /* 未使用 / Flash 預設狀態 */
    BANK_USAGE_ACTIVE       = 0x01, /* 目前執行中的正式版本（App 確認健康後寫入） */
    BANK_USAGE_PREV         = 0x02, /* 上一版，保留作 rollback */
    BANK_USAGE_INCOMING     = 0x03, /* 正在接收 / 處理中（patch 前暫態） */
    BANK_USAGE_RELAY_METER  = 0x04, /* 暫存 METER 韌體 .bin */
    BANK_USAGE_RELAY_READER = 0x05, /* 暫存 READER 韌體 .bin */
    BANK_USAGE_VALID        = 0x06, /* OTA 接收完成且 CRC 驗證通過，等待 App 確認 */
} FW_BankUsage_t;

/* 韌體健康狀態 */
typedef enum {
    FW_HEALTH_UNVERIFIED = 0x00, /* 新版本尚未通過 App 端健康確認流程 */
    FW_HEALTH_UNKNOWN    = 0x00, /* 同 UNVERIFIED（別名） */
    FW_HEALTH_CONFIRMED  = 0xA5, /* App 已確認此版本穩定可用 */
} FW_Health_t;

#pragma pack(push, 1)

/* Data Flash 中的共用 FW_Info 結構（單一一份, 8 bytes） */
typedef struct {
    uint8_t  active_bank;    /* FW_BankId_t：0=Bank0, 1=Bank1 (0-indexed) */
    uint8_t  cmd;            /* FW_BtldCmd_t：控制 Bootloader 特殊行為 */
    uint8_t  bank0_usage;    /* FW_BankUsage_t：Bank0 目前的用途 */
    uint8_t  bank1_usage;    /* FW_BankUsage_t：Bank1 目前的用途 */
    uint8_t  health;         /* FW_Health_t：ACTIVE 版本是否已被 App 確認健康 */
    uint8_t  trial_counter;  /* 未確認健康狀態下的開機次數（供 Bootloader 判斷 rollback） */
    uint8_t  reserved[2];    /* 對齊 / 預留未來擴充 */
} FW_Info_t;                 /* 8 bytes — Bug B3/B4 修正: 舊版只有 2 bytes */

/* Bank 元資料結構，存放在 BANK0_META_BASE 或 BANK1_META_BASE (20 bytes) */
typedef struct {
    uint8_t  usage;              /* FW_BankUsage_t */
    uint8_t  health;             /* FW_Health_t */
    uint8_t  trial_counter;      /* 未確認健康下的開機次數 */
    uint8_t  reserved;
    uint32_t version;            /* 韌體版本號 */
    uint32_t fw_size;            /* 韌體大小（bytes） */
    uint32_t fw_crc32;           /* 韌體 CRC32（patch 後重新計算） */
    uint32_t region_table_addr;  /* RegionTable 在韌體中的偏移（0=無需 patch） */
} Bank_MetaInfo_t;               /* 20 bytes */

#pragma pack(pop)

/* Helper 宏 */
#define FW_INFO_ACTIVE_BANK(info)   ((FW_BankId_t)((info)->active_bank))
#define FW_INFO_CMD(info)           ((FW_BtldCmd_t)((info)->cmd))
#define FW_INFO_HEALTH(info)        ((FW_Health_t)((info)->health))

#endif /* FW_INFO_H */
