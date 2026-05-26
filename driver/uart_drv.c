/*
 * uart_drv.c
 * ISR-driven RS485 driver for NUC1261 Bootloader.
 *
 * Hardware access is fully delegated to bsp_uart.c; this file
 * contains only ring-buffer logic, frame protocol, and API.
 * Porting to a new MCU: rewrite bsp_uart.c only.
 */

#include "bsp_uart.h"
#include "bsp_config.h"
#include "uart_drv.h"
#include <string.h>


/* ================================================================
 *  Private storage
 * ============================================================== */
static volatile _Bool HostTokenReady;
static uint8_t s_device_id = 0u;
static uint8_t s_rx_buf[MAX_UART_PACKET_LENGTH];
static uint8_t s_tx_buf[MAX_UART_PACKET_LENGTH];

/* ================================================================
 *  Channel instance
 * ============================================================== */
UART_Channel_t g_uart_host;

/* ================================================================
 *  Token buffer (ISR → main loop)
 * ============================================================== */
uint8_t        HostToken   [MAX_UART_PACKET_LENGTH];
uint8_t        HostTxBuffer[MAX_UART_PACKET_LENGTH];

/* ================================================================
 *  Forward declarations
 * ============================================================== */
static void UART_Generic_IRQHandler(UART_Channel_t *ch);

/* ================================================================
 *  IRQ callback — registered with bsp_uart during UART_Init
 * ============================================================== */
static void uart_irq_handler(void)
{
    UART_Generic_IRQHandler(&g_uart_host);
}

/* ================================================================
 *  UART_Generic_IRQHandler
 * ============================================================== */
static void UART_Generic_IRQHandler(UART_Channel_t *ch)
{
    uint8_t byte, i;

    BSP_UART_LatchIntStatus();   /* latch INTSTS before reading flags */

    /* ── RX ─────────────────────────────────────────────────── */
    if (BSP_UART_RxIntFlag())
    {
        while (BSP_UART_RxReady())
        {
            byte = BSP_UART_RxRead();
            /* Discard bytes until frame header arrives */
            if (ch->rx_cnt == 0u && byte != UART_FRAME_HEADER)
                continue;

            ch->rx_buf[ch->rx_wp] = byte;
            ch->rx_wp++;
            ch->rx_cnt++;
            if (ch->rx_wp >= MAX_UART_PACKET_LENGTH)
                ch->rx_wp = 0u;

            /* Full frame: check tail, copy to token buffer */
            if (ch->rx_cnt >= MAX_UART_PACKET_LENGTH)
            {
                /* Last received byte = slot just before wp */
                uint8_t tail_idx = (uint8_t)
                    ((ch->rx_wp + MAX_UART_PACKET_LENGTH - 1u) % MAX_UART_PACKET_LENGTH);

                if (ch->rx_buf[tail_idx] != UART_FRAME_TAIL)
                {
                    UART_ResetRx(ch);
                    continue;
                }

                for (i = 0u; i < MAX_UART_PACKET_LENGTH; i++)
                {
                    HostToken[i] = ch->rx_buf[ch->rx_rp];
                    ch->rx_rp++;
                    if (ch->rx_rp >= MAX_UART_PACKET_LENGTH)
                        ch->rx_rp = 0u;
                }
                UART_ResetRx(ch);
                HostTokenReady = 1u;
            }
        }
    }

    /* ── TX ─────────────────────────────────────────────────── */
    if (BSP_UART_TxIntFlag())
    {
        while (ch->tx_cnt > 0u && !BSP_UART_TxFull())
        {
            BSP_UART_TxWrite(ch->tx_buf[ch->tx_rp]);
            ch->tx_cnt--;
            ch->tx_rp++;
            if (ch->tx_rp >= MAX_UART_PACKET_LENGTH)
                ch->tx_rp = 0u;
        }

        if (ch->tx_cnt == 0u)
        {
            if (!BSP_UART_TxEmpty())
                return;   /* shift register not done; re-enter on next THRE */
            BSP_UART_TxPinDisable();
						BSP_UART_DisableTxInt();
            BSP_UART_EnableRxInt();
        }
    }
}

/* ================================================================
 *  UART_Init
 * ============================================================== */
