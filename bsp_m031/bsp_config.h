/* bsp_config.h — Board variant and hardware feature flags (M031LE3AE)
 *
 * Parallel to bsp/bsp_config.h; only the M031_Bootloader.uvprojx project
 * includes this directory, so NUC1261 build is unaffected.
 *
 * To override for a different M031 board, set preprocessor symbols in the
 * Keil project Options for Target instead of editing this file. */

#ifndef BSP_CONFIG_H
#define BSP_CONFIG_H

/* RS485 AUD direction control on UART0.
 * TX pin (PA14) is switched between GPIO and UART function manually;
 * nRTS MFP is NOT configured (same pattern as NUC1261 sub-board). */
#define RS485

/* Host UART channel — M031 bootloader always uses UART0.
 * PA15 = RXD, PA14 = TXD, 57600 baud. */
#define BTLD_HOST_UART_CH   0U

#endif /* BSP_CONFIG_H */
