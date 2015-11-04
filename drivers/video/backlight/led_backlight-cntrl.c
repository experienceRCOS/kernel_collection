/*
 * drivers/video/backlight/led_backlight-cntrl.c 
 *
 * Copyright (C) 2013 Renesas Mobile Corp.
 * All rights reserved.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/leds.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <mach/r8a7373.h>

#include <linux/hrtimer.h>
#include <linux/kthread.h>
#include <linux/wakelock.h>

#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

#include <linux/tpu_pwm.h>
#include <linux/module.h>
#include <linux/mfd/tc3589x.h>
#include <linux/backlight.h>
#include<linux/led_backlight-cntrl.h>
#include<mach/gpio.h>

/* Functions prototype */
static int  set_backlight_brightness(struct backlight_device *bd);
static int control_led_backlight(int value);
static __init int led_dev_init(void);
/* Macro */

#define TPU_CHANNEL "TPU0TO3"

#define CLK_SOURCE_CP 0 /* CP clock Common Peripheral*/
#define CLK_SOURCE_CP_DIV4 1 /* CP/4 */
#define CLK_SOURCE_CP_DIV16 2 /* CP/16 */
#define CLK_SOURCE_CP_DIV64 3 /* CP/64 */

/* Global variables */
static struct wake_lock wakelock;
static const int periodical_cycle = 260; /* PWM periodic cycle */
static const int backlight_brightness = 192; /* Initial value
						for LCD backlight */
static int gpio_pin;
DEFINE_MUTEX(led_mutex);  /* Initialize mutex */

/*
 * set_backlight_brightness: Control LCD backlight
 * @led_cdev: Pointer to LCD backlight device
 * @value: Brightness value
 * return: None
 */
static int  set_backlight_brightness(struct backlight_device *bd)
{
	int value = bd->props.brightness;
	control_led_backlight(value);
	return 0;
}

/*
 * control_led_backlight: Control LCD backlight using PWM signal
 * @value: Brightness value
 * return:
 *   + 0: Success
 *   + Others: Errors of TPU functions
 */
static int control_led_backlight(int  value)
{
	int ret = 0;
	int duty_cycle;
	static void *handle;
	printk("control_led_backlight %d\n", value);
	/* Calculate duty cycle basing on brightness value */
	duty_cycle = (periodical_cycle + 1) -
			(value * (periodical_cycle + 1) / 255);
	/* Enable PWM signal */
	if (value) {
		/* Open TPU channel to use, clock source is CP/4 */
		if (!handle) {
			gpio_free(gpio_pin);
			gpio_request(gpio_pin, NULL);
			gpio_direction_none_port(gpio_pin);
#ifdef CONFIG_TPU_PWM
			ret = tpu_pwm_open(TPU_CHANNEL, CLK_SOURCE_CP, &handle);

			if (ret) {
				printk(KERN_ERR "tpu_pwm_open() %d\n", ret);
				return ret;
			}
#endif
		}

#ifdef CONFIG_TPU_PWM
		/* Enable PWM signal with specific duty cycle and periodic cycle */
		ret = tpu_pwm_enable(handle, TPU_PWM_START,
					duty_cycle, periodical_cycle);
#endif
		if (ret) {
			return ret;
		}
		return ret;
	} else {
		if (handle) {
#ifdef CONFIG_TPU_PWM
			/* Disable PWM signal */
			ret = tpu_pwm_enable(handle, TPU_PWM_STOP,
						duty_cycle, periodical_cycle);
			if (ret) {
				printk(KERN_ERR "Disable PWM signal %d\n", ret);
				return ret;
			}
			/* Close TPU channel */
			ret = tpu_pwm_close(handle);

			if (ret) {
				printk(KERN_ERR "tpu_pwm_close() %d\n", ret);
				return ret;
			}
#endif
			handle = NULL;
		}

		return ret;
	}
}


static struct backlight_ops led_backlight_ops = {
        .update_status  = set_backlight_brightness,
};

/*probe*/
static int __init led_probe(struct platform_device *pdev)
{
	struct platform_led_backlight_data *data = pdev->dev.platform_data;      
	struct backlight_device *bl;
        struct backlight_properties props;

	memset(&props, 0, sizeof(struct backlight_properties));
	gpio_pin = data->gpio_port;
        props.max_brightness = data->max_brightness;
        props.type = BACKLIGHT_PLATFORM;
	bl = backlight_device_register("panel", &pdev->dev,
			pdev, &led_backlight_ops, &props);
        if (IS_ERR(bl)) {
                dev_err(&pdev->dev, "failed to register backlight\n");
                return 0;
        }
	bl->props.max_brightness = data->max_brightness;
	bl->props.brightness = data->dft_brightness;
	platform_set_drvdata(pdev, bl);
	control_led_backlight(backlight_brightness);
	return 0;
}


static struct platform_driver led_drv = {
	.driver = {
		.name = "led-backlight",
	},
};

/*
 * led_dev_init: Init LED common driver.
 * Parameter: None
 * return:
 *   + 0: Success
 *   + Others: Error of platform_driver_probe function
 */
static __init int led_dev_init(void)
{
	printk(" led_init\n");
	wake_lock_init(&wakelock, WAKE_LOCK_SUSPEND, "led_wakelock");
	return platform_driver_probe(&led_drv, led_probe);
}

late_initcall(led_dev_init);
MODULE_DESCRIPTION("led device");
MODULE_LICENSE("GPL");
