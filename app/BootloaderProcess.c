/******************************************************************************
 * @file     BootloaderProcess.c
 * @brief    OTA 接收狀態機（offset-patch 重構版）
 *
 * 主要變更：
 *  - BL 自動選 inactive bank（移除 host 傳 bank）
 *  - UPDATE_META 新 payload：payload_size / fw_image_size / transport_crc32
 *  - 移除 region_table_addr
 *  - 完成判斷改為 rx_offset >= payload_size（含 metadata page）
 *  - transport CRC 覆蓋整個 payload（payload_size bytes）
 ******************************************************************************/

#include "NUC1261.h"
#include "MyDef.h"
#include "BootloaderProcess.h"
#include "flash_service.h"
#include "uart_drv.h"
#include <string.h>

/* ─── OTA State Machine ─── */
typedef enum {
    BL_OTA_IDLE      = 0,
    BL_OTA_READY,
    BL_OTA_RECEIVING,
    BL_OTA_DONE,
    BL_OTA_ERROR
} BL_OtaState_t;

typedef struct {
    BL_OtaState_t state;
    uint8_t       target_bank;      /* HandleEnterReq 自選：inactive bank */
    uint32_t      payload_size;     /* 完整 OTA payload（fw + 補齊 + metadata page） */
    uint32_t      fw_image_size;    /* raw fw image 大小（不含 metadata page） */
    uint32_t      transport_crc32;  /* CRC32 of entire payload（payload_size bytes） */
    uint32_t      version;
    uint32_t      meta_version;
    uint32_t      rx_offset;
    uint32_t      deadline_ms;
} BL_OtaCtx_t;

/* ─── Shared Variables (main.c 讀取以組裝 OtaApplyCtx_t) ─── */
uint8_t         BankID;
Bank_MetaInfo_t NewBankMeta;
uint32_t        OtaPayloadSize;
__attribute__((aligned(4))) uint8_t Aprom_Page_Buff[BSP_FLASH_PAGE_SIZE];
__attribute__((aligned(4))) uint8_t Next_Aprom_Page_Buff[BSP_FLASH_PAGE_SIZE];

static BL_OtaCtx_t s_ctx;
static uint8_t     s_device_id = 0u;

void BootloaderProcess_Init(uint8_t device_id)
{
    s_device_id = device_id;
}

/* ─── Static Handler Declarations ─── */
static void HandleEnterReq(void);
static void HandleUpdateReq(const uint8_t *pl, uint16_t len);
static void HandleStoreReq(const uint8_t *pl, uint16_t len);

/* ─── HandleEnterReq ─────────────────────────────────────────────────────── */
static void HandleEnterReq(void)
{
    FW_Info_t fw;
    uint8_t   active;

    FlashService_ReadFWInfo(&fw);
    active = fw.active_bank;

    if (active == (uint8_t)FW_BANK_0)
        s_ctx.target_bank = (uint8_t)FW_BANK_1;
    else if (active == (uint8_t)FW_BANK_1)
        s_ctx.target_bank = (uint8_t)FW_BANK_0;
    else
        s_ctx.target_bank = (uint8_t)FW_BANK_0;  /* 無有效 active bank 時預設 Bank0 */

    s_ctx.state       = BL_OTA_READY;
    s_ctx.deadline_ms = BL_GetTickMs() + OTA_RECV_TIMEOUT_MS;

    {
        uint8_t enter_rsp[2] = { FLAG_OTA_UPDATE, s_ctx.target_bank };
        BL_UART_SendRsp(OTA_CMD_ENTER_RSP, enter_rsp, 2u);
    }
}

/* ─── HandleUpdateReq ────────────────────────────────────────────────────── */
static void HandleUpdateReq(const uint8_t *pl, uint16_t len)
{
    uint8_t rsp;

    if (len < OTA_PL_UPDATE_MIN_LEN)
        return;

    memcpy(&s_ctx.payload_size,    pl + OTA_PL_PAYLOAD_SIZE_OFF,  sizeof(uint32_t));
    memcpy(&s_ctx.fw_image_size,   pl + OTA_PL_FW_IMAGE_SIZE_OFF, sizeof(uint32_t));
    memcpy(&s_ctx.transport_crc32, pl + OTA_PL_TRANSPORT_CRC_OFF, sizeof(uint32_t));
    memcpy(&s_ctx.version,         pl + OTA_PL_VERSION_OFF,       sizeof(uint32_t));
    memcpy(&s_ctx.meta_version,    pl + OTA_PL_META_VERSION_OFF,  sizeof(uint32_t));
    s_ctx.rx_offset = 0u;

    /* 合法性：page 對齊、不超出 bank、至少有一頁 metadata */
    if (s_ctx.payload_size == 0u ||
        (s_ctx.payload_size % BSP_FLASH_PAGE_SIZE) != 0u ||
        s_ctx.payload_size > BSP_BANK_SIZE ||
        s_ctx.fw_image_size == 0u ||
        s_ctx.fw_image_size >= s_ctx.payload_size)
    {
        rsp = 0xFFu;
        BL_UART_SendRsp(OTA_CMD_ERROR_RSP, &rsp, 1u);
        s_ctx.state = BL_OTA_ERROR;
        return;
    }

    FlashService_EraseBank(s_ctx.target_bank);

    s_ctx.state       = BL_OTA_RECEIVING;
    s_ctx.deadline_ms = BL_GetTickMs() + OTA_RECV_TIMEOUT_MS;

    rsp = 0x00u;
    BL_UART_SendRsp(OTA_CMD_STATUS_RSP, &rsp, 1u);
}

