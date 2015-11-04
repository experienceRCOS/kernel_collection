/*
 * Copyright (C) 2012 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/init.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/irq.h>
#include <linux/rfkill.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/serial_core.h>
#include <mach/r8a7373.h>
//#include <asm/mach-types.h>
#include <mach/board-bluetooth-broadcom.h>

#define BT_REG_GPIO GPIO_PORT268
#define BT_WAKE_GPIO GPIO_PORT262
#define BT_HOST_WAKE_GPIO GPIO_PORT272
#if defined (CONFIG_BT_BCM4330) || defined (CONFIG_BT_BCM4343)
/*4330,4334 has one more GPIO used to RESET Control*/
#define BT_RESET_GPIO GPIO_PORT15
#endif

#define BT_LPM_ENABLE
/*For BCM4330,BCM4343,BCM4334 */
struct platform_device broadcom_bluetooth_device = {
	.name = "broadcom_bluetooth",
	.id = -1,
};

//Not used. Registering broadcom_bluetooth_device is set in board-model.c
void __init add_broadcom_bluetooth_device(void)
{
	int ret = platform_device_register(&broadcom_bluetooth_device);
	if (ret)
		pr_err("%s failed to add bcm_bt_rfkill_data %d", __func__, ret);
}

static struct rfkill *bt_rfkill;
static bool bt_enabled;

#ifdef BT_LPM_ENABLE
struct bcm_bt_lpm {
	int wake;
	int host_wake;

	struct hrtimer enter_lpm_timer;
	ktime_t enter_lpm_delay;

	struct uart_port *uport;

	struct wake_lock wake_lock;
	char wake_lock_name[100];
} bt_lpm;
#endif

static int broadcom_bt_rfkill_set_power(void *data, bool blocked)
{
	// rfkill_ops callback. Turn transmitter on when blocked is false
	static int rfkill_pwr_init = 1;
	printk(KERN_DEBUG "%s: %s\n", __func__, (blocked ? "off" : "on" ));

	if (!blocked) {
#if defined (CONFIG_BT_BCM4330) || defined (CONFIG_BT_BCM4343)
		gpio_set_value(BT_RESET_GPIO, 1);
#endif
		gpio_set_value(BT_REG_GPIO, 1);
		if (rfkill_pwr_init) {
                       msleep(50);
                       gpio_set_value(BT_REG_GPIO, 0);
                       msleep(50);
                       gpio_set_value(BT_REG_GPIO, 1);
                       rfkill_pwr_init = 0;
		}
	} else {
		gpio_set_value(BT_REG_GPIO, 0);
#if defined (CONFIG_BT_BCM4330) || defined (CONFIG_BT_BCM4343)
		gpio_set_value(BT_RESET_GPIO, 0);
#endif
	}

	bt_enabled = !blocked;

	return 0;
}

static const struct rfkill_ops broadcom_bt_rfkill_ops = {
	.set_block = broadcom_bt_rfkill_set_power,
};

static void set_wake_locked(int wake)
{
//	printk(KERN_DEBUG "%s: %s\n", __func__, (wake ? "lock" : "unlock"));
	bt_lpm.wake = wake;

	gpio_set_value(BT_WAKE_GPIO, wake);
	if (!wake)
		wake_unlock(&bt_lpm.wake_lock);
        else if(wake)
		wake_lock(&bt_lpm.wake_lock);
}

static enum hrtimer_restart enter_lpm(struct hrtimer *timer) {
	unsigned long flags;
	spin_lock_irqsave(&bt_lpm.uport->lock, flags);
	set_wake_locked(0);
	spin_unlock_irqrestore(&bt_lpm.uport->lock, flags);

	return HRTIMER_NORESTART;
}

#ifdef BT_LPM_ENABLE
/* 
  Assert BT WAKE UP. 
  This line will be deasserted just after lpm timer is elapsed(enter_lpm_delay).
  TODO : enter_lpm_delay is 1 sec now, so should be modified  
*/
void bcm_bt_lpm_exit_lpm_locked(struct uart_port *uport) {
//	printk("bcm_bt_lpm_exit_lpm_locked uport : 0x%p\n", uport);
	bt_lpm.uport = uport;

	hrtimer_try_to_cancel(&bt_lpm.enter_lpm_timer);

	set_wake_locked(1);

	hrtimer_start(&bt_lpm.enter_lpm_timer, bt_lpm.enter_lpm_delay,
		HRTIMER_MODE_REL);
}
#endif
EXPORT_SYMBOL(bcm_bt_lpm_exit_lpm_locked);

