/*
 * drivers/video/r-mobile/panel/panel_s6e88a0.c
 *
 * Copyright (C) 2013 Renesas Electronics Corporation
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

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ratelimit.h>

#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <mach/gpio.h>

#include <video/sh_mobile_lcdc.h>

#include <rtapi/screen_display.h>


#include <linux/platform_device.h>
#include <linux/fb.h>

#include <linux/regulator/consumer.h>
#include <linux/lcd.h>

#include <mach/memory-r8a7373.h>

/* #define S6E88A0_DRAW_BLACK_KERNEL */

#define S6E88A0_POWAREA_MNG_ENABLE
/* #define S6E88A0_GED_ORG */

/* #define S6E88A0_SWITCH_FRAMERATE_40HZ */

#ifdef S6E88A0_POWAREA_MNG_ENABLE
#include <rtapi/system_pwmng.h>
#endif
#include "s6e88a0_param.h"
#include "dynamic_aid_s6e88a0.h"
#include "panel_common.h"

/* panel size (mm) */
#define R_MOBILE_M_PANEL_SIZE_WIDTH	57
#define R_MOBILE_M_PANEL_SIZE_HEIGHT	94

#define R_MOBILE_M_PANEL_PIXEL_WIDTH	 480
#define R_MOBILE_M_PANEL_PIXEL_HEIGHT	 800
#define R_MOBILE_M_PANEL_LEFT_MARGIN	 8
#define R_MOBILE_M_PANEL_RIGHT_MARGIN	 340
#define R_MOBILE_M_PANEL_HSYNC_LEN	 8
#define R_MOBILE_M_PANEL_UPPER_MARGIN	 2
#define R_MOBILE_M_PANEL_LOWER_MARGIN	 13
#define R_MOBILE_M_PANEL_VSYNC_LEN	 1
#define R_MOBILE_M_PANEL_PIXCLOCK	 24439
#define R_MOBILE_M_PANEL_H_TOTAL	 836
#define R_MOBILE_M_PANEL_V_TOTAL	 816
#define LCD_DSITCKCR		0x00000007
#define LCD_DSI0PCKCR		0x0000003C
#define LCD_DSI0PHYCR		0x2A80001F
#define LCD_SYSCONF		0x00000F03
#define LCD_TIMSET0		0x4C2C6332
#define LCD_TIMSET1		0x00080092
#define LCD_DSICTRL		0x00000001
#define LCD_VMCTR1		0x0001003E
#define LCD_VMCTR2		0x00000410
#define LCD_VMLEN1		0x05A00000
#define LCD_VMLEN2		0x00160000
#define LCD_VMLEN3		0x00000000
#define LCD_VMLEN4		0x00000000
#define LCD_DTCTR		0x00000007
#define LCD_MLDHCNR		0x003C0068
#define LCD_MLDHSYNR		0x00010066
#define LCD_MLDHAJR		0x00040004
#define LCD_MLDVLNR		0x03200330
#define LCD_MLDVSYNR		0x0001032E
#define LCD_MLDMT1R		0x0400000B
#define LCD_LDDCKR		0x00010040
#define LCD_MLDDCKPAT1R		0x00000000
#define LCD_MLDDCKPAT2R		0x00000000
#define LCD_PHYTEST		0x0000038C

#define LCD_MASK_DSITCKCR	0x000000BF
#define LCD_MASK_DSI0PCKCR	0x0000703F
#define LCD_MASK_DSI0PHYCR	0x000000FF
#define LCD_MASK_SYSCONF	0x00000F0F
#define LCD_MASK_TIMSET0	0x7FFFF7F7
#define LCD_MASK_TIMSET1	0x003F03FF
#define LCD_MASK_DSICTRL	0x00000601
#define LCD_MASK_VMCTR1		0x00F3F03F
#define LCD_MASK_VMCTR2		0x07E2073B
#define LCD_MASK_VMLEN1		0xFFFFFFFF
#define LCD_MASK_VMLEN2		0xFFFFFFFF
#define LCD_MASK_VMLEN3		0xFFFFFFFF
#define LCD_MASK_VMLEN4		0xFFFF0000
#define LCD_MASK_DTCTR		0x00000002
#define LCD_MASK_MLDHCNR	0x07FF07FF
#define LCD_MASK_MLDHSYNR	0x001F07FF
#define LCD_MASK_MLDHAJR	0x07070707
#define LCD_MASK_MLDVLNR	0x1FFF1FFF
#define LCD_MASK_MLDVSYNR	0x001F1FFF
#define LCD_MASK_MLDMT1R	0x1F03FCCF
#define LCD_MASK_LDDCKR		0x0007007F
#define LCD_MASK_MLDDCKPAT1R	0x0FFFFFFF
#define LCD_MASK_MLDDCKPAT2R	0xFFFFFFFF
#define LCD_MASK_PHYTEST	0x000003CC


#define POWER_IS_ON(pwr)	((pwr) <= FB_BLANK_NORMAL)
static int S6E88A0_panel_suspend(void);
static int S6E88A0_panel_resume(void);
static void mipi_display_reset(void);
static void mipi_display_power_off(void);
static int S6E88A0_InitSequence(void *lcd_handle);


static struct fb_panel_info r_mobile_info = {
	.pixel_width	= R_MOBILE_M_PANEL_PIXEL_WIDTH,
	.pixel_height	= R_MOBILE_M_PANEL_PIXEL_HEIGHT,
	.size_width	= R_MOBILE_M_PANEL_SIZE_WIDTH,
	.size_height	= R_MOBILE_M_PANEL_SIZE_HEIGHT,
	.pixclock      = R_MOBILE_M_PANEL_PIXCLOCK,
	.left_margin   = R_MOBILE_M_PANEL_LEFT_MARGIN,
	.right_margin  = R_MOBILE_M_PANEL_RIGHT_MARGIN,
	.upper_margin  = R_MOBILE_M_PANEL_UPPER_MARGIN,
	.lower_margin  = R_MOBILE_M_PANEL_LOWER_MARGIN,
	.hsync_len     = R_MOBILE_M_PANEL_HSYNC_LEN,
	.vsync_len     = R_MOBILE_M_PANEL_VSYNC_LEN,
};

static screen_disp_lcd_if r_mobile_lcd_if_param = {
	.dsitckcr    = LCD_DSITCKCR,
	.dsi0pckcr   = LCD_DSI0PCKCR,
	.dsi0phycr   = LCD_DSI0PHYCR,
	.sysconf     = LCD_SYSCONF,
	.timset0     = LCD_TIMSET0,
	.timset1     = LCD_TIMSET1,
	.dsictrl     = LCD_DSICTRL,
	.vmctr1      = LCD_VMCTR1,
	.vmctr2      = LCD_VMCTR2,
	.vmlen1      = LCD_VMLEN1,
	.vmlen2      = LCD_VMLEN2,
	.vmlen3      = LCD_VMLEN3,
	.vmlen4      = LCD_VMLEN4,
	.dtctr       = LCD_DTCTR,
	.mldhcnr     = LCD_MLDHCNR,
	.mldhsynr    = LCD_MLDHSYNR,
	.mldhajr     = LCD_MLDHAJR,
	.mldvlnr     = LCD_MLDVLNR,
	.mldvsynr    = LCD_MLDVSYNR,
	.mldmt1r     = LCD_MLDMT1R,
	.lddckr      = LCD_LDDCKR,
	.mlddckpat1r = LCD_MLDDCKPAT1R,
	.mlddckpat2r = LCD_MLDDCKPAT2R,
	.phytest     = LCD_PHYTEST,
};

static screen_disp_lcd_if r_mobile_lcd_if_param_mask = {
	LCD_MASK_DSITCKCR,
	LCD_MASK_DSI0PCKCR,
	LCD_MASK_DSI0PHYCR,
	LCD_MASK_SYSCONF,
	LCD_MASK_TIMSET0,
	LCD_MASK_TIMSET1,
	LCD_MASK_DSICTRL,
	LCD_MASK_VMCTR1,
	LCD_MASK_VMCTR2,
	LCD_MASK_VMLEN1,
	LCD_MASK_VMLEN2,
	LCD_MASK_VMLEN3,
	LCD_MASK_VMLEN4,
	LCD_MASK_DTCTR,
	LCD_MASK_MLDHCNR,
	LCD_MASK_MLDHSYNR,
	LCD_MASK_MLDHAJR,
	LCD_MASK_MLDVLNR,
	LCD_MASK_MLDVSYNR,
	LCD_MASK_MLDMT1R,
	LCD_MASK_LDDCKR,
	LCD_MASK_MLDDCKPAT1R,
	LCD_MASK_MLDDCKPAT2R,
	LCD_MASK_PHYTEST,
};

static unsigned int reset_gpio;
static unsigned int irq_portno;
static struct regulator *power_ldo_3v;
static struct regulator *power_ldo_1v8;

struct specific_cmdset {
	unsigned char cmd;
	unsigned char *data;
	int size;
};
#define MIPI_DSI_DCS_LONG_WRITE		(0x39)
#define MIPI_DSI_GEN_LONG_WRITE		(0x29)
#define MIPI_DSI_DCS_SHORT_WRITE_PARAM	(0x15)
#define MIPI_DSI_GEN_SHORT_WRITE_PARAM	(0x23)
#define MIPI_DSI_DCS_SHORT_WRITE	(0x05)
#define MIPI_DSI_GEN_SHORT_WRITE	(0x03)
#define MIPI_DSI_SET_MAX_RETURN_PACKET	(0x37)
#define MIPI_DSI_DCS_READ		(0x06)
#define MIPI_DSI_DELAY			(0x00)
#define MIPI_DSI_BLACK			(0x01)
#define MIPI_DSI_END			(0xFF)

#define LEVEL_IS_HBM(level)		(level >= 6)

#define MAX_GAMMA			350
#define DEFAULT_GAMMA_LEVEL		GAMMA_143CD

#define LDI_ID_REG			0x04
#define LDI_ID_LEN			3
#define LDI_MTP_REG			0xC8
#define LDI_MTP_LEN			(GAMMA_PARAM_SIZE - 1)
#define LDI_ELVSS_REG			0xB6
#define LDI_ELVSS_LEN			(ELVSS_PARAM_SIZE - 1) /* ELVSS + 
						HBM V151 ~ V35 + ELVSS 17th */

#define LDI_HBM_REG			0xB5
#define LDI_HBM_LEN			28	/* HBM V255 + HBM ELVSS + 
					white coordination + manufacture date */

#define MIN_BRIGHTNESS		0
#define MAX_BRIGHTNESS		255
#define DEFAULT_BRIGHTNESS		143
#define INIT_BRIGHTNESS	MAX_BRIGHTNESS
#define S6E88A0_INIT_RETRY_COUNT 3
static  unsigned char SEQ_DISPLAY_OFF[] = {
	0x28,
	0x00, 0x00
};

static  unsigned char SEQ_SLEEP_IN[] = {
	0x10,
	0x00, 0x00
};
static  unsigned char SEQ_ELVS_CONDITION[] = {
	0xB6,
	0x28, 0x0B,
};
#define SMART_DIMMING_DEBUG 0
#ifdef SMART_DIMMING_DEBUG
#define smtd_dbg(format, arg...)	printk(format, ##arg)
#else
#define smtd_dbg(format, arg...)
#endif

#if defined(CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG)
static struct specific_cmdset mauc0_cmd[] = {
	{ MIPI_DSI_DCS_LONG_WRITE,  maucctr0,  sizeof(maucctr0) },
	{ MIPI_DSI_END,             NULL,      0                }
};

static struct specific_cmdset mauc1_cmd[] = {
	{ MIPI_DSI_DCS_LONG_WRITE,  maucctr1,  sizeof(maucctr1) },
	{ MIPI_DSI_END,             NULL,      0                }
};
#endif

static int is_dsi_read_enabled;
static int power_supplied;

static struct fb_info *common_fb_info;

enum lcdfreq_level_idx {
	LEVEL_NORMAL,		/* 60Hz */
	LEVEL_LOW,		/* Power saving mode */
	LCDFREQ_LEVEL_END
};

