#ifndef OTA_SCHEDULER_H
#define OTA_SCHEDULER_H

#include <stdint.h>

/* OTA 指令碼 (Bug B8 修正: 使用 enum 取代 bitmask 比對) */
typedef enum {
    OTA_CMD_ENTER_REQ          = 0x20, /* Host → BL: 請求進入 OTA 模式 */
    OTA_CMD_ENTER_RSP          = 0x21, /* BL → Host: ACK / 中間回覆 */
    OTA_CMD_STATUS_RSP         = 0x21, /* BL → Host: OTA 完成回覆 (同 ENTER_RSP) */
    OTA_CMD_UPDATE_CHILD_REQ   = 0x23, /* Host → BL: 傳送 Bank/CRC/Size 等 Meta */
    OTA_CMD_STORE_CHILD_REQ    = 0x24, /* Host → BL: 傳送韌體資料 chunk */
    OTA_CMD_ERROR_RSP          = 0xFF, /* BL → Host: 錯誤回覆 */
} OtaCommand_t;

/* OTA 通訊參數 */
#define OTA_CHUNK_SIZE        92u     /* 每包 payload 最大 bytes (UART_PACKET_PAYLOAD_LEN) */
#define OTA_RECV_TIMEOUT_MS   5000u  /* 接收逾時 ms */

/* UpdateChild REQ payload 欄位偏移 */
#define OTA_PL_FW_SIZE_OFF       0u  /* uint32_t LE: 韌體大小 */
#define OTA_PL_FW_CRC_OFF        4u  /* uint32_t LE: 韌體 CRC32 (pre-patch) */
#define OTA_PL_BANK_OFF          8u  /* uint8_t:    目標 Bank (0 or 1) */
#define OTA_PL_VERSION_OFF       9u  /* uint32_t LE: 韌體版本號 */
#define OTA_PL_REGION_TBL_OFF   13u  /* uint32_t LE: RegionTable 偏移 (0=無) */
#define OTA_PL_UPDATE_MIN_LEN   17u  /* 最小有效 payload 長度 */

/* StoreChild REQ payload 欄位偏移 */
#define OTA_PL_CHUNK_OFF_OFF     0u  /* uint32_t LE: chunk 在韌體中的 byte offset */
#define OTA_PL_CHUNK_DATA_OFF    4u  /* data bytes 起始位置 */
#define OTA_PL_STORE_MIN_LEN     5u  /* 最小有效 payload 長度 */

#endif /* OTA_SCHEDULER_H */
