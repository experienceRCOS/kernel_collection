/*****************************************************************************
*  Copyright 2001 - 2012 Broadcom Corporation.  All rights reserved.
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
#include <linux/version.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/export.h>
#include <linux/irq.h>
#include <asm/mach/arch.h>
#include <asm/mach-types.h>
#include <linux/gpio.h>
#include <mach/hardware.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <linux/i2c.h>
#include <linux/mfd/core.h>
#include <linux/mfd/bcmpmu59xxx.h>
#include <linux/mfd/bcmpmu59xxx_reg.h>
#include <linux/power/bcmpmu-fg.h>
#include <linux/broadcom/bcmpmu-ponkey.h>
#include <mach/rdb/brcm_rdb_include.h>
#ifdef CONFIG_SEC_CHARGING_FEATURE
#include <linux/spa_power.h>
#endif
#include <linux/of_platform.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include "pm_params.h"
#include <plat/cpu.h>
#include <mach/chip_pinmux.h>
#include <mach/pinmux.h>
#include <linux/gpio.h>

#define PMU_DEVICE_I2C_ADDR	0x08
#define PMU_DEVICE_I2C_ADDR1	0x0c
#define PMU_DEVICE_INT_GPIO	29
#define PMU_DEVICE_I2C_BUSNO 4

#define BAFFINLITE_HW_REV1			123
#define BAFFINLITE_HW_REV2			124

static int bcmpmu_init_platform_hw(struct bcmpmu59xxx *bcmpmu);
static int bcmpmu_exit_platform_hw(struct bcmpmu59xxx *bcmpmu);

static struct pin_config gpio123_config[2] = {
	{
	// set GPIO123/Input/Pulldown
		.name = PN_DMIC0CLK,
		.func = PF_GPIO123,
		.reg.val = 0x443,
	},
	{
	// set GPIO123/Input/No Pull
		.name = PN_DMIC0CLK,
		.func = PF_GPIO123,
		.reg.val = 0x403,
	},
};

static struct pin_config gpio124_config[2] = {
	{
	// set GPIO124/Input/Pulldown
		.name = PN_DMIC0DQ,
		.func = PF_GPIO124,
		.reg.val = 0x443,
	},
	{
	// set GPIO124/Input/No Pull
		.name = PN_DMIC0DQ,
		.func = PF_GPIO124,
		.reg.val = 0x403,
	},
};

int get_hw_rev(void){
	int rev1;
	int rev2;
	pinmux_set_pin_config(&gpio123_config[0]);
	pinmux_set_pin_config(&gpio124_config[0]);

	gpio_request(BAFFINLITE_HW_REV1, "baffinlite_hw_revision1");
	gpio_request(BAFFINLITE_HW_REV2, "baffinlite_hw_revision2");
	gpio_direction_input(BAFFINLITE_HW_REV1);
	gpio_direction_input(BAFFINLITE_HW_REV2);

	rev1 = gpio_get_value(BAFFINLITE_HW_REV1)?1:0;
	rev2 = gpio_get_value(BAFFINLITE_HW_REV2)?1:0;

	gpio_free(BAFFINLITE_HW_REV1);
	gpio_free(BAFFINLITE_HW_REV2);
	pinmux_set_pin_config(&gpio123_config[1]);
	pinmux_set_pin_config(&gpio124_config[1]);

	if (rev1 == 0 && rev2 == 0)
		return 1;	//rev 0.0 or rev 0.1
	else if (rev1 == 1 && rev2 ==0)
		return 2;	//rev 0.2
	else
		return -1;	//error case
}

/* Used only when no bcmpmu dts entry found */
static struct bcmpmu59xxx_rw_data __initdata register_init_data[] = {
/* mask 0x00 is invalid value for mask */
	/* pin mux selection for pc3 and simldo1
	 * AUXONb Wakeup disabled */
	{.addr = PMU_REG_GPIOCTRL1, .val = 0x75, .mask = 0xFF},
	/*  enable PC3 function */
	{.addr = PMU_REG_GPIOCTRL2, .val = 0x0E, .mask = 0xFF},
	/* Mask Interrupt */
	{.addr = PMU_REG_INT1MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT2MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT3MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT4MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT5MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT6MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT7MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT8MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT9MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT10MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT11MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT12MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT13MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT14MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT15MSK, .val = 0xFF, .mask = 0xFF},
	{.addr = PMU_REG_INT16MSK, .val = 0xFF, .mask = 0xFF},
	/* Trickle charging timer setting */
	{.addr = PMU_REG_MBCCTRL1, .val = 0x38, .mask = 0x38},
	/*  disable software charger timer */
	{.addr = PMU_REG_MBCCTRL2, .val = 0x0, .mask = 0x04},
	/* SWUP */
	{.addr = PMU_REG_MBCCTRL3, .val = 0x04, .mask = 0x04},
	/*  disable BC12_EN */
	{.addr = PMU_REG_MBCCTRL5, .val = 0x0, .mask = 0x01},
	/*  ICCMAX to 1500mA*/
	{.addr = PMU_REG_MBCCTRL8, .val = 0x0B, .mask = 0xFF},
       /*  TCXCTRL*/
       {.addr = PMU_REG_TCXLDOCTRL, .val = 0xB8, .mask = 0xFF},
	/* NTC Hot Temperature Comparator*/
	{.addr = PMU_REG_CMPCTRL5, .val = 0x01, .mask = 0xFF},
	/* NTC Hot Temperature Comparator*/
	{.addr = PMU_REG_CMPCTRL6, .val = 0x05, .mask = 0xFF},
	/* NTC Cold Temperature Comparator */
	{.addr = PMU_REG_CMPCTRL7, .val = 0xFF, .mask = 0xFF},
	/* NTC Cold Temperature Comparator */
	{.addr = PMU_REG_CMPCTRL8, .val = 0xFA, .mask = 0xFF},
	/* NTC Hot Temperature Comparator bit 9,8 */
	{.addr = PMU_REG_CMPCTRL9, .val = 0x0F, .mask = 0xFF},



	/* ID detection method selection
	 *  current source Trimming */
	/* ID_METHOD=0 prevent leackage from ID detection */
	{.addr = PMU_REG_OTGCTRL8, .val = 0x52, .mask = 0xFF},
	{.addr = PMU_REG_OTGCTRL9, .val = 0x98, .mask = 0xFF},
	{.addr = PMU_REG_OTGCTRL10, .val = 0xF0, .mask = 0xFF},
	/*ADP_THR_RATIO*/
	{.addr = PMU_REG_OTGCTRL11, .val = 0x58, .mask = 0xFF},
	/* Enable ADP_PRB  ADP_DSCHG comparators */
	{.addr = PMU_REG_OTGCTRL12, .val = 0xC3, .mask = 0xFF},

/* Regulator configuration */
/* TODO regulator */
	{.addr = PMU_REG_FG_EOC_TH, .val = 0x64, .mask = 0xFF},

