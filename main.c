/**************************************************************************//**
 * @file     	main.c
 * @brief    	NUC1261 Menter Bootloader : Automatically patch if update to bank2
 * @version	 	2025.08.11 Patch bin file in Bootloader
 * @advantage	One version of bin file is needed, can be used in Center update Meter	Fw structure
 ******************************************************************************/
 
 /***			 @MemoryLayout			***
 +------------------------------+ => Max APROM size: 0x00020000
 |															| 
 |					Data Flash					| => @Purpose: Store meta information of Bank1,2
 |															|	=> Size: 0x1800 ~= 6kb
 +------------------------------+	=> Data flash base addr: 0x0001E800
 |															|
 |					APROM Bank2					| => @Purpose: Store APP code
 |															|	=> Size: 0xE800 ~= 60kb																											 +----------------------------+
 +------------------------------+	=> Bank2 base addr: 0x00010000																							 |														|		Only need to Send one Version of Bin file			
 |															|																																							 |						@Host 					|		to bank1 => write
 |					APROM Bank1					| => @Purpose: Store APP code																									 |														|		to bank2 => patch & write
 |															|	=> Size: 0xE000 ~= 57kb																											 +----------------------------+
 +------------------------------+	=> Bank1 base addr: 0x00002000																														
 |															|																																														
 |					Bootloader					| => @Purpose: The initial code executed on startup. It is responsible for loading and verifying the application in APROM Bank 1 or 2.
 |															|	=> Size: 0x2000 ~= 8kb
 +------------------------------+	=> Bank1 base addr: 0x00000000
 
 *	!!!	@Warning	!!!	
 When using keil c build @Fw Bin file, Plz set
 1:	 Options for target >> C/C++  >> @Select 	One Elf Section per function 
																		 @Select	Optimization Level :	???
 2:	 Options for target >> Linker >> @Select	Use Memory Layout from Target Dialog
																		 @Select	Make RW Sections Position Independant
																		 @Select	Make RO Sections Position Independant 
***/

#include 	"stdio.h"
#include	"MyDef.h"
#include 	"NUC1261.h"
#include 	"MeterV52PinConfig.h"
#include	"uart_drv.h"
#include	"BootloaderProcess.h"


#include "Select_fw.h"
#include "fw_metadata.h"
#include "Select_fw.h"

#define PLLCTL_SETTING      CLK_PLLCTL_72MHz_HIRC
#define PLL_CLOCK           71884800



//	Functions
void ReadMyDeviceID(void);

//	Variables
uint8_t MyDeviceID;
uint32_t g_apromSize;
	
