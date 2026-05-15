/****************************************************************************
 * @file     uart_drv.c
 * @version  V1.33.0006
 * @brief    uart init, IRQ Functions code file
 *            Bug B6/B7 修正: 移除 ISR 內 busy-wait TX，改為純非阻塞模式
 *
 * SPDX-License-Identifier: Apache-2.0
 *****************************************************************************/

#include 	"NUC1261.h"
#include	"MyDef.h"
#include	"ExternFunc.h"
#include	"uart_drv.h"
#include <string.h>

//	Functions
void UART0_Init(void);
void UART1_Init(void);
void ResetHostUART(void);
void ResetMeterUART(void);
void CalChecksumH(void);
uint8_t _SendStringToHOST(uint8_t *Str, uint8_t len);

//	Variables
_Bool	MeterTokenReady;
_Bool	HostTokenReady;

uint8_t HOSTRxQ_wp, HOSTRxQ_rp, HOSTRxQ_cnt;
uint8_t HOSTTxQ_wp, HOSTTxQ_rp, HOSTTxQ_cnt;
uint8_t METERRxQ_wp, METERRxQ_rp, METERRxQ_cnt;
uint8_t METERTxQ_wp, METERTxQ_rp, METERTxQ_cnt;

uint8_t HOSTRxQ[MAX_UART_PACKET_LENGTH];
uint8_t HOSTTxQ[MAX_UART_PACKET_LENGTH];
uint8_t METERRxQ[MAX_UART_PACKET_LENGTH];
uint8_t METERTxQ[MAX_UART_PACKET_LENGTH];

uint8_t HostToken[MAX_UART_PACKET_LENGTH];
uint8_t MeterToken[MAX_UART_PACKET_LENGTH];

uint8_t HostTxBuffer[MAX_UART_PACKET_LENGTH];
uint8_t MeterTxBuffer[MAX_UART_PACKET_LENGTH];

//	Uart IRQ
void UART02_IRQHandler(void)
{
		uint32_t u32IntSts ;
		uint8_t Rxbuf,i;

		u32IntSts = UART0->INTSTS;
		if(u32IntSts & UART_INTSTS_RDAINT_Msk)
		{		
				/* Get all the input characters */
				while(UART_IS_RX_READY(UART0))
				{
						/* Get the character from UART Buffer */
						Rxbuf = UART_READ(UART0);
					
						METERRxQ[METERRxQ_wp] = Rxbuf;
						METERRxQ_wp++;
						METERRxQ_cnt++;
						if ( METERRxQ_wp >= MAX_UART_PACKET_LENGTH ) 
								METERRxQ_wp=0;
						
						if ( METERRxQ[0] != UART_BUFF_HEAD ) 
						{
								METERRxQ_wp=0;
								METERRxQ_cnt=0;
								METERRxQ_rp=0;
						}	
						
						if( METERRxQ_cnt >= MAX_UART_PACKET_LENGTH )
						{
								if ( METERRxQ[MAX_UART_PACKET_LENGTH-1] != UART_BUFF_TAIL ) 
								{
										METERRxQ_wp=0;
										METERRxQ_cnt=0;
										METERRxQ_rp=0;
										return;
								}
								
								for(i=0; i<MAX_UART_PACKET_LENGTH; i++)
								{
										MeterToken[i]=METERRxQ[METERRxQ_rp];
										METERRxQ_cnt--;
										METERRxQ_rp++;
										if(METERRxQ_rp >= MAX_UART_PACKET_LENGTH) 
												METERRxQ_rp = 0 ;
								}
								METERRxQ_wp=0;
								METERRxQ_cnt=0;
								METERRxQ_rp=0;
								MeterTokenReady = TRUE ;
						} 
				}	
		}
		
		u32IntSts = UART0->INTSTS;
		if(u32IntSts & UART_INTSTS_THREINT_Msk)
		{
				if(METERTxQ_cnt > 0)
				{      			
						while(UART_IS_TX_FULL(UART0));  /* Wait Tx is not full to transmit data */
						UART_WRITE(UART0, METERTxQ[METERTxQ_rp]);      
						METERTxQ_cnt--;				                   
						METERTxQ_rp++;
						if(METERTxQ_rp >= MAX_UART_PACKET_LENGTH) 
								METERTxQ_rp = 0 ;		
				} else {			
						/* Disable UART RDA and THRE interrupt */
						UART_DisableInt(UART0, (UART_INTEN_THREIEN_Msk));
						UART_EnableInt(UART0, (UART_INTEN_RDAIEN_Msk));
				}                	
		}
}


