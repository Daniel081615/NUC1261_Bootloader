/**************************************************************************//**
 * @file      main.c
 * @brief     NUC1261 Meter Bootloader — composition root
 *
 * Wires the BL_ProtocolOps_t DI struct using HAL functions.
 * App layer: may only include HAL, Service, and Protocol headers.
 ******************************************************************************/

#include "hal_sys.h"            /* HAL_System_Init, HAL_GetDeviceID, HAL_WDT_Feed, HAL_LED_RToggle */
#include "hal_uart.h"           /* HAL_UART_Init, HAL_SysTick_Init, HAL_UART_* */
#include "BootloaderProcess.h"
#include "ota_offset_patcher.h"
#include "Select_fw.h"
#include "flash_service.h"

/* ─── Protocol DI 組裝根（HAL 是允許觸及的最低層） ─── */
static const BL_ProtocolOps_t g_bl_ops = {
    .uart_poll          = HAL_UART_Poll,
    .uart_has_packet    = HAL_UART_HasPacket,
    .uart_get_packet    = HAL_UART_GetPacket,
    .uart_send_rsp      = HAL_UART_SendRsp,
    .get_tick_ms        = HAL_GetTickMs,
    .wdt_feed           = HAL_WDT_Feed,
    .led_toggle         = HAL_LED_RToggle,
    .flash_read_fw_info = FlashService_ReadFWInfo,
    .flash_erase_bank   = FlashService_EraseBank,
    .flash_write_fw     = FlashService_WriteFirmware,
    .flash_verify_crc   = FlashService_VerifyBankCRC,
};

int main(void)
{
    uint8_t       device_id;
    OtaApplyCtx_t apply_ctx;

    HAL_System_Init();                  /* pin-mux → clock → WDT → LED init → device-ID sample */
    device_id = HAL_GetDeviceID();

    HAL_UART_Init(device_id);
    HAL_SysTick_Init();
    BootloaderProcess_Init(device_id, &g_bl_ops);

    FlashService_Init();

    Boot_SelectFW();

    while (1)
    {
        BootloaderProcess();

        /* BootloaderProcess() 僅在 OTA_DONE 後返回；BankID/NewBankMeta/OtaPayloadSize 此時有效 */
        apply_ctx.target_bank  = BankID;
        apply_ctx.meta         = &NewBankMeta;
        apply_ctx.payload_size = OtaPayloadSize;

        /* 成功：跳入 App，不返回。失敗：erase bank 後返回，迴圈重進 BootloaderProcess() */
        OtaOffsetPatcher_Apply(&apply_ctx);
    }
}