void SYS_Init(void)
{

		#ifdef 	MeterV5_2
		/* Set PF multi-function pins for XT1_OUT(PF.3) and XT1_IN(PF.4) */
		SYS->GPF_MFPL = (SYS->GPF_MFPL & (~SYS_GPF_MFPL_PF3MFP_Msk)) | SYS_GPF_MFPL_PF3MFP_XT1_OUT;
		SYS->GPF_MFPL = (SYS->GPF_MFPL & (~SYS_GPF_MFPL_PF4MFP_Msk)) | SYS_GPF_MFPL_PF4MFP_XT1_IN;	
		
		#else
		/* Set PF multi-function pins for X32_OUT(PF.0) and X32_IN(PF.1) */
		SYS->GPF_MFPL = (SYS->GPF_MFPL & (~SYS_GPF_MFPL_PF0MFP_Msk)) | SYS_GPF_MFPL_PF0MFP_X32_OUT;
		SYS->GPF_MFPL = (SYS->GPF_MFPL & (~SYS_GPF_MFPL_PF1MFP_Msk)) | SYS_GPF_MFPL_PF1MFP_X32_IN;
		#endif
		
		/*---------------------------------------------------------------------------------------------------------*/
		/* Init System Clock                                                                                       */
		/*---------------------------------------------------------------------------------------------------------*/

		/* Enable HIRC, HXT and LXT clock */
		/* Enable Internal RC 22.1184MHz clock */
		CLK_EnableXtalRC(CLK_PWRCTL_HIRCEN_Msk | CLK_PWRCTL_HXTEN_Msk | CLK_PWRCTL_LIRCEN_Msk);

		/* Wait for HIRC, HXT and LXT clock ready */
		CLK_WaitClockReady(CLK_STATUS_HIRCSTB_Msk | CLK_STATUS_HXTSTB_Msk);

		/* Select HCLK clock source as HIRC and HCLK clock divider as 1 */
		CLK_SetHCLK(CLK_CLKSEL0_HCLKSEL_HIRC, CLK_CLKDIV0_HCLK(1));

		/* Set core clock as PLL_CLOCK from PLL */
		CLK_SetCoreClock(PLL_CLOCK);
		

		CLK_SetSysTickClockSrc(CLK_CLKSEL0_STCLKSEL_HCLK_DIV2);

		/* Enable UART module clock */
		CLK_EnableModuleClock(UART0_MODULE);	
		CLK_EnableModuleClock(UART1_MODULE);
		CLK_EnableModuleClock(UART2_MODULE);
		CLK_EnableModuleClock(WDT_MODULE);

		/* Select UART module clock source as HXT and UART module clock divider as 1 */
		CLK_SetModuleClock(UART0_MODULE, CLK_CLKSEL1_UARTSEL_HXT, CLK_CLKDIV0_UART(1));
		CLK_SetModuleClock(UART1_MODULE, CLK_CLKSEL1_UARTSEL_HXT, CLK_CLKDIV0_UART(1));
		CLK_SetModuleClock(UART2_MODULE, CLK_CLKSEL1_UARTSEL_HXT, CLK_CLKDIV0_UART(1));
		CLK_SetModuleClock(WDT_MODULE, CLK_CLKSEL1_WDTSEL_LIRC, 0);	
		
		/*---------------------------------------------------------------------------------------------------------*/
		/* Init I/O Multi-function                                                                                 */
		/*---------------------------------------------------------------------------------------------------------*/

		/* Set PD multi-function pins for UART0 RXD(PD.0) and TXD(PD.1) */
		SYS->GPD_MFPL = (SYS->GPD_MFPL & (~SYS_GPD_MFPL_PD0MFP_Msk)) | SYS_GPD_MFPL_PD0MFP_UART0_RXD;
		SYS->GPD_MFPL = (SYS->GPD_MFPL & (~SYS_GPD_MFPL_PD1MFP_Msk)) | SYS_GPD_MFPL_PD1MFP_UART0_TXD;
		
		/* Set PE multi-function pins for UART1 RXD(PE.13) and TXD(PE.12) */
		SYS->GPE_MFPH = (SYS->GPE_MFPH & (~SYS_GPE_MFPH_PE13MFP_Msk)) | SYS_GPE_MFPH_PE13MFP_UART1_RXD;
		SYS->GPE_MFPH = (SYS->GPE_MFPH & (~SYS_GPE_MFPH_PE12MFP_Msk)) | SYS_GPE_MFPH_PE12MFP_UART1_TXD;
		SYS->GPE_MFPH = (SYS->GPE_MFPH & (~SYS_GPE_MFPH_PE11MFP_Msk)) | SYS_GPE_MFPH_PE11MFP_UART1_nRTS;
		/* Set PC multi-function pins for UART2 RXD(PC.3) and TXD(PC.2) */
		SYS->GPC_MFPL = (SYS->GPC_MFPL & (~SYS_GPC_MFPL_PC3MFP_Msk)) | SYS_GPC_MFPL_PC3MFP_UART2_RXD;
		SYS->GPC_MFPL = (SYS->GPC_MFPL & (~SYS_GPC_MFPL_PC2MFP_Msk)) | SYS_GPC_MFPL_PC2MFP_UART2_TXD;		
		SYS->GPC_MFPL = (SYS->GPC_MFPL & (~SYS_GPC_MFPL_PC1MFP_Msk)) | SYS_GPC_MFPL_PC1MFP_UART2_nRTS;	
}

void WDT_Init(void)
{
		/*	Shut previous WDT	*/
		WDT_Close();
		/* WDT SETUP*/
		//	6.5536s
		WDT_Open(WDT_TIMEOUT_2POW16, WDT_RESET_DELAY_18CLK, TRUE, FALSE);
		WDT_EnableInt();
		NVIC_EnableIRQ(WDT_IRQn);
		WDT_RESET_COUNTER();
}

void DataFlashConfig(void)
{
		
		uint32_t au32Config[2], u32BaseAddr;

		FMC_Open();
		FMC_ReadConfig(au32Config, 2);
		
		//	Data Flash Address
		u32BaseAddr = FMC_ReadDataFlashBaseAddr();
	
		if(u32BaseAddr == FW_INFO_BASE) return;

		// 清除 BOOT SELECTION（bit 7:6）
		au32Config[0] &= ~(0x3 << 6);
		// 設為 APROM + IAP（bit 7:6 = 10）
		au32Config[0] |= (0x2 << 6);

		// 開啟 Data Flash（bit 0 = 0）
		au32Config[0] &= ~0x1;

		// 設定 Data Flash 起始位址
		au32Config[1] = FW_INFO_BASE;

		FMC_ENABLE_CFG_UPDATE();
		FMC_WriteConfig(au32Config, 2);
		
		NVIC_SystemReset();

}

int main(void)
{
		uint8_t i;

		MeterV52PinConfig_init();
		
		/* Unlock protected registers */
		SYS_UnlockReg();

		SYS_Init();
		WDT_Init();	
    SYS_UnlockReg();
	
		/**/
    UART0_Init();
		UART1_Init();
	
    FMC_Open();
		ReadMyDeviceID();
		
		DataFlashConfig();
    g_apromSize = APROM_SIZE;

    //  Select Bootloader, Bank1 or Bank2
    BankSelectProcess();

    while (1)
    {
        BootloaderProcess();
        PatchProcess();
    }
}

void ReadMyDeviceID(void)
{	
		
		if ( PB2 )
			MyDeviceID |= BIT0;
		
		if ( PB3 )
			MyDeviceID |= BIT1;
		
		if ( PB4 )
			MyDeviceID |= BIT2;
		
		if ( PB5 )
			MyDeviceID |= BIT3;
		
		if ( PB6 )
			MyDeviceID |= BIT4;
		
		if ( PB7 )
			MyDeviceID |= BIT5;
}