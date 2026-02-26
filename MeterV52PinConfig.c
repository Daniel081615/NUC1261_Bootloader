/****************************************************************************
 * @file     MeterV52PinConfig.c
 * @version  V1.33.0005
 * @Date     Wed Jan 28 2026 17:21:09 GMT+0800 (台北標準時間)
 * @brief    NuMicro generated code file
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2013-2026 Nuvoton Technology Corp. All rights reserved.
*****************************************************************************/

/********************
MCU:NUC1261NE4AE(QFN48)
Pin Configuration:
Pin5:UART0_RXD
Pin7:UART0_TXD
Pin9:PD.3
Pin11:PF.0
Pin12:PF.1
Pin13:PF.2
Pin14:PD.7
Pin15:XT1_OUT
Pin16:XT1_IN
Pin19:PC.0
Pin20:PC.1
Pin21:UART2_TXD
Pin22:UART2_RXD
Pin23:PC.4
Pin24:PE.0
Pin25:ICE_CLK
Pin26:ICE_DAT
Pin27:PE.10
Pin28:UART1_nRTS
Pin29:UART1_TXD
Pin30:UART1_RXD
Pin37:UART0_nRTS
Pin39:PA.1
Pin40:PA.0
Pin44:PB.0
Pin45:PB.1
Module Configuration:
UART0_nRTS(Pin:37)
UART0_RXD(Pin:5)
UART0_TXD(Pin:7)
PD.3(Pin:9)
PD.7(Pin:14)
PF.0(Pin:11)
PF.1(Pin:12)
PF.2(Pin:13)
XT1_IN(Pin:16)
XT1_OUT(Pin:15)
PC.0(Pin:19)
PC.1(Pin:20)
PC.4(Pin:23)
UART2_RXD(Pin:22)
UART2_TXD(Pin:21)
PE.0(Pin:24)
PE.10(Pin:27)
ICE_CLK(Pin:25)
ICE_DAT(Pin:26)
UART1_nRTS(Pin:28)
UART1_RXD(Pin:30)
UART1_TXD(Pin:29)
PA.0(Pin:40)
PA.1(Pin:39)
PB.0(Pin:44)
PB.1(Pin:45)
GPIO Configuration:
PA.0:PA.0(Pin:40)
PA.1:PA.1(Pin:39)
PA.3:UART0_nRTS(Pin:37)
PB.0:PB.0(Pin:44)
PB.1:PB.1(Pin:45)
PC.0:PC.0(Pin:19)
PC.1:PC.1(Pin:20)
PC.2:UART2_TXD(Pin:21)
PC.3:UART2_RXD(Pin:22)
PC.4:PC.4(Pin:23)
PD.0:UART0_RXD(Pin:5)
PD.1:UART0_TXD(Pin:7)
PD.3:PD.3(Pin:9)
PD.7:PD.7(Pin:14)
PE.0:PE.0(Pin:24)
PE.6:ICE_CLK(Pin:25)
PE.7:ICE_DAT(Pin:26)
PE.10:PE.10(Pin:27)
PE.11:UART1_nRTS(Pin:28)
PE.12:UART1_TXD(Pin:29)
PE.13:UART1_RXD(Pin:30)
PF.0:PF.0(Pin:11)
PF.1:PF.1(Pin:12)
PF.2:PF.2(Pin:13)
PF.3:XT1_OUT(Pin:15)
PF.4:XT1_IN(Pin:16)
********************/

#include "NUC1261.h"

void MeterV52PinConfig_init_ice(void)
{
    SYS->GPE_MFPL &= ~(SYS_GPE_MFPL_PE7MFP_Msk | SYS_GPE_MFPL_PE6MFP_Msk);
    SYS->GPE_MFPL |= (SYS_GPE_MFPL_PE7MFP_ICE_DAT | SYS_GPE_MFPL_PE6MFP_ICE_CLK);

    return;
}

void MeterV52PinConfig_deinit_ice(void)
{
    SYS->GPE_MFPL &= ~(SYS_GPE_MFPL_PE7MFP_Msk | SYS_GPE_MFPL_PE6MFP_Msk);

    return;
}

void MeterV52PinConfig_init_pa(void)
{
    SYS->GPA_MFPL &= ~(SYS_GPA_MFPL_PA1MFP_Msk | SYS_GPA_MFPL_PA0MFP_Msk);
    SYS->GPA_MFPL |= (SYS_GPA_MFPL_PA1MFP_GPIO | SYS_GPA_MFPL_PA0MFP_GPIO);

    return;
}