/*---------------------------------------------------------------------------------------------------------*/
/*  UART1 Handler                                                                                       */
/*---------------------------------------------------------------------------------------------------------*/
void UART1_IRQHandler(void)
{
		uint32_t u32IntSts;
		uint8_t Rxbuf,i;

		u32IntSts = UART1->INTSTS;
		
		if(u32IntSts & UART_INTSTS_RDAINT_Msk)
		{
				/* Get all the input characters */
				while(UART_IS_RX_READY(UART1))
				{
						/* Get the character from UART Buffer */
						Rxbuf = UART_READ(UART1);

						HOSTRxQ[HOSTRxQ_wp] = Rxbuf;
						HOSTRxQ_wp++;
						HOSTRxQ_cnt++;
						if ( HOSTRxQ_wp >= MAX_UART_PACKET_LENGTH ) 
								HOSTRxQ_wp=0;
						
						if ( HOSTRxQ[0] != UART_BUFF_HEAD ) 
						{
							HOSTRxQ_wp	=0;
							HOSTRxQ_cnt	=0;
							HOSTRxQ_rp	=0;
						}	
						
						if( HOSTRxQ_cnt >= MAX_UART_PACKET_LENGTH )
						{
								if ( HOSTRxQ[MAX_UART_PACKET_LENGTH-1] !=  UART_BUFF_TAIL) 
								{
										HOSTRxQ_wp	=0;
										HOSTRxQ_cnt	=0;
										HOSTRxQ_rp	=0;
										return;
								}
								for(i=0;i<MAX_UART_PACKET_LENGTH;i++)
								{
										HostToken[i]=HOSTRxQ[HOSTRxQ_rp];
										HOSTRxQ_cnt--;
										HOSTRxQ_rp++;
										if(HOSTRxQ_rp >= MAX_UART_PACKET_LENGTH) 
												HOSTRxQ_rp = 0 ;
								}
								HOSTRxQ_wp	=0;
								HOSTRxQ_cnt	=0;
								HOSTRxQ_rp	=0;
								HostTokenReady = TRUE ;
						} 
				}
		}

		u32IntSts = UART1->INTSTS;
		if(u32IntSts & UART_INTSTS_THREINT_Msk)
		{
				if(HOSTTxQ_cnt > 0)
				{
						/* Bug B6 修正: 移除 while(UART_IS_TX_FULL) busy-wait；
						 * THRE 中斷觸發時 TX FIFO 必然有空間，直接寫入即可 */
						UART_WRITE(UART1, HOSTTxQ[HOSTTxQ_rp]);
						HOSTTxQ_cnt--;
						HOSTTxQ_rp++;
						if(HOSTTxQ_rp >= MAX_UART_PACKET_LENGTH)
								HOSTTxQ_rp = 0;
				} else {
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

void UART0_Init(void)
{
    /* Reset UART0 */
    SYS_ResetModule(UART0_RST);

    /* UART0 clock source: HIRC, divider = 1 (依你 SYS_Init 設定) */
    CLK_SetModuleClock(UART0_MODULE, CLK_CLKSEL1_UARTSEL_HXT, CLK_CLKDIV0_UART(1));

    /* Basic UART setting (baud, data bits, parity, stop) */
    UART_Open(UART0, 57600);

		#ifdef RS485
		//	RS485 AUD Config
		RS485_AUD_Config(UART0);
		#endif

    /* Enable RX interrupt only */
    UART_EnableInt(UART0, UART_INTEN_RDAIEN_Msk);
}


void UART1_Init()
{
    /* Reset UART0 */
    SYS_ResetModule(UART1_RST);

    /* UART0 clock source: HIRC, divider = 1 (依你 SYS_Init 設定) */
    CLK_SetModuleClock(UART1_MODULE, CLK_CLKSEL1_UARTSEL_HXT, CLK_CLKDIV0_UART(1));

    /* Basic UART setting (baud, data bits, parity, stop) */
    UART_Open(UART1, 57600);
	
		#ifdef RS485
		//	RS485 AUD Config
		RS485_AUD_Config(UART1);
		#endif

    /* Enable RX interrupt only */
    UART_EnableInt(UART1, UART_INTEN_RDAIEN_Msk);
}

void CalChecksumH(void)
{
		uint8_t i;
		uint8_t Checksum;
		HostTxBuffer[0] = 0x55 ;
		HostTxBuffer[1] = MyDeviceID ;
		Checksum = 0 ;
		for (i=1;i<(MAX_UART_PACKET_LENGTH-2);i++)
		{
			Checksum += HostTxBuffer[i];
		}	
		HostTxBuffer[MAX_UART_PACKET_LENGTH-2] = Checksum ;
		HostTxBuffer[MAX_UART_PACKET_LENGTH-1] = '\n' ;  
		_SendStringToHOST(HostTxBuffer,MAX_UART_PACKET_LENGTH);	
}

uint8_t _SendStringToHOST(uint8_t *Str, uint8_t len)
{
		uint8_t idx;

		if( (HOSTTxQ_cnt+len) > MAX_UART_PACKET_LENGTH )
		{
				return 0x01;
		} else {
				for(idx=0; idx<len; idx++)
				{
						HOSTTxQ[HOSTTxQ_wp] = Str[idx];
						HOSTTxQ_wp++;
						if(HOSTTxQ_wp>=MAX_UART_PACKET_LENGTH)
						{
								HOSTTxQ_wp=0;
						}
						HOSTTxQ_cnt++;
				}
				UART_EnableInt(UART1, (UART_INTEN_THREIEN_Msk));
				/* Bug B7 修正: 移除 while(TXEMPTYF) 阻塞等待；ISR 非同步排空 TxQ */
		}
		return 0x00;
}
//	Clear Uart Token
void ResetHostUART(void)
{
		HOSTRxQ_cnt = 0 ; 
		HOSTRxQ_wp 	= 0 ;
		HOSTRxQ_rp 	= 0 ;
		HOSTTxQ_cnt = 0 ;
		HOSTTxQ_wp 	= 0 ;
		HOSTTxQ_rp 	= 0 ;
}

void ResetMeterUART(void)
{
		METERRxQ_wp 	= 0 ;
		METERRxQ_rp 	= 0 ;
		METERRxQ_cnt 	= 0 ;
		METERTxQ_wp 	= 0 ;
		METERTxQ_rp 	= 0 ;
		METERTxQ_cnt 	= 0 ;
}

/* ═══════════════════════════════════════════════════════════
 *  SysTick — 1 ms 計數器
 * ═══════════════════════════════════════════════════════════ */

static volatile uint32_t s_ms_tick = 0u;

void SysTick_Handler(void)
{
    s_ms_tick++;
}

void BL_SysTickInit(void)
{
    /* SystemCoreClock 由 Nuvoton CMSIS 在 system_nuc1261.c 中更新 */
    SysTick_Config(SystemCoreClock / 1000u);
}

uint32_t BL_GetTickMs(void)
{
    return s_ms_tick;
}

/* ═══════════════════════════════════════════════════════════
 *  BL UART API — 包裝現有 ISR 驅動環形緩衝區
 * ═══════════════════════════════════════════════════════════ */

void BL_UART_Poll(void)
{
    /* ISR 已處理收發，主迴圈不需額外輪詢 */
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

/* 組 MAX_UART_PACKET_LENGTH(100) bytes 固定幀並非阻塞入 TxQ */
void BL_UART_SendRsp(uint8_t cmd, const uint8_t *payload, uint16_t len)
{
    uint16_t i;
    uint16_t copy_len;

    memset(HostTxBuffer, 0, MAX_UART_PACKET_LENGTH);
    HostTxBuffer[2] = cmd;

    if (payload != NULL && len > 0u)
    {
        /* payload 放在 byte 3 開始，最多到 byte 97 (留 checksum + tail) */
        copy_len = len;
        if (copy_len > (uint16_t)(MAX_UART_PACKET_LENGTH - 5u))
            copy_len = (uint16_t)(MAX_UART_PACKET_LENGTH - 5u);

        for (i = 0u; i < copy_len; i++)
            HostTxBuffer[3u + i] = payload[i];
    }

    CalChecksumH();   /* 填 [0..1] header、計算 checksum、呼叫 _SendStringToHOST */
}