	/* Logan rev02 Xtal RTC = 13pF shows 32767.93Hz */
	{.addr = PMU_REG_RTC_C2C1_XOTRIM, .val = 0xBB, .mask = 0xFF},
	{.addr = PMU_REG_FGOCICCTRL, .val = 0x02, .mask = 0xFF},
	 /* FG power down */
	{.addr = PMU_REG_FGCTRL1, .val = 0x00, .mask = 0xFF},
	/* Enable operation mode for PC3PC2PC1 */
	{.addr = PMU_REG_GPLDO2PMCTRL2, .val = 0x38, .mask = 0xFF},
	 /* PWMLED blovk powerdown */
	{.addr =  PMU_REG_PWMLEDCTRL1, .val = 0x23, .mask = 0xFF},
	{.addr = PMU_REG_HSCP3, .val = 0x00, .mask = 0xFF},
	 /* HS audio powerdown feedback path */
	{.addr =  PMU_REG_IHF_NGMISC, .val = 0x0C, .mask = 0xFF},
	/* NTC BiasSynchronous Mode,Host Enable Control NTC_PM0 Disable*/
	{.addr =  PMU_REG_CMPCTRL14, .val = 0x13, .mask = 0xFF},
	{.addr =  PMU_REG_CMPCTRL15, .val = 0x01, .mask = 0xFF},
	/* BSI Bias Host Control, Synchronous Mode Enable */

	{.addr =  PMU_REG_CMPCTRL16, .val = 0x13, .mask = 0xFF},
	/* BSI_EN_PM0 disable */
	{.addr =  PMU_REG_CMPCTRL17, .val = 0x01, .mask = 0xFF},
	/* Mask RTM conversion */
	{.addr =  PMU_REG_ADCCTRL1, .val = 0x08, .mask = 0x08},
	/* EN_SESS_VALID  disable ID detection */
	{.addr = PMU_REG_OTGCTRL1 , .val = 0x10, .mask = 0xFF},

	/* SDSR2 NM1 voltage - 1.24 */
	{.addr = PMU_REG_SDSR2VOUT1 , .val = 0x28, .mask = 0x3F},
	/* SDSR2 LPM voltage - 1.24V */
	{.addr = PMU_REG_SDSR2VOUT2 , .val = 0x28, .mask = 0x3F},
	/* IOSR1 LPM voltage - 1.8V */
	{.addr = PMU_REG_IOSR1VOUT2 , .val = 0x3E, .mask = 0x3F},

	/*from h/w team for power consumption*/
	{.addr = PMU_REG_PASRCTRL1 , .val = 0x00, .mask = 0x06},
	{.addr = PMU_REG_PASRCTRL6 , .val = 0x00, .mask = 0xF0},
	{.addr = PMU_REG_PASRCTRL7 , .val = 0x00, .mask = 0x3F},
	{.addr = PMU_REG_FGCTRL1 , .val = 0x40, .mask = 0xFF},
	{.addr = PMU_REG_FGOPMODCTRL , .val = 0x01, .mask = 0xFF},
	{.addr = PMU_REG_FGOCICCTRL , .val = 0x04, .mask = 0xFF},
	{.addr = PMU_REG_CMPCTRL14 , .val = 0x12, .mask = 0xFF},
	{.addr = PMU_REG_CMPCTRL15 , .val = 0x0, .mask = 0xFF},
	{.addr = PMU_REG_CMPCTRL16 , .val = 0x12, .mask = 0xFF},
	{.addr = PMU_REG_CMPCTRL17 , .val = 0x00, .mask = 0xFF},
	{.addr = PMU_REG_PLLPMCTRL , .val = 0x00, .mask = 0xFF},

	/*RFLDO and AUDLDO pulldown disable MobC00290043*/
	{.addr = PMU_REG_RFLDOCTRL , .val = 0x40, .mask = 0x40},
	{.addr = PMU_REG_AUDLDOCTRL , .val = 0x40, .mask = 0x40},

};

__weak struct regulator_consumer_supply rf_supply[] = {
	{.supply = "rf"},
};
static struct regulator_init_data bcm59xxx_rfldo_data = {
	.constraints = {
			.name = "rfldo",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask = REGULATOR_CHANGE_STATUS |
			REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_MODE,
			.valid_modes_mask = REGULATOR_MODE_NORMAL |
			REGULATOR_MODE_IDLE |
			REGULATOR_MODE_STANDBY,
			.always_on = 0,
			.initial_mode = REGULATOR_MODE_STANDBY,
			},
	.num_consumer_supplies = ARRAY_SIZE(rf_supply),
	.consumer_supplies = rf_supply,
};

__weak struct regulator_consumer_supply cam1_supply[] = {
	{.supply = "cam1"},
};
static struct regulator_init_data bcm59xxx_camldo1_data = {
	.constraints = {
			.name = "camldo",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE |
			REGULATOR_CHANGE_VOLTAGE,
			.always_on = 1,
			.initial_mode = REGULATOR_MODE_IDLE,
			},
	.num_consumer_supplies = ARRAY_SIZE(cam1_supply),
	.consumer_supplies = cam1_supply,
};

__weak struct regulator_consumer_supply cam2_supply[] = {
	{.supply = "cam2"},
};
static struct regulator_init_data bcm59xxx_camldo2_data = {
	.constraints = {
			.name = "camldo2",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE |
			REGULATOR_CHANGE_VOLTAGE,
			.always_on = 1,
			.initial_mode = REGULATOR_MODE_IDLE,
			},
	.num_consumer_supplies = ARRAY_SIZE(cam2_supply),
	.consumer_supplies = cam2_supply,
};

__weak struct regulator_consumer_supply sim1_supply[] = {
	{.supply = "sim_vcc"},
};
static struct regulator_init_data bcm59xxx_simldo1_data = {
	.constraints = {
			.name = "simldo1",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE |
			REGULATOR_CHANGE_MODE,
			.always_on = 0,
			.valid_modes_mask = REGULATOR_MODE_NORMAL |
			REGULATOR_MODE_IDLE | REGULATOR_MODE_STANDBY,
			},
	.num_consumer_supplies = ARRAY_SIZE(sim1_supply),
	.consumer_supplies = sim1_supply,
};

__weak struct regulator_consumer_supply sim2_supply[] = {
	{.supply = "sim2_vcc"},
};
static struct regulator_init_data bcm59xxx_simldo2_data = {
	.constraints = {
			.name = "simldo2",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE |
			REGULATOR_CHANGE_MODE,
			.always_on = 0,
			.valid_modes_mask = REGULATOR_MODE_NORMAL |
			REGULATOR_MODE_IDLE | REGULATOR_MODE_STANDBY,
			},
	.num_consumer_supplies = ARRAY_SIZE(sim2_supply),
	.consumer_supplies = sim2_supply,
};

