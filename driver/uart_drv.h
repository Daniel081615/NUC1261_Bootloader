/*
 * uart_drv.h
 * ISR-driven single-channel RS485 driver for NUC1261 Bootloader.
 *
 * Channel : UART1 (RS485 AUD, hardware automatic direction via nRTS)
 * Frame   : [0x55][DeviceID][CMD][payload…][checksum][0x0A], fixed 100 bytes
 */

#ifndef __UART_DRV_H__
#define __UART_DRV_H__

#include <stdint.h>

/* ================================================================
 *  Frame constants
 * ============================================================== */
#define MAX_UART_PACKET_LENGTH  100u
#define UART_FRAME_HEADER       0x55u
#define UART_FRAME_TAIL         0x0Au

/* ================================================================
 *  Channel descriptor
 *  inst: opaque UART_T* — only uart_drv.c casts it
 * ============================================================== */
typedef struct {
    uint8_t *rx_buf;
    uint8_t  rx_wp;
    uint8_t  rx_rp;
    uint8_t  rx_cnt;
    uint8_t *tx_buf;
    uint8_t  tx_wp;
    uint8_t  tx_rp;
    uint8_t  tx_cnt;
} UART_Channel_t;

/* ================================================================
 *  Channel instance (single host channel in bootloader)
 * ============================================================== */
extern UART_Channel_t g_uart_host;

/* ================================================================
 *  Token buffer (ISR → main loop)
 * ============================================================== */
extern uint8_t        HostToken   [MAX_UART_PACKET_LENGTH];
extern uint8_t        HostTxBuffer[MAX_UART_PACKET_LENGTH];

/* ================================================================
 *  Init / Tick
 * ============================================================== */
void     UART_Init     (uint8_t device_id);
void     BL_SysTickInit(void);
uint32_t BL_GetTickMs  (void);

/* ================================================================
 *  Reset helpers
 * ============================================================== */
void UART_ResetRx(UART_Channel_t *ch);
void UART_ResetTx(UART_Channel_t *ch);
void UART_Reset  (UART_Channel_t *ch);

/* ================================================================
 *  Packet API (ISR-driven; Poll is a no-op)
 * ============================================================== */
void           UART_Poll     (void);
_Bool          UART_HasPacket(void);
const uint8_t *UART_GetPacket(void);

/* ================================================================
 *  Frame build + send
 *  BuildFrame fills header, id, checksum, tail into frame_buf.
 *  Caller sets frame_buf[2..frame_len-3] (cmd + payload) before calling.
 *  SendFrame enqueues frame_buf and enables THREIEN; non-blocking.
 *  Returns 0 on success, 1 if TxQ has no room.
 * ============================================================== */
void  UART_BuildFrame(UART_Channel_t *ch, uint8_t *frame_buf,
                      uint8_t frame_len, uint8_t id);
_Bool UART_SendFrame (UART_Channel_t *ch, uint8_t *frame_buf,
                      uint8_t frame_len);

/* Convenience: build + send a 100-byte protocol response in one call. */
void  UART_SendRsp(uint8_t cmd, const uint8_t *payload, uint16_t len);

#endif /* __UART_DRV_H__ */
