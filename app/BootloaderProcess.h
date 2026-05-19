#ifndef __HOSTPROCESS_H__
#define __HOSTPROCESS_H__

#include "NUC1261.h"
#include "fw_info.h"
#include "ota_scheduler.h"
#include "bsp_flash.h"

/* ─── Host Protocol Flags ─── */
#define FLAG_OTA_UPDATE         BIT7

/* ─── Legacy ALIVE/SYS_INFO (backward-compat with Center host) ─── */
#define METER_CMD_ALIVE         0x10u
#define METER_RSP_SYS_INFO      0x31u

/*
 * 以下 extern 供 main.c（組合根）在 OTA 完成後組裝 OtaApplyCtx_t 使用。
 */
extern uint8_t          BankID;          /* target bank，由 HandleEnterReq 自選 */
extern Bank_MetaInfo_t  NewBankMeta;     /* patcher 讀 fw_size，並寫入 fw_crc32 */
extern uint32_t         OtaPayloadSize;  /* payload_size，用於定位 metadata page */
extern __attribute__((aligned(4))) uint8_t Aprom_Page_Buff[];
extern __attribute__((aligned(4))) uint8_t Next_Aprom_Page_Buff[];

/* ─── API ─── */
void BootloaderProcess_Init(uint8_t device_id);
void BootloaderProcess(void);

#endif /* __HOSTPROCESS_H__ */