struct lcd_info {
	enum lcdfreq_level_idx	level;	/* Current level */
	struct device_attribute	*attr;	/* Hold attribute info */
	unsigned int			bl;
	unsigned int			auto_brightness;
	unsigned int			acl_enable;
	unsigned int			siop_enable;
	unsigned int			current_acl;
	unsigned int			current_bl;
	unsigned int			current_elvss;
	unsigned int			ldi_enable;
	unsigned int			power;
	struct mutex		lock;	/* Lock for change frequency */
	struct mutex		bl_lock;
	struct device		*dev;	/* Hold device of LCD */
	struct lcd_device	*ld;	/* LCD device info */
	struct backlight_device		*bd;
	unsigned char			id[LDI_ID_LEN];
	unsigned char			**gamma_table;
	unsigned char			**elvss_table;
	struct dynamic_aid_param_t	daid;
	unsigned char		aor[GAMMA_MAX][ARRAY_SIZE(SEQ_AOR_CONTROL)];
	unsigned int		connected;
	unsigned int		coordinate[2];
};
static struct lcd_info lcd_info_data;

static const unsigned int candela_table[GAMMA_MAX] = {
	5,	6,	7,	8,	9,	10,	11,	12,	13,	14,	15,	16,	17,
	19,	20,	21,	22,	24,	25,	27,	29,
	30,	32,	34,	37,	39,	41,	44,	47,
	50,	53,	56,	60,	64,	68,	72,	77,
	82,	87,	93,	98,	105,	111,	119,	126,
	134,	143,	152,	162,	172,	183,	195,	207,
	220,	234,	249,	265,	282,	300, 316,	333,	350
};
static int initialize_now;
#if defined(CONFIG_FB_LCD_ESD)
static struct workqueue_struct *lcd_wq;
static struct work_struct esd_detect_work;
static int esd_detect_irq;
static irqreturn_t lcd_esd_irq_handler(int irq, void *dev_id);
static unsigned int esd_irq_portno;
static char *esd_devname  = "panel_esd_irq";
static int esd_irq_requested;
#endif /* CONFIG_FB_LCD_ESD */

#if defined(CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG) || defined(CONFIG_FB_LCD_ESD)
#define ESD_CHECK_DISABLE 0
#define ESD_CHECK_ENABLE 1

static struct mutex esd_check_mutex;
static int esd_check_flag;
#endif /* CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG or CONFIG_FB_LCD_ESD */

#if defined(CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG)
#define DURATION_TIME 3000 /* 3000ms */
#define SHORT_DURATION_TIME 500 /* 500ms */
static int esd_duration;
static struct delayed_work esd_check_work;
#endif /* CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG */



#if defined(CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG) || defined(CONFIG_FB_LCD_ESD)
static int S6E88A0_panel_simple_reset(void)
{
	void *screen_handle;
	screen_disp_stop_lcd disp_stop_lcd;
	screen_disp_start_lcd start_lcd;
	screen_disp_delete disp_delete;
	int ret;
	screen_disp_set_rt_standby set_rt_standby;
	screen_disp_set_dsi_mode set_dsi_mode;

#ifdef S6E88A0_POWAREA_MNG_ENABLE
	void *system_handle;
	system_pmg_param powarea_end_notify;
	system_pmg_param powarea_start_notify;
	system_pmg_delete pmg_delete;

	system_handle = system_pwmng_new();
#endif

	pr_debug("%s\n", __func__);

#if defined(CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG)
	esd_duration = SHORT_DURATION_TIME;
#endif /* CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG */
	is_dsi_read_enabled = 0;
	screen_handle =  screen_display_new();

	/* Start suspend sequence */
	/* GPIO control */
	mipi_display_power_off();

	disp_stop_lcd.handle		= screen_handle;
	disp_stop_lcd.output_mode	= RT_DISPLAY_LCD1;
	ret = screen_display_stop_lcd(&disp_stop_lcd);
	if (ret != SMAP_LIB_DISPLAY_OK)
		pr_debug("display_stop_lcd err!\n");
	/* End suspend sequence */

#ifdef S6E88A0_POWAREA_MNG_ENABLE
	/* Notifying the Beginning of Using Power Area */
	pr_debug("End A4LC power area\n");
	powarea_end_notify.handle		= system_handle;
	powarea_end_notify.powerarea_name	= RT_PWMNG_POWERAREA_A4LC;
	ret = system_pwmng_powerarea_end_notify(&powarea_end_notify);
	if (ret != SMAP_LIB_PWMNG_OK) {
		pr_debug("system_pwmng_powerarea_end_notify err!\n");
		goto out;
	}

	msleep(20);

	/* Notifying the Beginning of Using Power Area */
	pr_debug("Start A4LC power area\n");
	powarea_start_notify.handle		= system_handle;
	powarea_start_notify.powerarea_name	= RT_PWMNG_POWERAREA_A4LC;
	ret = system_pwmng_powerarea_start_notify(&powarea_start_notify);
	if (ret != SMAP_LIB_PWMNG_OK) {
		pr_debug("system_pwmng_powerarea_start_notify err!\n");
		goto out;
	}
#endif /* S6E88A0_POWAREA_MNG_ENABLE */

	msleep(20);

	/* LCD panel reset */
	mipi_display_reset();
	/* Start resume sequence */
	/* Start a display to LCD */
	start_lcd.handle	= screen_handle;
	start_lcd.output_mode	= RT_DISPLAY_LCD1;
	ret = screen_display_start_lcd(&start_lcd);
	if (ret != SMAP_LIB_DISPLAY_OK) {
		pr_debug("disp_start_lcd err!\n");
		goto out;
	}

	/* RT Standby Mode (FROMHS,TOHS) */
	set_rt_standby.handle	= screen_handle;
	set_rt_standby.rt_standby	= DISABLE_RT_STANDBY;
	screen_display_set_rt_standby(&set_rt_standby);

	/* Set DSI mode (FROMHS,TOHS) */
	set_dsi_mode.handle	= screen_handle;
	set_dsi_mode.dsimode	= DSI_FROMHS;
	screen_display_set_dsi_mode(&set_dsi_mode);

	printk(KERN_INFO"\n GPIO RESET\n");
	gpio_direction_output(reset_gpio, 1);
	usleep_range(2000, 2000);
	gpio_direction_output(reset_gpio, 0);
	usleep_range(2000, 2000);
	gpio_direction_output(reset_gpio, 1);
	
	msleep(120);
	set_dsi_mode.dsimode	= DSI_TOHS;
	screen_display_set_dsi_mode(&set_dsi_mode);			

	/* Transmit DSI command peculiar to a panel */
	ret = S6E88A0_InitSequence(screen_handle);
	/* RT Standby Mode (FROMHS,TOHS) */
	set_rt_standby.handle	= screen_handle;
	set_rt_standby.rt_standby	= ENABLE_RT_STANDBY;
	screen_display_set_rt_standby(&set_rt_standby);
		
	if (ret != 0) {
		pr_debug("panel_specific_cmdset err!\n");
		goto out;
	}
	/* End resume sequence */

	is_dsi_read_enabled = 1;
	lcd_info_data.ldi_enable = 1;
	pr_debug("S6E88A0_panel simple initialized\n");

out:
	disp_delete.handle = screen_handle;
	screen_display_delete(&disp_delete);

#ifdef S6E88A0_POWAREA_MNG_ENABLE
	pmg_delete.handle = system_handle;
	system_pwmng_delete(&pmg_delete);
#endif /* S6E88A0_POWAREA_MNG_ENABLE */

	return ret;
}
#endif /* CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG or CONFIG_FB_LCD_ESD */

#if defined(CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG)
static int S6E88A0_panel_check(void)
{
	unsigned char rdnumed;
	unsigned char rddpm;
	unsigned char wonder[3];
	unsigned char rdidic[3];
	unsigned char exp_rdidic[3] = {0x55, 0x10, 0x05};
	void *screen_handle;
	screen_disp_delete disp_delete;
	int ret;

	screen_handle =  screen_display_new();

	/*Read Number of Errors on DSI*/
	ret = panel_dsi_read(MIPI_DSI_DCS_READ, 0x05, 1, &rdnumed);
	pr_debug("read_data(0x05) = %02X : ret(%d)\n", rdnumed, ret);
	if (rdnumed != 0x00)
		ret = -1;
	if (ret != 0)
		goto out;

	/*Read Display Power Mode*/
	ret = panel_dsi_read(MIPI_DSI_DCS_READ, 0x0A, 1, &rddpm);
	pr_debug("read_data(0x0A) = %02X : ret(%d)\n", rddpm, ret);
	if (rddpm != 0x9C)
		ret = -1;
	if (ret != 0)
		goto out;

	/*Read ID for IC Vender Code*/
	ret = panel_specific_cmdset(screen_handle, mauc1_cmd);
	if (ret != 0)
		goto out;
	ret = panel_dsi_read(MIPI_DSI_DCS_READ, 0xC5, 1, rdidic);
	pr_debug("read_data(0xC5) = %02X : ret(%d) page 1\n",
							rdidic[0], ret);
	if (exp_rdidic[0] != rdidic[0])
		ret = -1;
	if (ret != 0)
		goto out;

	/*for check switching page*/
	ret = panel_specific_cmdset(screen_handle, mauc0_cmd);
	if (ret != 0)
		goto out;
	ret = panel_dsi_read(MIPI_DSI_DCS_READ, 0xC5, 1, wonder);
	pr_debug("read_data(0xC5) = %02X : ret(%d) page 0\n",
							wonder[0], ret);
	if (wonder[0] == rdidic[0])
		ret = -1;
	if (ret != 0)
		goto out;

out:
	disp_delete.handle = screen_handle;
	screen_display_delete(&disp_delete);

	return ret;
}

static void S6E88A0_panel_esd_check_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	int retry = S6E88A0_INIT_RETRY_COUNT;

	/* For the disable entering suspend */
	mutex_lock(&esd_check_mutex);

	while ((esd_check_flag == ESD_CHECK_ENABLE))
		while ((esd_check_flag == ESD_CHECK_ENABLE) &&
					(S6E88A0_panel_simple_reset())) {
			if (retry <= 0) {
				esd_check_flag = ESD_CHECK_DISABLE;
				pr_debug("retry count 0!!!!\n");
				break;
			}
			retry--;
			msleep(20);
		}

	if (esd_check_flag == ESD_CHECK_ENABLE)
		schedule_delayed_work(dwork, msecs_to_jiffies(esd_duration));

	/* Enable suspend */
	mutex_unlock(&esd_check_mutex);
}
#endif
#if 0
static int S6E88A0_power_on(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	ret = S6E88A0_panel_resume();

	return ret;
}

static int S6E88A0_power_off(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	ret = S6E88A0_panel_suspend();

	msleep(135);

	return ret;
}

static int S6E88A0_power(struct lcd_info *lcd, int power)
{
	int ret = 0;

	if (POWER_IS_ON(power) && !POWER_IS_ON(lcd->power))
		ret = S6E88A0_power_on(lcd);
	else if (!POWER_IS_ON(power) && POWER_IS_ON(lcd->power))
		ret = S6E88A0_power_off(lcd);

	if (!ret)
		lcd->power = power;

	return ret;
}

static int S6E88A0_set_power(struct lcd_device *ld, int power)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	if (power != FB_BLANK_UNBLANK && power != FB_BLANK_POWERDOWN &&
		power != FB_BLANK_NORMAL) {
		dev_err(&lcd->ld->dev, "power value should be 0, 1 or 4.\n");
		return -EINVAL;
	}

	return S6E88A0_power(lcd, power);
}

static int S6E88A0_get_power(struct lcd_device *ld)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	return lcd->power;
}
#endif
static struct lcd_ops S6E88A0_lcd_ops = {
	.set_power = NULL,
	.get_power = NULL,
};



#if defined(CONFIG_FB_LCD_ESD)
static void lcd_esd_detect(struct work_struct *work)
{
	int retry = S6E88A0_INIT_RETRY_COUNT;

	/* For the disable entering suspend */
	mutex_lock(&esd_check_mutex);

	pr_debug("[LCD] %s\n", __func__);

	/* esd recovery */
	while ((S6E88A0_panel_simple_reset()) &&
				(esd_check_flag == ESD_CHECK_ENABLE)) {
		if (retry <= 0) {
			void *screen_handle;
			screen_disp_stop_lcd disp_stop_lcd;
			screen_disp_delete disp_delete;
			int ret;

			screen_handle =  screen_display_new();

			mipi_display_power_off();

			disp_stop_lcd.handle		= screen_handle;
			disp_stop_lcd.output_mode	= RT_DISPLAY_LCD1;
			ret = screen_display_stop_lcd(&disp_stop_lcd);
			if (ret != SMAP_LIB_DISPLAY_OK)
				pr_debug("display_stop_lcd err!\n");

			disp_delete.handle = screen_handle;
			screen_display_delete(&disp_delete);

			esd_check_flag = ESD_CHECK_DISABLE;
			pr_debug("retry count 0!!!!\n");
			break;
		}
		retry--;
		msleep(20);
	}

	if (esd_check_flag == ESD_CHECK_ENABLE)
		enable_irq(esd_detect_irq);

	/* Enable suspend */
	mutex_unlock(&esd_check_mutex);
}

