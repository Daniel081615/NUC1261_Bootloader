#ifndef OTA_SCHEDULER_H
#define OTA_SCHEDULER_H

#include <stdint.h>

/* OTA 指令碼 */
typedef enum {
    OTA_CMD_ENTER_REQ          = 0x20, /* Host → BL: 請求進入 OTA 模式 */
    OTA_CMD_ENTER_RSP          = 0x21, /* BL → Host: ACK */
    OTA_CMD_STATUS_RSP         = 0x21, /* BL → Host: 進度/完成回覆（同 ENTER_RSP） */
    OTA_CMD_UPDATE_CHILD_REQ   = 0x23, /* Host → BL: 傳送 payload 描述 meta */
    OTA_CMD_STORE_CHILD_REQ    = 0x24, /* Host → BL: 傳送韌體資料 chunk */
    OTA_CMD_ERROR_RSP          = 0xFF, /* BL → Host: 錯誤回覆 */
} OtaCommand_t;

/* OTA 通訊參數 */
#define BL_FRAME_SIZE         100u   /* UART 固定幀長（含 head/dev/cmd/chk/tail） */
#define OTA_CHUNK_SIZE        88u    /* 每包資料 bytes */
#define OTA_RECV_TIMEOUT_MS   5000u  /* 接收逾時 ms */

/*
 * UpdateChild REQ payload 欄位偏移
 *
 *   payload_size    完整 OTA payload（fw image 補齊至 page 邊界 + 末尾 metadata page）
 *   fw_image_size   純韌體 image 大小（不含 metadata page）
 *   transport_crc32 CRC32(payload_size bytes) — 驗收整包
 *   version         韌體版本號
 *   meta_version    對應 OTA_META_VERSION，BL 用來確認格式相容
 *
 * 已移除：bank（BL 自選 inactive bank）
 *         region_table_addr（不再使用）
 *         舊 fw_crc32 pre-patch（改由 BL patch 後自算）
 */
#define OTA_PL_PAYLOAD_SIZE_OFF      0u  /* uint32_t LE */
#define OTA_PL_FW_IMAGE_SIZE_OFF     4u  /* uint32_t LE */
#define OTA_PL_TRANSPORT_CRC_OFF     8u  /* uint32_t LE */
#define OTA_PL_VERSION_OFF          12u  /* uint32_t LE */
#define OTA_PL_META_VERSION_OFF     16u  /* uint16_t LE */
#define OTA_PL_UPDATE_MIN_LEN       18u

/* StoreChild REQ payload 欄位偏移（結構不變） */
#define OTA_PL_CHUNK_OFF_OFF     0u  /* uint32_t LE: chunk 在 OTA payload 中的 byte offset */
#define OTA_PL_CHUNK_DATA_OFF    4u  /* data 起始位置 */
#define OTA_PL_STORE_MIN_LEN     5u

#endif /* OTA_SCHEDULER_H */
