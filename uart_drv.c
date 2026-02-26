/****************************************************************************
 * @file     uart_drv.c
 * @version  V1.33.0005
 * @Date     Wed Feb 20 2026 17:21:09 GMT+0800 (台北標準時間)
 * @brief    uart init, IRQ Functions code file
 *
 * SPDX-License-Identifier: Apache-2.0
 *
echnology Corp. All rights reserved.
*****************************************************************************/

#include 	"NUC1261.h"
#include	"MyDef.h"
#include	"ExternFunc.h"
#include	"uart_drv.h"

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
						while(UART_IS_TX_FULL(UART1));  /* Wait Tx is not full to transmit data */
						UART_WRITE(UART1, HOSTTxQ[HOSTTxQ_rp]);      
						HOSTTxQ_cnt--;				                   
						HOSTTxQ_rp++;
						if(HOSTTxQ_rp >= MAX_UART_PACKET_LENGTH) 
								HOSTTxQ_rp = 0 ;		
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
				return 0x01 ;
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
				UART_EnableInt(UART1, (UART_INTEN_THREIEN_Msk ));
		}
		while (!(UART1->FIFOSTS & UART_FIFOSTS_TXEMPTYF_Msk));
		return 0x00 ;
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
