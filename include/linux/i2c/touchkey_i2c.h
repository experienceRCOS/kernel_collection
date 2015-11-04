#ifndef _LINUX_CYPRESS_TOUCHKEY_I2C_H
#define _LINUX_CYPRESS_TOUCHKEY_I2C_H

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/hrtimer.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/delay.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

/* Touchkey Register */
#define KEYCODE_REG			0x00

#define TK_BIT_PRESS_EV		0x08
#define TK_BIT_KEYCODE		0x07

#define TK_BIT_AUTOCAL		0x80

#define I2C_M_WR 0		/* for i2c */

#define DEVICE_NAME "sec_touchkey"

/* JB added to make it compile */
#define TK_UPDATE_FAIL 0
#define TK_UPDATE_DOWN 1
#define TK_UPDATE_PASS 2

#define TK_CMD_LED_ON 3
#define TK_CMD_LED_OFF 4


/* end JB added */

struct touchkey_platform_data {
	int (*power_on)(bool on);
	int (*led_power_on)(bool on);
	int (*init_platform_hw)(void);
	int (*set_clkdata_i2c)(void);
	int (*set_clkdata_gpio)(void);
	int (*suspend) (void);
	int (*resume) (void);
	
	int gpio_scl;
	int gpio_sda;
	int gpio_int;

	bool i2c_gpio;
	int bus_num;
};

#define TK_HAS_FIRMWARE_UPDATE

#endif /* _LINUX_CYPRESS_TOUCHKEY_I2C_H */