static irqreturn_t lcd_esd_irq_handler(int irq, void *dev_id)
{
	if (dev_id == &esd_irq_requested) {
		pr_debug("[LCD] %s\n", __func__);

		disable_irq_nosync(esd_detect_irq);
		queue_work(lcd_wq, &esd_detect_work);
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}
#endif /* CONFIG_FB_LCD_ESD */



#define DCS_LONG_WR		(0x39)
#define DCS_WR_1_PARA	(0x15)
#define DCS_WR_NO_PARA	(0x05)
#define MIPI_DSI_DCS_READ		(0x06)
static int _s6e88a0_write(void *screen_handle,  unsigned char *seq, int len)
{
	int size;
	unsigned char *wbuf;
	int ret = 0;

	screen_disp_write_dsi_long  write_dsi_l;
	screen_disp_write_dsi_short write_dsi_s;

	if (!lcd_info_data.connected)
		return 0;

	mutex_lock(&lcd_info_data.lock);

	size = len;
	wbuf = seq;

	if (size == 1) {
			pr_debug("panel_cmdset SHORT Write\n");
			write_dsi_s.handle         = screen_handle;
			write_dsi_s.output_mode    = RT_DISPLAY_LCD1;
			write_dsi_s.data_id        = DCS_WR_NO_PARA;
			write_dsi_s.reg_address    = wbuf[0];
			write_dsi_s.write_data     = 0x00;
			write_dsi_s.reception_mode = RT_DISPLAY_RECEPTION_ON;
			ret = screen_display_write_dsi_short_packet(
					&write_dsi_s);
			if (ret != SMAP_LIB_DISPLAY_OK) {
				pr_debug("disp_write_dsi_short err %d!\n", ret);

			}
	} else if (size == 2) {
			pr_debug("panel_cmdset SHORT Write with param\n");
			write_dsi_s.handle         = screen_handle;
			write_dsi_s.output_mode    = RT_DISPLAY_LCD1;
			write_dsi_s.data_id        = DCS_WR_1_PARA;
			write_dsi_s.reg_address    = wbuf[0];
			write_dsi_s.write_data     = wbuf[1];
			write_dsi_s.reception_mode = RT_DISPLAY_RECEPTION_ON;
			ret = screen_display_write_dsi_short_packet(
					&write_dsi_s);
			if (ret != SMAP_LIB_DISPLAY_OK) {
				pr_debug("disp_write_dsi_short err %d!\n", ret);

			}
		}
	else {
			pr_debug("panel_cmdset LONG Write\n");
			write_dsi_l.handle         = screen_handle;
			write_dsi_l.output_mode    = RT_DISPLAY_LCD1;
			write_dsi_l.data_id        = DCS_LONG_WR;
			write_dsi_l.data_count     = size;
			write_dsi_l.write_data     = wbuf;
			write_dsi_l.reception_mode	= RT_DISPLAY_RECEPTION_ON;
			write_dsi_l.send_mode = RT_DISPLAY_SEND_MODE_HS;
			ret = screen_display_write_dsi_long_packet(
					&write_dsi_l);
			if (ret != SMAP_LIB_DISPLAY_OK) {
				pr_debug("display_write_dsi_long err %d!\n", ret);

			}
		}

	mutex_unlock(&lcd_info_data.lock);

	return ret;
}

static int s6e88a0_write(void *screen_handle,  unsigned char *seq, int len)
{
	int ret = 0;
	int retry_cnt = 1;

retry:
	ret = _s6e88a0_write(screen_handle, seq, len);
	if (ret) {
		if (retry_cnt) {
			pr_debug("%s: retry: %d\n", __func__, retry_cnt);
			retry_cnt--;
			goto retry;
		} else
			pr_debug("%s: 0x%02x\n", __func__, seq[1]);
	}

	return ret;
}

static int _s6e88a0_read(void *screen_handle, const u8 addr, u16 count, u8 *buf)
{
	int ret = 0;

	screen_disp_read_dsi_short read_dsi_s;
	screen_disp_write_dsi_short write_dsi_s;

	if (!lcd_info_data.connected)
		return ret;

	mutex_lock(&lcd_info_data.lock);
	/* DSI read */
	/* Set maximum return packet size  */
	write_dsi_s.handle		= screen_handle;
	write_dsi_s.output_mode	= RT_DISPLAY_LCD1;
	write_dsi_s.data_id		= MIPI_DSI_SET_MAX_RETURN_PACKET;
	write_dsi_s.reg_address	= count;
	write_dsi_s.write_data		= 0x00;
	write_dsi_s.reception_mode      = RT_DISPLAY_RECEPTION_ON;
	ret = screen_display_write_dsi_short_packet(&write_dsi_s);
	if (ret != SMAP_LIB_DISPLAY_OK) {
		pr_debug("disp_write_dsi_short err!\n");
		}
	read_dsi_s.handle		= screen_handle;
	read_dsi_s.output_mode		= RT_DISPLAY_LCD1;
	read_dsi_s.data_id		= MIPI_DSI_DCS_READ;
	read_dsi_s.reg_address		= addr;
	read_dsi_s.write_data		= 0;
	read_dsi_s.data_count		= count;
	read_dsi_s.read_data		= &buf[0];
	ret = screen_display_read_dsi_short_packet(&read_dsi_s);

	if (ret != SMAP_LIB_DISPLAY_OK)
		pr_debug("disp_dsi_read err! ret = %d\n", ret);
	mutex_unlock(&lcd_info_data.lock);
	return ret;
}

static int s6e88a0_read(void *screen_handle, const u8 addr, u8 *buf, u16 count, u8 retry_cnt)
{
	int ret = 0;

retry:
	ret = _s6e88a0_read(screen_handle, addr, count, buf);
	if (ret) {
		if (retry_cnt) {
			pr_debug("%s: retry: %d\n", __func__, retry_cnt);
			retry_cnt--;
			goto retry;
		} else
			pr_debug("%s: 0x%02x\n", __func__, addr);
	}

	return ret;
}
static void mipi_display_reset(void)
{
	pr_debug("%s\n", __func__);

	/* Already power supply */
	if (power_supplied) {
		pr_debug("Already power supply!\n");
		goto out;
	}

	if (regulator_enable(power_ldo_1v8))
		pr_debug("enabling vlcd_1v8 failed, err\n");

	if (regulator_enable(power_ldo_3v))
		pr_debug("enabling vlcd_3v failed, err\n");

//	gpio_direction_output(reset_gpio, 0);
//	msleep(100);

//	gpio_direction_output(reset_gpio, 1);
//	msleep(100);
out:
	power_supplied = true;
}

static void mipi_display_power_off(void)
{
	pr_debug("%s\n", __func__);

	/* Already not power supply */
	if (!power_supplied) {
		pr_debug("Already not power supply!\n");
		goto out;
	}

	/* GPIO control */
	gpio_direction_output(reset_gpio, 0);
	msleep(20);
	regulator_disable(power_ldo_3v);
	regulator_disable(power_ldo_1v8);
	msleep(25);

out:
	power_supplied = false;
}

static int S6E88A0_DemiseSequence(void *lcd_handle)
{
	s6e88a0_write(lcd_handle, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
	msleep(34);
	s6e88a0_write(lcd_handle, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));
	msleep(120);
	return 0;
}
static int s6e88a0_read_id(void *lcd_handle, u8 *buf);
static int update_brightness(void *lcd_handle, u8 force);
static int S6E88A0_panel_draw(void *screen_handle)
{
	screen_disp_draw disp_draw;
	int ret;

	printk("%s\n", __func__);

	/* Memory clean */
	disp_draw.handle = screen_handle;
	disp_draw.output_mode = RT_DISPLAY_LCD1;
	disp_draw.draw_rect.x = 0;
	disp_draw.draw_rect.y = 0;
	disp_draw.draw_rect.width = R_MOBILE_M_PANEL_PIXEL_WIDTH;
	disp_draw.draw_rect.height = R_MOBILE_M_PANEL_PIXEL_HEIGHT;
#ifdef CONFIG_FB_SH_MOBILE_RGB888
	disp_draw.format = RT_DISPLAY_FORMAT_RGB888;
#else
	disp_draw.format = RT_DISPLAY_FORMAT_ARGB8888;
#endif
	disp_draw.buffer_id = RT_DISPLAY_BUFFER_A;
	disp_draw.buffer_offset = 0;
	disp_draw.rotate = RT_DISPLAY_ROTATE_270;
	ret = screen_display_draw(&disp_draw);
	if (ret != SMAP_LIB_DISPLAY_OK) {
		pr_err("screen_display_draw err!\n");
		return -1;
	}

	return 0;
}

static int S6E88A0_panel_draw_black(void *screen_handle)
{
	u32 panel_width  = R_MOBILE_M_PANEL_PIXEL_WIDTH;
	u32 panel_height = R_MOBILE_M_PANEL_PIXEL_HEIGHT;
	screen_disp_draw disp_draw;
	int ret;

	printk("%s\n", __func__);

#ifdef S6E88A0_DRAW_BLACK_KERNEL
	pr_info(
		"num_registered_fb = %d\n", num_registered_fb);

	if (!num_registered_fb) {
		pr_err(
			"num_registered_fb err!\n");
		return -1;
	}
	if (!registered_fb[0]->fix.smem_start) {
		pr_err(
			"registered_fb[0]->fix.smem_start"
			" is NULL err!\n");
		return -1;
	}
	pr_info(
	       "registerd_fb[0]-> fix.smem_start: %08x\n"
	       "screen_base :%08x\n"
	       "fix.smem_len :%08x\n",
	       (unsigned)(registered_fb[0]->fix.smem_start),
	       (unsigned)(registered_fb[0]->screen_base),
	       (unsigned)(registered_fb[0]->fix.smem_len));
	memset(registered_fb[0]->screen_base, 0x0,
			registered_fb[0]->fix.smem_len);
#endif

	/* Memory clean */
	disp_draw.handle = screen_handle;
#ifdef S6E88A0_DRAW_BLACK_KERNEL
	disp_draw.output_mode = RT_DISPLAY_LCD1;
	disp_draw.buffer_id   = RT_DISPLAY_BUFFER_A;
#else
	disp_draw.output_mode = RT_DISPLAY_LCD1_ASYNC;
	disp_draw.buffer_id   = RT_DISPLAY_DRAW_BLACK;
#endif
	disp_draw.draw_rect.x = 0;
	disp_draw.draw_rect.y = 0;
	disp_draw.draw_rect.width  = panel_width;
	disp_draw.draw_rect.height = panel_height;
#ifdef CONFIG_FB_SH_MOBILE_RGB888
	disp_draw.format = RT_DISPLAY_FORMAT_RGB888;
#else
	disp_draw.format = RT_DISPLAY_FORMAT_ARGB8888;
#endif
	disp_draw.buffer_offset = 0;
	disp_draw.rotate = RT_DISPLAY_ROTATE_270;
	ret = screen_display_draw(&disp_draw);
	if (ret != SMAP_LIB_DISPLAY_OK) {
		pr_err("screen_display_draw err!\n");
		return -1;
	}

	return 0;
}

static int S6E88A0_InitSequence(void *lcd_handle)
{
	int ret = 0;
	unsigned char read_data[60];
	msleep(5);

	s6e88a0_write(lcd_handle, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	//s6e88a0_write(lcd_handle, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));

	//s6e88a0_write(lcd_handle, SEQ_SRC_LATCH, ARRAY_SIZE(SEQ_SRC_LATCH));
	//s6e88a0_write(lcd_handle, SEQ_AVDD, ARRAY_SIZE(SEQ_AVDD));
	s6e88a0_write(lcd_handle, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));

	msleep(25);

	ret = s6e88a0_read_id(lcd_handle, &read_data[0]);
	if (ret < 0) {
		return -1;
	}
	printk(KERN_INFO "read_data(RDID0) = %02X\n", read_data[0]);
	printk(KERN_INFO "read_data(RDID1) = %02X\n", read_data[1]);
	printk(KERN_INFO "read_data(RDID2) = %02X\n",	read_data[2]);

	s6e88a0_write(lcd_handle, SEQ_AVDD, ARRAY_SIZE(SEQ_AVDD));	

	update_brightness(lcd_handle, 1);

	msleep(120);
	S6E88A0_panel_draw(lcd_handle);
	S6E88A0_panel_draw_black(lcd_handle);

	//s6e88a0_write(lcd_handle, SEQ_AVC, ARRAY_SIZE(SEQ_AVC));
	s6e88a0_write(lcd_handle, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	s6e88a0_write(lcd_handle, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
	S6E88A0_panel_draw(lcd_handle);
	S6E88A0_panel_draw_black(lcd_handle);

	return ret;
}
static int s6e88a0_read_id(void *lcd_handle, u8 *buf)
{
	int ret = 0;

	ret = s6e88a0_read(lcd_handle, LDI_ID_REG, buf, LDI_ID_LEN, 1);
	if (ret < 0) {
		lcd_info_data.connected = 0;
		pr_err("panel is not connected well\n");
	}
	return ret;
}
static int s6e88a0_read_mtp(void *lcd_handle, u8 *buf)
{
	int ret;
	ret = s6e88a0_read(lcd_handle, LDI_MTP_REG, buf, LDI_MTP_LEN, 1);

	printk(KERN_INFO"s6e88a0_read_mtp %02xh %d \n", LDI_MTP_REG, LDI_MTP_LEN);
#if 0
	int i;
	smtd_dbg("s6e88a0_read_mtp %02xh %d \n", LDI_MTP_REG, LDI_MTP_LEN);
	for (i = 0; i < LDI_MTP_LEN; i++)
		smtd_dbg("0x%02x, ",  (int)buf[i]);
	smtd_dbg("\n");
#endif
	return ret;
}

static int s6e88a0_read_elvss(void *lcd_handle, u8 *buf)
{
	int ret;

	ret = s6e88a0_read(lcd_handle, LDI_ELVSS_REG, buf, LDI_ELVSS_LEN, 1);

	printk(KERN_INFO "s6e88a0_read_elvss %02xh %d\n", LDI_ELVSS_REG, LDI_ELVSS_LEN);
#if 0
	smtd_dbg("s6e88a0_read_elvss %02xh %d\n", LDI_ELVSS_REG, LDI_ELVSS_LEN);
	for (i = 0; i < LDI_ELVSS_LEN; i++)
		smtd_dbg("%02dth value is %02x\n", i+1, (int)buf[i]);
		smtd_dbg("0x%02x, ", (int)buf[i]);
	smtd_dbg("\n");

#endif
	return ret;
}

static void s6e88a0_read_coordinate(void *lcd_handle, u8 *buf)
{
	int ret = 0;

	ret = s6e88a0_read(lcd_handle, LDI_HBM_REG, buf, LDI_HBM_LEN, 1);

	if (ret < 0)
		dev_err(&lcd_info_data.ld->dev, "%s failed\n", __func__);

	lcd_info_data.coordinate[0] = buf[19] << 8 | buf[20];	/* X */
	lcd_info_data.coordinate[1] = buf[21] << 8 | buf[22];	/* Y */
	printk(KERN_INFO "s6e88a0_read_coordinate %02xh %d\n", LDI_HBM_REG, LDI_HBM_LEN);

#if 0
	smtd_dbg("s6e88a0_read_coordinate %02xh %d\n", LDI_HBM_REG, LDI_HBM_LEN);
	for (i = 0; i < LDI_HBM_LEN; i++)
		smtd_dbg("%02dth value is %02x\n", i+1, (int)buf[i]);
		smtd_dbg("0x%02x, ", (int)buf[i]);
	smtd_dbg("\n");
#endif
}

static int get_backlight_level_from_brightness(int brightness)
{
	int backlightlevel;

	switch (brightness) {
	case 0:
		backlightlevel = 0;
		break;
	case 1 ... 10:
		backlightlevel = GAMMA_6CD;
		break;
	case 11 ... 13:
		backlightlevel = GAMMA_7CD;
		break;		
	case 14 ... 16:
		backlightlevel = GAMMA_8CD;
		break;		
	case 17 ... 19:
		backlightlevel = GAMMA_9CD;
		break;		
	case 20 ... 22:
		backlightlevel = GAMMA_10CD;
		break;
	case 23 ... 25:
		backlightlevel = GAMMA_11CD;
		break;
	case 26 ... 28:
		backlightlevel = GAMMA_12CD;
		break;
	case 29 ... 31:
		backlightlevel = GAMMA_13CD;
		break;
	case 32 ... 34:
		backlightlevel = GAMMA_14CD;
		break;
	case 35 ... 37:
		backlightlevel = GAMMA_15CD;
		break;		
	case 38 ... 40:
		backlightlevel = GAMMA_16CD;
		break;		
	case 41 ... 43:
		backlightlevel = GAMMA_17CD;
		break;
	case 44 ... 47:
		backlightlevel = GAMMA_19CD;
		break;
	case 48 ... 50:
		backlightlevel = GAMMA_20CD;
		break;
	case 51 ... 53:
		backlightlevel = GAMMA_21CD;
		break;		
	case 54 ... 55:
		backlightlevel = GAMMA_22CD;
		break;
	case 56 ... 58:
		backlightlevel = GAMMA_24CD;
		break;		
	case 59 ... 61:
		backlightlevel = GAMMA_25CD;
		break;
	case 62 ... 64:
		backlightlevel = GAMMA_27CD;
		break;
	case 65 ... 67:
		backlightlevel = GAMMA_29CD;
		break;
	case 68 ... 70:
		backlightlevel = GAMMA_30CD;
		break;
	case 71 ... 73:
		backlightlevel = GAMMA_32CD;
		break;
	case 74 ... 76:
		backlightlevel = GAMMA_34CD;
		break;
	case 77 ... 79:
		backlightlevel = GAMMA_37CD;
		break;
	case 80 ... 82:
		backlightlevel = GAMMA_39CD;
		break;
	case 83 ... 85:
		backlightlevel = GAMMA_41CD;
		break;
	case 86 ... 88:
		backlightlevel = GAMMA_44CD;
		break;
	case 89 ... 91:
		backlightlevel = GAMMA_47CD;
		break;
	case 92 ... 94:
		backlightlevel = GAMMA_50CD;
		break;
	case 95 ... 97:
		backlightlevel = GAMMA_53CD;
		break;
	case 98 ... 100:
		backlightlevel = GAMMA_56CD;
		break;
	case 101 ... 103:
		backlightlevel = GAMMA_60CD;
		break;
	case 104 ... 106:
		backlightlevel = GAMMA_64CD;
		break;
	case 107 ... 109:
		backlightlevel = GAMMA_68CD;
		break;
	case 110 ... 112:
		backlightlevel = GAMMA_72CD;
		break;
	case 113 ... 115:
		backlightlevel = GAMMA_77CD;
		break;
	case 116 ... 118:
		backlightlevel = GAMMA_82CD;
		break;
	case 119 ... 121:
		backlightlevel = GAMMA_87CD;
		break;
	case 122 ... 124:
		backlightlevel = GAMMA_93CD;
		break;
	case 125 ... 126:
		backlightlevel = GAMMA_98CD;
		break;
	case 127 ... 128:
		backlightlevel = GAMMA_105CD;
		break;
	case 129 ... 130:
		backlightlevel = GAMMA_111CD;
		break;
	case 131 ... 132:
		backlightlevel = GAMMA_119CD;
		break;
	case 133 ... 134:
		backlightlevel = GAMMA_126CD;
		break;
	case 135 ... 136:
		backlightlevel = GAMMA_134CD;
		break;
	case 137 ... 138:
		backlightlevel = GAMMA_143CD;
		break;
	case 139 ... 141:
		backlightlevel = GAMMA_152CD;
		break;
	case 142 ... 144:
		backlightlevel = GAMMA_162CD;
		break;
	case 145 ... 152:
		backlightlevel = GAMMA_172CD;
		break;
	case 153 ... 160:
		backlightlevel = GAMMA_183CD;
		break;
	case 161 ... 168:
		backlightlevel = GAMMA_195CD;
		break;
	case 169 ... 176:
		backlightlevel = GAMMA_207CD;
		break;
	case 177 ... 184:
		backlightlevel = GAMMA_220CD;
		break;
	case 185 ... 192:
		backlightlevel = GAMMA_234CD;
		break;
	case 193 ... 201:
		backlightlevel = GAMMA_249CD;
		break;
	case 202 ... 210:
		backlightlevel = GAMMA_265CD;
		break;
	case 211 ... 219:
		backlightlevel = GAMMA_282CD;
		break;
	case 220 ... 228:
		backlightlevel = GAMMA_300CD;
		break;
	case 229 ... 237:
		backlightlevel = GAMMA_316CD;
		break;
	case 238 ... 246:
		backlightlevel = GAMMA_333CD;
		break;
	case 247 ... 255:
		backlightlevel = GAMMA_350CD;
		break;
	default:
		backlightlevel = DEFAULT_GAMMA_LEVEL;
		break;
	}

	return backlightlevel;
}

static int s6e88a0_gamma_ctl(struct lcd_info *lcd)
{

#if 0
	int i;
	char *temp = lcd_info_data.gamma_table[lcd_info_data.bl];
	for (i = 0; i < GAMMA_PARAM_SIZE; i++) {
		smtd_dbg("0x%0x, ", *temp++);
	}
#endif
	s6e88a0_write(lcd, lcd_info_data.gamma_table[lcd_info_data.bl], GAMMA_PARAM_SIZE);

	return 0;
}

static int s6e88a0_aid_parameter_ctl(struct lcd_info *lcd, u8 force)
{
	if (force)
		goto aid_update;
	else if (lcd_info_data.aor[lcd_info_data.bl][0x04] !=  lcd_info_data.aor[lcd_info_data.current_bl][0x04])
		goto aid_update;
	else if (lcd_info_data.aor[lcd_info_data.bl][0x05] !=  lcd_info_data.aor[lcd_info_data.current_bl][0x05])
		goto aid_update;
	else
		goto exit;

aid_update:
	s6e88a0_write(lcd, lcd_info_data.aor[lcd_info_data.bl], AID_PARAM_SIZE);
exit:
	s6e88a0_write(lcd, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	return 0;
}

static int s6e88a0_set_acl(struct lcd_info *lcd, u8 force)
{
	int ret = 0, level;
	char *temp;
	level = ACL_STATUS_40P;
	if (lcd_info_data.siop_enable || LEVEL_IS_HBM(lcd_info_data.auto_brightness))
		goto acl_update;

	if (!lcd_info_data.acl_enable)
		level = ACL_STATUS_0P;

acl_update:
	temp = (unsigned char *)ACL_CUTOFF_TABLE[level];
	if (force || lcd_info_data.current_acl != ACL_CUTOFF_TABLE[level][1]) {
		ret = s6e88a0_write(lcd, temp, ACL_PARAM_SIZE);
		lcd_info_data.current_acl = ACL_CUTOFF_TABLE[level][1];
		dev_info(&lcd_info_data.ld->dev, "acl: %d, auto_brightness: %d\n", lcd_info_data.current_acl, lcd_info_data.auto_brightness);
	}

	if (!ret)
		ret = -EPERM;

	return ret;
}

static int s6e88a0_set_elvss(struct lcd_info *lcd, u8 force)
{
	int ret = 0, elvss_level = 0;
	u32 candela = candela_table[lcd_info_data.bl];

	switch (candela) {
	case 0 ... 98:
		elvss_level = ELVSS_STATUS_98;
		break;
	case 99 ... 105:
		elvss_level = ELVSS_STATUS_105;
		break;
	case 106 ... 111:
		elvss_level = ELVSS_STATUS_111;
		break;
	case 112 ... 119:
		elvss_level = ELVSS_STATUS_119;
		break;
	case 120 ... 126:
		elvss_level = ELVSS_STATUS_126;
		break;
	case 127 ... 134:
		elvss_level = ELVSS_STATUS_134;
		break;
	case 135 ... 143:
		elvss_level = ELVSS_STATUS_143;
		break;
	case 144 ... 152:
		elvss_level = ELVSS_STATUS_152;
		break;
	case 153 ... 162:
		elvss_level = ELVSS_STATUS_162;
		break;
	case 163 ... 172:
		elvss_level = ELVSS_STATUS_172;
		break;
	case 173 ... 183:
		elvss_level = ELVSS_STATUS_183;
		break;
	case 184 ... 195:
		elvss_level = ELVSS_STATUS_195;
		break;
	case 196 ... 207:
		elvss_level = ELVSS_STATUS_207;
		break;
	case 208 ... 220:
		elvss_level = ELVSS_STATUS_220;
		break;
	case 221 ... 234:
		elvss_level = ELVSS_STATUS_234;
		break;
	case 235 ... 249:
		elvss_level = ELVSS_STATUS_249;
		break;
	case 250 ... 265:
		elvss_level = ELVSS_STATUS_265;
		break;
	case 266 ... 282:
		elvss_level = ELVSS_STATUS_282;
		break;
	case 283 ... 300:
		elvss_level = ELVSS_STATUS_300;
		break;
	case 301 ... 316:
		elvss_level = ELVSS_STATUS_316;
		break;	
	case 317 ... 333:
		elvss_level = ELVSS_STATUS_333;
		break;	
	case 334 ... 350:
		elvss_level = ELVSS_STATUS_350;
		break;			
	case 400:
		elvss_level = ELVSS_STATUS_HBM;
		break;
	default:
		elvss_level = ELVSS_STATUS_350;
		break;
	}

	if (force || lcd_info_data.current_elvss != elvss_level) {
		ret = s6e88a0_write(lcd, lcd_info_data.elvss_table[elvss_level],
						ELVSS_PARAM_SIZE);
		lcd_info_data.current_elvss = elvss_level;
	}

#if 0
	smtd_dbg("elvss: %d, %x, %x\n", elvss_level,
		lcd_info_data.elvss_table[elvss_level][3],
				lcd_info_data.elvss_table[elvss_level][17]);
#endif
	if (!ret) {
		ret = -EPERM;
		goto elvss_err;
	}

elvss_err:
	return ret;
}

static void init_dynamic_aid(struct lcd_info *lcd)
{
	lcd_info_data.daid.vreg = VREG_OUT_X1000;
	lcd_info_data.daid.iv_tbl = index_voltage_table;
	lcd_info_data.daid.iv_max = IV_MAX;
	lcd_info_data.daid.mtp = kzalloc(IV_MAX * CI_MAX * sizeof(int), GFP_KERNEL);
	lcd_info_data.daid.gamma_default = gamma_default;
	lcd_info_data.daid.formular = gamma_formula;
	lcd_info_data.daid.vt_voltage_value = vt_voltage_value;

	lcd_info_data.daid.ibr_tbl = index_brightness_table;
	lcd_info_data.daid.ibr_max = IBRIGHTNESS_MAX;
	lcd_info_data.daid.br_base = brightness_base_table;
	lcd_info_data.daid.gc_tbls = gamma_curve_tables;
	lcd_info_data.daid.gc_lut = gamma_curve_lut;
	lcd_info_data.daid.offset_gra = offset_gradation;
	lcd_info_data.daid.offset_color = (const struct rgb_t(*)[])offset_color;
}

static void init_mtp_data(struct lcd_info *lcd, const u8 *mtp_data)
{
	int i, c, j;
	int *mtp;

	mtp = lcd_info_data.daid.mtp;

	for (c = 0, j = 0; c < CI_MAX; c++, j++) {
		if (mtp_data[j++] & 0x01)
			mtp[(IV_MAX-1)*CI_MAX+c] = mtp_data[j] * (-1);
		else
			mtp[(IV_MAX-1)*CI_MAX+c] = mtp_data[j];
	}

	for (i = IV_203; i >= 0; i--) {
		for (c = 0; c < CI_MAX; c++, j++) {
			if (mtp_data[j] & 0x80)
				mtp[CI_MAX*i+c] = (mtp_data[j] & 0x7F) * (-1);
			else
				mtp[CI_MAX*i+c] = mtp_data[j];
		}
	}
#if 0  
  smtd_dbg("\n mtp_data %d\n", IV_MAX*CI_MAX);
	for (i = 0, j = 0; i <= IV_MAX; i++)
		for (c = 0; c < CI_MAX; c++, j++)
			smtd_dbg("%d, ", mtp_data[j]);
	smtd_dbg("\n ");
	smtd_dbg("\n mtp %d\n", IV_MAX*CI_MAX);
	for (i = 0, j = 0; i <= IV_MAX; i++)
		for (c = 0; c < CI_MAX; c++, j++)
			smtd_dbg("%d, ", mtp[j]);
	smtd_dbg("\n");
	for (i = 0, j = 0; i <= IV_MAX; i++)
		for (c = 0; c < CI_MAX; c++, j++)
			smtd_dbg("mtp_data[%d] = %d\n", j, mtp_data[j]);
	for (i = 0, j = 0; i < IV_MAX; i++)
		for (c = 0; c < CI_MAX; c++, j++)
			smtd_dbg("mtp[%d] = %d\n", j, mtp[j]);
	for (i = 0, j = 0; i < IV_MAX; i++) {
		for (c = 0; c < CI_MAX; c++, j++)
			smtd_dbg("%04d ", mtp[j]);
	smtd_dbg("\n");
	}
#endif
}

static int init_gamma_table(struct lcd_info *lcd , const u8 *mtp_data)
{
	int i, c, j, v;
	int ret = 0;
	int *pgamma;
	int **gamma_table;

	/* allocate memory for local gamma table */
	gamma_table = kzalloc(IBRIGHTNESS_MAX * sizeof(int *), GFP_KERNEL);
	if (!gamma_table) {
		pr_err("failed to allocate gamma table\n");
		ret = -ENOMEM;
		goto err_alloc_gamma_table;
	}

	for (i = 0; i < IBRIGHTNESS_MAX; i++) {
		gamma_table[i] = kzalloc(IV_MAX*CI_MAX * sizeof(int), GFP_KERNEL);
		if (!gamma_table[i]) {
			pr_err("failed to allocate gamma\n");
			ret = -ENOMEM;
			goto err_alloc_gamma;
		}
	}

	/* allocate memory for gamma table */
	lcd_info_data.gamma_table = kzalloc(GAMMA_MAX * sizeof(u8 *), GFP_KERNEL);
	if (!lcd_info_data.gamma_table) {
		pr_err("failed to allocate gamma table 2\n");
		ret = -ENOMEM;
		goto err_alloc_gamma_table2;
	}

	for (i = 0; i < GAMMA_MAX; i++) {
		lcd_info_data.gamma_table[i] = kzalloc(GAMMA_PARAM_SIZE * sizeof(u8), GFP_KERNEL);
		if (!lcd_info_data.gamma_table[i]) {
			pr_err("failed to allocate gamma 2\n");
			ret = -ENOMEM;
			goto err_alloc_gamma2;
		}
		lcd_info_data.gamma_table[i][0] = 0xCA;
	}

	/* calculate gamma table */
	init_mtp_data(lcd, mtp_data);
	dynamic_aid(lcd_info_data.daid, gamma_table);

	/* relocate gamma order */
	for (i = 0; i < GAMMA_MAX; i++) {
		/* Brightness table */
		v = IV_MAX - 1;
		pgamma = &gamma_table[i][v * CI_MAX];
		for (c = 0, j = 1; c < CI_MAX; c++, pgamma++) {
			if (*pgamma & 0x100)
				lcd_info_data.gamma_table[i][j++] = 1;
			else
				lcd_info_data.gamma_table[i][j++] = 0;

			lcd_info_data.gamma_table[i][j++] = *pgamma & 0xff;
		}

		for (v = IV_MAX - 2; v >= 0; v--) {
			pgamma = &gamma_table[i][v * CI_MAX];
			for (c = 0; c < CI_MAX; c++, pgamma++)
				lcd_info_data.gamma_table[i][j++] = *pgamma;
		}
#if 0
		smtd_dbg("candela_table %03d: ", candela_table[i]);
		for (v = 0; v < GAMMA_PARAM_SIZE; v++)
			smtd_dbg("%03d ", lcd_info_data.gamma_table[i][v]);
		smtd_dbg("\n");
#endif
	}

	/* free local gamma table */
	for (i = 0; i < IBRIGHTNESS_MAX; i++)
		kfree(gamma_table[i]);
	kfree(gamma_table);

	return 0;

err_alloc_gamma2:
	while (i > 0) {
		kfree(lcd_info_data.gamma_table[i-1]);
		i--;
	}
	kfree(lcd_info_data.gamma_table);
err_alloc_gamma_table2:
	i = IBRIGHTNESS_MAX;
err_alloc_gamma:
	while (i > 0) {
		kfree(gamma_table[i-1]);
		i--;
	}
	kfree(gamma_table);
err_alloc_gamma_table:
	return ret;
}

static int init_aid_dimming_table(struct lcd_info *lcd, const u8 *mtp_data)
{
	int i;

	for (i = 0; i < GAMMA_MAX; i++) {
		memcpy(lcd_info_data.aor[i], SEQ_AOR_CONTROL, ARRAY_SIZE(SEQ_AOR_CONTROL));
		lcd_info_data.aor[i][4] = aor_cmd[i][0];
		lcd_info_data.aor[i][5] = aor_cmd[i][1];
#if 0
		int j;
		for (j = 0; j < ARRAY_SIZE(SEQ_AOR_CONTROL); j++)
			smtd_dbg("%02X ", lcd_info_data.aor[i][j]);
		smtd_dbg("\n");
#endif
	}

	return 0;
}

static int init_elvss_table(struct lcd_info *lcd, u8 *elvss_data)
{
	int i, j, ret;

	lcd_info_data.elvss_table = kzalloc(ELVSS_STATUS_MAX * sizeof(u8 *), GFP_KERNEL);

	if (IS_ERR_OR_NULL(lcd_info_data.elvss_table)) {
		pr_err("failed to allocate elvss table\n");
		ret = -ENOMEM;
		goto err_alloc_elvss_table;
	}

	for (i = 0; i < ELVSS_STATUS_MAX; i++) {
		lcd_info_data.elvss_table[i] = kzalloc(ELVSS_PARAM_SIZE * sizeof(u8), GFP_KERNEL);
		if (IS_ERR_OR_NULL(lcd_info_data.elvss_table[i])) {
			pr_err("failed to allocate elvss\n");
			ret = -ENOMEM;
			goto err_alloc_elvss;
		}

		for (j = 0; j < LDI_ELVSS_LEN; j++)
			lcd_info_data.elvss_table[i][j+1] = elvss_data[j];

		lcd_info_data.elvss_table[i][0] = 0xB6;
		lcd_info_data.elvss_table[i][1] = 0x28;
		lcd_info_data.elvss_table[i][2] = ELVSS_TABLE[i];
	}

	return 0;

err_alloc_elvss:
	/* should be kfree elvss with k */
	while (i > 0) {
		kfree(lcd_info_data.elvss_table[i-1]);
		i--;
	}
	kfree(lcd_info_data.elvss_table);
err_alloc_elvss_table:
	return ret;
}
#if 0
static int init_hbm_parameter(void *lcd_handle, u8 *elvss_data, u8 *hbm_data)
{
	/* CA 1~6 = B5 13~18 */
	lcd_info_data.gamma_table[GAMMA_HBM][1] = hbm_data[12];
	lcd_info_data.gamma_table[GAMMA_HBM][2] = hbm_data[13];
	lcd_info_data.gamma_table[GAMMA_HBM][3] = hbm_data[14];
	lcd_info_data.gamma_table[GAMMA_HBM][4] = hbm_data[15];
	lcd_info_data.gamma_table[GAMMA_HBM][5] = hbm_data[16];
	lcd_info_data.gamma_table[GAMMA_HBM][6] = hbm_data[17];

	/* CA 7~9 = B5 26~28 */
	lcd_info_data.gamma_table[GAMMA_HBM][7] = hbm_data[25];
	lcd_info_data.gamma_table[GAMMA_HBM][8] = hbm_data[26];
	lcd_info_data.gamma_table[GAMMA_HBM][9] = hbm_data[27];

	/* CA 10~21 = B6 3~14 */
	lcd_info_data.gamma_table[GAMMA_HBM][10] = elvss_data[2];
	lcd_info_data.gamma_table[GAMMA_HBM][11] = elvss_data[3];
	lcd_info_data.gamma_table[GAMMA_HBM][12] = elvss_data[4];
	lcd_info_data.gamma_table[GAMMA_HBM][13] = elvss_data[5];
	lcd_info_data.gamma_table[GAMMA_HBM][14] = elvss_data[6];
	lcd_info_data.gamma_table[GAMMA_HBM][15] = elvss_data[7];
	lcd_info_data.gamma_table[GAMMA_HBM][16] = elvss_data[8];
	lcd_info_data.gamma_table[GAMMA_HBM][17] = elvss_data[9];
	lcd_info_data.gamma_table[GAMMA_HBM][18] = elvss_data[10];
	lcd_info_data.gamma_table[GAMMA_HBM][19] = elvss_data[11];
	lcd_info_data.gamma_table[GAMMA_HBM][20] = elvss_data[12];
	lcd_info_data.gamma_table[GAMMA_HBM][21] = elvss_data[13];

	/* B6 17th = B5 19th */
	lcd_info_data.elvss_table[ELVSS_STATUS_HBM][17] = hbm_data[18];

	return 0;
}
#endif
static int update_brightness(void *lcd_handle, u8 force)
{
	u32 brightness;

	if (!lcd_info_data.connected)
			return 0;

	mutex_lock(&lcd_info_data.bl_lock);

	brightness = lcd_info_data.bd->props.brightness;
	pr_debug("update_brightness %d \n", brightness);
	lcd_info_data.bl = get_backlight_level_from_brightness(brightness);
#if 0
	if (LEVEL_IS_HBM(lcd_info_data.auto_brightness) && (brightness == lcd_info_data.bd->props.max_brightness))
		lcd_info_data.bl = GAMMA_HBM;
#endif
	if ((force) || ((lcd_info_data.ldi_enable) && (lcd_info_data.current_bl != lcd_info_data.bl))) {
		s6e88a0_gamma_ctl(lcd_handle);

		s6e88a0_aid_parameter_ctl(lcd_handle, force);

		s6e88a0_set_elvss(lcd_handle, force);

		s6e88a0_set_acl(lcd_handle, force);

		lcd_info_data.current_bl = lcd_info_data.bl;

/*
		smtd_dbg("bl=%d, candela=%d\n", \
			 lcd_info_data.bl, candela_table[lcd_info_data.bl]);
*/
	}

	mutex_unlock(&lcd_info_data.bl_lock);

	return 0;
}


int S6E88A0_BacklightInit(void *lcd_handle)
{
	int ret = 0;
	unsigned char read_data[60];
	u8 mtp_data[LDI_MTP_LEN] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	u8 elvss_data[LDI_ELVSS_LEN] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0 };
	u8 hbm_data[LDI_HBM_LEN] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	lcd_info_data.bl = DEFAULT_GAMMA_LEVEL;
	lcd_info_data.current_bl = lcd_info_data.bl;
	lcd_info_data.bd->props.brightness = DEFAULT_BRIGHTNESS;
	lcd_info_data.acl_enable = 0;
	lcd_info_data.current_acl = 0;
	lcd_info_data.auto_brightness = 0;
	lcd_info_data.connected = 1;
	lcd_info_data.siop_enable = 0;

	s6e88a0_write(lcd_handle, SEQ_TEST_KEY_ON_F0,
				ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	s6e88a0_write(lcd_handle, SEQ_TEST_KEY_ON_FC,
				ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	s6e88a0_write(lcd_handle, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));

	msleep(20);
	s6e88a0_write(lcd_handle, SEQ_GAMMA_CONDITION_SET,
				ARRAY_SIZE(SEQ_GAMMA_CONDITION_SET));
	s6e88a0_write(lcd_handle, SEQ_AOR_CONTROL,
				ARRAY_SIZE(SEQ_AOR_CONTROL));
	s6e88a0_write(lcd_handle, SEQ_ELVS_CONDITION,
				ARRAY_SIZE(SEQ_ELVS_CONDITION));
	s6e88a0_write(lcd_handle, SEQ_GAMMA_UPDATE,
				ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	msleep(120);
	s6e88a0_write(lcd_handle, SEQ_AVDD, ARRAY_SIZE(SEQ_AVDD));
	ret = s6e88a0_read_id(lcd_handle, &read_data[0]);
	if (ret < 0) {
		pr_err("panel is not connected well\n");
		lcd_info_data.connected = 0;
		return -1;
	}
	printk(KERN_INFO "read_data(RDID0) = %02X\n", read_data[0]);
	printk(KERN_INFO "read_data(RDID1) = %02X\n", read_data[1]);
	printk(KERN_INFO "read_data(RDID2) = %02X\n",	read_data[2]);

	msleep(120);
	//s6e88a0_write(lcd_handle, SEQ_AVC, ARRAY_SIZE(SEQ_AVC));
	s6e88a0_write(lcd_handle, SEQ_TEST_KEY_OFF_FC,
			ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	s6e88a0_write(lcd_handle, SEQ_DISPLAY_ON,
				ARRAY_SIZE(SEQ_DISPLAY_ON));
	if (lcd_info_data.connected)
		msleep(20);
	s6e88a0_read_mtp(lcd_handle, mtp_data);
	s6e88a0_read_elvss(lcd_handle, elvss_data);
	s6e88a0_read_coordinate(lcd_handle, hbm_data);
	init_dynamic_aid(lcd_handle);
	ret = init_gamma_table(lcd_handle, mtp_data);
	ret += init_aid_dimming_table(lcd_handle, mtp_data);
	ret += init_elvss_table(lcd_handle, elvss_data);
	//ret += init_hbm_parameter(lcd_handle, elvss_data, hbm_data);
	lcd_info_data.ldi_enable = 1;
#if 0	
	for (i = 0; i < GAMMA_MAX; i++) {
		smtd_dbg("%03d: ", candela_table[i]);
		for (j = 0; j < GAMMA_PARAM_SIZE; j++)
			smtd_dbg("%02X, ", lcd_info_data.gamma_table[i][j]);
		smtd_dbg("\n");
	}
	for (i = 0; i < ELVSS_STATUS_MAX; i++) {
		for (j = 0; j < ELVSS_PARAM_SIZE; j++)
			smtd_dbg("%02X, ", lcd_info_data.elvss_table[i][j]);
		smtd_dbg("\n");
	}
#endif
	return 0;
}

static int S6E88A0_panel_init(unsigned int mem_size)
{
	void *screen_handle;
	screen_disp_start_lcd start_lcd;
	screen_disp_stop_lcd disp_stop_lcd;
	screen_disp_set_lcd_if_param set_lcd_if_param;
	screen_disp_set_address set_address;
	screen_disp_delete disp_delete;
	int ret = 0;
	int retry_count = S6E88A0_INIT_RETRY_COUNT;
	screen_disp_set_dsi_mode set_dsi_mode;
	screen_disp_set_rt_standby set_rt_standby;

#ifdef S6E88A0_POWAREA_MNG_ENABLE
	void *system_handle;
	system_pmg_param powarea_start_notify;
	system_pmg_delete pmg_delete;
#endif

	pr_debug("%s\n", __func__);

	initialize_now = true;

#ifdef S6E88A0_POWAREA_MNG_ENABLE
	pr_debug("Start A4LC power area\n");
	system_handle = system_pwmng_new();

	/* Notifying the Beginning of Using Power Area */
	powarea_start_notify.handle		= system_handle;
	powarea_start_notify.powerarea_name	= RT_PWMNG_POWERAREA_A4LC;
	ret = system_pwmng_powerarea_start_notify(&powarea_start_notify);
	if (ret != SMAP_LIB_PWMNG_OK)
		pr_debug("system_pwmng_powerarea_start_notify err!\n");
	pmg_delete.handle = system_handle;
	system_pwmng_delete(&pmg_delete);
#endif

	screen_handle =  screen_display_new();

	mipi_display_reset();

	/* Setting peculiar to panel */
	set_lcd_if_param.handle			= screen_handle;
	set_lcd_if_param.port_no		= irq_portno;
	set_lcd_if_param.lcd_if_param		= &r_mobile_lcd_if_param;
	set_lcd_if_param.lcd_if_param_mask	= &r_mobile_lcd_if_param_mask;
	ret = screen_display_set_lcd_if_parameters(&set_lcd_if_param);
	if (ret != SMAP_LIB_DISPLAY_OK) {
		pr_debug("disp_set_lcd_if_parameters err!\n");
		goto out;
	}

	/* Setting FB address */
	set_address.handle	= screen_handle;
	set_address.output_mode	= RT_DISPLAY_LCD1;
	set_address.buffer_id	= RT_DISPLAY_BUFFER_A;
	set_address.address	= g_fb_start;
	set_address.size	= g_fb_sz;
	ret = screen_display_set_address(&set_address);
	if (ret != SMAP_LIB_DISPLAY_OK) {
		pr_debug("disp_set_address err!\n");
		goto out;
	}

	/* Start a display to LCD */
	start_lcd.handle	= screen_handle;
	start_lcd.output_mode	= RT_DISPLAY_LCD1;
	ret = screen_display_start_lcd(&start_lcd);
	if (ret != SMAP_LIB_DISPLAY_OK) {
		pr_debug("disp_start_lcd err!\n");
		goto out;
	}
	/* RT Standby Mode (FROMHS,TOHS) */
	set_rt_standby.handle	= screen_handle;
	set_rt_standby.rt_standby	= DISABLE_RT_STANDBY;
	screen_display_set_rt_standby(&set_rt_standby);
retry:
	is_dsi_read_enabled = 1;

	/* Initalize Backlight Parameter */
	if (retry_count == S6E88A0_INIT_RETRY_COUNT)
	ret = S6E88A0_BacklightInit(screen_handle);
	else {
		S6E88A0_InitSequence(screen_handle);
		ret = S6E88A0_BacklightInit(screen_handle);
	}

	if (ret != 0) {
		pr_debug("panel_specific_cmdset err!\n");
		is_dsi_read_enabled = 0;
		if (retry_count == 0) {
			pr_debug("retry count 0!!!!\n");
			mipi_display_power_off();
			disp_stop_lcd.handle		= screen_handle;
			disp_stop_lcd.output_mode	= RT_DISPLAY_LCD1;
			ret = screen_display_stop_lcd(&disp_stop_lcd);
			if (ret != SMAP_LIB_DISPLAY_OK)
				pr_debug("display_stop_lcd err!\n");

			ret = -ENODEV;
			goto out;
		} else {
			retry_count--;

			mipi_display_power_off();

			/* Stop a display to LCD */
			disp_stop_lcd.handle		= screen_handle;
			disp_stop_lcd.output_mode	= RT_DISPLAY_LCD1;
			ret = screen_display_stop_lcd(&disp_stop_lcd);
			if (ret != SMAP_LIB_DISPLAY_OK) {
				pr_debug("display_stop_lcd err!\n");
				goto out;
			}

			disp_delete.handle = screen_handle;
			screen_display_delete(&disp_delete);
			screen_handle =  screen_display_new();

			mipi_display_reset();
			
			/* Start a display to LCD */
			start_lcd.handle	= screen_handle;
			start_lcd.output_mode	= RT_DISPLAY_LCD1;
			ret = screen_display_start_lcd(&start_lcd);
			if (ret != SMAP_LIB_DISPLAY_OK) {
				pr_debug("disp_start_lcd err!\n");
				goto out;
			}

			/* Set DSI mode (FROMHS,TOHS) */
			set_dsi_mode.handle	= screen_handle;
			set_dsi_mode.dsimode	= DSI_FROMHS;
			screen_display_set_dsi_mode(&set_dsi_mode);

			printk(KERN_INFO"\n GPIO RESET\n");
			gpio_direction_output(reset_gpio, 1);
			usleep_range(2000, 2000);
			gpio_direction_output(reset_gpio, 0);
			usleep_range(2000, 2000);
			gpio_direction_output(reset_gpio, 1);
	
			msleep(120);
			set_dsi_mode.dsimode	= DSI_TOHS;
			screen_display_set_dsi_mode(&set_dsi_mode);			

			goto retry;
		}
	}

	pr_debug("Panel initialized with Video mode\n");

#if defined(CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG)
	esd_duration = DURATION_TIME;
	schedule_delayed_work(&esd_check_work, msecs_to_jiffies(esd_duration));
#endif /* CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG */

#if defined(CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG) || defined(CONFIG_FB_LCD_ESD)
	esd_check_flag = ESD_CHECK_ENABLE;
#endif /* CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG or CONFIG_FB_LCD_ESD */

#if defined(CONFIG_FB_LCD_ESD)
	ret = request_irq(esd_detect_irq, lcd_esd_irq_handler,
			IRQF_ONESHOT, esd_devname, &esd_irq_requested);
	if (ret != 0)
		pr_debug("request_irq err! =%d\n", ret);
	else
		esd_irq_requested = true;
#endif /* CONFIG_FB_LCD_ESD */

out:
	/* RT Standby Mode (FROMHS,TOHS) */
	set_rt_standby.handle	= screen_handle;
	set_rt_standby.rt_standby	= ENABLE_RT_STANDBY;
	screen_display_set_rt_standby(&set_rt_standby);
	
	disp_delete.handle = screen_handle;
	screen_display_delete(&disp_delete);

	initialize_now = false;

	return ret;
}

static int S6E88A0_panel_suspend(void)
{
	void *screen_handle;
	screen_disp_stop_lcd disp_stop_lcd;
	screen_disp_start_lcd start_lcd;
	screen_disp_delete disp_delete;
	int ret;

#ifdef S6E88A0_POWAREA_MNG_ENABLE
	void *system_handle;
	system_pmg_param powarea_end_notify;
	system_pmg_delete pmg_delete;
#endif

#if defined(CONFIG_FB_LCD_ESD)
	if (esd_irq_requested)
		free_irq(esd_detect_irq, &esd_irq_requested);
	esd_irq_requested = false;
#endif /* CONFIG_FB_LCD_ESD */

#if defined(CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG) || defined(CONFIG_FB_LCD_ESD)
	esd_check_flag = ESD_CHECK_DISABLE;
	mutex_lock(&esd_check_mutex);
#endif /* CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG or CONFIG_FB_LCD_ESD */

	pr_debug("%s\n", __func__);

	screen_handle =  screen_display_new();

	is_dsi_read_enabled = 0;
	lcd_info_data.ldi_enable = 0;
	#if 0
	/* Stop a display to LCD */
	disp_stop_lcd.handle		= screen_handle;
	disp_stop_lcd.output_mode	= RT_DISPLAY_LCD1;
	ret = screen_display_stop_lcd(&disp_stop_lcd);
	if (ret != SMAP_LIB_DISPLAY_OK)
		pr_debug("display_stop_lcd err!\n");
	#endif

	start_lcd.handle	= screen_handle;
	start_lcd.output_mode	= RT_DISPLAY_LCD1;
	ret = screen_display_start_lcd(&start_lcd);
	if (ret != SMAP_LIB_DISPLAY_OK)
		pr_debug("disp_start_lcd err!\n");

	/* Transmit DSI command peculiar to a panel */
	ret = S6E88A0_DemiseSequence(screen_handle);
	if (ret != 0) {
		pr_debug("panel_specific_cmdset err!\n");
		/* continue */
	}

	/* Stop a display to LCD */
	disp_stop_lcd.handle		= screen_handle;
	disp_stop_lcd.output_mode	= RT_DISPLAY_LCD1;
	ret = screen_display_stop_lcd(&disp_stop_lcd);
	if (ret != SMAP_LIB_DISPLAY_OK)
		pr_debug("display_stop_lcd err!\n");

	mipi_display_power_off();

	disp_delete.handle = screen_handle;
	screen_display_delete(&disp_delete);

#ifdef S6E88A0_POWAREA_MNG_ENABLE
	pr_debug("End A4LC power area\n");
	system_handle = system_pwmng_new();

	/* Notifying the Beginning of Using Power Area */
	powarea_end_notify.handle		= system_handle;
	powarea_end_notify.powerarea_name	= RT_PWMNG_POWERAREA_A4LC;
	ret = system_pwmng_powerarea_end_notify(&powarea_end_notify);
	if (ret != SMAP_LIB_PWMNG_OK)
		pr_debug("system_pwmng_powerarea_end_notify err!\n");

	pmg_delete.handle = system_handle;
	system_pwmng_delete(&pmg_delete);
#endif

#if defined(CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG) || defined(CONFIG_FB_LCD_ESD)
	mutex_unlock(&esd_check_mutex);
#endif /* CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG or CONFIG_FB_LCD_ESD */


	return 0;
}

static int S6E88A0_panel_resume(void)
{
	void *screen_handle;
	screen_disp_start_lcd start_lcd;
	screen_disp_stop_lcd disp_stop_lcd;
	screen_disp_delete disp_delete;
	int ret = 0;
	int retry_count = S6E88A0_INIT_RETRY_COUNT;
	screen_disp_set_dsi_mode set_dsi_mode;
	screen_disp_set_rt_standby set_rt_standby;
	
#ifdef S6E88A0_POWAREA_MNG_ENABLE
	void *system_handle;
	system_pmg_param powarea_start_notify;
	system_pmg_delete pmg_delete;
#endif

#if defined(CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG) || defined(CONFIG_FB_LCD_ESD)
	/* Wait for end of check ESD */
	mutex_lock(&esd_check_mutex);
#endif /* CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG or CONFIG_FB_LCD_ESD */

	pr_debug("%s\n", __func__);

#ifdef S6E88A0_POWAREA_MNG_ENABLE
	pr_debug("Start A4LC power area\n");
	system_handle = system_pwmng_new();

	/* Notifying the Beginning of Using Power Area */
	powarea_start_notify.handle		= system_handle;
	powarea_start_notify.powerarea_name	= RT_PWMNG_POWERAREA_A4LC;
	ret = system_pwmng_powerarea_start_notify(&powarea_start_notify);
	if (ret != SMAP_LIB_PWMNG_OK)
		pr_debug("system_pwmng_powerarea_start_notify err!\n");
	pmg_delete.handle = system_handle;
	system_pwmng_delete(&pmg_delete);
#endif

retry:

	screen_handle =  screen_display_new();
	//printk("SSK mipi_display_reset \n");
	/* LCD panel reset */
	mipi_display_reset();
	/* Start a display to LCD */
	start_lcd.handle	= screen_handle;
	start_lcd.output_mode	= RT_DISPLAY_LCD1;
	ret = screen_display_start_lcd(&start_lcd);
	if (ret != SMAP_LIB_DISPLAY_OK) {
		pr_debug("disp_start_lcd err!\n");
		goto out;
	}

	/* RT Standby Mode (FROMHS,TOHS) */
	set_rt_standby.handle	= screen_handle;
	set_rt_standby.rt_standby	= DISABLE_RT_STANDBY;
	screen_display_set_rt_standby(&set_rt_standby);

	/* Set DSI mode (FROMHS,TOHS) */
	set_dsi_mode.handle	= screen_handle;
	set_dsi_mode.dsimode	= DSI_FROMHS;
	screen_display_set_dsi_mode(&set_dsi_mode);

	//printk(KERN_INFO"\n GPIO RESET\n");
	gpio_direction_output(reset_gpio, 1);
	usleep_range(2000, 2000);
	gpio_direction_output(reset_gpio, 0);
	usleep_range(2000, 2000);
	gpio_direction_output(reset_gpio, 1);
	
	msleep(120);
	set_dsi_mode.dsimode	= DSI_TOHS;
	screen_display_set_dsi_mode(&set_dsi_mode);

	/* Transmit DSI command peculiar to a panel */
	ret = S6E88A0_InitSequence(screen_handle);
	if (ret != 0) {
		pr_debug("panel_specific_cmdset err!\n");
		is_dsi_read_enabled = 0;
		/* RT Standby Mode (FROMHS,TOHS) */
		set_rt_standby.handle	= screen_handle;
		set_rt_standby.rt_standby	= ENABLE_RT_STANDBY;
		screen_display_set_rt_standby(&set_rt_standby);
	
		if (retry_count == 0) {
			pr_debug("retry count 0!!!!\n");

			mipi_display_power_off();

			disp_stop_lcd.handle		= screen_handle;
			disp_stop_lcd.output_mode	= RT_DISPLAY_LCD1;
			ret = screen_display_stop_lcd(&disp_stop_lcd);
			if (ret != SMAP_LIB_DISPLAY_OK)
				pr_debug("display_stop_lcd err!\n");

			ret = -ENODEV;
			goto out;
		} else {
			retry_count--;

			mipi_display_power_off();

			/* Stop a display to LCD */
			disp_stop_lcd.handle		= screen_handle;
			disp_stop_lcd.output_mode	= RT_DISPLAY_LCD1;
			ret = screen_display_stop_lcd(&disp_stop_lcd);
			if (ret != SMAP_LIB_DISPLAY_OK)
				pr_debug("display_stop_lcd err!\n");

			disp_delete.handle = screen_handle;
			screen_display_delete(&disp_delete);
			goto retry;
		}
	}
		/* RT Standby Mode (FROMHS,TOHS) */
		set_rt_standby.handle	= screen_handle;
		set_rt_standby.rt_standby	= ENABLE_RT_STANDBY;
		screen_display_set_rt_standby(&set_rt_standby);

#if defined(CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG)
	/* Schedule check ESD */
	esd_duration = DURATION_TIME;
	schedule_delayed_work(&esd_check_work, msecs_to_jiffies(esd_duration));
#endif /* CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG */

#if defined(CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG) || defined(CONFIG_FB_LCD_ESD)
	esd_check_flag = ESD_CHECK_ENABLE;
#endif /* CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG or CONFIG_FB_LCD_ESD */

#if defined(CONFIG_FB_LCD_ESD)
	ret = request_irq(esd_detect_irq, lcd_esd_irq_handler,
			IRQF_ONESHOT, esd_devname, &esd_irq_requested);
	if (ret != 0)
		pr_debug("request_irq err! =%d\n", ret);
	else
		esd_irq_requested = true;

#endif /* CONFIG_FB_LCD_ESD */

out:
	disp_delete.handle = screen_handle;
	screen_display_delete(&disp_delete);

#if defined(CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG) || defined(CONFIG_FB_LCD_ESD)
	mutex_unlock(&esd_check_mutex);
#endif /* CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG or CONFIG_FB_LCD_ESD */
	lcd_info_data.ldi_enable = 1;
	return ret;
}

static int s6e88a0_set_brightness(struct backlight_device *bd)
{
	int ret = 0;
	void *screen_handle;
	screen_disp_delete disp_delete;
	printk(KERN_INFO "%s: brightness=%d\n", __func__, bd->props.brightness);
/*	smtd_dbg("%s: brightness=%d\n", __func__, bd->props.brightness);*/
	if (bd->props.brightness < MIN_BRIGHTNESS ||
		bd->props.brightness > bd->props.max_brightness) {
		pr_err("lcd brightness should be %d to %d. now %d\n",
			MIN_BRIGHTNESS, lcd_info_data.bd->props.max_brightness, bd->props.brightness);
		return -EINVAL;
	}
	if (lcd_info_data.ldi_enable) {
		screen_handle =  screen_display_new();

		ret = update_brightness(screen_handle, 0);
		disp_delete.handle = screen_handle;
		screen_display_delete(&disp_delete);
	if (ret < 0) {
			pr_err("err in %s\n", __func__);
			return -EINVAL;
		}
	}

	return ret;
}

static int s6e88a0_get_brightness(struct backlight_device *bd)
{
	struct lcd_info *lcd = bl_get_data(bd);

	return candela_table[lcd->bl];
}

static const struct backlight_ops s6e88a0_backlight_ops  = {
	.get_brightness = s6e88a0_get_brightness,
	.update_status = s6e88a0_set_brightness,
};

static ssize_t power_reduce_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	char temp[3];

	sprintf(temp, "%d\n", lcd->acl_enable);
	strcpy(buf, temp);

	return strlen(buf);
}

static ssize_t power_reduce_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;
	int rc;
	void *screen_handle;
	screen_disp_delete disp_delete;
	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (lcd->acl_enable != value) {
			dev_info(dev, "%s: %d, %d\n", __func__, lcd->acl_enable, value);
			mutex_lock(&lcd->bl_lock);
			lcd->acl_enable = value;
			mutex_unlock(&lcd->bl_lock);
			if (lcd->ldi_enable) {
				screen_handle =  screen_display_new();
				update_brightness(screen_handle, 1);
				disp_delete.handle = screen_handle;
				screen_display_delete(&disp_delete);
			}
		}
	}
	return size;
}

static ssize_t lcd_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char temp[] = "SMD_401001\n";
	strcat(buf, temp);
	return strlen(buf);
}

static ssize_t window_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	char temp[15];
	void *screen_handle;
	screen_disp_delete disp_delete;

	if (lcd->ldi_enable) {
		screen_handle =  screen_display_new();
		s6e88a0_read_id(screen_handle, lcd->id);
		disp_delete.handle = screen_handle;
		screen_display_delete(&disp_delete);
	}
	sprintf(temp, "%x %x %x\n", lcd->id[0], lcd->id[1], lcd->id[2]);
	strcat(buf, temp);
	return strlen(buf);
}

static ssize_t auto_brightness_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	char temp[3];
	sprintf(temp, "%d\n", lcd->auto_brightness);
	strcpy(buf, temp);
	return strlen(buf);
}

