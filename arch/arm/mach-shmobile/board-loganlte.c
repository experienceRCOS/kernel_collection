/*
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
#include <linux/dma-mapping.h>
#include <mach/irqs.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/hwspinlock.h>
#include <linux/pinctrl/machine.h>
#include <linux/pinctrl/pinconf-generic.h>
#include <linux/platform_data/rt_boot_pdata.h>
#include <mach/common.h>
#include <mach/r8a7373.h>
#include <mach/gpio.h>
#include <mach/setup-u2usb.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <linux/mmc/host.h>
#include <video/sh_mobile_lcdc.h>
#include <linux/irqchip/arm-gic.h>
#include <mach/poweroff.h>
#include <mach/dev-wifi.h>
#ifdef CONFIG_MFD_D2153
#include <linux/d2153/core.h>
#include <linux/d2153/d2153_aad.h>
#include <linux/d2153/pmic.h>
#endif
#include <linux/ktd259b_bl.h>
#include <mach/setup-u2spa.h>
#include <mach/setup-u2vibrator.h>
#include <mach/setup-u2ion.h>
#include <mach/setup-u2rcu.h>
#include <mach/setup-u2camera.h>
#include <linux/proc_fs.h>
#if defined(CONFIG_RENESAS_GPS)|| defined(CONFIG_GPS_CSR_GSD5T)
#include <mach/dev-gps.h>
#endif
#if defined(CONFIG_RENESAS_NFC)
#ifdef CONFIG_PN544_NFC
#include <mach/dev-renesas-nfc.h>
#endif
#endif
#if defined(CONFIG_SAMSUNG_MHL)
#include <mach/dev-edid.h>
#endif
#ifdef CONFIG_USB_OTG
#include <linux/usb/tusb1211.h>
#endif
#if defined(CONFIG_GPS_BCM4752)
#include <mach/dev-gps.h>
#endif
#if defined(CONFIG_SEC_DEBUG)
#include <mach/sec_debug.h>
#include <mach/sec_debug_inform.h>
#endif
#if defined(CONFIG_SND_SOC_SH4_FSI)
#include <mach/setup-u2audio.h>
#endif /* CONFIG_SND_SOC_SH4_FSI */
#include <linux/i2c/fm34_we395.h>
#if defined(CONFIG_PN547_NFC) || defined(CONFIG_NFC_PN547)
#include <linux/nfc/pn547.h>
#endif
#if defined(CONFIG_SAMSUNG_SENSOR)
#include <mach/dev-sensor.h>
#endif
#if defined(CONFIG_BCMI2CNFC) || defined(CONFIG_PN547_NFC)  || defined(CONFIG_NFC_PN547)
#include <mach/dev-nfc.h>
#endif

#include <mach/dev-bt.h>

#include <mach/setup-u2stm.h>

#ifdef CONFIG_MFD_RT8973
#include <mach/dev-muic_rt8973.h>
#endif

#include "sh-pfc.h"
#include <linux/bcm.h>
#include <mach/setup-u2fb.h>

static int unused_gpios_logan_rev1[] = {
				GPIO_PORT4, GPIO_PORT27, GPIO_PORT33, GPIO_PORT36, GPIO_PORT104,
				GPIO_PORT140, GPIO_PORT141, GPIO_PORT142, GPIO_PORT198,
				GPIO_PORT200, GPIO_PORT201, GPIO_PORT219, GPIO_PORT224,
				GPIO_PORT225, GPIO_PORT226, GPIO_PORT227, GPIO_PORT228,
				GPIO_PORT229, GPIO_PORT230, GPIO_PORT231, GPIO_PORT232,
				GPIO_PORT233, GPIO_PORT234, GPIO_PORT235, GPIO_PORT236,
				GPIO_PORT237, GPIO_PORT238, GPIO_PORT239, GPIO_PORT240,
				GPIO_PORT241, GPIO_PORT242, GPIO_PORT243, GPIO_PORT244,
				GPIO_PORT245, GPIO_PORT246, GPIO_PORT247, GPIO_PORT248,
				GPIO_PORT249, GPIO_PORT250, GPIO_PORT251, GPIO_PORT252,
				GPIO_PORT253, GPIO_PORT254, GPIO_PORT255, GPIO_PORT256,
				GPIO_PORT257, GPIO_PORT258, GPIO_PORT259, GPIO_PORT271,
				GPIO_PORT275, GPIO_PORT276, GPIO_PORT277, GPIO_PORT294,
				GPIO_PORT295, GPIO_PORT296, GPIO_PORT297, GPIO_PORT298,
				GPIO_PORT299, GPIO_PORT311, GPIO_PORT312, GPIO_PORT325
};