/* ─── HandleStoreReq ─────────────────────────────────────────────────────── */
static void HandleStoreReq(const uint8_t *pl, uint16_t len)
{
    uint32_t       chunk_offset;
    const uint8_t *data;
    uint32_t       remaining;
    uint32_t       write_len;
    uint32_t       write_len_aligned;

    if (len < OTA_PL_STORE_MIN_LEN)
        return;

    memcpy(&chunk_offset, pl + OTA_PL_CHUNK_OFF_OFF, sizeof(chunk_offset));
    data = pl + OTA_PL_CHUNK_DATA_OFF;

    remaining         = s_ctx.payload_size - chunk_offset;
    write_len         = (remaining > (uint32_t)OTA_CHUNK_SIZE) ? (uint32_t)OTA_CHUNK_SIZE : remaining;
    write_len_aligned = (write_len + 3u) & ~3u;

    memset(Aprom_Page_Buff, 0xFFu, write_len_aligned);
    memcpy(Aprom_Page_Buff, data, write_len);

    FlashService_WriteFirmware(s_ctx.target_bank, chunk_offset,
                               (const uint8_t *)Aprom_Page_Buff, write_len_aligned);

    s_ctx.rx_offset   = chunk_offset + write_len;
    s_ctx.deadline_ms = BL_GetTickMs() + OTA_RECV_TIMEOUT_MS;

    if (s_ctx.rx_offset >= s_ctx.payload_size)
    {
        /* transport CRC 覆蓋整個 payload（fw + metadata page） */
        if (FlashService_VerifyBankCRC(s_ctx.target_bank,
                                       s_ctx.payload_size,
                                       s_ctx.transport_crc32))
        {
            uint8_t rsp;

            BankID                    = s_ctx.target_bank;
            OtaPayloadSize            = s_ctx.payload_size;
            NewBankMeta.usage         = (uint8_t)BANK_USAGE_INCOMING;
            NewBankMeta.health        = (uint8_t)FW_HEALTH_UNVERIFIED;
            NewBankMeta.trial_counter = 0u;
            NewBankMeta.version       = s_ctx.version;
            NewBankMeta.fw_size       = s_ctx.fw_image_size;  /* raw fw，不含 metadata page */
            NewBankMeta.fw_crc32      = 0u;                   /* patcher 計算後填入 */

            s_ctx.state = BL_OTA_DONE;

            rsp = 0x00u;
            BL_UART_SendRsp(OTA_CMD_STATUS_RSP, &rsp, 1u);
        }
        else
        {
            uint8_t rsp;

            FlashService_EraseBank(s_ctx.target_bank);
            s_ctx.rx_offset = 0u;
            s_ctx.state     = BL_OTA_ERROR;

            rsp = 0xFFu;
            BL_UART_SendRsp(OTA_CMD_ERROR_RSP, &rsp, 1u);
        }
    }
    else
    {
        uint8_t rsp[4];
        memcpy(rsp, &s_ctx.rx_offset, sizeof(uint32_t));
        BL_UART_SendRsp(OTA_CMD_STATUS_RSP, rsp, (uint16_t)sizeof(rsp));
    }
}

/* ─── BootloaderProcess ───────────────────────────────────────────────────── */
void BootloaderProcess(void)
{
    const uint8_t *pkt;
    uint8_t        cmd;
    uint8_t        chk;
    uint8_t        i;
    const uint8_t *pl;

    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.state = BL_OTA_IDLE;

    while (1)
    {
        BL_WDT_Reset();
        BL_UART_Poll();

        if (s_ctx.state != BL_OTA_IDLE && s_ctx.state != BL_OTA_DONE)
        {
            if (BL_GetTickMs() >= s_ctx.deadline_ms)
                s_ctx.state = BL_OTA_IDLE;
        }

        if (s_ctx.state == BL_OTA_DONE)
            return;   /* OtaOffsetPatcher_Apply() 在 main 迴圈接手 */

        if (!BL_UART_HasPacket())
            continue;

        pkt = BL_UART_GetPacket();

        chk = 0u;
        for (i = 1u; i < (MAX_UART_PACKET_LENGTH - 2u); i++)
            chk += pkt[i];
        if (pkt[MAX_UART_PACKET_LENGTH - 2u] != chk)
            continue;

        if (pkt[1u] != s_device_id)
            continue;

        cmd = pkt[2u];
        pl  = &pkt[3u];

        LED_R_TOGGLE();

        switch (cmd)
        {
            case OTA_CMD_ENTER_REQ:
                HandleEnterReq();
                break;

            case OTA_CMD_UPDATE_CHILD_REQ:
                if (s_ctx.state == BL_OTA_READY)
                    HandleUpdateReq(pl, (uint16_t)(MAX_UART_PACKET_LENGTH - 5u));
                break;

            case OTA_CMD_STORE_CHILD_REQ:
                if (s_ctx.state == BL_OTA_RECEIVING)
                    HandleStoreReq(pl, OTA_CHUNK_SIZE + OTA_PL_CHUNK_DATA_OFF);
                break;

            case METER_CMD_ALIVE:
            {
                uint8_t rsp[2] = { FLAG_OTA_UPDATE, 0xFFu };
                BL_UART_SendRsp(METER_RSP_SYS_INFO, rsp, (uint16_t)sizeof(rsp));
                break;
            }

            default:
                break;
        }
    }
}