static ssize_t auto_brightness_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;
	int rc;
	void *screen_handle;
	screen_disp_delete disp_delete;
	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (lcd->auto_brightness != value) {
			dev_info(dev, "%s: %d, %d\n", __func__, lcd->auto_brightness, value);
			mutex_lock(&lcd->bl_lock);
			lcd->auto_brightness = value;
			mutex_unlock(&lcd->bl_lock);
			if (lcd->ldi_enable) {
				screen_handle =  screen_display_new();
				update_brightness(screen_handle, 0);
				disp_delete.handle = screen_handle;
				screen_display_delete(&disp_delete);
	}
		}
	}
	return size;
}

static ssize_t siop_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	char temp[3];
	sprintf(temp, "%d\n", lcd->siop_enable);
	strcpy(buf, temp);
	return strlen(buf);
}

static ssize_t siop_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	int value;
	int rc;
	void *screen_handle;
	screen_disp_delete disp_delete;
	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		if (lcd->siop_enable != value) {
			dev_info(dev, "%s: %d, %d\n", __func__, lcd->siop_enable, value);
			mutex_lock(&lcd->bl_lock);
			lcd->siop_enable = value;
			mutex_unlock(&lcd->bl_lock);
			if (lcd->ldi_enable) {
				screen_handle =  screen_display_new();
				update_brightness(screen_handle, 1);
				disp_delete.handle = screen_handle;
				screen_display_delete(&disp_delete);
			}
		}
	}
	return size;
}
#if 0
static ssize_t color_coordinate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	sprintf(buf, "%d, %d\n", lcd->coordinate[0], lcd->coordinate[1]);
	return strlen(buf);
}
#endif
static ssize_t manufacture_date_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	u16 year;
	u8 month, manufacture_data[LDI_HBM_LEN] = {0,};

	if (lcd->ldi_enable)
		s6e88a0_read(lcd, LDI_HBM_REG, manufacture_data, LDI_HBM_LEN, 1);

	year = ((manufacture_data[23] & 0xF0) >> 4) + 2011;
	month = manufacture_data[23] & 0xF;

	sprintf(buf, "%d, %d, %d\n", year, month, manufacture_data[24]);

	return strlen(buf);
}

