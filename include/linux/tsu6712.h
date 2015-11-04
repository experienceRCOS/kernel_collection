/*
 * Copyright (C) 2012 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */


#ifndef TSU6712_H
#define TSU6712_H

#include <linux/types.h>

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

enum {
	TSU6712_DETACHED,
	TSU6712_ATTACHED
};

enum {
	TSU6712_DETACHED_DOCK = 0,
	TSU6712_ATTACHED_DESK_DOCK,
	TSU6712_ATTACHED_CAR_DOCK,
};

enum {
	SWITCH_PORT_AUTO = 0,
	SWITCH_PORT_USB,
	SWITCH_PORT_AUDIO,
	SWITCH_PORT_UART,
	SWITCH_PORT_VAUDIO,
	SWITCH_PORT_USB_OPEN,
	SWITCH_PORT_ALL_OPEN,
};

extern void tsu6712_manual_switching(int path);
extern void tsu6712_otg_detach(void);
extern void tsu6712_otg_set_autosw_pda(void);
extern int get_cable_type(void);
extern ssize_t ld_set_switch_buf(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);

extern ssize_t ld_set_switch_buf(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count);


#endif