void MeterV52PinConfig_deinit_pa(void)
{
    SYS->GPA_MFPL &= ~(SYS_GPA_MFPL_PA1MFP_Msk | SYS_GPA_MFPL_PA0MFP_Msk);

    return;
}

void MeterV52PinConfig_init_pb(void)
{
    SYS->GPB_MFPL &= ~(SYS_GPB_MFPL_PB1MFP_Msk | SYS_GPB_MFPL_PB0MFP_Msk);
    SYS->GPB_MFPL |= (SYS_GPB_MFPL_PB1MFP_GPIO | SYS_GPB_MFPL_PB0MFP_GPIO);

    return;
}

void MeterV52PinConfig_deinit_pb(void)
{
    SYS->GPB_MFPL &= ~(SYS_GPB_MFPL_PB1MFP_Msk | SYS_GPB_MFPL_PB0MFP_Msk);

    return;
}

void MeterV52PinConfig_init_pc(void)
{
    SYS->GPC_MFPL &= ~(SYS_GPC_MFPL_PC4MFP_Msk | SYS_GPC_MFPL_PC1MFP_Msk | SYS_GPC_MFPL_PC0MFP_Msk);
    SYS->GPC_MFPL |= (SYS_GPC_MFPL_PC4MFP_GPIO | SYS_GPC_MFPL_PC1MFP_GPIO | SYS_GPC_MFPL_PC0MFP_GPIO);

    return;
}

void MeterV52PinConfig_deinit_pc(void)
{
    SYS->GPC_MFPL &= ~(SYS_GPC_MFPL_PC4MFP_Msk | SYS_GPC_MFPL_PC1MFP_Msk | SYS_GPC_MFPL_PC0MFP_Msk);

    return;
}

void MeterV52PinConfig_init_pd(void)
{
    SYS->GPD_MFPL &= ~(SYS_GPD_MFPL_PD7MFP_Msk | SYS_GPD_MFPL_PD3MFP_Msk);
    SYS->GPD_MFPL |= (SYS_GPD_MFPL_PD7MFP_GPIO | SYS_GPD_MFPL_PD3MFP_GPIO);

    return;
}

void MeterV52PinConfig_deinit_pd(void)
{
    SYS->GPD_MFPL &= ~(SYS_GPD_MFPL_PD7MFP_Msk | SYS_GPD_MFPL_PD3MFP_Msk);

    return;
}

void MeterV52PinConfig_init_pe(void)
{
    SYS->GPE_MFPH &= ~(SYS_GPE_MFPH_PE10MFP_Msk);
    SYS->GPE_MFPH |= (SYS_GPE_MFPH_PE10MFP_GPIO);
    SYS->GPE_MFPL &= ~(SYS_GPE_MFPL_PE0MFP_Msk);
    SYS->GPE_MFPL |= (SYS_GPE_MFPL_PE0MFP_GPIO);

    return;
}

void MeterV52PinConfig_deinit_pe(void)
{
    SYS->GPE_MFPH &= ~(SYS_GPE_MFPH_PE10MFP_Msk);
    SYS->GPE_MFPL &= ~(SYS_GPE_MFPL_PE0MFP_Msk);

    return;
}

void MeterV52PinConfig_init_pf(void)
{
    SYS->GPF_MFPL &= ~(SYS_GPF_MFPL_PF2MFP_Msk | SYS_GPF_MFPL_PF1MFP_Msk | SYS_GPF_MFPL_PF0MFP_Msk);
    SYS->GPF_MFPL |= (SYS_GPF_MFPL_PF2MFP_GPIO | SYS_GPF_MFPL_PF1MFP_GPIO | SYS_GPF_MFPL_PF0MFP_GPIO);

    return;
}

void MeterV52PinConfig_deinit_pf(void)
{
    SYS->GPF_MFPL &= ~(SYS_GPF_MFPL_PF2MFP_Msk | SYS_GPF_MFPL_PF1MFP_Msk | SYS_GPF_MFPL_PF0MFP_Msk);

    return;
}

void MeterV52PinConfig_init_uart0(void)
{
    SYS->GPA_MFPL &= ~(SYS_GPA_MFPL_PA3MFP_Msk);
    SYS->GPA_MFPL |= (SYS_GPA_MFPL_PA3MFP_UART0_nRTS);
    SYS->GPD_MFPL &= ~(SYS_GPD_MFPL_PD1MFP_Msk | SYS_GPD_MFPL_PD0MFP_Msk);
    SYS->GPD_MFPL |= (SYS_GPD_MFPL_PD1MFP_UART0_TXD | SYS_GPD_MFPL_PD0MFP_UART0_RXD);

    return;
}