static void update_host_wake_locked(int host_wake)
{
	printk(KERN_DEBUG "%s: %s\n", __func__, (host_wake ? "lock" : "unlock"));

	if (host_wake == bt_lpm.host_wake)
		return;

	bt_lpm.host_wake = host_wake;

	if (host_wake) {
		wake_lock(&bt_lpm.wake_lock);
	} else {
		// Take a timed wakelock, so that upper layers can take it.
		// The chipset deasserts the hostwake lock, when there is no
		// more data to send.
		wake_lock_timeout(&bt_lpm.wake_lock, HZ/2);
	}
}

static irqreturn_t host_wake_isr(int irq, void *dev)
{
	int host_wake;
	unsigned long flags;

	printk(KERN_DEBUG "%s: irq number = %i\n", __func__, irq);

	host_wake = gpio_get_value(BT_HOST_WAKE_GPIO);
	irq_set_irq_type(irq, host_wake ? IRQF_TRIGGER_LOW : IRQF_TRIGGER_HIGH);

	if (!bt_lpm.uport) {
		bt_lpm.host_wake = host_wake;
		return IRQ_HANDLED;
	}

	spin_lock_irqsave(&bt_lpm.uport->lock, flags);
	update_host_wake_locked(host_wake);
	spin_unlock_irqrestore(&bt_lpm.uport->lock, flags);

	return IRQ_HANDLED;
}