__weak struct regulator_consumer_supply sd_supply[] = {
	{.supply = "sd_vcc"},
	REGULATOR_SUPPLY("vddmmc", "sdhci.3"), /* 0x3f1b0000.sdhci */
	{.supply = "dummy"},
};
static struct regulator_init_data bcm59xxx_sdldo_data = {
	.constraints = {
			.name = "sdldo",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(sd_supply),
	.consumer_supplies = sd_supply,
};
__weak struct regulator_consumer_supply sdx_supply[] = {
	{.supply = "sdx_vcc"},
	REGULATOR_SUPPLY("vddo", "sdhci.3"), /* 0x3f1b0000.sdhci */
	{.supply = "dummy"},
};
static struct regulator_init_data bcm59xxx_sdxldo_data = {
	.constraints = {
			.name = "sdxldo",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(sdx_supply),
	.consumer_supplies = sdx_supply,
};

__weak struct regulator_consumer_supply mmc1_supply[] = {
	{.supply = "mmc1_vcc"},
};
static struct regulator_init_data bcm59xxx_mmcldo1_data = {
	.constraints = {
			.name = "mmcldo1",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE |
			REGULATOR_CHANGE_VOLTAGE,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(mmc1_supply),
	.consumer_supplies = mmc1_supply,
};

__weak struct regulator_consumer_supply mmc2_supply[] = {
	{.supply = "mmc2_vcc"},
};
static struct regulator_init_data bcm59xxx_mmcldo2_data = {
	.constraints = {
			.name = "mmcldo2",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE |
			REGULATOR_CHANGE_VOLTAGE,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(mmc2_supply),
	.consumer_supplies = mmc2_supply,
};

__weak struct regulator_consumer_supply aud_supply[] = {
	{.supply = "audldo_uc"},
};
static struct regulator_init_data bcm59xxx_audldo_data = {
	.constraints = {
			.name = "audldo",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE |
			REGULATOR_CHANGE_MODE,
			.always_on = 1,
			.initial_mode = REGULATOR_MODE_STANDBY,
			.valid_modes_mask = REGULATOR_MODE_NORMAL |
			REGULATOR_MODE_IDLE | REGULATOR_MODE_STANDBY,
			},
	.num_consumer_supplies = ARRAY_SIZE(aud_supply),
	.consumer_supplies = aud_supply,
};

__weak struct regulator_consumer_supply usb_supply[] = {
	{.supply = "usb_vcc"},
};
static struct regulator_init_data bcm59xxx_usbldo_data = {
	.constraints = {
			.name = "usbldo",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(usb_supply),
	.consumer_supplies = usb_supply,
};

__weak struct regulator_consumer_supply mic_supply[] = {
	{.supply = "micldo_uc"},
};
static struct regulator_init_data bcm59xxx_micldo_data = {
	.constraints = {
			.name = "micldo",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(mic_supply),
	.consumer_supplies = mic_supply,
};


__weak struct regulator_consumer_supply vib_supply[] = {
	{.supply = "vibldo_uc"},
};
static struct regulator_init_data bcm59xxx_vibldo_data = {
	.constraints = {
			.name = "vibldo",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE,
			.always_on = 0,

			},
	.num_consumer_supplies = ARRAY_SIZE(vib_supply),
	.consumer_supplies = vib_supply,
};

__weak struct regulator_consumer_supply gpldo1_supply[] = {
	{.supply = "gpldo1_uc"},
};
static struct regulator_init_data bcm59xxx_gpldo1_data = {
	.constraints = {
			.name = "gpldo1",
			.min_uV = 1200000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(gpldo1_supply),
	.consumer_supplies = gpldo1_supply,
};

__weak struct regulator_consumer_supply gpldo2_supply[] = {
	{.supply = "gpldo2_uc"},
};
static struct regulator_init_data bcm59xxx_gpldo2_data = {
	.constraints = {
			.name = "gpldo2",
			.min_uV = 1200000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE |
			REGULATOR_CHANGE_VOLTAGE,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(gpldo2_supply),
	.consumer_supplies = gpldo2_supply,
};

__weak struct regulator_consumer_supply gpldo3_supply[] = {
	{.supply = "gpldo3_uc"},
};
static struct regulator_init_data bcm59xxx_gpldo3_data = {
	.constraints = {
			.name = "gpldo3",
			.min_uV = 1200000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE \
				| REGULATOR_CHANGE_MODE,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(gpldo3_supply),
	.consumer_supplies = gpldo3_supply,
};

__weak struct regulator_consumer_supply tcxldo_supply[] = {
	{.supply = "tcxldo_uc"},
};
static struct regulator_init_data bcm59xxx_tcxldo_data = {
	.constraints = {
			.name = "tcxldo",
			.min_uV = 1300000,
			.max_uV = 3300000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_MODE |
			REGULATOR_CHANGE_VOLTAGE,
			.always_on = 1,
			},
	.num_consumer_supplies = ARRAY_SIZE(tcxldo_supply),
	.consumer_supplies = tcxldo_supply,
};

__weak struct regulator_consumer_supply lvldo1_supply[] = {
	{.supply = "lvldo1_uc"},
};
static struct regulator_init_data bcm59xxx_lvldo1_data = {
	.constraints = {
			/* CAM0_1v8 */
			.name = "lvldo1",
			.min_uV = 1000000,
			.max_uV = 1800000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE|
			REGULATOR_CHANGE_MODE,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(lvldo1_supply),
	.consumer_supplies = lvldo1_supply,
};

__weak struct regulator_consumer_supply lvldo2_supply[] = {
	{.supply = "lvldo2_uc"},
};
static struct regulator_init_data bcm59xxx_lvldo2_data = {
	.constraints = {
			.name = "lvldo2",
			.min_uV = 1000000,
			.max_uV = 1786000,
			.valid_ops_mask =
			REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE|
			REGULATOR_CHANGE_MODE ,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(lvldo2_supply),
	.consumer_supplies = lvldo2_supply,
};

__weak struct regulator_consumer_supply vsr_supply[] = {
	{.supply = "vsr_uc"},
};
static struct regulator_init_data bcm59xxx_vsr_data = {
	.constraints = {
			.name = "vsrldo",
			.min_uV = 860000,
			.max_uV = 1800000,
			.valid_ops_mask =
			REGULATOR_CHANGE_MODE | REGULATOR_CHANGE_VOLTAGE \
				| REGULATOR_CHANGE_STATUS,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(vsr_supply),
	.consumer_supplies = vsr_supply,
};

__weak struct regulator_consumer_supply csr_supply[] = {
	{.supply = "csr_uc"},
};

static struct regulator_init_data bcm59xxx_csr_data = {
	.constraints = {
			.name = "csrldo",
			.min_uV = 700000,
			.max_uV = 1440000,
			.valid_ops_mask = REGULATOR_CHANGE_STATUS |
			REGULATOR_CHANGE_MODE,
			.always_on = 1,
			.initial_mode = REGULATOR_MODE_STANDBY,
			},
	.num_consumer_supplies = ARRAY_SIZE(csr_supply),
	.consumer_supplies = csr_supply,
};

__weak struct regulator_consumer_supply mmsr_supply[] = {
	{.supply = "mmsr_uc"},
};

static struct regulator_init_data bcm59xxx_mmsr_data = {
	.constraints = {
			.name = "mmsrldo",
			.min_uV = 860000,
			.max_uV = 1800000,
			.valid_ops_mask = REGULATOR_CHANGE_STATUS |
			REGULATOR_CHANGE_MODE,
			.always_on = 1,
			.initial_mode = REGULATOR_MODE_IDLE,
			},
	.num_consumer_supplies = ARRAY_SIZE(mmsr_supply),
	.consumer_supplies = mmsr_supply,
};

__weak struct regulator_consumer_supply sdsr1_supply[] = {
	{.supply = "sdsr1_uc"},
};

static struct regulator_init_data bcm59xxx_sdsr1_data = {
	.constraints = {
			.name = "sdsr1ldo",
			.min_uV = 860000,
			.max_uV = 1800000,
			.valid_ops_mask = REGULATOR_CHANGE_STATUS |
			REGULATOR_CHANGE_MODE,
			.always_on = 1,
			.initial_mode = REGULATOR_MODE_IDLE,
			},
	.num_consumer_supplies = ARRAY_SIZE(sdsr1_supply),
	.consumer_supplies = sdsr1_supply,
};

__weak struct regulator_consumer_supply sdsr2_supply[] = {
	{.supply = "sdsr2_uc"},
};

static struct regulator_init_data bcm59xxx_sdsr2_data = {
	.constraints = {
			.name = "sdsr2ldo",
			.min_uV = 860000,
			.max_uV = 1800000,
			.valid_ops_mask = REGULATOR_CHANGE_STATUS |
			REGULATOR_CHANGE_MODE,
			.always_on = 1,
			.initial_mode = REGULATOR_MODE_IDLE,
			},
	.num_consumer_supplies = ARRAY_SIZE(sdsr2_supply),
	.consumer_supplies = sdsr2_supply,
};

__weak struct regulator_consumer_supply iosr1_supply[] = {
	{.supply = "iosr1_uc"},
};

static struct regulator_init_data bcm59xxx_iosr1_data = {
	.constraints = {
			.name = "iosr1ldo",
			.min_uV = 860000,
			.max_uV = 1800000,
			.valid_ops_mask = REGULATOR_CHANGE_STATUS |
			REGULATOR_CHANGE_MODE,
			.always_on = 1,
			.initial_mode = REGULATOR_MODE_IDLE,
			},
	.num_consumer_supplies = ARRAY_SIZE(iosr1_supply),
	.consumer_supplies = iosr1_supply,
};


__weak struct regulator_consumer_supply iosr2_supply[] = {
	{.supply = "iosr2_uc"},
};

static struct regulator_init_data bcm59xxx_iosr2_data = {
	.constraints = {
			.name = "iosr2ldo",
			.min_uV = 860000,
			.max_uV = 1800000,
			.valid_ops_mask = REGULATOR_CHANGE_STATUS,
			.always_on = 0,
			},
	.num_consumer_supplies = ARRAY_SIZE(iosr2_supply),
	.consumer_supplies = iosr2_supply,
};


struct bcmpmu59xxx_regulator_init_data
	bcm59xxx_regulators[BCMPMU_REGULATOR_MAX] = {
		[BCMPMU_REGULATOR_RFLDO] = {
			.id = BCMPMU_REGULATOR_RFLDO,
			.initdata = &bcm59xxx_rfldo_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC2),
			.name = "rf",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_CAMLDO1] = {
			.id = BCMPMU_REGULATOR_CAMLDO1,
			.initdata = &bcm59xxx_camldo1_data,
			.pc_pins_map = PCPIN_MAP_ENC(0, PMU_PC2|PMU_PC3),
			.name = "cam1",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_CAMLDO2] = {
			.id = BCMPMU_REGULATOR_CAMLDO2,
			.initdata = &bcm59xxx_camldo2_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC1|PMU_PC2|PMU_PC3),
			.name = "cam2",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_SIMLDO1] = {
			.id = BCMPMU_REGULATOR_SIMLDO1,
			.initdata = &bcm59xxx_simldo1_data,
			.pc_pins_map = PCPIN_MAP_ENC(0, PMU_PC1),
			.name = "sim1",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_SIMLDO2] = {
			.id = BCMPMU_REGULATOR_SIMLDO2,
			.initdata = &bcm59xxx_simldo2_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC1),
			.name = "sim2",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_SDLDO] = {
			.id = BCMPMU_REGULATOR_SDLDO,
			.initdata = &bcm59xxx_sdldo_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC1|PMU_PC2|PMU_PC3),
			.name = "sd",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_SDXLDO] = {
			.id = BCMPMU_REGULATOR_SDXLDO,
			.initdata = &bcm59xxx_sdxldo_data,
			.pc_pins_map =
				 PCPIN_MAP_ENC(0, PMU_PC1|PMU_PC2|PMU_PC3),
			.name = "sdx",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_MMCLDO1] = {
			.id = BCMPMU_REGULATOR_MMCLDO1,
			.initdata = &bcm59xxx_mmcldo1_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC1|PMU_PC2|PMU_PC3),
			.name = "mmc1",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_MMCLDO2] = {
			.id = BCMPMU_REGULATOR_MMCLDO2,
			.initdata = &bcm59xxx_mmcldo2_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC1|PMU_PC2|PMU_PC3),
			.name = "mmc2",
			.req_volt = 0,
		},