static int unused_gpios_logan_rev2[] = {
				GPIO_PORT4, GPIO_PORT21, GPIO_PORT26, GPIO_PORT27, GPIO_PORT36,
				GPIO_PORT44, GPIO_PORT46, GPIO_PORT86, GPIO_PORT87,
				GPIO_PORT104, GPIO_PORT140, GPIO_PORT141, GPIO_PORT142,
				GPIO_PORT198, GPIO_PORT199, GPIO_PORT200, GPIO_PORT201,
				GPIO_PORT202, GPIO_PORT219, GPIO_PORT224, GPIO_PORT225,
				GPIO_PORT226, GPIO_PORT227, GPIO_PORT228, GPIO_PORT229,
				GPIO_PORT230, GPIO_PORT231, GPIO_PORT232, GPIO_PORT233,
				GPIO_PORT234, GPIO_PORT235, GPIO_PORT236, GPIO_PORT237,
				GPIO_PORT238, GPIO_PORT239, GPIO_PORT240, GPIO_PORT241,
				GPIO_PORT242, GPIO_PORT243, GPIO_PORT244, GPIO_PORT245,
				GPIO_PORT246, GPIO_PORT247, GPIO_PORT248, GPIO_PORT249,
				GPIO_PORT250, GPIO_PORT251, GPIO_PORT252, GPIO_PORT253,
				GPIO_PORT254, GPIO_PORT255, GPIO_PORT256, GPIO_PORT257,
				GPIO_PORT258, GPIO_PORT259, GPIO_PORT271, GPIO_PORT275,
				GPIO_PORT276, GPIO_PORT277, GPIO_PORT294, GPIO_PORT295,
				GPIO_PORT296, GPIO_PORT297, GPIO_PORT298, GPIO_PORT299,
				GPIO_PORT311, GPIO_PORT312, GPIO_PORT325,
};


#ifdef CONFIG_MFD_RT8973
	extern void hawaii_muic_init(void);
#endif
void (*shmobile_arch_reset)(char mode, const char *cmd);

static int board_rev_proc_show(struct seq_file *s, void *v)
{
	seq_printf(s, "%x", system_rev);

	return 0;
}

static int board_rev_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, board_rev_proc_show, NULL);
}

static const struct file_operations board_rev_ops = {
	.open = board_rev_proc_open,
	.read = seq_read,
	.release = single_release,
};

struct fm34_platform_data fm34_data = {
	.set_mclk = NULL,
	.gpio_pwdn = GPIO_PORT26,
	.gpio_rst = GPIO_PORT44,
	.gpio_bp = GPIO_PORT46,
};

/* D2153 setup */
#ifdef CONFIG_MFD_D2153
/*TODO: move these externs to header*/
extern struct d2153_regl_init_data
	      d2153_regulators_init_data[D2153_NUMBER_OF_REGULATORS];
extern struct d2153_regl_map regl_map[D2153_NUMBER_OF_REGULATORS];

struct d2153_platform_data d2153_pdata = {
	.hwmon_pdata = &d2153_adc_pdata,
	.pbat_platform  = &pbat_pdata,
	.regulator_data = d2153_regulators_init_data,
	.regl_map = regl_map,
};
#define LDO_CONSTRAINTS(__id)					\
	(d2153_pdata.regulator_data[__id].initdata->constraints)

#define SET_LDO_ALWAYS_ON(__id) (LDO_CONSTRAINTS(__id).always_on = 1)

#define SET_LDO_BOOT_ON(__id) (LDO_CONSTRAINTS(__id).boot_on = 1)

