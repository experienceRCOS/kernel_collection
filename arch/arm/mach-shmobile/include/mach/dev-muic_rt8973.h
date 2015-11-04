#ifndef __ARM_ARCH_DEV_MUIC_RT8973_H
#define __ARM_ARCH_DEV_MUIC_RT8973_H

void __init dev_muic_rt8973_init(void);
#define UART_AT_MODE           1001
#define UART_INVALID_MODE      1002
#define UART_EMPTY_CR          1003
#define UART_EMPTY_CRLF        1004
#define UART_AT_MODE_MODECHAN  1005
#define SWITCH_AT       103
#define SWITCH_ISI      104
#define SWITCH_MODECHAN_02 105
#define SWITCH_MODECHAN_00 106
#define MUIC_INTERRUPT_QUEUE_MAX 8
#define MUIC_INTERRUPT_QUEUE_MIN 0
#define CABLE_USB_ATTACHED 100
#define CABLE_USB_DETACHED 101
#define CABLE_AC_ATTACHED 200
#define CABLE_AC_DETACHED 201
#define CABLE_UART_ATTACHED 200
#define CABLE_UART_DETACHED 201
#define USE_USB_UART_SWITCH


extern ssize_t ld_set_switch_buf(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);

extern ssize_t ld_set_switch_buf(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);

int get_cable_type(void);
void uas_jig_force_sleep_rt8973(void);

#endif