		[BCMPMU_REGULATOR_AUDLDO] = {
			.id = BCMPMU_REGULATOR_AUDLDO,
			.initdata = &bcm59xxx_audldo_data,
			.pc_pins_map = PCPIN_MAP_ENC(0, PMU_PC2|PMU_PC3),
			.name = "aud",
			.req_volt = 0,
#if defined(CONFIG_MACH_HAWAII_SS_COMMON)
			.reg_value = 0x01,
			.reg_value2 = 0x05, /* 0x11 in Capri */
			.off_value = 0x02,
			.off_value2 = 0x0a, /* 0x22 in Capri */
#endif
		},

		[BCMPMU_REGULATOR_MICLDO] = {
			.id = BCMPMU_REGULATOR_MICLDO,
			.initdata = &bcm59xxx_micldo_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, 0),  /*Not used*/
			.name = "mic",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_USBLDO] = {
			.id = BCMPMU_REGULATOR_USBLDO,
			.initdata = &bcm59xxx_usbldo_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC1|PMU_PC2|PMU_PC3),
			.name = "usb",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_VIBLDO] = {
			.id = BCMPMU_REGULATOR_VIBLDO,
			.initdata = &bcm59xxx_vibldo_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC1|PMU_PC2|PMU_PC3),
			.name = "vib",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_GPLDO1] = {
			.id = BCMPMU_REGULATOR_GPLDO1,
			.initdata = &bcm59xxx_gpldo1_data,
			.pc_pins_map = PCPIN_MAP_ENC(0, 0), /*Not used*/
			.name = "gp1",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_GPLDO2] = {
			.id = BCMPMU_REGULATOR_GPLDO2,
			.initdata = &bcm59xxx_gpldo2_data,
			.pc_pins_map = PCPIN_MAP_ENC(0, 0), /*Not used*/
			.name = "gp2",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_GPLDO3] = {
			.id = BCMPMU_REGULATOR_GPLDO3,
			.initdata = &bcm59xxx_gpldo3_data,
			.pc_pins_map = PCPIN_MAP_ENC(0, PMU_PC2),
			.name = "gp3",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_TCXLDO] = {
			.id = BCMPMU_REGULATOR_TCXLDO,
			.initdata = &bcm59xxx_tcxldo_data,
			.pc_pins_map = PCPIN_MAP_ENC(0, 0),
			.name = "tcx",
			.req_volt = 0,
		},
	#if 1 /*defined(CONFIG_MACH_HAWAII_SS_LOGAN_REV01)*/
		[BCMPMU_REGULATOR_LVLDO1] = {
			.id = BCMPMU_REGULATOR_LVLDO1,
			.initdata = &bcm59xxx_lvldo1_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC1|PMU_PC2|PMU_PC3),
			.name = "lv1",
			.req_volt = 0,
		},
	#else
		[BCMPMU_REGULATOR_LVLDO1] = {
			.id = BCMPMU_REGULATOR_LVLDO1,
			.initdata = &bcm59xxx_lvldo1_data,
			.pc_pins_map = PCPIN_MAP_ENC(0, 0), /*Not used*/
			.name = "lv1",
			.req_volt = 0,
		},
	#endif
		[BCMPMU_REGULATOR_LVLDO2] = {
			.id = BCMPMU_REGULATOR_LVLDO2,
			.initdata = &bcm59xxx_lvldo2_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC1|PMU_PC2|PMU_PC3),
			.name = "lv2",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_VSR] = {
			.id = BCMPMU_REGULATOR_VSR,
			.initdata = &bcm59xxx_vsr_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC1|PMU_PC2|PMU_PC3),
			.name = "vsr",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_CSR] = {
			.id = BCMPMU_REGULATOR_CSR,
			.initdata = &bcm59xxx_csr_data,
			.pc_pins_map = PCPIN_MAP_ENC(0, PMU_PC3),
			.name = "csr",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_MMSR] = {
			.id = BCMPMU_REGULATOR_MMSR,
			.initdata = &bcm59xxx_mmsr_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC2),
			.name = "mmsr",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_SDSR1] = {
			.id = BCMPMU_REGULATOR_SDSR1,
			.initdata = &bcm59xxx_sdsr1_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC1|PMU_PC2|PMU_PC3),
			.name = "sdsr1",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_SDSR2] = {
			.id = BCMPMU_REGULATOR_SDSR2,
			.initdata = &bcm59xxx_sdsr2_data,
			.pc_pins_map = PCPIN_MAP_ENC(0, PMU_PC2|PMU_PC3),
			.name = "sdsr2",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_IOSR1] = {
			.id = BCMPMU_REGULATOR_IOSR1,
			.initdata = &bcm59xxx_iosr1_data,
			.pc_pins_map =
				PCPIN_MAP_ENC(0, PMU_PC1|PMU_PC2|PMU_PC3),
			.name = "iosr1",
			.req_volt = 0,
		},
		[BCMPMU_REGULATOR_IOSR2] = {
			.id = BCMPMU_REGULATOR_IOSR2,
			.initdata = &bcm59xxx_iosr2_data,
			.pc_pins_map = PCPIN_MAP_ENC(0, 0), /*not used*/
			.name = "iosr2",
			.req_volt = 0,
		},

	};