#define SET_LDO_APPLY_UV(__id) (LDO_CONSTRAINTS(__id).apply_uV = true)

static void __init d2153_init_board_defaults(void)
{
	SET_LDO_ALWAYS_ON(D2153_BUCK_1); /* VCORE */
	SET_LDO_ALWAYS_ON(D2153_BUCK_2); /* VIO2 */
	SET_LDO_ALWAYS_ON(D2153_BUCK_3); /* VIO1 */
	SET_LDO_ALWAYS_ON(D2153_BUCK_4); /* VCORE_RF */
	SET_LDO_ALWAYS_ON(D2153_BUCK_5); /* VANA1_RF */
	SET_LDO_ALWAYS_ON(D2153_BUCK_6); /* VPAM */

	/* VDIG_RF */
	SET_LDO_ALWAYS_ON(D2153_LDO_1);

	/* VMMC */
	SET_LDO_ALWAYS_ON(D2153_LDO_3);
	SET_LDO_APPLY_UV(D2153_LDO_3);

	/* VVCTCXO */
	SET_LDO_ALWAYS_ON(D2153_LDO_4);

	/* VMIPI */
	SET_LDO_ALWAYS_ON(D2153_LDO_5);

	/* VDD_MOTOR */
	SET_LDO_APPLY_UV(D2153_LDO_16);

	/* This is assumed with device tree, so always set it for consistency */
	regulator_has_full_constraints();
}

#endif

/* I2C */
static struct i2c_board_info __initdata i2c0_devices_d2153[] = {
#if defined(CONFIG_MFD_D2153)
	{
		/* for D2153 PMIC driver */
		I2C_BOARD_INFO("d2153", D2153_PMIC_I2C_ADDR),
		.platform_data = &d2153_pdata,
		.irq = irq_pin(28),
	},
#endif /* CONFIG_MFD_D2153 */
};

static struct platform_ktd259b_backlight_data bcm_ktd259b_backlight_data = {
	.max_brightness = 255,
	.dft_brightness = 143,
	.ctrl_pin = 47,
};

static struct platform_device bcm_backlight_devices = {
	.name = "ktd259b_backlight",
	.id   = -1,
	.dev  = {
		.platform_data = &bcm_ktd259b_backlight_data,
	},
};

static struct i2c_board_info __initdata i2c3_devices[] = {
#if defined(CONFIG_USE_MUIC)
	{
		I2C_BOARD_INFO("rt8973", (0x28 >> 1)),
			.platform_data = NULL,
			.irq           = irq_pin(41),
	},
#endif
#if defined(CONFIG_RT8973)
	{
		I2C_BOARD_INFO("rt8973", 0x28>>1),
		.platform_data = NULL,
		.irq = irq_pin(41),
	},
#endif
#if defined(CONFIG_CHARGER_SMB328A)
	{
		I2C_BOARD_INFO("smb328a", (0xA9 >> 1)),
		.irq            = irq_pin(19),
	},
#endif

};

/* Rhea Ray specific platform devices */
static struct platform_device *plat_devices[] __initdata = {
	&bcm_backlight_devices,
};

#if 0
static struct led_regulator_platform_data key_backlight_data = {
	.name   = "button-backlight",
};

static struct platform_device key_backlight_device = {
	.name = "leds-regulator",
	.id   = 0,
	.dev  = {
		.platform_data = &key_backlight_data,
	},
};
#endif


static struct i2c_board_info i2cm_devices_d2153[] = {
	{
		I2C_BOARD_INFO(FM34_MODULE_NAME, 0x60),
		.platform_data = &fm34_data,
	},
};

static struct rt_boot_platform_data rt_boot_pdata = {
	.screen0 = {
		.height = 800,
		.width = 480,
		.stride = 480,
		.mode = 1,
	},
	.screen1 = {
		.height = 0,
		.width = 0,
		.stride = 0,
		.mode = 0,
	},
};

static struct platform_device rt_boot_device = {
	.name = "rt_boot",
	.dev.platform_data = &rt_boot_pdata,
};

