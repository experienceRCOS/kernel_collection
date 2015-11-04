
/*****************************************************************************
 *  Copyright 2001 - 2014 Broadcom Corporation.  All rights reserved.
 *
 *  Unless you and Broadcom execute a separate written software license
 *  agreement governing use of this software, this software is licensed to you
 *  under the terms of the GNU General Public License version 2, available at
 *  http://www.gnu.org/licenses/old-license/gpl-2.0.html (the "GPL").
 *
 *  Notwithstanding the above, under no circumstances may you combine this
 *  software in any way with any other Broadcom software provided under a
 *  license other than the GPL, without Broadcom's express prior written
 *  consent.
 *
 *****************************************************************************/
#include <mach/setup-u2spa.h>
#include <linux/wakelock.h>
#include <mach/r8a7373.h>
#include <mach/gpio.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/bcm.h>

#include <linux/d2153/core.h>
#include <linux/d2153/d2153_battery.h>
#include <linux/d2153/d2153_reg.h>
#include <linux/d2153/hwmon.h>

static int chrgr_curr_lmt[] = {
	[POWER_SUPPLY_TYPE_UNKNOWN] = 0,
	[POWER_SUPPLY_TYPE_BATTERY] = 0,
	[POWER_SUPPLY_TYPE_UPS] = 0,
	[POWER_SUPPLY_TYPE_MAINS] = 1300,
	[POWER_SUPPLY_TYPE_USB] = 500,
	[POWER_SUPPLY_TYPE_USB_DCP] = 1300,
	[POWER_SUPPLY_TYPE_USB_CDP] = 500,
	[POWER_SUPPLY_TYPE_USB_ACA] = 500,
};

struct bcm_chrgr_pdata chrgr_pdata = {
	.chrgr_curr_lmt_tbl = chrgr_curr_lmt,
};

static struct platform_device bcm_chrgr_device = {
	.name = "bcm_chrgr",
	.id = -1,
	.dev.platform_data = &chrgr_pdata,
};

struct bcm_accessories_pdata {
	int bcm_accy;
};
struct bcm_accessories_pdata bcm_accy_pdata = {
	/* Just passing platform data. It is not used*/
	.bcm_accy = 0xDCBA,
};

static struct platform_device bcm_accy_device = {
	.name = "bcm_accy",
	.id = -1,
	.dev.platform_data = &bcm_accy_pdata,
};
#if defined(CONFIG_MACH_AMETHYST)
/* Based on ERTJ0EG103FA NTC's R-T curve data*/
/* TODO: These values are to be used in d2153_battery.c also */
static struct d2153_ntc_temp_tb batt_ntc_temp_tb[] = {
	{1486, -250},		/* -25 */
	{1153, -200},		/* -20 */
	{902, -150},		/* -15 */
	{712, -100},		/* -10 */
	{566, -50},		/* -5  */
	{453,  0},		/* 0   */
	{365,  50},		/* 5   */
	{296,  100},		/* 10  */
	{241,  150},		/* 15  */
	{198,  200},		/* 20  */
	{164,  250},		/* 25  */
	{136,  300},		/* 30  */
	{114,  350},		/* 35  */
	{95,  400},		/* 40  */
	{81,  450},		/* 45  */
	{68,  500},		/* 50  */
	{58,  550},		/* 55  */
	{50,  600},		/* 60  */
	{43,  650},		/* 65  */
	{37,  700},		/* 70  */
};

struct d2153_battery_platform_data pbat_pdata = {
	.battery_technology = POWER_SUPPLY_TECHNOLOGY_LION,
	.battery_capacity = BAT_CAPACITY_2200MA,
	.vf_lower = 250,
	.vf_upper = 510,
	.bat_temp_adc = D2153_ADC_TEMPERATURE_1,
	.batt_ntc_temp_tb = &batt_ntc_temp_tb[0],
	.batt_ntc_temp_tb_len = ARRAY_SIZE(batt_ntc_temp_tb),
	.flags = 0,
	.recharge_voltage = 4300,
	.regulated_vol = 4350,
	.poll_rate = 300,
	.fake_room_temp_adc = 164,
	.adc_conv_mode = D2153_ADC_IN_AUTO,
	.adc_config_mode = D2153_ADC_FAST,
};

#elif defined(CONFIG_MACH_HEATLTE) || defined(CONFIG_MACH_AFYONLTE) ||\
	defined(CONFIG_MACH_VIVALTOLTE)

static struct d2153_ntc_temp_tb batt_ntc_temp_tb[] = {
	{3000, -250},		/* -25 */
	{2350, -200},		/* -20 */
	{1850, -150},		/* -15 */
	{1480, -100},		/* -10 */
	{1180, -50},		/* -5  */
	{945,  0},		/* 0    */
	{765,  50},		/* 5    */
	{620,  100},		/* 10  */
	{510,  150},		/* 15  */
	{420,  200},		/* 20  */
	{345,  250},		/* 25  */
	{285,  300},		/* 30  */
	{240,  350},		/* 35  */
	{200,  400},		/* 40  */
	{170,  450},		/* 45  */
	{143,  500},		/* 50  */
	{122,  550},		/* 55  */
	{104,  600},		/* 60  */
	{89,  650},		/* 65  */
	{77,  700},		/* 70  */
};