void UART_Init(uint8_t device_id)
{
    s_device_id = device_id;

    g_uart_host.rx_buf = s_rx_buf;
    g_uart_host.tx_buf = s_tx_buf;
    UART_Reset(&g_uart_host);

    BSP_UART_RegisterCallback(uart_irq_handler);
    BSP_UART_HW_Init(57600u);

#if defined(RS485) && (BTLD_HOST_UART_CH == 1U)
    BSP_UART_RS485_AUD();
#endif
    BSP_UART_TxPinDisable();
    BSP_UART_EnableRxInt();
}

/* ================================================================
 *  SysTick — forwarded to BSP
 * ============================================================== */
void     BL_SysTickInit(void) { BSP_SysTick_Init(); }
uint32_t BL_GetTickMs  (void) { return BSP_GetTickMs(); }

/* ================================================================
 *  Reset helpers
 * ============================================================== */
void UART_ResetRx(UART_Channel_t *ch)
{
    ch->rx_wp  = 0u;
    ch->rx_rp  = 0u;
    ch->rx_cnt = 0u;
}

void UART_ResetTx(UART_Channel_t *ch)
{
    ch->tx_wp  = 0u;
    ch->tx_rp  = 0u;
    ch->tx_cnt = 0u;
}

void UART_Reset(UART_Channel_t *ch)
{
    UART_ResetRx(ch);
    UART_ResetTx(ch);
}

/* ================================================================
 *  Packet API
 * ============================================================== */
void UART_Poll(void)
{
    /* ISR-driven; nothing to do */
}

_Bool UART_HasPacket(void)
{
    return HostTokenReady;
}

const uint8_t *UART_GetPacket(void)
{
    HostTokenReady = 0u;
    return HostToken;
}

/* ================================================================
 *  UART_BuildFrame
 *  Caller fills frame_buf[2..frame_len-3] (cmd + payload) first.
 *  This function adds: header, id, checksum, tail.
 * ============================================================== */
void UART_BuildFrame(UART_Channel_t *ch, uint8_t *frame_buf,
                     uint8_t frame_len, uint8_t id)
{
    uint8_t i, checksum = 0u;
    (void)ch;   /* reserved for future multi-channel use */

    frame_buf[0] = UART_FRAME_HEADER;
    frame_buf[1] = id;

    for (i = 1u; i < (uint8_t)(frame_len - 2u); i++)
        checksum += frame_buf[i];

    frame_buf[frame_len - 2u] = checksum;
    frame_buf[frame_len - 1u] = UART_FRAME_TAIL;
}

/* ================================================================
 *  UART_SendFrame — non-blocking enqueue
 *  Critical section guards against ISR modifying tx_cnt mid-check.
 *  Returns 0 on success, 1 if TxQ has insufficient space.
 * ============================================================== */
_Bool UART_SendFrame(UART_Channel_t *ch, uint8_t *frame_buf,
                     uint8_t frame_len)
{
    uint8_t i;

    BSP_UART_EnterCritical();
    if ((ch->tx_cnt + frame_len) > MAX_UART_PACKET_LENGTH)
    {
        BSP_UART_ExitCritical();
        return 1u;
    }

    for (i = 0u; i < frame_len; i++)
    {
        ch->tx_buf[ch->tx_wp] = frame_buf[i];
        ch->tx_wp++;
        if (ch->tx_wp >= MAX_UART_PACKET_LENGTH)
            ch->tx_wp = 0u;
        ch->tx_cnt++;
    }
    BSP_UART_ExitCritical();

    BSP_UART_TxPinEnable();
    BSP_UART_EnableTxInt();
    return 0u;
}

/* ================================================================
 *  UART_SendRsp — build + send a 100-byte protocol response
 * ============================================================== */
void UART_SendRsp(uint8_t cmd, const uint8_t *payload, uint16_t len)
{
    uint16_t copy_len;

    memset(HostTxBuffer, 0u, MAX_UART_PACKET_LENGTH);
    HostTxBuffer[2] = cmd;

    if (payload != NULL && len > 0u)
    {
        copy_len = (len > (uint16_t)(MAX_UART_PACKET_LENGTH - 5u))
                 ? (uint16_t)(MAX_UART_PACKET_LENGTH - 5u) : len;
        memcpy(&HostTxBuffer[3], payload, copy_len);
    }

    UART_BuildFrame(&g_uart_host, HostTxBuffer,
                    (uint8_t)MAX_UART_PACKET_LENGTH, s_device_id);
    UART_SendFrame (&g_uart_host, HostTxBuffer,
                    (uint8_t)MAX_UART_PACKET_LENGTH);
}