void board_restart(char mode, const char *cmd)
{
	printk(KERN_INFO "%s\n", __func__);
	shmobile_do_restart(mode, cmd, APE_RESETLOG_U2EVM_RESTART);
}

static unsigned long pin_pullup_conf[] __maybe_unused = {
	PIN_CONF_PACKED(PIN_CONFIG_BIAS_PULL_UP, 1),
};

static struct pinctrl_map loganlte_pinctrl_map[] __initdata = {
	SH_PFC_MUX_GROUP_DEFAULT("renesas_mmcif.0",
				 "mmc0_data8", "mmc0"),
	SH_PFC_MUX_GROUP_DEFAULT("renesas_mmcif.0",
				 "mmc0_ctrl", "mmc0"),
	SH_PFC_MUX_GROUP_DEFAULT("renesas_sdhi.0",
				 "sdhi0_data4", "sdhi0"),
	SH_PFC_MUX_GROUP_DEFAULT("renesas_sdhi.0",
				 "sdhi0_ctrl", "sdhi0"),
	SH_PFC_MUX_GROUP_DEFAULT("renesas_sdhi.0",
				 "sdhi0_cd", "sdhi0"),
};

int __init u2fb_reserve()
{
	return setup_u2fb_reserve(&rt_boot_pdata.screen0);
}