struct d2153_battery_platform_data pbat_pdata = {
	.battery_technology = POWER_SUPPLY_TECHNOLOGY_LION,
	.battery_capacity = BAT_CAPACITY_2100MA,
	.vf_lower = 250,
	.vf_upper = 510,
	.bat_temp_adc = D2153_ADC_TEMPERATURE_1,
	.batt_ntc_temp_tb = &batt_ntc_temp_tb[0],
	.batt_ntc_temp_tb_len = ARRAY_SIZE(batt_ntc_temp_tb),
	.flags = 0,
	.recharge_voltage = 4300,
	.regulated_vol = 4350,
	.poll_rate = 300,
	.fake_room_temp_adc = 345,
	.adc_conv_mode = D2153_ADC_IN_AUTO,
	.adc_config_mode = D2153_ADC_FAST,
};

#elif defined(CONFIG_MACH_LOGANLTE)
static struct d2153_ntc_temp_tb batt_ntc_temp_tb[] = {
	{3000, -250},		/* -25 */
	{2350, -200},		/* -20 */
	{1850, -150},		/* -15 */
	{1480, -100},		/* -10 */
	{1180, -50},		/* -5  */
	{945,  0},		/* 0    */
	{765,  50},		/* 5    */
	{620,  100},		/* 10  */
	{510,  150},		/* 15  */
	{420,  200},		/* 20  */
	{345,  250},		/* 25  */
	{285,  300},		/* 30  */
	{240,  350},		/* 35  */
	{200,  400},		/* 40  */
	{170,  450},		/* 45  */
	{143,  500},		/* 50  */
	{122,  550},		/* 55  */
	{104,  600},		/* 60  */
	{89,  650},		/* 65  */
	{77,  700},		/* 70  */
};

struct d2153_battery_platform_data pbat_pdata = {
	.battery_technology = POWER_SUPPLY_TECHNOLOGY_LION,
	.battery_capacity = BAT_CAPACITY_1800MA,
	.vf_lower = 250,
	.vf_upper = 510,
	.bat_temp_adc = D2153_ADC_TEMPERATURE_1,
	.batt_ntc_temp_tb = &batt_ntc_temp_tb[0],
	.batt_ntc_temp_tb_len = ARRAY_SIZE(batt_ntc_temp_tb),
	.flags = 0,
	.recharge_voltage = 4300,
	.regulated_vol = 4350,
	.poll_rate = 300,
	.fake_room_temp_adc = 345,
	.adc_conv_mode = D2153_ADC_IN_AUTO,
	.adc_config_mode = D2153_ADC_FAST,
};

#elif defined(CONFIG_MACH_P35B)
static struct d2153_ntc_temp_tb batt_ntc_temp_tb[] = {
	{1486, -250},		/* -25 */
	{1153, -200},		/* -20 */
	{902, -150},		/* -15 */
	{712, -100},		/* -10 */
	{566, -50},		/* -5  */
	{453,  0},		/* 0   */
	{365,  50},		/* 5   */
	{296,  100},		/* 10  */
	{241,  150},		/* 15  */
	{198,  200},		/* 20  */
	{164,  250},		/* 25  */
	{136,  300},		/* 30  */
	{114,  350},		/* 35  */
	{95,  400},		/* 40  */
	{81,  450},		/* 45  */
	{68,  500},		/* 50  */
	{58,  550},		/* 55  */
	{50,  600},		/* 60  */
	{43,  650},		/* 65  */
	{37,  700},		/* 70  */
};

struct d2153_battery_platform_data pbat_pdata = {
	.battery_technology = POWER_SUPPLY_TECHNOLOGY_LION,
	.battery_capacity = BAT_CAPACITY_2200MA,
	.vf_lower = 250,
	.vf_upper = 510,
	.bat_temp_adc = D2153_ADC_TEMPERATURE_1,
	.batt_ntc_temp_tb = &batt_ntc_temp_tb[0],
	.batt_ntc_temp_tb_len = ARRAY_SIZE(batt_ntc_temp_tb),
	.flags = 0,
	.recharge_voltage = 4300,
	.regulated_vol = 4350,
	.poll_rate = 300,
	.fake_room_temp_adc = 164,
	.adc_conv_mode = D2153_ADC_IN_AUTO,
	.adc_config_mode = D2153_ADC_FAST,
};

#else

