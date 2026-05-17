/******************************************************************************
 * @file     BootloaderProcess.c
 * @brief    OTA 接收狀態機（重構版）
 *           使用 BL_UART API + FlashService，修正 Bug B8（bitmask→enum switch）
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
    uint8_t       target_bank;
    uint32_t      fw_total_size;
    uint32_t      fw_expected_crc;
    uint32_t      version;
    uint32_t      region_table_addr;
    uint32_t      rx_offset;
    uint32_t      deadline_ms;
} BL_OtaCtx_t;

/* ─── Shared Variables (used by main.c as composition root) ─── */
static _Bool    _fgPatchEnable;   /* internal only; main.c reads via BootloaderProcess_PatchReady() */
uint8_t         BankID;
Bank_MetaInfo_t NewBankMeta;
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
    uint8_t rsp = FLAG_OTA_UPDATE;

    s_ctx.state       = BL_OTA_READY;
    s_ctx.deadline_ms = BL_GetTickMs() + OTA_RECV_TIMEOUT_MS;

    BL_UART_SendRsp(OTA_CMD_ENTER_RSP, &rsp, 1u);
}

/* ─── HandleUpdateReq ────────────────────────────────────────────────────── */
static void HandleUpdateReq(const uint8_t *pl, uint16_t len)
{
    uint8_t rsp;

    if (len < OTA_PL_UPDATE_MIN_LEN)
        return;

    // 使用 memcpy 取代直接解引用指標
    memcpy(&s_ctx.fw_total_size, pl + OTA_PL_FW_SIZE_OFF, sizeof(uint32_t));
    memcpy(&s_ctx.fw_expected_crc, pl + OTA_PL_FW_CRC_OFF, sizeof(uint32_t));
    s_ctx.target_bank = pl[OTA_PL_BANK_OFF];
    memcpy(&s_ctx.version, pl + OTA_PL_VERSION_OFF, sizeof(uint32_t));
    memcpy(&s_ctx.region_table_addr, pl + OTA_PL_REGION_TBL_OFF, sizeof(uint32_t));
    s_ctx.rx_offset = 0u;

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
    data              = pl + OTA_PL_CHUNK_DATA_OFF;
    remaining         = s_ctx.fw_total_size - chunk_offset;
    write_len         = (remaining > (uint32_t)OTA_CHUNK_SIZE) ? (uint32_t)OTA_CHUNK_SIZE : remaining;
    write_len_aligned = (write_len + 3u) & ~3u;

    /* Copy to aligned buffer (pkt payload ptr may be misaligned on Cortex-M0) */
    memset(Aprom_Page_Buff, 0xFFu, write_len_aligned);
    memcpy(Aprom_Page_Buff, data, write_len);

    FlashService_WriteFirmware(s_ctx.target_bank, chunk_offset,
                               (const uint8_t *)Aprom_Page_Buff, write_len_aligned);

    s_ctx.rx_offset   = chunk_offset + write_len;
    s_ctx.deadline_ms = BL_GetTickMs() + OTA_RECV_TIMEOUT_MS;

    if (s_ctx.rx_offset >= s_ctx.fw_total_size)
    {
        if (FlashService_VerifyBankCRC(s_ctx.target_bank,
                                       s_ctx.fw_total_size,
                                       s_ctx.fw_expected_crc) == 0)
        {
            uint8_t rsp;

            BankID                        = s_ctx.target_bank;
            NewBankMeta.usage             = (uint8_t)BANK_USAGE_INCOMING;
            NewBankMeta.health            = (uint8_t)FW_HEALTH_UNVERIFIED;
            NewBankMeta.trial_counter     = 0u;
            NewBankMeta.version           = s_ctx.version;
            NewBankMeta.fw_size           = s_ctx.fw_total_size;
            NewBankMeta.fw_crc32          = s_ctx.fw_expected_crc;
            NewBankMeta.region_table_addr = s_ctx.region_table_addr;

            _fgPatchEnable = TRUE;
            s_ctx.state    = BL_OTA_DONE;

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
        *(uint32_t *)rsp = s_ctx.rx_offset;
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

        /* Timeout: any in-progress state aborts back to IDLE */
        if (s_ctx.state != BL_OTA_IDLE && s_ctx.state != BL_OTA_DONE)
        {
            if (BL_GetTickMs() >= s_ctx.deadline_ms)
                s_ctx.state = BL_OTA_IDLE;
        }

        if (s_ctx.state == BL_OTA_DONE)
            return;   /* PatchProcess() runs next in main loop */

        if (!BL_UART_HasPacket())
            continue;

        pkt = BL_UART_GetPacket();

        /* Checksum verification (bytes [1..97]) */
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

        /* Bug B8 fix: explicit switch on cmd values, no bitmask */
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
