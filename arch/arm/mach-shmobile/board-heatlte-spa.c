/*
 * arch/arm/mach-shmobile/setup-u2spa.c
 *
 * Device initialization for spa_power and spa_agent.c
 *
 * Copyright (C) 2013 Renesas Mobile Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <mach/setup-u2spa.h>
#include <linux/wakelock.h>
#include <mach/r8a7373.h>
#include <mach/gpio.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>

#if defined(CONFIG_SEC_CHARGING_FEATURE)
#include <linux/spa_power.h>
#include <linux/spa_agent.h>

extern unsigned int system_rev;

/* Samsung charging feature
 +++ for board files, it may contain changeable values */
 

static struct spa_temp_tb batt_temp_tb[] = {
	{4076, -250},		/* -25 */
	{4066, -200},		/* -20 */
	{4056, -150},		/* -15 */
	{4040, -100},		/* -10 */
	{3933, -50},		/* -5  */
	{3849,  0},			/* 0    */
	{3573,  50},			/* 5    */
	{2967,  100},		/* 10  */
	{2687,  150},		/* 15  */
	{2039,  200},		/* 20  */
	{1723,  250},		/* 25  */
	{1367,  300},		/* 30  */
	{1157,  350},		/* 35  */
	{968,  400},		/* 40  */
	{868,  450},		/* 45  */
	{694,  500},		/* 50  */
	{609,  550},		/* 55  */
	{526,  600},		/* 60  */
	{445,  650},			/* 65  */
	{373,  700},			/* 70  */
	{300,  750},			/* 75  */
	{250,  800},			/* 80  */
	{230,  850},			/* 85  */
	{220,  900},			/* 90  */
};



static struct spa_temp_tb batt_temp_tb_REV02[] = {
	{2611, -250},		/* -25 */
	{2481, -200},		/* -20 */
	{1968, -150},		/* -15 */
	{1426, -100},		/* -10 */
	{1288, -50},		/* -5  */
	{962,  0},			/* 0    */
	{837,  50},			/* 5    */
	{641,  100},		/* 10  */
	{512,  150},		/* 15  */
	{439,  200},		/* 20  */
	{365,  250},		/* 25  */
	{305,  300},		/* 30  */
	{253,  350},		/* 35  */
	{210,  400},		/* 40  */
	{181,  450},		/* 45  */
	{150,  500},		/* 50  */
	{127,  550},		/* 55  */
	{112,  600},		/* 60  */
	{97,  650},			/* 65  */
	{79,  700},			/* 70  */
	{70,  750},			/* 75  */
	{60,  800},			/* 80  */
	{50,  850},			/* 85  */
	{40,  900},			/* 90  */
};

struct spa_power_data spa_power_pdata = {
	.charger_name = "spa_agent_chrg",
	.batt_cell_name = "SDI_SDI",
#if defined(CONFIG_SPA_SUPPLEMENTARY_CHARGING)
	.eoc_current = 150,
	.recharging_eoc = 75,
	.backcharging_time = 30,
	.lpm_compensation_eoc_current = 50,
#else
	.eoc_current = 75,
#endif
	.recharge_voltage = 4300,
	.charging_cur_usb = 500,
	.charging_cur_cdp_usb = 1000,
	.charging_cur_wall = 1000,

	.suspend_temp_hot = 600,
	.recovery_temp_hot = 460,
	.suspend_temp_cold = -50,
	.recovery_temp_cold = 0,

	.charge_timer_limit = CHARGE_TIMER_6HOUR,
	.regulated_vol = 4350,
	.batt_temp_tb = &batt_temp_tb[0],
	.batt_temp_tb_len = ARRAY_SIZE(batt_temp_tb),
};


static struct platform_device spa_power_device = {
	.name = "spa_power",
	.id = -1,
	.dev.platform_data = &spa_power_pdata,
};

static struct platform_device spa_agent_device = {
	.name = "spa_agent",
	.id = -1,
};

static int spa_power_init(void)
{
	int ret;
	ret = platform_device_register(&spa_agent_device);
	if (ret < 0)
		return ret;
	ret = platform_device_register(&spa_power_device);
	if (ret < 0)
		return ret;

	if(system_rev >= 4){	
		spa_power_pdata.batt_temp_tb= &batt_temp_tb_REV02[0];
		spa_power_pdata.batt_temp_tb_len =  ARRAY_SIZE(batt_temp_tb_REV02);
	}else{
		spa_power_pdata.batt_temp_tb = &batt_temp_tb[0];
		spa_power_pdata.batt_temp_tb_len =  ARRAY_SIZE(batt_temp_tb);
	}
		
	return 0;
}
#endif

void spa_init(void)
{
#if defined(CONFIG_CHARGER_SMB328A)
	gpio_request(GPIO_PORT19, NULL);
	gpio_direction_input(GPIO_PORT19);
	gpio_pull_up_port(GPIO_PORT19);
#endif

#if defined(CONFIG_BATTERY_BQ27425)
	gpio_request(GPIO_PORT105, NULL);
	gpio_direction_input(GPIO_PORT105);
	gpio_pull_up_port(GPIO_PORT105);
#endif

#if defined(CONFIG_SEC_CHARGING_FEATURE)
	spa_power_init();
#endif
}
