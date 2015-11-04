#ifndef __BOARD_BLUETOOTH_BROADCOM_H
#define __BOARD_BLUETOOTH_BROADCOM_H

extern struct platform_device broadcom_bluetooth_device;

extern void bcm_bt_lpm_exit_lpm_locked(struct uart_port *uport);

#endif // __BOARD_BLUETOOTH_BROADCOM_H
