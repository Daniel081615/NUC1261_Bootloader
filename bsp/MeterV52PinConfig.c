/****************************************************************************
 * @file     MeterV52PinConfig.c
 * @brief    NUC1261 pin mux initialisation (flattened — no sub-function calls)
 *
 * SPDX-License-Identifier: Apache-2.0
 * Copyright (C) 2013-2026 Nuvoton Technology Corp. All rights reserved.
*****************************************************************************/

#include "NUC1261.h"

void MeterV52PinConfig_init(void)
{
    /* ICE (PE6/PE7) + PE0 GPIO — merged GPE_MFPL */
    SYS->GPE_MFPL = (SYS->GPE_MFPL
                     & ~(SYS_GPE_MFPL_PE7MFP_Msk | SYS_GPE_MFPL_PE6MFP_Msk | SYS_GPE_MFPL_PE0MFP_Msk))
                   | (SYS_GPE_MFPL_PE7MFP_ICE_DAT | SYS_GPE_MFPL_PE6MFP_ICE_CLK
                      | SYS_GPE_MFPL_PE0MFP_GPIO);

    /* PA GPIO + UART0_nRTS — merged GPA_MFPL */
    SYS->GPA_MFPL = (SYS->GPA_MFPL
                     & ~(SYS_GPA_MFPL_PA3MFP_Msk | SYS_GPA_MFPL_PA1MFP_Msk | SYS_GPA_MFPL_PA0MFP_Msk))
                   | (SYS_GPA_MFPL_PA3MFP_UART0_nRTS | SYS_GPA_MFPL_PA1MFP_GPIO
                      | SYS_GPA_MFPL_PA0MFP_GPIO);

    /* PB GPIO */
    SYS->GPB_MFPL = (SYS->GPB_MFPL
                     & ~(SYS_GPB_MFPL_PB1MFP_Msk | SYS_GPB_MFPL_PB0MFP_Msk))
                   | (SYS_GPB_MFPL_PB1MFP_GPIO | SYS_GPB_MFPL_PB0MFP_GPIO);

    /* PC GPIO + UART2 — merged GPC_MFPL */
    SYS->GPC_MFPL = (SYS->GPC_MFPL
                     & ~(SYS_GPC_MFPL_PC4MFP_Msk | SYS_GPC_MFPL_PC3MFP_Msk | SYS_GPC_MFPL_PC2MFP_Msk
                         | SYS_GPC_MFPL_PC1MFP_Msk | SYS_GPC_MFPL_PC0MFP_Msk))
                   | (SYS_GPC_MFPL_PC4MFP_GPIO | SYS_GPC_MFPL_PC3MFP_UART2_RXD
                      | SYS_GPC_MFPL_PC2MFP_UART2_TXD | SYS_GPC_MFPL_PC1MFP_GPIO
                      | SYS_GPC_MFPL_PC0MFP_GPIO);

    /* PD GPIO + UART0 data — merged GPD_MFPL */
    SYS->GPD_MFPL = (SYS->GPD_MFPL
                     & ~(SYS_GPD_MFPL_PD7MFP_Msk | SYS_GPD_MFPL_PD3MFP_Msk
                         | SYS_GPD_MFPL_PD1MFP_Msk | SYS_GPD_MFPL_PD0MFP_Msk))
                   | (SYS_GPD_MFPL_PD7MFP_GPIO | SYS_GPD_MFPL_PD3MFP_GPIO
                      | SYS_GPD_MFPL_PD1MFP_UART0_TXD | SYS_GPD_MFPL_PD0MFP_UART0_RXD);

    /* PE10 GPIO + UART1 — merged GPE_MFPH */
    SYS->GPE_MFPH = (SYS->GPE_MFPH
                     & ~(SYS_GPE_MFPH_PE13MFP_Msk | SYS_GPE_MFPH_PE12MFP_Msk
                         | SYS_GPE_MFPH_PE11MFP_Msk | SYS_GPE_MFPH_PE10MFP_Msk))
                   | (SYS_GPE_MFPH_PE13MFP_UART1_RXD | SYS_GPE_MFPH_PE12MFP_UART1_TXD
                      | SYS_GPE_MFPH_PE11MFP_UART1_nRTS | SYS_GPE_MFPH_PE10MFP_GPIO);

    /* PF GPIO + XT1 — merged GPF_MFPL */
    SYS->GPF_MFPL = (SYS->GPF_MFPL
                     & ~(SYS_GPF_MFPL_PF4MFP_Msk | SYS_GPF_MFPL_PF3MFP_Msk | SYS_GPF_MFPL_PF2MFP_Msk
                         | SYS_GPF_MFPL_PF1MFP_Msk | SYS_GPF_MFPL_PF0MFP_Msk))
                   | (SYS_GPF_MFPL_PF4MFP_XT1_IN | SYS_GPF_MFPL_PF3MFP_XT1_OUT
                      | SYS_GPF_MFPL_PF2MFP_GPIO | SYS_GPF_MFPL_PF1MFP_GPIO
                      | SYS_GPF_MFPL_PF0MFP_GPIO);
}

/*** (C) COPYRIGHT 2013-2026 Nuvoton Technology Corp. ***/
