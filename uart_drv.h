/****************************************************************************
 * @file     uart_drv.h
 * @version  V1.33.0005
 * @Date     Wed Feb 20 2026 17:21:09 GMT+0800 (台北標準時間)
 * @brief    uart driver extern code file
 *
 * SPDX-License-Identifier: Apache-2.0
 *
echnology Corp. All rights reserved.
*****************************************************************************/

#ifndef __UART_DRV_H__
#define	__UART_DRV_H__

#include "MyDef.h"

//	Extern Functions
extern void UART0_Init(void);
extern void UART1_Init(void);
extern void CalChecksumH(void);
extern void ResetHostUART(void);
extern void ResetMeterUART(void);


//	Extern variables
extern _Bool	MeterTokenReady;
extern _Bool	HostTokenReady;

extern uint8_t HOSTRxQ_wp, HOSTRxQ_rp, HOSTRxQ_cnt;
extern uint8_t HOSTTxQ_wp, HOSTTxQ_rp, HOSTTxQ_cnt;
extern uint8_t METERRxQ_wp, METERRxQ_rp, METERRxQ_cnt;
extern uint8_t METERTxQ_wp, METERTxQ_rp, METERTxQ_cnt;

extern uint8_t HOSTRxQ[MAX_UART_PACKET_LENGTH];
extern uint8_t HOSTTxQ[MAX_UART_PACKET_LENGTH];
extern uint8_t METERRxQ[MAX_UART_PACKET_LENGTH];
extern uint8_t METERTxQ[MAX_UART_PACKET_LENGTH];

extern uint8_t HostToken[MAX_UART_PACKET_LENGTH];
extern uint8_t MeterToken[MAX_UART_PACKET_LENGTH];

extern uint8_t HostTxBuffer[MAX_UART_PACKET_LENGTH];
extern uint8_t MeterTxBuffer[MAX_UART_PACKET_LENGTH];
#endif