#ifdef BT_LPM_ENABLE
static int bcm_bt_lpm_init(struct platform_device *pdev)
{
	int irq;
	int ret;
	int rc;

	printk(KERN_ALERT "%s: Enter\n", __func__);

	rc = gpio_request(BT_WAKE_GPIO, "broadcom_wake_gpio");
	if (unlikely(rc)) {
		return rc;
	}

	rc = gpio_request(BT_HOST_WAKE_GPIO, "broadocm_host_wake_gpio");
	if (unlikely(rc)) {
		gpio_free(BT_WAKE_GPIO);
		return rc;
	}

	hrtimer_init(&bt_lpm.enter_lpm_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	bt_lpm.enter_lpm_delay = ktime_set(1, 0);  /* 1 sec */
	bt_lpm.enter_lpm_timer.function = enter_lpm;

	bt_lpm.host_wake = 0;

	irq = gpio_to_irq(BT_HOST_WAKE_GPIO);
	ret = request_irq(irq, host_wake_isr, IRQF_TRIGGER_HIGH,
		"bt host_wake", NULL);
	if (ret) {
		gpio_free(BT_WAKE_GPIO);
		gpio_free(BT_HOST_WAKE_GPIO);
		return ret;
	}

	ret = irq_set_irq_wake(irq, 1);
	if (ret) {
		gpio_free(BT_WAKE_GPIO);
		gpio_free(BT_HOST_WAKE_GPIO);
		return ret;
	}

	gpio_direction_output(BT_WAKE_GPIO, 0);
	gpio_direction_input(BT_HOST_WAKE_GPIO);

	*((volatile u8 *)BT_WAKE_GPIO_CR) = 0x10; /* Config BT IRQ with output*/

	snprintf(bt_lpm.wake_lock_name, sizeof(bt_lpm.wake_lock_name),
			"BTLowPower");
	wake_lock_init(&bt_lpm.wake_lock, WAKE_LOCK_SUSPEND,
			 bt_lpm.wake_lock_name);

	printk("%s: Done\n", __func__);

	return 0;
}
#endif

static int broadcom_bluetooth_probe(struct platform_device *pdev)
{
	int ret = 0;
	int rc = -EINVAL;

	printk(KERN_ALERT "%s: Enter\n", __func__);

	rc = gpio_request(BT_REG_GPIO, "broadcom_shutdown_gpio");
	if (unlikely(rc))
		return rc;
#if defined (CONFIG_BT_BCM4330) || defined (CONFIG_BT_BCM4343)
	rc = gpio_request(BT_RESET_GPIO, "broadcom_reset_gpio");
	if (unlikely(rc))
		return rc;
#endif
	gpio_direction_output(BT_REG_GPIO, 1);
#if defined (CONFIG_BT_BCM4330) || defined (CONFIG_BT_BCM4343)
	gpio_direction_output(BT_RESET_GPIO, 0);
	//gpio_pull_off_port(BT_RESET_GPIO);
#endif
	bt_rfkill = rfkill_alloc("broadcom Bluetooth", &pdev->dev,
				RFKILL_TYPE_BLUETOOTH, &broadcom_bt_rfkill_ops,
				NULL);

	if (unlikely(!bt_rfkill)) {
		gpio_free(BT_REG_GPIO);
#if defined (CONFIG_BT_BCM4330) || defined (CONFIG_BT_BCM4343)
		gpio_free(BT_RESET_GPIO);
#endif
		return -ENOMEM;
	}

	printk(KERN_ALERT "%s: RFKILL Alloc Done\n", __func__);

	rfkill_init_sw_state(bt_rfkill, 0);//Set BT_EN default to low
	rc = rfkill_register(bt_rfkill);

	if (unlikely(rc)) {
		rfkill_destroy(bt_rfkill);
		gpio_free(BT_REG_GPIO);
#if defined (CONFIG_BT_BCM4330) || defined (CONFIG_BT_BCM4343)
		gpio_free(BT_RESET_GPIO);
#endif
		return -1;
	}

	printk(KERN_ALERT "%s: RFKILL registered\n", __func__);

	rfkill_set_states(bt_rfkill, true, false);
	broadcom_bt_rfkill_set_power(NULL, true);

#ifdef BT_LPM_ENABLE
	ret = bcm_bt_lpm_init(pdev);
	if (ret) {
		rfkill_unregister(bt_rfkill);
		rfkill_destroy(bt_rfkill);

		gpio_free(BT_REG_GPIO);
#if defined (CONFIG_BT_BCM4330) || defined (CONFIG_BT_BCM4343)
		gpio_free(BT_RESET_GPIO);
#endif
		printk(KERN_ERR "%s: bcm_lpm_init Failed ! err = %d\n", __func__, ret);
	}
#endif
	printk(KERN_ALERT "%s: Done\n", __func__);

	return ret;
}

static int broadcom_bluetooth_remove(struct platform_device *pdev)
{
	printk("%s: Enter\n", __func__);

	rfkill_unregister(bt_rfkill);
	rfkill_destroy(bt_rfkill);

	gpio_free(BT_REG_GPIO);
	gpio_free(BT_WAKE_GPIO);
	gpio_free(BT_HOST_WAKE_GPIO);
#if defined (CONFIG_BT_BCM4330) || defined (CONFIG_BT_BCM4343)
	gpio_free(BT_RESET_GPIO);
#endif
	wake_lock_destroy(&bt_lpm.wake_lock);
	return 0;
}

int broadcom_bluetooth_suspend(struct platform_device *pdev, pm_message_t state)
{
	int irq = gpio_to_irq(BT_HOST_WAKE_GPIO);
	int host_wake;

	printk(KERN_ALERT "%s: Enter\n", __func__);

	disable_irq(irq);
	host_wake = gpio_get_value(BT_HOST_WAKE_GPIO);

	if (host_wake) {
		enable_irq(irq);
		return -EBUSY;
	}

	return 0;
}

int broadcom_bluetooth_resume(struct platform_device *pdev)
{
	int irq = gpio_to_irq(BT_HOST_WAKE_GPIO);

	printk(KERN_ALERT "%s: Enter\n", __func__);

	enable_irq(irq);
	return 0;
}

static struct platform_driver broadcom_bluetooth_platform_driver = {
	.probe = broadcom_bluetooth_probe,
	.remove = broadcom_bluetooth_remove,
	.suspend = broadcom_bluetooth_suspend,
	.resume = broadcom_bluetooth_resume,
	.driver = {
		   .name = "broadcom_bluetooth",
		   .owner = THIS_MODULE,
		   },
};

int __init broadcom_bluetooth_init(void)
{
	printk("%s: Enter\n", __func__);

	bt_enabled = false;
	return platform_driver_register(&broadcom_bluetooth_platform_driver);
}

static void __exit broadcom_bluetooth_exit(void)
{
	platform_driver_unregister(&broadcom_bluetooth_platform_driver);
}

module_init(broadcom_bluetooth_init);
module_exit(broadcom_bluetooth_exit);

MODULE_ALIAS("platform:broadcom_bt");
MODULE_DESCRIPTION("broadcom_bluetooth");
MODULE_LICENSE("GPL");
