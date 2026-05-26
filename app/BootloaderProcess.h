#ifndef __HOSTPROCESS_H__
#define __HOSTPROCESS_H__

#include <stdint.h>
#include "fw_info.h"
#include "ota_scheduler.h"

/* ─── Host Protocol Flags ─── */
#define FLAG_OTA_UPDATE         0x80u

/* ─── Legacy ALIVE/SYS_INFO (backward-compat with Center host) ─── */
#define METER_CMD_ALIVE         0x10u
#define METER_RSP_SYS_INFO      0x31u

/*
 * 以下 extern 供 main.c（組合根）在 OTA 完成後組裝 OtaApplyCtx_t 使用。
 */
extern uint8_t          BankID;          /* target bank，由 HandleEnterReq 自選 */
extern Bank_MetaInfo_t  NewBankMeta;     /* patcher 讀 fw_size，並寫入 fw_crc32 */
extern uint32_t         OtaPayloadSize;  /* payload_size，用於定位 metadata page */

/* ─── Protocol DI：Protocol 層唯一允許的 DI 點 ─── */
typedef struct {
    void           (*uart_poll)(void);
    _Bool          (*uart_has_packet)(void);
    const uint8_t *(*uart_get_packet)(void);
    void           (*uart_send_rsp)(uint8_t cmd, const uint8_t *payload, uint16_t len);
    uint32_t       (*get_tick_ms)(void);
    void           (*wdt_feed)(void);
    void           (*led_toggle)(void);
    void           (*flash_read_fw_info)(FW_Info_t *fw);
    int32_t        (*flash_erase_bank)(uint8_t bank);
    int32_t        (*flash_write_fw)(uint8_t bank, uint32_t offset,
                                     const uint8_t *data, uint32_t byte_len);
    _Bool          (*flash_verify_crc)(uint8_t bank, uint32_t fw_size, uint32_t expected_crc);
} BL_ProtocolOps_t;

/* ─── API ─── */
void BootloaderProcess_Init(uint8_t device_id, const BL_ProtocolOps_t *ops);
void BootloaderProcess(void);

#endif /* __HOSTPROCESS_H__ */
