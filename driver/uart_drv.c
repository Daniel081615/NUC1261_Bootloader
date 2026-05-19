/****************************************************************************
 * @file     uart_drv.c
 * @brief    UART init, IRQ, BL UART API
 *           Bug B6/B7: ISR non-blocking TX (no busy-wait)
 *
 * SPDX-License-Identifier: Apache-2.0
 *****************************************************************************/

#include "NUC1261.h"
#include "MyDef.h"
#include "uart_drv.h"
#include <string.h>

/* Variables */
static uint8_t s_device_id = 0u;

volatile _Bool HostTokenReady;

uint8_t HOSTRxQ_wp, HOSTRxQ_rp, HOSTRxQ_cnt;
uint8_t HOSTTxQ_wp, HOSTTxQ_rp, HOSTTxQ_cnt;

uint8_t HOSTRxQ[MAX_UART_PACKET_LENGTH];
uint8_t HOSTTxQ[MAX_UART_PACKET_LENGTH];

uint8_t HostToken[MAX_UART_PACKET_LENGTH];
uint8_t HostTxBuffer[MAX_UART_PACKET_LENGTH];

/* UART0/2 shared IRQ — UART0 is not initialised in the bootloader;
 * this stub prevents the default handler from running if noise fires the IRQ. */
void UART02_IRQHandler(void) {}

/*---------------------------------------------------------------------------------------------------------*/
/*  UART1 Handler (Host / Center communication)                                                            */
/*---------------------------------------------------------------------------------------------------------*/
void UART1_IRQHandler(void)
{
    uint32_t u32IntSts;
    uint8_t  Rxbuf, i;

    u32IntSts = UART1->INTSTS;

    if (u32IntSts & UART_INTSTS_RDAINT_Msk)
    {
        while (UART_IS_RX_READY(UART1))
        {
            Rxbuf = UART_READ(UART1);

            HOSTRxQ[HOSTRxQ_wp] = Rxbuf;
            HOSTRxQ_wp++;
            HOSTRxQ_cnt++;
            if (HOSTRxQ_wp >= MAX_UART_PACKET_LENGTH)
                HOSTRxQ_wp = 0;

            if (HOSTRxQ[0] != UART_BUFF_HEAD)
            {
                HOSTRxQ_wp  = 0;
                HOSTRxQ_cnt = 0;
                HOSTRxQ_rp  = 0;
            }

            if (HOSTRxQ_cnt >= MAX_UART_PACKET_LENGTH)
            {
                if (HOSTRxQ[MAX_UART_PACKET_LENGTH - 1] != UART_BUFF_TAIL)
                {
                    HOSTRxQ_wp  = 0;
                    HOSTRxQ_cnt = 0;
                    HOSTRxQ_rp  = 0;
                    return;
                }
                for (i = 0; i < MAX_UART_PACKET_LENGTH; i++)
                {
                    HostToken[i] = HOSTRxQ[HOSTRxQ_rp];
                    HOSTRxQ_cnt--;
                    HOSTRxQ_rp++;
                    if (HOSTRxQ_rp >= MAX_UART_PACKET_LENGTH)
                        HOSTRxQ_rp = 0;
                }
                HOSTRxQ_wp  = 0;
                HOSTRxQ_cnt = 0;
                HOSTRxQ_rp  = 0;
                HostTokenReady = TRUE;
            }
        }
    }

    u32IntSts = UART1->INTSTS;
    if (u32IntSts & UART_INTSTS_THREINT_Msk)
    {
        if (HOSTTxQ_cnt > 0)
        {
            /* Bug B6: no busy-wait; THRE fires only when FIFO has space */
            UART_WRITE(UART1, HOSTTxQ[HOSTTxQ_rp]);
            HOSTTxQ_cnt--;
            HOSTTxQ_rp++;
            if (HOSTTxQ_rp >= MAX_UART_PACKET_LENGTH)
                HOSTTxQ_rp = 0;
        }
        else
        {
            UART_DisableInt(UART1, (UART_INTEN_THREIEN_Msk));
            UART_EnableInt(UART1, (UART_INTEN_RDAIEN_Msk));
        }
    }
}