void MeterV52PinConfig_deinit_uart0(void)
{
    SYS->GPA_MFPL &= ~(SYS_GPA_MFPL_PA3MFP_Msk);
    SYS->GPD_MFPL &= ~(SYS_GPD_MFPL_PD1MFP_Msk | SYS_GPD_MFPL_PD0MFP_Msk);

    return;
}

void MeterV52PinConfig_init_uart1(void)
{
    SYS->GPE_MFPH &= ~(SYS_GPE_MFPH_PE13MFP_Msk | SYS_GPE_MFPH_PE12MFP_Msk | SYS_GPE_MFPH_PE11MFP_Msk);
    SYS->GPE_MFPH |= (SYS_GPE_MFPH_PE13MFP_UART1_RXD | SYS_GPE_MFPH_PE12MFP_UART1_TXD | SYS_GPE_MFPH_PE11MFP_UART1_nRTS);

    return;
}

void MeterV52PinConfig_deinit_uart1(void)
{
    SYS->GPE_MFPH &= ~(SYS_GPE_MFPH_PE13MFP_Msk | SYS_GPE_MFPH_PE12MFP_Msk | SYS_GPE_MFPH_PE11MFP_Msk);

    return;
}

void MeterV52PinConfig_init_uart2(void)
{
    SYS->GPC_MFPL &= ~(SYS_GPC_MFPL_PC3MFP_Msk | SYS_GPC_MFPL_PC2MFP_Msk);
    SYS->GPC_MFPL |= (SYS_GPC_MFPL_PC3MFP_UART2_RXD | SYS_GPC_MFPL_PC2MFP_UART2_TXD);

    return;
}

void MeterV52PinConfig_deinit_uart2(void)
{
    SYS->GPC_MFPL &= ~(SYS_GPC_MFPL_PC3MFP_Msk | SYS_GPC_MFPL_PC2MFP_Msk);

    return;
}

void MeterV52PinConfig_init_xt1(void)
{
    SYS->GPF_MFPL &= ~(SYS_GPF_MFPL_PF4MFP_Msk | SYS_GPF_MFPL_PF3MFP_Msk);
    SYS->GPF_MFPL |= (SYS_GPF_MFPL_PF4MFP_XT1_IN | SYS_GPF_MFPL_PF3MFP_XT1_OUT);

    return;
}

void MeterV52PinConfig_deinit_xt1(void)
{
    SYS->GPF_MFPL &= ~(SYS_GPF_MFPL_PF4MFP_Msk | SYS_GPF_MFPL_PF3MFP_Msk);

    return;
}

void MeterV52PinConfig_init(void)
{
    //SYS->GPA_MFPL = 0x00003000UL;
    //SYS->GPB_MFPL = 0x00000000UL;
    //SYS->GPC_MFPL = 0x00003300UL;
    //SYS->GPD_MFPL = 0x00000033UL;
    //SYS->GPE_MFPH = 0x00333000UL;
    //SYS->GPE_MFPL = 0x11000000UL;
    //SYS->GPF_MFPL = 0x00011000UL;

    MeterV52PinConfig_init_ice();
    MeterV52PinConfig_init_pa();
    MeterV52PinConfig_init_pb();
    MeterV52PinConfig_init_pc();
    MeterV52PinConfig_init_pd();
    MeterV52PinConfig_init_pe();
    MeterV52PinConfig_init_pf();
    MeterV52PinConfig_init_uart0();
    MeterV52PinConfig_init_uart1();
    MeterV52PinConfig_init_uart2();
    MeterV52PinConfig_init_xt1();

    return;
}

void MeterV52PinConfig_deinit(void)
{
    MeterV52PinConfig_deinit_ice();
    MeterV52PinConfig_deinit_pa();
    MeterV52PinConfig_deinit_pb();
    MeterV52PinConfig_deinit_pc();
    MeterV52PinConfig_deinit_pd();
    MeterV52PinConfig_deinit_pe();
    MeterV52PinConfig_deinit_pf();
    MeterV52PinConfig_deinit_uart0();
    MeterV52PinConfig_deinit_uart1();
    MeterV52PinConfig_deinit_uart2();
    MeterV52PinConfig_deinit_xt1();

    return;
}

/*** (C) COPYRIGHT 2013-2026 Nuvoton Technology Corp. ***/