static ssize_t parameter_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct lcd_info *lcd = dev_get_drvdata(dev);
	char *pos = buf;
	unsigned char temp[50] = {0,};
	int i;

	if (!lcd->ldi_enable)
		return -EINVAL;

	s6e88a0_write(lcd, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));

	/* ID */
	s6e88a0_read(lcd, LDI_ID_REG, temp, LDI_ID_LEN, 1);
	pos += sprintf(pos, "ID    [04]: ");
	for (i = 0; i < LDI_ID_LEN; i++)
		pos += sprintf(pos, "%02x, ", temp[i]);
	pos += sprintf(pos, "\n");

	/* ACL */
	s6e88a0_read(lcd, 0x56, temp, ACL_PARAM_SIZE, 1);
	pos += sprintf(pos, "ACL   [56]: ");
	for (i = 0; i < ACL_PARAM_SIZE; i++)
		pos += sprintf(pos, "%02x, ", temp[i]);
	pos += sprintf(pos, "\n");

	/* ACL Parameter */
	s6e88a0_read(lcd, 0xB5, temp, 5, 1);
	pos += sprintf(pos, "ACL   [B5]: ");
	for (i = 0; i < 5; i++)
		pos += sprintf(pos, "%02x, ", temp[i]);
	pos += sprintf(pos, "\n");

	/* ACL Result */
	s6e88a0_read(lcd, 0xB3, temp, 13, 1);
	pos += sprintf(pos, "ACL   [B3]: ");
	for (i = 0; i < 13; i++)
		pos += sprintf(pos, "%02x, ", temp[i]);
	pos += sprintf(pos, "\n");

	/* ELVSS */
	s6e88a0_read(lcd, LDI_ELVSS_REG, temp, ELVSS_PARAM_SIZE, 1);
	pos += sprintf(pos, "ELVSS [B6]: ");
	for (i = 0; i < ELVSS_PARAM_SIZE; i++)
		pos += sprintf(pos, "%02x, ", temp[i]);
	pos += sprintf(pos, "\n");

	/* GAMMA */
	s6e88a0_read(lcd, 0xCA, temp, GAMMA_PARAM_SIZE, 1);
	pos += sprintf(pos, "GAMMA [CA]: ");
	for (i = 0; i < GAMMA_PARAM_SIZE; i++)
		pos += sprintf(pos, "%02x, ", temp[i]);
	pos += sprintf(pos, "\n");

	/* MTP */
	s6e88a0_read(lcd, 0xC8, temp, GAMMA_PARAM_SIZE, 1);
	pos += sprintf(pos, "MTP   [C8]: ");
	for (i = 0; i < GAMMA_PARAM_SIZE; i++)
		pos += sprintf(pos, "%02x, ", temp[i]);
	pos += sprintf(pos, "\n");

	s6e88a0_write(lcd, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));

	return pos - buf;
}