static struct d2153_ntc_temp_tb batt_ntc_temp_tb[] = {
	{3000, -250},		/* -25 */
	{2350, -200},		/* -20 */
	{1850, -150},		/* -15 */
	{1480, -100},		/* -10 */
	{1180, -50},		/* -5  */
	{945,  0},		/* 0    */
	{765,  50},		/* 5    */
	{620,  100},		/* 10  */
	{510,  150},		/* 15  */
	{420,  200},		/* 20  */
	{345,  250},		/* 25  */
	{285,  300},		/* 30  */
	{240,  350},		/* 35  */
	{200,  400},		/* 40  */
	{170,  450},		/* 45  */
	{143,  500},		/* 50  */
	{122,  550},		/* 55  */
	{104,  600},		/* 60  */
	{89,  650},		/* 65  */
	{77,  700},		/* 70  */
};

struct d2153_battery_platform_data pbat_pdata = {
	.battery_technology = POWER_SUPPLY_TECHNOLOGY_LION,
	.battery_capacity = BAT_CAPACITY_2100MA,
	.vf_lower = 250,
	.vf_upper = 510,
	.bat_temp_adc = D2153_ADC_TEMPERATURE_1,
	.batt_ntc_temp_tb = &batt_ntc_temp_tb[0],
	.batt_ntc_temp_tb_len = ARRAY_SIZE(batt_ntc_temp_tb),
	.flags = 0,
	.recharge_voltage = 4300,
	.regulated_vol = 4350,
	.poll_rate = 300,
	.fake_room_temp_adc = 345,
	.adc_conv_mode = D2153_ADC_IN_AUTO,
	.adc_config_mode = D2153_ADC_FAST,
};

#endif

struct adc_cont_in_auto adc_cont_inven[D2153_ADC_CHANNEL_MAX - 1] = {
/* This array is for setting ADC_CONT register about each channel.*/
	/* VBAT_S channel */
	[D2153_ADC_VOLTAGE] = {
		.adc_preset_val = 0,
		.adc_cont_val = (D2153_ADC_AUTO_EN_MASK |
			D2153_AUTO_VBAT_EN_MASK),
		.adc_msb_res = D2153_VDD_RES_VBAT_RES_REG,
		.adc_lsb_res = D2153_ADC_RES_AUTO1_REG,
		.adc_lsb_mask = ADC_RES_MASK_LSB,
	},
	/* TEMP_1 channel */
	[D2153_ADC_TEMPERATURE_1] = {
		.adc_preset_val = D2153_TEMP1_ISRC_EN_MASK,
		.adc_cont_val = (D2153_ADC_AUTO_EN_MASK),
		.adc_msb_res = D2153_TBAT1_RES_TEMP1_RES_REG,
		.adc_lsb_res = D2153_ADC_RES_AUTO1_REG,
		.adc_lsb_mask = ADC_RES_MASK_MSB,
	},
	/*	TEMP_2 channel */
	[D2153_ADC_TEMPERATURE_2] = {
		.adc_preset_val =  D2153_TEMP2_ISRC_EN_MASK,
		.adc_cont_val = (D2153_ADC_AUTO_EN_MASK),
		.adc_msb_res = D2153_TBAT2_RES_TEMP2_RES_REG,
		.adc_lsb_res = D2153_ADC_RES_AUTO3_REG,
		.adc_lsb_mask = ADC_RES_MASK_LSB,
	},
	/* VF channel */
	[D2153_ADC_VF] = {
		.adc_preset_val = D2153_AD4_ISRC_ENVF_ISRC_EN_MASK,
		.adc_cont_val = (D2153_ADC_AUTO_EN_MASK
						| D2153_AUTO_VF_EN_MASK),
		.adc_msb_res = D2153_ADCIN4_RES_VF_RES_REG,
		.adc_lsb_res = D2153_ADC_RES_AUTO2_REG,
		.adc_lsb_mask = ADC_RES_MASK_LSB,
	},
	/* AIN channel */
	[D2153_ADC_AIN] = {
		.adc_preset_val = D2153_AD5_ISRC_EN_MASK,
		.adc_cont_val = (D2153_ADC_AUTO_EN_MASK	|
			D2153_AUTO_AIN_EN_MASK),
		.adc_msb_res = D2153_ADCIN5_RES_AIN_RES_REG,
		.adc_lsb_res = D2153_ADC_RES_AUTO2_REG,
		.adc_lsb_mask = ADC_RES_MASK_MSB
	},
};

struct d2153_hwmon_platform_data d2153_adc_pdata = {
	.adc_cont_inven = adc_cont_inven,
	.adc_cont_inven_len = ARRAY_SIZE(adc_cont_inven),
};

int bcm_init(void)
{
	int ret;
#if defined(CONFIG_CHARGER_FAN5405)
	gpio_request(GPIO_PORT19, NULL);
	gpio_direction_input(GPIO_PORT19);
	gpio_pull_up_port(GPIO_PORT19);
#endif

	ret = platform_device_register(&bcm_chrgr_device);
	if (ret < 0)
		return ret;
	ret = platform_device_register(&bcm_accy_device);
	if (ret < 0)
		return ret;
	return 0;
}
