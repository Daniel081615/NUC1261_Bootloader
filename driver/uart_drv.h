/****************************************************************************
 * @file     uart_drv.h
 * @brief    UART driver extern declarations
 *
 * SPDX-License-Identifier: Apache-2.0
*****************************************************************************/

#ifndef __UART_DRV_H__
#define __UART_DRV_H__

#include "MyDef.h"
#include <stdint.h>

#define MAX_HOST_TOKEN_LENGTH  MAX_UART_PACKET_LENGTH  /* 100 bytes */

/* Init */
extern void UART1_Init(uint8_t device_id);
extern void ResetHostUART(void);

/* BL UART / Timing API */
extern void           BL_SysTickInit(void);
extern uint32_t       BL_GetTickMs(void);
extern void           BL_UART_Poll(void);
extern _Bool          BL_UART_HasPacket(void);
extern const uint8_t *BL_UART_GetPacket(void);
extern void           BL_UART_SendRsp(uint8_t cmd,
                                      const uint8_t *payload,
                                      uint16_t len);

#define BL_WDT_Reset()  WDT_RESET_COUNTER()

/* Extern variables */
extern volatile _Bool HostTokenReady;

extern uint8_t HOSTRxQ_wp, HOSTRxQ_rp, HOSTRxQ_cnt;
extern uint8_t HOSTTxQ_wp, HOSTTxQ_rp, HOSTTxQ_cnt;

extern uint8_t HOSTRxQ[MAX_UART_PACKET_LENGTH];
extern uint8_t HOSTTxQ[MAX_UART_PACKET_LENGTH];

extern uint8_t HostToken[MAX_UART_PACKET_LENGTH];
extern uint8_t HostTxBuffer[MAX_UART_PACKET_LENGTH];

#endif
