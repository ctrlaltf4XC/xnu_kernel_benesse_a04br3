/*
 * Copyright (c) 2000-2021 Apple Inc. All rights reserved.
 */
#ifndef _PEXPERT_ARM_PROTOS_H
#define _PEXPERT_ARM_PROTOS_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern vm_offset_t pe_arm_get_soc_base_phys(void);
extern uint32_t pe_arm_init_interrupts(void *args);
extern void pe_arm_init_debug(void *args);

#ifdef  PEXPERT_KERNEL_PRIVATE
extern void console_write_unbuffered(char);
#endif
int serial_init(void);

/**
 * Forbid or allow transmission over each serial until they receive data.
 */
void
serial_set_on_demand(bool);

#if HIBERNATION
void serial_hibernation_init(void);
#endif /* HIBERNATION */
int serial_getc(void);
void serial_putc(char);
void uart_putc(char);
#ifdef PRIVATE
void serial_putc_options(char, bool);
void uart_putc_options(char, bool);
#endif /* PRIVATE */
int uart_getc(void);

void pe_init_fiq(void);
#if defined(ARM64_BOARD_CONFIG_A04BR3)
#define A04BR3_VTIMER_INTID 27
#define A04BR3_IPI_SGI 1
void a04br3_gic_init(void);
uint32_t a04br3_gic_acknowledge(void);
void a04br3_gic_complete(uint32_t iar);
void a04br3_gic_signal_self(void);
void a04br3_gic_signal_cpu(unsigned logical);
uint32_t a04br3_gic_ipi_count(void);
uint32_t a04br3_gic_timer_count(void);
void a04br3_watchdog_quiesce(void);
#endif
#if defined(ARM64_BOARD_CONFIG_BEGONIA)
void begonia_gic_init(void);
uint32_t begonia_gic_acknowledge(void);
void begonia_gic_complete(uint32_t iar);
void begonia_gic_signal_self(void);
void begonia_gic_signal_cpu(unsigned logical);
uint32_t begonia_gic_ipi_count(void);
uint32_t begonia_gic_timer_count(void);
void begonia_gic_secondary_init(unsigned logical);
#endif

#ifdef PRIVATE
/**
 * One hot ids to distinquish between all supported serial devices
 */
typedef enum serial_device {
	SERIAL_UNKNOWN=0x0,
	SERIAL_APPLE_UART=0x1,
	SERIAL_DOCKCHANNEL=0x2,
	SERIAL_PI3_UART=0x4,
	SERIAL_VMAPPLE_UART=0x8,
	SERIAL_DCC_UART=0x10,
	SERIAL_MTK_UART=0x20
} serial_device_t;

kern_return_t serial_irq_enable(serial_device_t device);
kern_return_t serial_irq_action(serial_device_t device);
bool serial_irq_filter(serial_device_t device);

void serial_go_to_sleep(void);
#endif /* PRIVATE */

int switch_to_serial_console(void);
void switch_to_old_console(int);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* _PEXPERT_ARM_PROTOS_H */