static void RS485_AUD_Config(UART_T *uart)
{
    uart->FUNCSEL = UART_FUNCSEL_RS485;
    uart->ALTCTL  = UART_ALTCTL_RS485AUD_Msk;
    uart->MODEM   = (uart->MODEM & ~UART_MODEM_RTSACTLV_Msk) | UART_RTS_IS_HIGH_LEV_ACTIVE;
    uart->TOUT    = 0;
    uart->FIFO   &= ~(UART_FIFO_RFITL_Msk | UART_FIFO_RTSTRGLV_Msk);
}

void UART1_Init(uint8_t device_id)
{
    s_device_id = device_id;
    SYS_ResetModule(UART1_RST);
    CLK_SetModuleClock(UART1_MODULE, CLK_CLKSEL1_UARTSEL_HXT, CLK_CLKDIV0_UART(1));
    UART_Open(UART1, 57600);

#ifdef RS485
    RS485_AUD_Config(UART1);
#endif

    UART_EnableInt(UART1, UART_INTEN_RDAIEN_Msk);
}

static uint8_t _SendStringToHOST(uint8_t *Str, uint8_t len)
{
    uint8_t idx;

    if ((HOSTTxQ_cnt + len) > MAX_UART_PACKET_LENGTH)
        return 0x01;

    for (idx = 0; idx < len; idx++)
    {
        HOSTTxQ[HOSTTxQ_wp] = Str[idx];
        HOSTTxQ_wp++;
        if (HOSTTxQ_wp >= MAX_UART_PACKET_LENGTH)
            HOSTTxQ_wp = 0;
        HOSTTxQ_cnt++;
    }
    UART_EnableInt(UART1, (UART_INTEN_THREIEN_Msk));
    /* Bug B7: no blocking wait — ISR drains TxQ asynchronously */
    return 0x00;
}

static void CalChecksumH(void)
{
    uint8_t i, Checksum;
    HostTxBuffer[0] = 0x55;
    HostTxBuffer[1] = s_device_id;
    Checksum = 0;
    for (i = 1; i < (MAX_UART_PACKET_LENGTH - 2); i++)
        Checksum += HostTxBuffer[i];
    HostTxBuffer[MAX_UART_PACKET_LENGTH - 2] = Checksum;
    HostTxBuffer[MAX_UART_PACKET_LENGTH - 1] = '\n';
    _SendStringToHOST(HostTxBuffer, MAX_UART_PACKET_LENGTH);
}

void ResetHostUART(void)
{
    HOSTRxQ_cnt = 0;
    HOSTRxQ_wp  = 0;
    HOSTRxQ_rp  = 0;
    HOSTTxQ_cnt = 0;
    HOSTTxQ_wp  = 0;
    HOSTTxQ_rp  = 0;
}

/* ═══════════════════════════════════════════════════════════
 *  SysTick — 1 ms counter
 * ═══════════════════════════════════════════════════════════ */

static volatile uint32_t s_ms_tick = 0u;

void SysTick_Handler(void)
{
    s_ms_tick++;
}

void BL_SysTickInit(void)
{
    SysTick_Config(SystemCoreClock / 1000u);
}

uint32_t BL_GetTickMs(void)
{
    return s_ms_tick;
}

/* ═══════════════════════════════════════════════════════════
 *  BL UART API
 * ═══════════════════════════════════════════════════════════ */

void BL_UART_Poll(void)
{
    /* ISR-driven; nothing to poll */
}

_Bool BL_UART_HasPacket(void)
{
    return HostTokenReady;
}

const uint8_t *BL_UART_GetPacket(void)
{
    HostTokenReady = 0;
    return HostToken;
}

void BL_UART_SendRsp(uint8_t cmd, const uint8_t *payload, uint16_t len)
{
    uint16_t i, copy_len;

    memset(HostTxBuffer, 0, MAX_UART_PACKET_LENGTH);
    HostTxBuffer[2] = cmd;

    if (payload != NULL && len > 0u)
    {
        copy_len = len;
        if (copy_len > (uint16_t)(MAX_UART_PACKET_LENGTH - 5u))
            copy_len = (uint16_t)(MAX_UART_PACKET_LENGTH - 5u);

        for (i = 0u; i < copy_len; i++)
            HostTxBuffer[3u + i] = payload[i];
    }

    CalChecksumH();
}