/* logan compilation fix */

/*Ponkey platform data*/
struct pkey_timer_act pkey_t3_action = {
	.flags = PKEY_SMART_RST_PWR_EN,
	.action = PKEY_ACTION_SMART_RESET,
	.timer_dly = PKEY_ACT_DELAY_7S,
	.timer_deb = PKEY_ACT_DEB_1S,
	.ctrl_params = PKEY_SR_DLY_30MS,
};

struct bcmpmu59xxx_pkey_pdata pkey_pdata = {
	.press_deb = PKEY_DEB_100MS,
	.release_deb = PKEY_DEB_100MS,
	.wakeup_deb = PKEY_WUP_DEB_1000MS,
	.t3 = &pkey_t3_action,
};
/*
struct bcmpmu59xxx_pok_pdata pok_pdata = {
	.hard_reset_en = -1,
	.restart_en = -1,
	.pok_hold_deb = -1,
	.pok_shtdwn_dly = -1,
	.pok_restart_dly = -1,
	.pok_restart_deb = -1,
	.pok_lock = 1,
	.pok_turn_on_deb = -1,
};
*/
struct bcmpmu59xxx_audio_pdata audio_pdata = {
	.ihf_autoseq_dis = 100,
};

struct bcmpmu59xxx_rpc_pdata rpc_pdata = {
	.delay = 30000, /*rpc delay - 30 sec*/
	.fw_delay = 5000, /* for fw_cnt use this */
	.fw_cnt = 4,
	.poll_time = 120000, /* 40c-60c 120 sec */
	.htem_poll_time = 8000, /* > 60c 8 sec */
	.mod_tem = 400, /* 40 C*/
	.htem = 600, /* 60 C*/
};

struct bcmpmu59xxx_regulator_pdata rgltr_pdata = {
	.bcmpmu_rgltr = bcm59xxx_regulators,
	.num_rgltr = ARRAY_SIZE(bcm59xxx_regulators),
};

static struct bcmpmu_adc_lut batt_temp_map_bom[] = {
	{16, 1000},			/* 100 C */
	{20, 950},			/* 95 C */
	{24, 900},			/* 90 C */
	{28, 850},			/* 85 C */
	{32, 800},			/* 80 C */
	{36, 750},			/* 75 C */
	{52, 700},			/* 70 C */
	{60, 650},			/* 65 C */
	{67, 600},			/* 60 C */
	{84, 550},			/* 55 C */
	{95, 500},			/* 50 C */
	{115, 450},			/* 45 C */
	{137, 400},			/* 40 C */
	{169, 350},			/* 35 C */
	{200, 300},			/* 30 C */
	{245, 250},			/* 25 C */
	{277, 200},			/* 20 C */
	{338, 150},			/* 15 C */
	{385, 100},			/* 10 C */
	{457, 50},			/* 5 C */
	{513, 0},			/* 0 C */
	{583, -50},			/* -5 C */
	{654, -100},			/* -10 C */
	{719, -150},			/* -15 C */
	{780, -200},			/* -20 C */
	{816, -250},			/* -25 C */
	{860, -300},			/* -30 C */
	{900, -350},			/* -35 C */
	{932, -400},			/* -40 C */
};

