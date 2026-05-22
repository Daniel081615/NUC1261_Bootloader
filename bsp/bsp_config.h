/* bsp_config.h — Board variant and hardware feature flags (NUC1261 / MeterV5.2)
 *
 * Include this header in BSP and Driver files that need variant guards.
 * Only bsp_init.c and uart_drv.c should include this directly.
 *
 * To override for a different board variant, comment out the defines here
 * or set them as project-level preprocessor symbols in the IDE instead. */

#ifndef BSP_CONFIG_H
#define BSP_CONFIG_H

/* HXT crystal on PF3/PF4.  Comment out to use PF0/PF1 X32 variant. */
#define MeterV5_2

/* UART1 in RS485 AUD mode (hardware automatic direction control via nRTS).
 * Comment out to use plain full-duplex UART. */
#define RS485

/* Host UART channel for bootloader communication:
 *   1 = UART1  Master board  (PE13 RXD / PE12 TXD / PE11 nRTS)
 *   0 = UART0  Sub board     (PD0  RXD / PD1  TXD )
 * Both channels use RS485 AUD mode when RS485 is defined. */
#define BTLD_HOST_UART_CH   1U

#endif /* BSP_CONFIG_H */
