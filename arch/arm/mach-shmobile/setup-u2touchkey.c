/*
 * arch/arm/mach-shmobile/setup-u2touchkey.c
 *
 * Copyright (C) 2012 Renesas Mobile Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */
#include <linux/kernel.h>
#if 1 /*!defined(CONFIG_MACH_HEATLTE) && !defined(CONFIG_KEYBOARD_CYPRESS_TOUCH)*/
#include <linux/i2c/touchkey_i2c.h>
#else
#include <linux/i2c/cypress_touchkey.h>
#endif
#include <mach/r8a7373.h>
#include <mach/irqs.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <mach/gpio.h>

#if defined(CONFIG_MACH_HEATLTE) && defined(CONFIG_KEYBOARD_CYPRESS_TOUCH)
#define CYPRESS_TOUCHKEY_ADR		0x20
#define CYPRESS_TOUCHKEY_IRQ_GPIO	GPIO_PORT29 /*29*/
#define TOUCH_1V8_EN			GPIO_PORT21 /*21*/
#define TOUCH_1V8_ON				1
#define TOUCH_1V8_OFF				0
#define CYPRESS_TOUCHKEY_OK_KEY		GPIO_PORT25//25
#define CYPRESS_TOUCHKEY_I2C_BUSNUM	5
#else
static struct i2c_board_info i2c_touchkey[];
#endif

extern unsigned int system_rev;

#if !defined(CONFIG_MACH_HEATLTE) && !defined(CONFIG_KEYBOARD_CYPRESS_TOUCH)
void touchkey_init_hw(void)
{

}
#else
void touchkey_init_hw(void)
{

	int ret = 0;
	/*TouchKey regulator Configuration*/
	printk("%s - configure GPIO%d for TouchKey Regulator\n",__func__,TOUCH_1V8_EN);
	ret = gpio_request(TOUCH_1V8_EN, NULL);
	if (ret) {
		printk("%s - gpio_request:TOUCH_1V8_EN => %d\n",__func__, ret);
		gpio_free(TOUCH_1V8_EN);
		ret = gpio_request(TOUCH_1V8_EN, NULL);
	}
	ret = gpio_direction_output(TOUCH_1V8_EN,TOUCH_1V8_OFF);
    if (ret) {
		printk("%s - gpio_direction_output:TOUCH_1V8_EN => %d\n",__func__, ret);
	}
	gpio_pull_up_port(TOUCH_1V8_EN);	
	/*TouchKey Interrupt  Configuration*/
	printk("%s - configure GPIO%d for TouchKey INT\n",__func__,CYPRESS_TOUCHKEY_IRQ_GPIO);
	ret = gpio_request(CYPRESS_TOUCHKEY_IRQ_GPIO, NULL);
	if (ret) {
		printk("%s - gpio_request:CYPRESS_TOUCHKEY_IRQ_GPIO => %d\n",__func__, ret);
		gpio_free(CYPRESS_TOUCHKEY_IRQ_GPIO);
		ret = gpio_request(CYPRESS_TOUCHKEY_IRQ_GPIO, NULL);
	}
	ret = gpio_direction_input(CYPRESS_TOUCHKEY_IRQ_GPIO);
	if (ret) {
		printk("%s - gpio_direction_input:CYPRESS_TOUCHKEY_IRQ_GPIO => %d\n",__func__, ret);
	}
	gpio_pull_up_port(CYPRESS_TOUCHKEY_IRQ_GPIO);
}

#endif
static int touchkey_suspend(void)
{
#if 0
	struct regulator *regulator;
	regulator = regulator_get(NULL, TK_REGULATOR_NAME);
	if (IS_ERR(regulator))
		return 0;
	if (regulator_is_enabled(regulator))
		regulator_force_disable(regulator);

	regulator_put(regulator);
#endif
	return 1;
}

static int touchkey_resume(void)
{
#if 0
	struct regulator *regulator;
	regulator = regulator_get(NULL, TK_REGULATOR_NAME);
	if (IS_ERR(regulator))
		return 0;
	regulator_enable(regulator);
	regulator_put(regulator);
#endif
	return 1;
}

#if !defined(CONFIG_MACH_HEATLTE) && !defined(CONFIG_KEYBOARD_CYPRESS_TOUCH)
static int touchkey_power_on(bool on)
{
	int ret;
	if (on)
		ret = touchkey_resume();
	else
		ret = touchkey_suspend();

	return ret;
}
#else
static int touchkey_power_on(bool on)
{
	struct regulator *regulator;
	int ret;

	if(on){
		regulator = regulator_get(NULL, "key_led");
		if (IS_ERR(regulator))
			return 0;
		ret = regulator_enable(regulator);
		printk("%s - regulator_enable: %d\n",__func__, ret);
		regulator_put(regulator);		
		printk(KERN_INFO "%s :ON\n",__func__);
		/* As we are setting this GPIO21 in dev-touchpanel.c*/
		/*so commented out here*/
		/*gpio_set_value(TOUCH_1V8_EN,TOUCH_1V8_ON);*/
		if(system_rev>=3)	
		gpio_set_value(TOUCH_1V8_EN,TOUCH_1V8_ON);
		
	}
	else{
		printk(KERN_INFO "%s :OFF\n",__func__);
		/* As we are setting this GPIO21 in dev-touchpanel.c*/
		/*so commented out here*/
		/*gpio_set_value(TOUCH_1V8_EN,TOUCH_1V8_OFF);*/
		if(system_rev>=3)
		gpio_set_value(TOUCH_1V8_EN,TOUCH_1V8_OFF);
		regulator = regulator_get(NULL, "key_led");
		if (IS_ERR(regulator))
			return 0;
		if (regulator_is_enabled(regulator))
		{
			ret = regulator_disable(regulator);
			printk("%s - regulator_disable: %d\n",__func__, ret);
		}
		regulator_put(regulator);		
	}

	return 0;
}