static struct bcmpmu_adc_lut batt_temp_map[] = {
	{16, 1000},			/* 100 C */
	{20, 950},			/* 95 C */
	{24, 900},			/* 90 C */
	{28, 850},			/* 85 C */
	{32, 800},			/* 80 C */
	{36, 750},			/* 75 C */
	{52, 700},			/* 70 C */
	{60, 650},			/* 65 C */
	{67, 600},			/* 60 C */
	{84, 550},			/* 55 C */
	{95, 500},			/* 50 C */
	{115, 450},			/* 45 C */
	{137, 400},			/* 40 C */
	{169, 350},			/* 35 C */
	{200, 300},			/* 30 C */
	{245, 250},			/* 25 C */
	{277, 200},			/* 20 C */
	{338, 150},			/* 15 C */
	{385, 100},			/* 10 C */
	{457, 50},			/* 5 C */
	{513, 0},			/* 0 C */
	{583, -50},			/* -5 C */
	{654, -100},			/* -10 C */
	{719, -150},			/* -15 C */
	{780, -200},			/* -20 C */
	{816, -250},			/* -25 C */
	{860, -300},			/* -30 C */
	{900, -350},			/* -35 C */
	{932, -400},			/* -40 C */
};
struct bcmpmu_adc_pdata adc_pdata[PMU_ADC_CHANN_MAX] = {
	[PMU_ADC_CHANN_VMBATT] = {
					.flag = 0,
					.volt_range = 4800,
					.adc_offset = 0,
					.lut = NULL,
					.lut_len = 0,
					.name = "vmbatt",
					.reg = PMU_REG_ADCCTRL3,
	},
	[PMU_ADC_CHANN_VBBATT] = {
					.flag = 0,
					.volt_range = 4800,
					.adc_offset = 0,
					.lut = NULL,
					.lut_len = 0,
					.name = "vbbatt",
					.reg = PMU_REG_ADCCTRL5,
	},
	[PMU_ADC_CHANN_VBUS] = {
					.flag = 0,
					.volt_range = 14400,
					.adc_offset = 0,
					.lut = NULL,
					.lut_len = 0,
					.name = "vbus",
					.reg = PMU_REG_ADCCTRL9,
	},
	[PMU_ADC_CHANN_IDIN] = {
					.flag = 0,
					.volt_range = 1200,
					.adc_offset = 0,
					.lut = NULL,
					.lut_len = 0,
					.name = "idin",
					.reg = PMU_REG_ADCCTRL11,
	},
	[PMU_ADC_CHANN_NTC] = {
					.flag = 0,
					.volt_range = 1200,
					.adc_offset = 0,
					.lut = batt_temp_map,
					.lut_len = ARRAY_SIZE(batt_temp_map),
					.name = "ntc",
					.reg = PMU_REG_ADCCTRL13,
	},
	[PMU_ADC_CHANN_BSI] = {
					.flag = 0,
					.volt_range = 1200,
					.adc_offset = 0,
					.lut = NULL,
					.lut_len = 0,
					.name = "bsi",
					.reg = PMU_REG_ADCCTRL15,
	},
	[PMU_ADC_CHANN_DIE_TEMP] = {
					.flag = 0,
					.volt_range = 1200,
					.adc_offset = 0,
					.lut = NULL,
					.lut_len = 0,
					.name = "dietemp",
					.reg = PMU_REG_ADCCTRL25,
	},
	[PMU_ADC_CHANN_BOM] = {
					.flag = 0,
					.volt_range = 1200,
					.adc_offset = 0,
					.lut = batt_temp_map_bom,
					.lut_len = ARRAY_SIZE(batt_temp_map_bom),
					.name = "bom",
					.reg = PMU_REG_ADCCTRL17,
	},
	[PMU_ADC_CHANN_32KTEMP] = {
					.flag = 0,
					.volt_range = 1200,
					.adc_offset = 0,
					.lut = batt_temp_map,
					.lut_len = ARRAY_SIZE(batt_temp_map),
					.name = "32ktemp",
					.reg = PMU_REG_ADCCTRL21,
	},
	[PMU_ADC_CHANN_PATEMP] = {
					.flag = 0,
					.volt_range = 1200,
					.adc_offset = 0,
					.lut = batt_temp_map,
					.lut_len = ARRAY_SIZE(batt_temp_map),
					.name = "patemp",
					.reg = PMU_REG_ADCCTRL21,
	},
	[PMU_ADC_CHANN_ALS] = {
					.flag = 0,
					.volt_range = 1200,
					.adc_offset = 0,
					.lut = NULL,
					.lut_len = 0,
					.name = "als",
					.reg = PMU_REG_ADCCTRL21,
	},
};

/* BaffinLite SS EB535163LU-AABC817AS,2100mAH */
static struct batt_volt_cap_map ss_baffinlite_volt_cap_lut[] = {
	{4323, 100},
	{4260, 95},
	{4204, 90},
	{4151, 85},
	{4099, 80},
	{4062, 75},
	{3990, 70},
	{3965, 65},
	{3927, 60},
	{3879, 55},
	{3843, 50},
	{3820, 45},
	{3802, 40},
	{3789, 35},
	{3778, 29},
	{3771, 24},
	{3751, 19},
	{3716, 14},
	{3694, 9},
	{3693, 8},
	{3692, 7},
	{3691, 6},
	{3688, 5},
	{3676, 4},
	{3643, 3},
	{3590, 2},
	{3516, 1},
	{3400, 0},
};

static struct batt_eoc_curr_cap_map ss_baffinlite_eoc_cap_lut[] = {
	{290, 90},
	{270, 91},
	{250, 92},
	{228, 93},
	{208, 94},
	{180, 95},
	{165, 96},
	{145, 97},
	{125, 98},
	{100, 99},
	{85, 100},
	{0,  100},
};

static struct batt_cutoff_cap_map ss_baffinlite_cutoff_cap_lut[] = {
	{3460, 2},
	{3430, 1},
	{3400, 0},
};

/* BaffinLite SS EB535163LU-AABC817AS,2100mAH */
static struct batt_esr_temp_lut ss_baffinlite_esr_temp_lut[] = {
	{
		.temp = -200,
		.reset = 0, .fct = 373, .guardband = 50,
		.esr_vl_lvl = 3657, .esr_vm_lvl = 3788, .esr_vh_lvl = 4260,
		.esr_vl_slope = 1859,  .esr_vl_offset = -4193,
		.esr_vm_slope = -8789, .esr_vm_offset = 34749,
		.esr_vh_slope = 379,   .esr_vh_offset = 22,
		.esr_vf_slope = -4483, .esr_vf_offset = 20733,
	},
	{
		.temp = -100,
		.reset = 0, .fct = 700, .guardband = 50,
		.esr_vl_lvl = 3692, .esr_vm_lvl = 3788, .esr_vh_lvl = 4323,
		.esr_vl_slope = -5228, .esr_vl_offset = 20884,
		.esr_vm_slope = -6276, .esr_vm_offset = 24753,
		.esr_vh_slope = -390,  .esr_vh_offset = 2457,
		.esr_vf_slope = -2867, .esr_vf_offset = 13165,
	},
	{
		.temp = 0,
		.reset = 0, .fct = 965, .guardband = 30,
		.esr_vl_lvl = 3692, .esr_vm_lvl = 3754, .esr_vh_lvl = 4323,
		.esr_vl_slope = -3094, .esr_vl_offset = 12571,
		.esr_vm_slope = -7352, .esr_vm_offset = 28290,
		.esr_vh_slope = -535,  .esr_vh_offset = 2696,
		.esr_vf_slope = -1484, .esr_vf_offset = 6798,
	},
	{
		.temp = 100,
		.reset = 0, .fct = 989, .guardband = 30,
		.esr_vl_lvl = 3773, .esr_vm_lvl = 4323, .esr_vh_lvl = 4336,
		.esr_vl_slope = -2087, .esr_vl_offset = 8192,
		.esr_vm_slope = -184,  .esr_vm_offset = 1011,
		.esr_vh_slope = -747,  .esr_vh_offset = 3445,
		.esr_vf_slope = -747,  .esr_vf_offset = 3445,
	},
	{
		.temp = 200,
		.reset = 0, .fct = 1000, .guardband = 30,
		.esr_vl_lvl = 3692, .esr_vm_lvl = 3841, .esr_vh_lvl = 3925,
		.esr_vl_slope = -1201, .esr_vl_offset = 4676,
		.esr_vm_slope = -368,  .esr_vm_offset = 1601,
		.esr_vh_slope = 584,   .esr_vh_offset = -2056,
		.esr_vf_slope = -253,  .esr_vf_offset = 1229,
	},
};