static DEVICE_ATTR(power_reduce, 0664, power_reduce_show, power_reduce_store);
static DEVICE_ATTR(lcd_type, 0444, lcd_type_show, NULL);
static DEVICE_ATTR(window_type, 0444, window_type_show, NULL);
static DEVICE_ATTR(auto_brightness, 0664, auto_brightness_show, auto_brightness_store);
static DEVICE_ATTR(siop_enable, 0664, siop_enable_show, siop_enable_store);
/*static DEVICE_ATTR(color_coordinate, 0444, color_coordinate_show, NULL);*/
static DEVICE_ATTR(manufacture_date, 0444, manufacture_date_show, NULL);
static DEVICE_ATTR(parameter, 0444, parameter_show, NULL);

static int S6E88A0_panel_probe(struct fb_info *info,
			    struct fb_panel_hw_info hw_info)
{
	int ret;
	struct platform_device *pdev;
	struct resource *res_irq_port;

	printk("%s\n", __func__);

	reset_gpio = hw_info.gpio_reg;

	/* GPIO control */
	gpio_request(reset_gpio, NULL);

	/* fb parent device info to platform_device */
	pdev = to_platform_device(info->device);

	/* get resource info from platform_device */
	res_irq_port	= platform_get_resource_byname(pdev,
							IORESOURCE_MEM,
							"panel_irq_port");
	if (!res_irq_port) {
		pr_debug("panel_irq_port is NULL!!\n");
		return -ENODEV;
	}
	irq_portno = res_irq_port->start;
	power_ldo_3v = regulator_get(NULL, "vlcd_3v");
	power_ldo_1v8 = regulator_get(NULL, "vlcd_1v8");