static void __init board_init(void)
{
	int stm_select = -1;    // Shall tell how to route STM traces. See setup-u2stm.c for details.
	int inx = 0;

	r8a7373_zq_calibration();

	r8a7373_avoid_a2slpowerdown_afterL2sync();
	sh_pfc_register_mappings(loganlte_pinctrl_map,
				 ARRAY_SIZE(loganlte_pinctrl_map));
	r8a7373_pinmux_init();

	if (!proc_create("board_revision", 0444, NULL, &board_rev_ops))
		pr_warn("creation of /proc/board_revision failed\n");

	stm_select = u2evm_init_stm_select();

	r8a7373_add_standard_devices();

	/* r8a7373_hwlock_gpio request has moved to pfc-r8a7373.c */
	r8a7373_hwlock_cpg = hwspin_lock_request_specific(SMCPG);
	r8a7373_hwlock_sysc = hwspin_lock_request_specific(SMSYSC);

	if (!shmobile_is_older(U2_VERSION_2_0)) {
		__raw_writew(0x0022, GPIO_DRVCR_SD0);
		__raw_writew(0x0022, GPIO_DRVCR_SIM1);
		__raw_writew(0x0022, GPIO_DRVCR_SIM2);
	}
	shmobile_arch_reset = board_restart;

	printk(KERN_INFO "%s hw rev : %d\n", __func__, system_rev);

	/* Init unused GPIOs */
	if (system_rev <= BOARD_REV_010) {
		for (inx = 0; inx < ARRAY_SIZE(unused_gpios_logan_rev1); inx++)
			unused_gpio_port_init(unused_gpios_logan_rev1[inx]);
	} else {
		for (inx = 0; inx < ARRAY_SIZE(unused_gpios_logan_rev2); inx++)
			unused_gpio_port_init(unused_gpios_logan_rev2[inx]);
	}

#ifdef CONFIG_KEYBOARD_SH_KEYSC
	/* enable KEYSC */
	gpio_request(GPIO_FN_KEYIN0, NULL);
	gpio_request(GPIO_FN_KEYIN1, NULL);
	gpio_request(GPIO_FN_KEYIN2, NULL);
	gpio_request(GPIO_FN_KEYIN3, NULL);
	gpio_request(GPIO_FN_KEYIN4, NULL);
	gpio_request(GPIO_FN_KEYIN5, NULL);
	gpio_request(GPIO_FN_KEYIN6, NULL);

	gpio_pull_up_port(GPIO_PORT44);
	gpio_pull_up_port(GPIO_PORT45);
	gpio_pull_up_port(GPIO_PORT46);
	gpio_pull_up_port(GPIO_PORT47);
	gpio_pull_up_port(GPIO_PORT48);
	gpio_pull_up_port(GPIO_PORT96);
	gpio_pull_up_port(GPIO_PORT97);
#endif

	/* MMC RESET - does anyone use this? */
	gpio_request(GPIO_FN_MMCRST, NULL);

	/* Disable GPIO Enable at initialization */

	/* ===== CWS GPIO ===== */

#if defined(CONFIG_RENESAS_NFC)
	nfc_gpio_init();
#endif

	gpio_direction_none_port(GPIO_PORT309);

	if (0 != stm_select) {
		/* If STM Traces go to SDHI1 or NOWHERE, then SDHI0 can be used
		   for SD-Card */

		/* SDHI0 */
		gpio_request(GPIO_FN_SDHID0_0, NULL);
		gpio_request(GPIO_FN_SDHID0_1, NULL);
		gpio_request(GPIO_FN_SDHID0_2, NULL);
		gpio_request(GPIO_FN_SDHID0_3, NULL);
		gpio_request(GPIO_FN_SDHICMD0, NULL);
		gpio_direction_none_port(GPIO_PORT326);
		gpio_request(GPIO_FN_SDHICLK0, NULL);
		gpio_request(GPIO_PORT327, NULL);
		gpio_direction_input(GPIO_PORT327);
		gpio_pull_off_port(GPIO_PORT327);
		irq_set_irq_type(irq_pin(50), IRQ_TYPE_EDGE_BOTH);
		gpio_set_debounce(GPIO_PORT327, 1000);	/* 1msec */
		gpio_free(GPIO_PORT327);
		gpio_request(GPIO_FN_SDHICD0, NULL);
	}

	/* ES2.0: SIM powers */
	__raw_writel(__raw_readl(MSEL3CR) | (1<<27), MSEL3CR);

	/* WLAN Init and SDIO device call */
	if (1 != stm_select) {
		/* SDHI1 for WLAN */
		gpio_request(GPIO_FN_SDHID1_0, NULL);
		gpio_request(GPIO_FN_SDHID1_1, NULL);
		gpio_request(GPIO_FN_SDHID1_2, NULL);
		gpio_request(GPIO_FN_SDHID1_3, NULL);
		gpio_request(GPIO_FN_SDHICMD1, NULL);
		gpio_request(GPIO_FN_SDHICLK1, NULL);

		gpio_pull_up_port(GPIO_PORT293);
		gpio_pull_up_port(GPIO_PORT292);
		gpio_pull_up_port(GPIO_PORT291);
		gpio_pull_up_port(GPIO_PORT290);
		gpio_pull_up_port(GPIO_PORT289);
		/* move gpio request to board-renesas_wifi.c */
                /* WLAN Init API call */
#if defined(CONFIG_BRCM_UNIFIED_DHD_SUPPORT) || defined(CONFIG_RENESAS_WIFI)
                printk(KERN_ERR "Calling WLAN_INIT!\n");
                renesas_wlan_init();
                printk(KERN_ERR "DONE WLAN_INIT!\n");
#endif
	}

	gpio_request(GPIO_PORT28, NULL);
	gpio_direction_input(GPIO_PORT28);
	gpio_pull_up_port(GPIO_PORT28);

#if defined(CONFIG_MFD_D2153)
	irq_set_irq_type(irq_pin(28), IRQ_TYPE_LEVEL_LOW);
#endif /* CONFIG_MFD_D2153 */

	/* Touch */
	gpio_request(GPIO_PORT32, NULL);
	gpio_direction_input(GPIO_PORT32);
	gpio_pull_up_port(GPIO_PORT32);

	usb_init(true);

#if defined(CONFIG_SND_SOC_SH4_FSI)
	if (system_rev < BOARD_REV_010)
		d2153_pdata.audio.fm34_device = DEVICE_EXIST;
	else
		d2153_pdata.audio.fm34_device = DEVICE_NONE;

	if ((BOARD_REV_010 < system_rev) &&
		(BOARD_REV_040 > system_rev)) {
			d2153_pdata.audio.aad_codec_detect_enable = true;
			d2153_pdata.audio.debounce_ms = D2153_AAD_JACK_DEBOUNCE_MS;
			d2153_pdata.audio.debounce_ms -= D2153_AAD_MICBIAS_SETUP_TIME_MS;
	} else {
			d2153_pdata.audio.aad_codec_detect_enable = false;
			d2153_pdata.audio.debounce_ms = D2153_AAD_JACK_DEBOUNCE_MS;
	}
	u2audio_init();
#endif /* CONFIG_SND_SOC_SH4_FSI */

#ifndef CONFIG_ARM_TZ
	r8a7373_l2cache_init();
#else
	/**
	 * In TZ-Mode of R-Mobile U2, it must notify the L2 cache
	 * related info to Secure World. However, SEC_HAL driver is not
	 * registered at the time of L2$ init because "r8a7373l2_cache_init()"
	 * function called more early.
	 */
	l2x0_init_later();
#endif

	add_primary_cam_flash_mic2871(GPIO_PORT99, GPIO_PORT100);
	add_s5k4ecgx_primary_camera();
	add_sr030pc50_secondary_camera();
	camera_init(GPIO_PORT3, GPIO_PORT20, GPIO_PORT45);

	u2_add_ion_device();
	u2_add_rcu_devices();
	add_bcmbt_lpm_device(GPIO_PORT262, GPIO_PORT272);

	d2153_init_board_defaults();
	i2c_register_board_info(0, i2c0_devices_d2153,
					ARRAY_SIZE(i2c0_devices_d2153));
/* GPS Init */
#if defined(CONFIG_RENESAS_GPS)
	gps_gpio_init();
#endif

#if defined(CONFIG_SAMSUNG_SENSOR)
	board_sensor_init();
#endif

#if defined(CONFIG_CHARGER_SMB328A)
	/* rev0.0 uses SMB328A, rev0.1 uses SMB327B */
	if (system_rev == BOARD_REV_000 || system_rev > BOARD_REV_040) {
		int i;
		for (i = 0; i < sizeof(i2c3_devices)/sizeof(struct i2c_board_info); i++) {
			if (strcmp(i2c3_devices[i].type, "smb328a")==0) {
				i2c3_devices[i].addr = (0x69 >> 1);
			}
		}
	}
#endif

	i2c_register_board_info(3, i2c3_devices, ARRAY_SIZE(i2c3_devices));
	/* I2CM interface used only on revision 0.0, for FM34 codec */
	if (system_rev == BOARD_REV_000)
		i2c_register_board_info(8, i2cm_devices_d2153,
					ARRAY_SIZE(i2cm_devices_d2153));

#if defined(CONFIG_GPS_BCM4752)
	/* GPS Init */
	gps_gpio_init();
#endif

	platform_add_devices(plat_devices,
					ARRAY_SIZE(plat_devices));
	/* panel intillization */
	r8a7373_set_panel_func(r_mobile_nt35510_panel_func);
#ifdef CONFIG_SEC_CHARGING_FEATURE
	/* PA devices init */
	spa_init();
#endif
	bcm_init();
#ifdef CONFIG_VIBRATOR_SS
	ss_vibrator_data.voltage = 2800000;
#endif
	u2_vibrator_init();

	printk(KERN_DEBUG "%s\n", __func__);

#if defined(CONFIG_PN547_NFC) || defined(CONFIG_NFC_PN547)
	pn547_device_i2c_register();
#endif
	platform_device_register(&rt_boot_device);
	
#ifdef CONFIG_MFD_RT8973
	dev_muic_rt8973_init();
#endif
}

static const char *logan_compat_dt[] __initdata = {
	"renesas,loganlte",
	NULL,
};

DT_MACHINE_START(LOGANLTE, "loganlte")
	.smp		= smp_ops(r8a7373_smp_ops),
	.map_io		= r8a7373_map_io,
	.init_irq       = r8a7373_init_irq,
	.init_early     = r8a7373_init_early,
	.init_machine   = board_init,
	.init_time	= r8a7373_timers_init,
	.init_late      = r8a7373_init_late,
	.restart        = board_restart,
	.reserve        = r8a7373_reserve,
	.dt_compat	= logan_compat_dt,
MACHINE_END
