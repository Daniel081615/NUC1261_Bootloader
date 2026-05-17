/****************************************************************************
 * @file     MyDef.h
 * @version  V1.33.0005
 * @Date     Wed Feb 20 2026 17:21:09 GMT+0800 (台北標準時間)
 * @brief    My Define
 *
 * SPDX-License-Identifier: Apache-2.0
 *
echnology Corp. All rights reserved.
*****************************************************************************/

#ifndef 	__MYDEF_H__
#define		__MYDEF_H__

#include "NUC1261.h"

#define		MeterV5_2
#define 	RS485		//	Define Uart transmission RS485

//	
#define   APROM_SIZE      0x00020000

#define 	LED_G_On()			(PD7 = 0)
#define 	LED_G_Off()			(PD7 = 1)
#define 	LED_G_TOGGLE()	(PD7 ^= 1)
#define 	LED_R_On()			(PF2 = 0)
#define 	LED_R_Off()			(PF2 = 1)
#define 	LED_R_TOGGLE()	(PF2 ^= 1)
#define 	LED_G1_On()			(PE0 = 0)
#define 	LED_G1_Off()		(PE0 = 1)
#define 	LED_G1_TOGGLE()	(PE0 ^= 1)
#define 	LED_R1_On()			(PC4 = 0)
#define 	LED_R1_Off()		(PC4 = 1)
#define 	LED_R1_TOGGLE()	(PC4 ^= 1)

//	Uart Define
#define 	UART_BUFF_HEAD	0x55
#define 	UART_BUFF_TAIL	0x0A
#define 	MAX_UART_PACKET_LENGTH	100

#define 	LOOKAHEAD_INSTR_COUNT		10


#endif