	if (power_ldo_3v == NULL || power_ldo_1v8 == NULL) {
		pr_debug("regulator_get failed\n");
		return -ENODEV;
	}
	if (regulator_enable(power_ldo_1v8)) {
		pr_err("regulator_enable failed\n");
		return -ENODEV;
	}
	if (regulator_enable(power_ldo_3v)) {
		pr_err("regulator_enable failed\n");
		return -ENODEV;
	}
	power_supplied = true;

	pr_debug("PMIC        : for panel power\n");
	pr_debug("GPIO_PORT%d : for panel reset\n", reset_gpio);
	pr_debug("IRQ%d       : for panel te\n", irq_portno);

	common_fb_info = info;
	is_dsi_read_enabled = 0;

	/* clear internal info */
	memset(&lcd_info_data, 0, sizeof(lcd_info_data));

	/* register sysfs for LCD */
	lcd_info_data.ld = lcd_device_register("panel", &pdev->dev,
			&lcd_info_data, &S6E88A0_lcd_ops);
	if (IS_ERR(lcd_info_data.ld))
		return PTR_ERR(lcd_info_data.ld);
	/* register device for backlight control */
	lcd_info_data.bd = backlight_device_register("panel",
		&pdev->dev, &lcd_info_data, &s6e88a0_backlight_ops, NULL);
	if (IS_ERR(lcd_info_data.bd)) {
		pr_err("failed to register backlight device\n");
		ret = PTR_ERR(lcd_info_data.bd);
		goto out_free_backlight;
	}
	lcd_info_data.power = FB_BLANK_UNBLANK;
	lcd_info_data.connected = 0;
	lcd_info_data.bd->props.max_brightness = MAX_BRIGHTNESS;
	lcd_info_data.bd->props.brightness = INIT_BRIGHTNESS;
	ret = device_create_file(&lcd_info_data.ld->dev, &dev_attr_power_reduce);
	if (ret < 0)
		pr_err("failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd_info_data.ld->dev, &dev_attr_lcd_type);
	if (ret < 0)
		pr_err("failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd_info_data.ld->dev, &dev_attr_window_type);
	if (ret < 0)
		pr_err("failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd_info_data.ld->dev, &dev_attr_auto_brightness);
	if (ret < 0)
		pr_err("failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd_info_data.ld->dev, &dev_attr_siop_enable);
	if (ret < 0)
		pr_err("failed to add sysfs entries, %d\n", __LINE__);

	/* ret = device_create_file(&lcd->ld->dev, &dev_attr_color_coordinate);
	if (ret < 0)
		dev_err(&lcd->ld->dev, "failed to add sysfs entries, %d\n", __LINE__); */

	ret = device_create_file(&lcd_info_data.ld->dev, &dev_attr_manufacture_date);
	if (ret < 0)
		pr_err("failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&lcd_info_data.ld->dev, &dev_attr_parameter);
	if (ret < 0)
		pr_err("failed to add sysfs entries, %d\n", __LINE__);

	mutex_init(&lcd_info_data.lock);
	mutex_init(&lcd_info_data.bl_lock);


#if defined(CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG) || defined(CONFIG_FB_LCD_ESD)
	esd_check_flag = ESD_CHECK_DISABLE;
	mutex_init(&esd_check_mutex);
#endif /* CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG or CONFIG_FB_LCD_ESD */

#if defined(CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG)
	INIT_DELAYED_WORK(&esd_check_work, S6E88A0_panel_esd_check_work);
#endif /* CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG */

#if defined(CONFIG_FB_LCD_ESD)
	/* get resource info from platform_device */
	res_irq_port	= platform_get_resource_byname(pdev,
							IORESOURCE_MEM,
							"panel_esd_irq_port");
	if (!res_irq_port) {
		pr_debug("panel_esd_irq_port is NULL!!\n");
		return -ENODEV;
	}
	esd_irq_portno = res_irq_port->start;
	/* GPIO control */
	gpio_request(esd_irq_portno, NULL);
	gpio_direction_input(esd_irq_portno);
	gpio_pull_off_port(esd_irq_portno);

	pr_debug("GPIO_PORT%d : for ESD detect\n", esd_irq_portno);

	lcd_wq = create_workqueue("lcd_esd_irq_wq");
	INIT_WORK(&esd_detect_work, lcd_esd_detect);
	esd_detect_irq = gpio_to_irq(esd_irq_portno);
	pr_debug("IRQ%d       : for ESD detect\n", esd_detect_irq);

#endif /* CONFIG_FB_LCD_ESD */
	printk("%s-----\n", __func__);
	return 0;
out_free_backlight:
	lcd_device_unregister(lcd_info_data.ld);

	return ret;
}

static int S6E88A0_panel_remove(struct fb_info *info)
{
	pr_debug("%s\n", __func__);

#if defined(CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG) || defined(CONFIG_FB_LCD_ESD)
	esd_check_flag = ESD_CHECK_DISABLE;

	/* Wait for end of check to power mode state */
	mutex_lock(&esd_check_mutex);
	mutex_unlock(&esd_check_mutex);
	mutex_destroy(&esd_check_mutex);
#endif /* CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG or CONFIG_FB_LCD_ESD */

#if defined(CONFIG_FB_LCD_ESD)
	free_irq(esd_detect_irq, &esd_irq_requested);
	gpio_free(esd_irq_portno);
#endif /* CONFIG_FB_LCD_ESD */

	gpio_free(reset_gpio);

	backlight_device_unregister(lcd_info_data.bd);
	/* unregister sysfs for LCD */
	lcd_device_unregister(lcd_info_data.ld);


	return 0;
}

static struct fb_panel_info S6E88A0_panel_info(void)
{
	pr_debug("%s\n", __func__);

	r_mobile_info.buff_address = g_fb_start;
	return r_mobile_info;
}

struct fb_panel_func r_mobile_s6e88a0_panel_func(int panel)
{

	struct fb_panel_func panel_func;

	pr_debug("%s\n", __func__);

	memset(&panel_func, 0, sizeof(struct fb_panel_func));

/* e.g. support (panel = RT_DISPLAY_LCD1) */

	if (panel == RT_DISPLAY_LCD1) {
		panel_func.panel_init    = S6E88A0_panel_init;
		panel_func.panel_suspend = S6E88A0_panel_suspend;
		panel_func.panel_resume  = S6E88A0_panel_resume;
		panel_func.panel_probe   = S6E88A0_panel_probe;
		panel_func.panel_remove  = S6E88A0_panel_remove;
		panel_func.panel_info    = S6E88A0_panel_info;
	}

	return panel_func;
}