/* BaffinLite SS EB535163LU-AABC817AS,2100mAH */
static struct bcmpmu_batt_property ss_baffinlite_props = {
	.model = "SS EB535163LU",
	.min_volt = 3400,
	.max_volt = 4350,
	.full_cap = 2100 * 3600,
	.one_c_rate = 2100,
	.volt_cap_lut = ss_baffinlite_volt_cap_lut,
	.volt_cap_lut_sz = ARRAY_SIZE(ss_baffinlite_volt_cap_lut),
	.esr_temp_lut = ss_baffinlite_esr_temp_lut,
	.esr_temp_lut_sz = ARRAY_SIZE(ss_baffinlite_esr_temp_lut),
	.eoc_cap_lut = ss_baffinlite_eoc_cap_lut,
	.eoc_cap_lut_sz = ARRAY_SIZE(ss_baffinlite_eoc_cap_lut),
	.cutoff_cap_lut = ss_baffinlite_cutoff_cap_lut,
	.cutoff_cap_lut_sz = ARRAY_SIZE(ss_baffinlite_cutoff_cap_lut),
};

static struct bcmpmu_batt_cap_levels ss_baffinlite_cap_levels = {
	.critical = 5,
	.low = 15,
	.normal = 75,
	.high = 95,
};

/* BaffinLite SS EB535163LU-AABC817AS,2100mAH */
static struct bcmpmu_batt_volt_levels ss_baffinlite_volt_levels = {
	.critical = 3400,
	.low = 3500,
	.normal = 3700,
	.high   = 4300,
	.crit_cutoff_cnt = 3,
	.vfloat_lvl = 0x14, /* 4.345 V */
	.vfloat_max = 0x14,
	.vfloat_gap = 150, /* in mV */
};

static struct bcmpmu_batt_cal_data ss_baffinlite_cal_data = {
	.volt_low = 3550,
	.cap_low = 5,  /* lower entering low calibration */
};

static struct bcmpmu_fg_pdata fg_pdata = {
	.batt_prop = &ss_baffinlite_props,
	.cap_levels = &ss_baffinlite_cap_levels,
	.volt_levels = &ss_baffinlite_volt_levels,
	.cal_data = &ss_baffinlite_cal_data,
	.sns_resist = 10,
	.sys_impedence = 33,
	/* End of charge current in mA */ /* Samsung spec TBD */
	.eoc_current = 100,
	/* enable HW EOC of PMU */
	.hw_maintenance_charging = false,
	/* floor during sleep from Hawaii HW workshop Dec7 2012 */
	.sleep_current_ua = 1460,
	.sleep_sample_rate = 32000,

	.fg_factor = 970,

	.poll_rate_low_batt =  120000, /* every 120 seconds */
	.poll_rate_crit_batt = 5000, /* every 5 Seconds */
	.ntc_high_temp = 680, /*battery too hot shdwn temp*/
};

struct bcmpmu_acld_pdata acld_pdata = {
	.acld_vbus_margin = 200,	/*mV*/
	.acld_vbus_thrs = 5950,
	.acld_vbat_thrs = 3500,
	/* CIG22H2R2MNE, rated current 1.6A  */
	.i_sat = 1600,		/* saturation current in mA
						for chrgr while using ACLD */
	.i_def_dcp = 700,
	.i_max_cc = 2200,
	.acld_cc_lmt = 1500,
	.otp_cc_trim = 0x1F,
};


struct bcmpmu59xxx_accy_pdata accy_pdata = {
	.flags = ACCY_USE_PM_QOS,
	.qos_pi_id = PI_MGR_PI_ID_ARM_SUB_SYSTEM,
};

#ifdef CONFIG_CHARGER_BCMPMU_SPA
struct bcmpmu59xxx_spa_pb_pdata spa_pb_pdata = {
	.chrgr_name = "bcmpmu_charger",
};
#endif /*CONFIG_CHARGER_BCMPMU_SPA*/

#ifdef CONFIG_SEC_CHARGING_FEATURE
struct spa_power_data spa_data = {
	.charger_name = "bcmpmu_charger",

#ifdef CONFIG_SEC_MAKE_LCD_TEST
	.suspend_temp_hot   =  1000,
	.recovery_temp_hot  =  400,
	.suspend_temp_cold  = -100,
	.recovery_temp_cold = 0,
#else
	.suspend_temp_hot	=  600,
	.recovery_temp_hot	=  460,
	.suspend_temp_cold	= -50,
	.recovery_temp_cold = 0,
#endif

#if defined(CONFIG_SPA_SUPPLEMENTARY_CHARGING)
	.eoc_current = 150,
	.backcharging_time = 30, /*mins*/
	.recharging_eoc = 60,
#else
	.eoc_current = 100,
#endif
	.recharge_voltage = 4300,
	.charging_cur_usb = 500,
	.charging_cur_wall = 651,
	.charge_timer_limit = CHARGE_TIMER_6HOUR,
};

static struct platform_device spa_power_device = {
	.name = "spa_power",
	.id = -1,
	.dev.platform_data = &spa_data,
};

static struct platform_device spa_ps_device = {
	.name = "spa_ps",
	.id = -1,
};

static struct platform_device *spa_devices[] = {
	&spa_power_device,
	&spa_ps_device,
};
#endif /*CONFIG_SEC_CHARGING_FEATURE*/