#endif

#if !defined(CONFIG_MACH_HEATLTE) && !defined(CONFIG_KEYBOARD_CYPRESS_TOUCH)
static int touchkey_led_power_on(bool on)
{
	return 1;
}

#else
static int touchkey_led_power_on(bool on)
{
#if 0
	struct regulator *regulator;
	int ret;

	if (on) {
		regulator = regulator_get(NULL, "key_led");
		if (IS_ERR(regulator))
			return 0;
		ret = regulator_enable(regulator);
		printk(KERN_INFO "%s - regulator_enable: %d\n",__func__, ret);
		regulator_put(regulator);
	} else {
		regulator = regulator_get(NULL, "key_led");
		if (IS_ERR(regulator))
			return 0;
		if (regulator_is_enabled(regulator))
		{
			ret = regulator_disable(regulator);
			printk(KERN_INFO "%s - regulator_disable: %d\n",__func__, ret);
		}
		regulator_put(regulator);
	}
#endif	
	printk(KERN_ERR "%s: %s\n",__func__,(on)?"on":"off");

	return 1;
}
#endif

static int touchkey_set_clkdata_gpio(void)
{
	int rc;
	gpio_free(510);
	gpio_free(514);
	gpio_free(86);
	gpio_free(87);	
	rc = gpio_request(86, "touchkey_scl");
	if (rc)
		pr_err("%s: gpio_request SCL fail [%d]\n",
				__func__, rc);
	rc = gpio_request(87, "touchkey_sda");
	if (rc)
		pr_err("%s: gpio_request SDA fail [%d]\n",
				__func__, rc);	
	
	printk("%s: \n", __func__);
	return 1;
}

static int touchkey_set_clkdata_i2c(void)
{
	int rc;
	gpio_free(86);
	gpio_free(87);
	gpio_free(510);
	gpio_free(514);	
	rc = gpio_request(510, "touchkey_scl");
	if (rc)
		pr_err("%s: gpio_request SCL fail [%d]\n",
				__func__, rc);
	rc = gpio_request(514, "touchkey_sda");
	if (rc)
		pr_err("%s: gpio_request SDA fail [%d]\n",
				__func__, rc);	
	printk("%s: \n", __func__);
	return 1;
}

#if !defined(CONFIG_MACH_HEATLTE) && !defined(CONFIG_KEYBOARD_CYPRESS_TOUCH)
#define TCKEY_SDA 27
#define TCKEY_SCL 26
static struct touchkey_platform_data touchkey_pdata = {
	.gpio_sda = TCKEY_SDA,	/* To do to set gpio */
	.gpio_scl = TCKEY_SCL,	/* To do to set gpio */
	.gpio_int = (int)NULL,	/* To do to set gpio */
	.init_platform_hw = touchkey_init_hw,
	.suspend = touchkey_suspend,
	.resume = touchkey_resume,
	.power_on = touchkey_power_on,
	.led_power_on = touchkey_led_power_on,
};


static struct i2c_board_info i2c_touchkey[] = {
	{
		I2C_BOARD_INFO("sec_touchkey", 0x20),
		.platform_data = &touchkey_pdata,
		.irq = irq_pin(43),
	},

};
#else
static struct touchkey_platform_data touchkey_pdata = {
	.i2c_gpio = false,	/* To do to set gpio */
	.bus_num = CYPRESS_TOUCHKEY_I2C_BUSNUM,
	.gpio_sda = 87,	/* To do to set gpio */
	.gpio_scl = 86,	/* To do to set gpio */	
	.gpio_int = CYPRESS_TOUCHKEY_IRQ_GPIO,	/* To do to set gpio */
	.init_platform_hw = touchkey_init_hw,
	.suspend = touchkey_suspend,
	.resume = touchkey_resume,
	.power_on = touchkey_power_on,
	.led_power_on = touchkey_led_power_on,
	.set_clkdata_i2c = touchkey_set_clkdata_i2c,
	.set_clkdata_gpio = touchkey_set_clkdata_gpio,	
};

static struct i2c_board_info touchkey_i2c_devices[] = {
	{
		I2C_BOARD_INFO("sec_touchkey_driver", CYPRESS_TOUCHKEY_ADR),
		.platform_data = &touchkey_pdata,
		.irq = irq_pin(CYPRESS_TOUCHKEY_IRQ_GPIO ),
	},
};
#endif

#if !defined(CONFIG_MACH_HEATLTE) && !defined(CONFIG_KEYBOARD_CYPRESS_TOUCH)
int __init touchkey_i2c_register_board_info(int busnum)
{
	return i2c_register_board_info(busnum, i2c_touchkey, ARRAY_SIZE(i2c_touchkey));
}
#else
int __init touchkey_i2c_register_board_info(void)
{
	printk(KERN_INFO "%s - register touchkey_i2c_device\n",__func__);
	return i2c_register_board_info(touchkey_pdata.bus_num, touchkey_i2c_devices, ARRAY_SIZE(touchkey_i2c_devices));
}
#endif