/* The subdevices of the bcmpmu59xxx */
static struct mfd_cell pmu59xxx_devs[] = {
	{
		.name = "bcmpmu59xxx-regulator",
		.id = -1,
		.platform_data = &rgltr_pdata,
		.pdata_size = sizeof(rgltr_pdata),
	},
	{
		.name = "bcmpmu_charger",
		.id = -1,
	},
	{
		.name = "bcmpmu59xxx-ponkey",
		.id = -1,
/* logan compilation fix */
		.platform_data = &pkey_pdata,
		.pdata_size = sizeof(pkey_pdata),
/*		.platform_data = &pok_pdata, */
/*		.pdata_size = sizeof(pok_pdata), */
	},
	{
		.name = "bcmpmu59xxx_rtc",
		.id = -1,
	},
	{
		.name = "bcmpmu_audio",
		.id = -1,
		.platform_data = &audio_pdata,
		.pdata_size = sizeof(audio_pdata),
	},
	{
		.name = "bcmpmu_accy",
		.id = -1,
		.platform_data = &accy_pdata,
		.pdata_size = sizeof(accy_pdata),
	},
	{
		.name = "bcmpmu_adc",
		.id = -1,
		.platform_data = adc_pdata,
		.pdata_size = sizeof(adc_pdata),
	},
#ifdef CONFIG_CHARGER_BCMPMU_SPA
	{
		.name = "bcmpmu_spa_pb",
		.id = -1,
		.platform_data = &spa_pb_pdata,
		.pdata_size = sizeof(spa_pb_pdata),
	},
#endif /*CONFIG_CHARGER_BCMPMU_SPA*/
	{
		.name = "bcmpmu_otg_xceiv",
		.id = -1,
	},
	{
		.name = "bcmpmu_rpc",
		.id = -1,
		.platform_data = &rpc_pdata,
		.pdata_size = sizeof(rpc_pdata),
	},
	{
		.name = "bcmpmu_fg",
		.id = -1,
		.platform_data = &fg_pdata,
		.pdata_size = sizeof(fg_pdata),
	},

};

static struct i2c_board_info pmu_i2c_companion_info[] = {
	{
	I2C_BOARD_INFO("bcmpmu_map1", PMU_DEVICE_I2C_ADDR1),
	},
};

static struct bcmpmu59xxx_platform_data bcmpmu_i2c_pdata = {
#if defined(CONFIG_KONA_PMU_BSC_HS_MODE)
	.i2c_pdata = { ADD_I2C_SLAVE_SPEED(BSC_BUS_SPEED_HS), },
#elif defined(CONFIG_KONA_PMU_BSC_HS_1MHZ)
	.i2c_pdata = { ADD_I2C_SLAVE_SPEED(BSC_BUS_SPEED_HS_1MHZ), },
#elif defined(CONFIG_KONA_PMU_BSC_HS_1625KHZ)
	.i2c_pdata = { ADD_I2C_SLAVE_SPEED(BSC_BUS_SPEED_HS_1625KHZ), },
#else
	.i2c_pdata = { ADD_I2C_SLAVE_SPEED(BSC_BUS_SPEED_50K), },
#endif
	.init = bcmpmu_init_platform_hw,
	.exit = bcmpmu_exit_platform_hw,
	.companion = BCMPMU_DUMMY_CLIENTS,
	.i2c_companion_info = pmu_i2c_companion_info,
	.i2c_adapter_id = PMU_DEVICE_I2C_BUSNO,
	.i2c_pagesize = 256,
	.init_data = register_init_data,
	.init_max = ARRAY_SIZE(register_init_data),
#ifdef CONFIG_CHARGER_BCMPMU_SPA
	.flags = BCMPMU_SPA_EN,
	.bc = BC_EXT_DETECT,
#else
	.bc = BCMPMU_BC_PMU_BC12,
#endif
	/* Assign PMU_ADC_REQ_NO_FORCE_MODE, for disabling force mode */
	.force_adc_mode = PMU_ADC_REQ_RTM_MODE,
	.bc = BC_EXT_DETECT,
	.charger_path = BCMPMU_OUTPUT_REGULATION,
	//.charger_path = BCMPMU_INPUT_REGULATION,
};

static struct i2c_board_info __initdata bcmpmu_i2c_info[] = {
	{
		I2C_BOARD_INFO("bcmpmu59xxx_i2c", PMU_DEVICE_I2C_ADDR),
		.platform_data = &bcmpmu_i2c_pdata,
		.irq = gpio_to_irq(PMU_DEVICE_INT_GPIO),
	},
};

int bcmpmu_get_pmu_mfd_cell(struct mfd_cell **pmu_cell)
{
	*pmu_cell  = pmu59xxx_devs;
	return ARRAY_SIZE(pmu59xxx_devs);
}
EXPORT_SYMBOL(bcmpmu_get_pmu_mfd_cell);

void bcmpmu_set_pullup_reg(void)
{
	u32 val1, val2;

	val1 = readl(KONA_CHIPREG_VA + CHIPREG_SPARE_CONTROL0_OFFSET);
	val2 = readl(KONA_PMU_BSC_VA + I2C_MM_HS_PADCTL_OFFSET);
	val1 |= (1 << 20 | 1 << 22);
	val2 |= (1 << I2C_MM_HS_PADCTL_PULLUP_EN_SHIFT);
	writel(val1, KONA_CHIPREG_VA + CHIPREG_SPARE_CONTROL0_OFFSET);
	/*      writel(val2, KONA_PMU_BSC_VA + I2C_MM_HS_PADCTL_OFFSET); */
}

static int bcmpmu_init_platform_hw(struct bcmpmu59xxx *bcmpmu)
{
	return 0;
}

static int bcmpmu_exit_platform_hw(struct bcmpmu59xxx *bcmpmu)
{
	pr_info("REG: pmu_exit_platform_hw called\n");
	return 0;
}

int __init board_bcm59xx_init(void)
{
	int             ret = 0;
	int             irq;

	bcmpmu_set_pullup_reg();
	ret = gpio_request(PMU_DEVICE_INT_GPIO, "bcmpmu59xxx-irq");
	if (ret < 0) {
		printk(KERN_ERR "<%s> failed at gpio_request\n", __func__);
		goto exit;
	}
	ret = gpio_direction_input(PMU_DEVICE_INT_GPIO);
	if (ret < 0) {

		printk(KERN_ERR "%s filed at gpio_direction_input.\n",
				__func__);
		goto exit;
	}
	irq = gpio_to_irq(PMU_DEVICE_INT_GPIO);
	bcmpmu_i2c_pdata.irq = irq;
	ret  = i2c_register_board_info(PMU_DEVICE_I2C_BUSNO,
			bcmpmu_i2c_info, ARRAY_SIZE(bcmpmu_i2c_info));
#if defined(CONFIG_SEC_CHARGING_FEATURE)
	platform_add_devices(spa_devices, ARRAY_SIZE(spa_devices));
#endif
/* Workaround for VDDFIX leakage during deepsleep.
   Will be fixed in Java A1 revision */
	if (is_pm_erratum(ERRATUM_VDDFIX_LEAKAGE))
		bcm59xxx_csr_data.constraints.initial_mode =
			REGULATOR_MODE_IDLE;
	return 0;
exit:
	return ret;
}

__init int board_pmu_init(void)
{
	return board_bcm59xx_init();
}
arch_initcall(board_pmu_init);