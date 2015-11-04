/*
 * drivers/video/r-mobile/panel/panel_hx8369b.c
 *
 * Copyright (C) 2014 Broadcom Corporation
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

#include <video/sh_mobile_lcdc.h>

#include <rtapi/screen_display.h>


#include <linux/platform_device.h>
#include <linux/fb.h>

#include <linux/regulator/consumer.h>
#include <linux/lcd.h>

#include <mach/memory-r8a7373.h>

#include "panel_common.h"

/* #define HX8369b_DRAW_BLACK_KERNEL */

#define HX8369b_POWAREA_MNG_ENABLE
/* #define HX8369b_GED_ORG */

/* #define HX8369b_SWITCH_FRAMERATE_40HZ */

#ifdef HX8369b_POWAREA_MNG_ENABLE
#include <rtapi/system_pwmng.h>
#endif

#include <linux/irqreturn.h>
#include <linux/interrupt.h>
#include <mach/gpio.h>
#include <mach/r8a7373.h>

/* panel size (mm) */
#define R_MOBILE_M_PANEL_SIZE_WIDTH     54
#define R_MOBILE_M_PANEL_SIZE_HEIGHT    95
#if 1
#define R_MOBILE_M_PANEL_PIXEL_WIDTH	 480
#define R_MOBILE_M_PANEL_PIXEL_HEIGHT	 800
#define R_MOBILE_M_PANEL_LEFT_MARGIN	 55
#define R_MOBILE_M_PANEL_RIGHT_MARGIN	 455
#define R_MOBILE_M_PANEL_HSYNC_LEN	 65
#define R_MOBILE_M_PANEL_UPPER_MARGIN	 19
#define R_MOBILE_M_PANEL_LOWER_MARGIN	 6
#define R_MOBILE_M_PANEL_VSYNC_LEN	 4
#define R_MOBILE_M_PANEL_PIXCLOCK	 19037
#define R_MOBILE_M_PANEL_H_TOTAL	 1055
#define R_MOBILE_M_PANEL_V_TOTAL	 829
#define LCD_DSITCKCR		0x00000007
#define LCD_DSI0PCKCR			 0xc014002e  // 7.6MHz
#define LCD_DSI0PHYCR			0x2a808e20  // 7.6MHz
#define LCD_SYSCONF			0x00000703
#define LCD_TIMSET0		0x4C2C7443
#define LCD_TIMSET1		0x000900A2
#define LCD_DSICTRL		0x00000001
#define LCD_VMCTR1		0x0001003E
#define LCD_VMCTR2		0x00000710
#define LCD_VMLEN1		0x05A00000
#define LCD_VMLEN2			0x00E601A2
#define LCD_VMLEN3		0x00000000
#define LCD_VMLEN4		0x00000000
#define LCD_DTCTR		0x00000007
#define LCD_MLDHCNR		0x003C0083
#define LCD_MLDHSYNR		0x00080074
#define LCD_MLDHAJR		0x00070107
#define LCD_MLDVLNR		0x0320033D
#define LCD_MLDVSYNR		0x00040327
#define LCD_MLDMT1R		0x0400000B
#define LCD_LDDCKR		0x00010040
#define LCD_MLDDCKPAT1R		0x00000000
#define LCD_MLDDCKPAT2R		0x00000000
#define LCD_PHYTEST		0x0000038C
#else
#define R_MOBILE_M_PANEL_PIXEL_WIDTH    480
#define R_MOBILE_M_PANEL_PIXEL_HEIGHT   800
#define R_MOBILE_M_PANEL_LEFT_MARGIN	 55
#define R_MOBILE_M_PANEL_RIGHT_MARGIN	 455
#define R_MOBILE_M_PANEL_HSYNC_LEN	 65
#define R_MOBILE_M_PANEL_UPPER_MARGIN	 19
#define R_MOBILE_M_PANEL_LOWER_MARGIN	 6
#define R_MOBILE_M_PANEL_VSYNC_LEN	 4
#define R_MOBILE_M_PANEL_PIXCLOCK	 24359
#define R_MOBILE_M_PANEL_H_TOTAL	 1055
#define R_MOBILE_M_PANEL_V_TOTAL	 829
#define LCD_DSITCKCR			0x00000007
#define LCD_DSI0PCKCR		0x00000038
#define LCD_DSI0PHYCR		0x2A80001D
#define LCD_SYSCONF		0x00000703
#define LCD_TIMSET0			0x4C2C6332
#define LCD_TIMSET1		0x00080092
#define LCD_DSICTRL			0x00000001
#define LCD_VMCTR1		0x0001063E
#define LCD_VMCTR2		0x00020718
#define LCD_VMLEN1		0x05A00000
#define LCD_VMLEN2		0x00E601A2
#define LCD_VMLEN3			0x00000000
#define LCD_VMLEN4			0x00000000
#define LCD_DTCTR			0x00000007
#define LCD_MLDHCNR		0x003C0083
#define LCD_MLDHSYNR		0x00080074
#define LCD_MLDHAJR		0x00070107
#define LCD_MLDVLNR		0x0320033D
#define LCD_MLDVSYNR		0x00040327
#define LCD_MLDMT1R                     0x0400000B
#define LCD_LDDCKR                     0x00010040
#define LCD_MLDDCKPAT1R                      0x00000000
#define LCD_MLDDCKPAT2R                      0x00000000
#define LCD_PHYTEST                   0x0000038C
#endif
#define LCD_MASK_DSITCKCR	0x000000BF

#define LCD_MASK_DSI0PCKCR	0xFF3F703F
#define LCD_MASK_DSI0PHYCR	0x000F3F3F

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

#ifdef HX8369b_SWITCH_FRAMERATE_40HZ
#define LCD_DSI0PCKCR_40HZ		0x0000003D
#define LCD_DSI0PHYCR_40HZ		0x2A800014
#else
#define LCD_DSI0PCKCR_30HZ		0x0000003A
#define LCD_DSI0PHYCR_30HZ		0x2A80000E
#endif


#define HX8369b_INIT_RETRY_COUNT 3

#define POWER_IS_ON(pwr)	((pwr) <= FB_BLANK_NORMAL)
static int HX8369b_panel_suspend(void);
static int HX8369b_panel_resume(void);
static void mipi_display_reset(void);
static void gpio_reset(void);
static void mipi_display_power_off(void);
static int HX8369b_panel_draw(void *screen_handle);
static int lcdfreq_resume(void);

#ifdef CONFIG_LDI_MDNIE_SUPPORT
extern void set_mdnie_enable(void);
#endif

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

#ifdef CONFIG_LDI_MDNIE_SUPPORT
#define MIPI_DSI_PANEL_MDNIE		(0x02)
#endif

static unsigned char setExtc[] = { 0xB9, 0xFF, 0x83, 0x69};

static unsigned char setPower[] = { 0xB1,
	0x0B, 0x83, 0x77, 0x00,	0x11, 0x11, 0x08, 0x08, 0x0C, 0x12 };

static unsigned char setSourceEQ[] = { 0xC6,
	0x41, 0xFF, 0x7A, 0xFF };

static unsigned char setSourceEQ2[] = { 0xE3,
	0x00, 0x00, 0x00, 0x00 };

static unsigned char setSourceOption[] = { 0xC0,
	0x73, 0x50, 0x00, 0x34, 0xC4, 0x00 };

static unsigned char setMIPIChar[] = { 0xBA,
	0x31, 0x00, 0x16, 0xCA, 0xB0, 0x0A, 0x00, 0x10, 0x28, 0x02,
	0x21, 0x21, 0x9A, 0x1A, 0x8F };

static unsigned char interfacePixelFormat[] = { 0x3A, 0x70 };

static unsigned char setRGBInterface[] = { 0xB3,
	0x83, 0x00, 0x31, 0x03, 0x01, 0x13, 0x06 };

static unsigned char setColumnInvertion[] = { 0xB4, 0x00 };
static unsigned char setDGC[] = { 0xCC, 0x0C };
static unsigned char setColor[] = { 0xEA, 0x72 };
static unsigned char setDisplay[] = { 0xB2, 0x03 };

static unsigned char setGIP[] = { 0xD5,
	0x00, 0x00, 0x0D, 0x00, 0x00, 0x00, 0x00, 0x12, 0x40, 0x00,
	0x00, 0x00, 0x01, 0x60, 0x37, 0x00, 0x00, 0x0F, 0x01, 0x02,
	0x47, 0x03, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x03, 0x00, 0x00, 0x18, 0x00, 0x00, 0x89, 0x00,
	0x11, 0x33, 0x55, 0x77, 0x31, 0x00, 0x00, 0x98, 0x00, 0x66,
	0x44, 0x22, 0x00, 0x02, 0x00, 0x00, 0x89, 0x00, 0x00, 0x22,
	0x44, 0x66, 0x20, 0x00, 0x00, 0x98, 0x00, 0x77, 0x55, 0x33,
	0x11, 0x13, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x03,
	0x00, 0xCF, 0xFF, 0xFF, 0x03, 0x00, 0xCF, 0xFF, 0xFF, 0x20,
	0x8C, 0x5A };

static unsigned char setGamma[] = { 0xE0,
	0x00, 0x00, 0x00, 0x0D, 0x0C, 0x3F, 0x18, 0x2C, 0x04, 0x0F,
	0x0E, 0x14, 0x17, 0x15, 0x16, 0x10, 0x13, 0x00, 0x00, 0x00,
	0x0C, 0x12, 0x3F, 0x17, 0x2C, 0x05, 0x08, 0x0E, 0x12, 0x16,
	0x14, 0x15, 0x11, 0x13, 0x01 };

static unsigned char set3Gamma[] = { 0xC1,
	0x01, 0x00, 0x08, 0x10, 0x18, 0x1F, 0x27, 0x2E, 0x34, 0x3E,
	0x48, 0x50, 0x58, 0x60, 0x68, 0x70, 0x78, 0x80, 0x88, 0x90,
	0x98, 0xA0, 0xA8, 0xB0, 0xB8, 0xC0, 0xC8, 0xD0, 0xD8, 0xE0,
	0xE8, 0xF0, 0xF7, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x08, 0x10, 0x18, 0x1F, 0x27, 0x2E,
	0x34, 0x3E, 0x48, 0x50, 0x58, 0x60, 0x68, 0x70, 0x78, 0x80,
	0x88, 0x90, 0x98, 0xA0, 0xA8, 0xB0, 0xB8, 0xC0, 0xC8, 0xD0,
	0xD8, 0xE0, 0xE8, 0xF0, 0xF7, 0xFF, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x10, 0x18, 0x1F,
	0x27, 0x2E, 0x34, 0x3E, 0x48, 0x50, 0x58, 0x60, 0x68, 0x70,
	0x78, 0x80, 0x88, 0x90, 0x98, 0xA0, 0xA8, 0xB0, 0xB8, 0xC0,
	0xC8, 0xD0, 0xD8, 0xE0, 0xE8, 0xF0, 0xF7, 0xFF, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

static unsigned char SleepOut[] = {	0x11 };
static unsigned char DisplayOn[] = { 0x29 };
static unsigned char DisplayOff[] = { 0x28 };
static unsigned char SleepIn[] = { 0x10 };

/*CABC PWM*/

#if 0
static unsigned char wrdisbv[] = {0x51, 0xff};
static unsigned char wrctrld[] = {0x53, 0x24};

#ifdef CONFIG_LDI_MDNIE_SUPPORT
static unsigned char wrcabc[] = {0x55, 0x00};
#else
static unsigned char wrcabc[] = {0x55, 0x01};
#endif
#endif

static const struct specific_cmdset initialize_cmdset[] = {
	/*Powersetting Start*/
	{ MIPI_DSI_DCS_LONG_WRITE,  setExtc,  sizeof(setExtc) },
	{ MIPI_DSI_DCS_LONG_WRITE,  setPower,   sizeof(setPower) },
	{ MIPI_DSI_DCS_LONG_WRITE,  setSourceEQ,   sizeof(setSourceEQ) },
	{ MIPI_DSI_DCS_LONG_WRITE,  setSourceEQ2,   sizeof(setSourceEQ2) },
	{ MIPI_DSI_DCS_LONG_WRITE,  setSourceOption,    sizeof(setSourceOption)   },
	/*Powersetting End*/
	{ MIPI_DSI_DELAY,           NULL,      10              },
	/* Initializing Start */
	{ MIPI_DSI_DCS_LONG_WRITE,  setMIPIChar, sizeof(setMIPIChar)},
	{ MIPI_DSI_DCS_LONG_WRITE,  interfacePixelFormat,    sizeof(interfacePixelFormat)   },
	{ MIPI_DSI_DCS_LONG_WRITE,  setRGBInterface,   sizeof(setRGBInterface)  },
	{ MIPI_DSI_DCS_LONG_WRITE, setColumnInvertion,  sizeof(setColumnInvertion)  },
	{ MIPI_DSI_DCS_LONG_WRITE,  setDGC, sizeof(setDGC)},
	{ MIPI_DSI_DCS_LONG_WRITE,  setColor,    sizeof(setColor) },
	{ MIPI_DSI_DCS_LONG_WRITE,  setDisplay, sizeof(setDisplay)},
	{ MIPI_DSI_DCS_LONG_WRITE,  setGIP,    sizeof(setGIP) },
	/* Initializing End */
	/* Gamma Setting Start*/
	{ MIPI_DSI_DCS_LONG_WRITE,  setGamma,    sizeof(setGamma)   },
	{ MIPI_DSI_DCS_LONG_WRITE,  set3Gamma, sizeof(set3Gamma)},
	/* Gamma Setting End*/

	{ MIPI_DSI_DCS_SHORT_WRITE, SleepOut, sizeof(SleepOut) },
	{ MIPI_DSI_DELAY,           NULL,      120              },
	{ MIPI_DSI_BLACK,           NULL,      0              },
#ifdef CONFIG_LDI_MDNIE_SUPPORT
	{ MIPI_DSI_PANEL_MDNIE,     NULL,      0                },
#endif
	{ MIPI_DSI_DCS_SHORT_WRITE, DisplayOn,    sizeof(DisplayOn)   },
	{ MIPI_DSI_DELAY,           NULL,      10              },
	{ MIPI_DSI_BLACK,           NULL,      0              },
	{ MIPI_DSI_END,             NULL,      0                }
};

static const struct specific_cmdset initialize_cmdset_1[] = {
	{ MIPI_DSI_BLACK, NULL, 0 },
	{ MIPI_DSI_END, NULL, 0 }
};


static unsigned char dpfrctr1[] = { 0xBD,
		0x01, 0x84, 0x07, 0x32, 0x00 };


#ifdef SWITCH_FRAMERATE_40HZ
static unsigned char dpfrctr1_40hz[] = { 0xBD,
		0x02, 0x45, 0x07, 0x32, 0x00 };
#else
static unsigned char dpfrctr1_30hz[] = { 0xBD,
		0x01, 0x84, 0x07, 0x32, 0x01 };
#endif

static struct specific_cmdset lcdfreq_cmd[] = {
	{ MIPI_DSI_DCS_LONG_WRITE,  dpfrctr1,  sizeof(dpfrctr1) },
	{ MIPI_DSI_END,             NULL,      0                }
};

static const struct specific_cmdset demise_cmdset[] = {
	{ MIPI_DSI_DCS_SHORT_WRITE, DisplayOff,   sizeof(DisplayOff)  },
	{ MIPI_DSI_DELAY,           NULL,      150              },
	{ MIPI_DSI_DCS_SHORT_WRITE, SleepIn,     sizeof(SleepIn)    },
	{ MIPI_DSI_DELAY,           NULL,      150              },
	{ MIPI_DSI_END,             NULL,      0                }
};


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

int exist_lcd = 1;
int alreadypower = 0;

static int is_dsi_read_enabled;
static int power_supplied;

static struct fb_info *common_fb_info;
static int panel_specific_cmdset(void *lcd_handle,
				   const struct specific_cmdset *cmdset);
enum lcdfreq_level_idx {
	LEVEL_NORMAL,		/* 60Hz */
	LEVEL_LOW,		/* Power saving mode */
	LCDFREQ_LEVEL_END
};

struct lcd_info {
	enum lcdfreq_level_idx	level;	/* Current level */
	struct mutex		lock;	/* Lock for change frequency */
	struct device		*dev;	/* Hold device of LCD */
	struct device_attribute	*attr;	/* Hold attribute info */
	struct lcd_device	*ld;	/* LCD device info */

	unsigned int			ldi_enable;
	unsigned int			power;
};

static struct lcd_info lcd_info_data;

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

static int lcdfreq_lock_free(struct device *dev)
{
	void *screen_handle;
	screen_disp_delete disp_delete;
	int ret;

	printk(KERN_DEBUG "%s\n", __func__);

	screen_handle =  screen_display_new();

	/* Transmit DSI command peculiar to a panel */
	ret = panel_specific_cmdset(screen_handle, lcdfreq_cmd);
	if (ret != 0) {
		printk(KERN_ALERT "panel_specific_cmdset err!\n");
		goto out;
	}

out:
	disp_delete.handle = screen_handle;
	screen_display_delete(&disp_delete);

	return ret;
}

#if defined(CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG) || defined(CONFIG_FB_LCD_ESD)
static int HX8369b_panel_simple_reset(void)
{
	void *screen_handle;
	screen_disp_stop_lcd disp_stop_lcd;
	screen_disp_start_lcd start_lcd;
	screen_disp_delete disp_delete;
	screen_disp_set_rt_standby set_rt_standby;
	int ret;

#ifdef HX8369b_POWAREA_MNG_ENABLE
	void *system_handle;
	system_pmg_param powarea_end_notify;
	system_pmg_param powarea_start_notify;
	system_pmg_delete pmg_delete;

	system_handle = system_pwmng_new();
#endif

	printk(KERN_ALERT "%s\n", __func__);

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
		printk(KERN_ALERT "display_stop_lcd err!\n");
	/* End suspend sequence */

#ifdef HX8369b_POWAREA_MNG_ENABLE
	/* Notifying the Beginning of Using Power Area */
	printk(KERN_INFO "End A4LC power area\n");
	powarea_end_notify.handle		= system_handle;
	powarea_end_notify.powerarea_name	= RT_PWMNG_POWERAREA_A4LC;
	ret = system_pwmng_powerarea_end_notify(&powarea_end_notify);
	if (ret != SMAP_LIB_PWMNG_OK) {
		printk(KERN_ALERT "system_pwmng_powerarea_end_notify err!\n");
		goto out;
	}

	msleep(20);

	/* Notifying the Beginning of Using Power Area */
	printk(KERN_INFO "Start A4LC power area\n");
	powarea_start_notify.handle		= system_handle;
	powarea_start_notify.powerarea_name	= RT_PWMNG_POWERAREA_A4LC;
	ret = system_pwmng_powerarea_start_notify(&powarea_start_notify);
	if (ret != SMAP_LIB_PWMNG_OK) {
		printk(KERN_ALERT "system_pwmng_powerarea_start_notify err!\n");
		goto out;
	}
#endif /* HX8369b_POWAREA_MNG_ENABLE */

	msleep(20);

	/* LCD panel reset */
	mipi_display_reset();

	/* Start resume sequence */
	/* Start a display to LCD */
	start_lcd.handle	= screen_handle;
	start_lcd.output_mode	= RT_DISPLAY_LCD1;
	ret = screen_display_start_lcd(&start_lcd);
	if (ret != SMAP_LIB_DISPLAY_OK) {
		printk(KERN_ALERT "disp_start_lcd err!\n");
		goto out;
	}

	/* RT Standby Mode (DISABLE_RT_STANDBY, ENABLE_RT_STANDBY) */
	set_rt_standby.handle	= screen_handle;
	set_rt_standby.rt_standby	= DISABLE_RT_STANDBY;
	screen_display_set_rt_standby(&set_rt_standby);

	gpio_reset();
	/* Transmit DSI command peculiar to a panel */
	ret = panel_specific_cmdset(screen_handle, initialize_cmdset);
	/* RT Standby Mode (DISABLE_RT_STANDBY, ENABLE_RT_STANDBY) */
	set_rt_standby.handle	= screen_handle;
	set_rt_standby.rt_standby	= ENABLE_RT_STANDBY;
	screen_display_set_rt_standby(&set_rt_standby);
	/* if error in initialize_cmdset */
	if (ret != 0) {
		printk(KERN_ALERT "panel_specific_cmdset err!\n");
		goto out;
	}
	/* End resume sequence */

	/* Resume frame rate */
	lcdfreq_resume();

	is_dsi_read_enabled = 1;

	/* Return from a black screen */
	ret = HX8369b_panel_draw(screen_handle);
	if (ret != 0) {
		printk(KERN_ALERT "HX8369b_panel_draw error!!\n");
		goto out;
	}

	printk(KERN_ALERT "HX8369b_panel simple initialized\n");

out:
	disp_delete.handle = screen_handle;
	screen_display_delete(&disp_delete);

#ifdef HX8369b_POWAREA_MNG_ENABLE
	pmg_delete.handle = system_handle;
	system_pwmng_delete(&pmg_delete);
#endif /* HX8369b_POWAREA_MNG_ENABLE */

	return ret;
}
#endif /* CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG or CONFIG_FB_LCD_ESD */

#if defined(CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG)
static int HX8369b_panel_check(void)
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
	printk(KERN_DEBUG "read_data(0x05) = %02X : ret(%d)\n", rdnumed, ret);
	if (rdnumed != 0x00)
		ret = -1;
	if (ret != 0)
		goto out;

	/*Read Display Power Mode*/
	ret = panel_dsi_read(MIPI_DSI_DCS_READ, 0x0A, 1, &rddpm);
	printk(KERN_DEBUG "read_data(0x0A) = %02X : ret(%d)\n", rddpm, ret);
	if (rddpm != 0x9C)
		ret = -1;
	if (ret != 0)
		goto out;

	/*Read ID for IC Vender Code*/
	ret = panel_specific_cmdset(screen_handle, mauc1_cmd);
	if (ret != 0)
		goto out;
	ret = panel_dsi_read(MIPI_DSI_DCS_READ, 0xC5, 1, rdidic);
	printk(KERN_DEBUG "read_data(0xC5) = %02X : ret(%d) page 1\n",
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
	printk(KERN_DEBUG "read_data(0xC5) = %02X : ret(%d) page 0\n",
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

static void HX8369b_panel_esd_check_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	int retry = HX8369b_INIT_RETRY_COUNT;

	pr_info("ESD : HX8369b_panel_esd_check_work %d\n",
			esd_check_flag);
	/* For the disable entering suspend */
	mutex_lock(&esd_check_mutex);

	while ((esd_check_flag == ESD_CHECK_ENABLE))
		while ((esd_check_flag == ESD_CHECK_ENABLE) &&
					(HX8369b_panel_simple_reset())) {
			if (retry <= 0) {
				esd_check_flag = ESD_CHECK_DISABLE;
				printk(KERN_ALERT "retry count 0!!!!\n");
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

static ssize_t level_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	pr_debug("%s\n", __func__);
	printk("level_show  DSI0PCKCR : 0x%0x DSI0PHYCR:0x%0x\n",readl(DSI0PCKCR),readl(DSI0PHYCR));
	return sprintf(buf, "%d\n", lcd_info_data.level);
}

static ssize_t level_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int value;
	int ret;

	printk(KERN_DEBUG "%s\n", __func__);

	mutex_lock(&lcd_info_data.lock);

	ret = kstrtoul(buf, 0, (unsigned long *)&value);

	printk(KERN_DEBUG "\t%s :: value=%d\n", __func__, value);

	if (value >= LCDFREQ_LEVEL_END) {
		count = -EINVAL;
		goto out;
	}
	if (1) {
		printk(KERN_DEBUG"DSI0PCKCR : 0x%0x\n",readl(DSI0PCKCR));
		if(value == 0) // set A
		{
			if((readl(DSI0PCKCR) & 0x18000000) != 0)
				writel((readl(DSI0PCKCR) & ~0x18000000) | 0x20000000, DSI0PCKCR);
			else
				printk(KERN_DEBUG"All ready Set A \n");
		}
		else
		{ //set B
			if((readl(DSI0PCKCR) & 0x18000000) == 0)
				writel((readl(DSI0PCKCR) & ~0x18000000) | 0x20000000, DSI0PCKCR);
			else
				printk(KERN_DEBUG"All ready Set B \n");
		}
	
		printk(KERN_DEBUG"UPDATE DSI0PCKCR : 0x%0x DSI0PHYCR:0x%0x\n",readl(DSI0PCKCR),readl(DSI0PHYCR));

		lcd_info_data.level	= value;
		goto out;
	}
	if (value) {
#ifdef HX8369b_SWITCH_FRAMERATE_40HZ
		/* set freq 40Hz */
		printk(KERN_ALERT "set low freq(40Hz)\n");

		r_mobile_lcd_if_param.dsi0pckcr = LCD_DSI0PCKCR_40HZ;
		r_mobile_lcd_if_param.dsi0phycr = LCD_DSI0PHYCR_40HZ;

		lcdfreq_cmd[0].data = dpfrctr1_40hz;
#else
		/* set freq 30Hz */
		printk(KERN_ALERT "set low freq(30Hz)\n");

		r_mobile_lcd_if_param.dsi0pckcr = LCD_DSI0PCKCR_30HZ;
		r_mobile_lcd_if_param.dsi0phycr = LCD_DSI0PHYCR_30HZ;

		lcdfreq_cmd[0].data = dpfrctr1_30hz;
#endif
	} else {
		/* set freq 60Hz */
		printk(KERN_ALERT "set normal freq(60Hz)\n");

		r_mobile_lcd_if_param.dsi0pckcr = LCD_DSI0PCKCR;
		r_mobile_lcd_if_param.dsi0phycr = LCD_DSI0PHYCR;

		lcdfreq_cmd[0].data = dpfrctr1;
	}

	ret = lcdfreq_lock_free(dev);
	if (ret) {
		printk(KERN_ALERT "%s fail\n", __func__);
		count = -EINVAL;
		goto out;
	}

	lcd_info_data.level	= value;
	lcd_info_data.dev	= dev;
	lcd_info_data.attr	= attr;
out:
	mutex_unlock(&lcd_info_data.lock);

	return count;
}

static int lcdfreq_resume(void)
{
	char buf[10];

	/* Resume when a frame rate is not 60Hz */
	if (lcd_info_data.level != 0) {
		sprintf(buf, "%d", lcd_info_data.level);
		level_store(lcd_info_data.dev,
			    lcd_info_data.attr, buf, 0);
	}

	return 0;
}

static ssize_t lcd_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char temp[] = "BOE_55BCF0\n";
	strcat(buf, temp);
	return strlen(buf);
}

static DEVICE_ATTR(level, S_IRUGO|S_IWUSR, level_show, level_store);
static DEVICE_ATTR(lcd_type, 0444, lcd_type_show, NULL);

static struct attribute *lcdfreq_attributes[] = {
	&dev_attr_level.attr,
	NULL,
};

static struct attribute_group lcdfreq_attr_group = {
	.name = "lcdfreq",
	.attrs = lcdfreq_attributes,
};

static int HX8369b_lcd_frequency_register(struct device *dev)
{
	struct lcd_info *lcdfreq = NULL;
	int ret;

	printk(KERN_DEBUG "%s\n", __func__);

	lcdfreq = &lcd_info_data;
	lcdfreq->dev = dev;
	lcdfreq->level = LEVEL_NORMAL;

	mutex_init(&lcdfreq->lock);

	ret = sysfs_create_group(&lcdfreq->dev->kobj, &lcdfreq_attr_group);
	if (ret < 0) {
		printk(KERN_ALERT "fail to add sysfs entries, %d\n", __LINE__);

		return ret;
	}

	printk(KERN_DEBUG "%s is done\n", __func__);

	return 0;
}

static void HX8369b_lcd_frequency_unregister(void)
{
	printk(KERN_DEBUG "%s\n", __func__);

	sysfs_remove_group(&lcd_info_data.dev->kobj,
					&lcdfreq_attr_group);
	mutex_destroy(&lcd_info_data.lock);

	printk(KERN_DEBUG "%s is done\n", __func__);

}


#ifdef HX8369b_GED_ORG
static int lcd_get_power(struct lcd_device *ld)
{
#if 0
	struct lcd_info *lcd = lcd_get_data(ld);

	return lcd->power;
#endif
	return true;
}


static struct lcd_ops HX8369b_lcd_ops = {
	.set_power = lcd_set_power,
	.get_power = lcd_get_power,
};

#else /* HX8369b_GED_ORG */
static struct lcd_ops HX8369b_lcd_ops = {
	.set_power = NULL,
	.get_power = NULL,
};

#endif /* HX8369b_GED_ORG */

#if defined(CONFIG_FB_LCD_ESD)
static void lcd_esd_detect(struct work_struct *work)
{
	int retry = HX8369b_INIT_RETRY_COUNT;

	/* For the disable entering suspend */
	mutex_lock(&esd_check_mutex);

	pr_info("[LCD] %s\n", __func__);

	/* esd recovery */
	while ((HX8369b_panel_simple_reset()) &&
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
				printk(KERN_ALERT "display_stop_lcd err!\n");

			disp_delete.handle = screen_handle;
			screen_display_delete(&disp_delete);

			esd_check_flag = ESD_CHECK_DISABLE;
			printk(KERN_ALERT "retry count 0!!!!\n");
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
		printk(KERN_DEBUG "[LCD] %s\n", __func__);

		disable_irq_nosync(esd_detect_irq);
		queue_work(lcd_wq, &esd_detect_work);
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}
#endif /* CONFIG_FB_LCD_ESD */

int panel_dsi_read(int id, int reg, int len, char *buf)
{
	void *screen_handle;
	screen_disp_write_dsi_short write_dsi_s;
	screen_disp_read_dsi_short read_dsi_s;
	screen_disp_delete disp_delete;
	int ret = 0;

	printk(KERN_DEBUG "%s\n", __func__);

	if (!is_dsi_read_enabled) {
		printk(KERN_ALERT "sequence error!!\n");
		return -EINVAL;
	}

	if ((len <= 0) || (len > 60) || (buf == NULL)) {
		printk(KERN_ALERT "argument error!!\n");
		return -EINVAL;
	}

	screen_handle =  screen_display_new();

	/* Set maximum return packet size  */
	write_dsi_s.handle		= screen_handle;
	write_dsi_s.output_mode	= RT_DISPLAY_LCD1;
	write_dsi_s.data_id		= MIPI_DSI_SET_MAX_RETURN_PACKET;
	write_dsi_s.reg_address	= len;
	write_dsi_s.write_data		= 0x00;
	write_dsi_s.reception_mode	= RT_DISPLAY_RECEPTION_ON;
	ret = screen_display_write_dsi_short_packet(&write_dsi_s);
	if (ret != SMAP_LIB_DISPLAY_OK) {
		printk(KERN_ALERT "disp_write_dsi_short err!\n");
		goto out;
	}

	/* DSI read */
	read_dsi_s.handle		= screen_handle;
	read_dsi_s.output_mode		= RT_DISPLAY_LCD1;
	read_dsi_s.data_id		= id;
	read_dsi_s.reg_address		= reg;
	read_dsi_s.write_data		= 0;
	read_dsi_s.data_count		= len;
	read_dsi_s.read_data		= &buf[0];
	ret = screen_display_read_dsi_short_packet(&read_dsi_s);
	if (ret != SMAP_LIB_DISPLAY_OK)
		printk(KERN_ALERT "disp_dsi_read err! ret = %d\n", ret);

out:
	disp_delete.handle = screen_handle;
	screen_display_delete(&disp_delete);

	return ret;
}

static int HX8369b_panel_draw(void *screen_handle)
{
	screen_disp_draw disp_draw;
	int ret;

	printk(KERN_DEBUG "%s\n", __func__);

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
		printk(KERN_ALERT "screen_display_draw err!\n");
		return -1;
	}

	return 0;
}

static int HX8369b_panel_draw_black(void *screen_handle)
{
	u32 panel_width  = R_MOBILE_M_PANEL_PIXEL_WIDTH;
	u32 panel_height = R_MOBILE_M_PANEL_PIXEL_HEIGHT;
	screen_disp_draw disp_draw;
	int ret;

	printk(KERN_DEBUG "%s\n", __func__);

#ifdef HX8369b_DRAW_BLACK_KERNEL
	printk(KERN_DEBUG
		"num_registered_fb = %d\n", num_registered_fb);

	if (!num_registered_fb) {
		printk(KERN_ALERT
			"num_registered_fb err!\n");
		return -1;
	}
	if (!registered_fb[0]->fix.smem_start) {
		printk(KERN_ALERT
			"registered_fb[0]->fix.smem_start"
			" is NULL err!\n");
		return -1;
	}
	printk(KERN_DEBUG
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
#ifdef HX8369b_DRAW_BLACK_KERNEL
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
		printk(KERN_ALERT "screen_display_draw err!\n");
		return -1;
	}

	return 0;
}

static int panel_specific_cmdset(void *lcd_handle,
				   const struct specific_cmdset *cmdset)
{
	int ret;
	int loop = 0;
	screen_disp_write_dsi_short write_dsi_s;
	screen_disp_write_dsi_long  write_dsi_l;

	printk(KERN_DEBUG "%s\n", __func__);

	while (0 <= loop) {
		switch (cmdset[loop].cmd) {
		case MIPI_DSI_DCS_LONG_WRITE:
		case MIPI_DSI_GEN_LONG_WRITE:
			printk(KERN_DEBUG "panel_cmdset LONG Write\n");
			write_dsi_l.handle         = lcd_handle;
			write_dsi_l.output_mode    = RT_DISPLAY_LCD1;
			write_dsi_l.data_id        = cmdset[loop].cmd;
			write_dsi_l.data_count     = cmdset[loop].size;
			write_dsi_l.write_data     = cmdset[loop].data;
			write_dsi_l.reception_mode = RT_DISPLAY_RECEPTION_ON;
			write_dsi_l.send_mode      = RT_DISPLAY_SEND_MODE_HS;
			ret = screen_display_write_dsi_long_packet(
					&write_dsi_l);
			if (ret != SMAP_LIB_DISPLAY_OK) {
				printk(KERN_ALERT
				       "display_write_dsi_long err %d!\n", ret);
				return -1;
			}
			break;
		case MIPI_DSI_DCS_SHORT_WRITE_PARAM:
		case MIPI_DSI_GEN_SHORT_WRITE_PARAM:
			printk(KERN_DEBUG
			       "panel_cmdset SHORT Write with param\n");
			write_dsi_s.handle         = lcd_handle;
			write_dsi_s.output_mode    = RT_DISPLAY_LCD1;
			write_dsi_s.data_id        = cmdset[loop].cmd;
			write_dsi_s.reg_address    = cmdset[loop].data[0];
			write_dsi_s.write_data     = cmdset[loop].data[1];
			write_dsi_s.reception_mode = RT_DISPLAY_RECEPTION_ON;
			ret = screen_display_write_dsi_short_packet(
					&write_dsi_s);
			if (ret != SMAP_LIB_DISPLAY_OK) {
				printk(KERN_ALERT
				       "disp_write_dsi_short err %d!\n", ret);
				return -1;
			}
			break;
		case MIPI_DSI_DCS_SHORT_WRITE:
		case MIPI_DSI_GEN_SHORT_WRITE:
			printk(KERN_DEBUG "panel_cmdset SHORT Write\n");
			write_dsi_s.handle         = lcd_handle;
			write_dsi_s.output_mode    = RT_DISPLAY_LCD1;
			write_dsi_s.data_id        = cmdset[loop].cmd;
			write_dsi_s.reg_address    = cmdset[loop].data[0];
			write_dsi_s.write_data     = 0x00;
			write_dsi_s.reception_mode = RT_DISPLAY_RECEPTION_ON;
			ret = screen_display_write_dsi_short_packet(
					&write_dsi_s);
			if (ret != SMAP_LIB_DISPLAY_OK) {
				printk(KERN_ALERT
				       "disp_write_dsi_short err %d!\n", ret);
				return -1;
			}
			break;
		case MIPI_DSI_BLACK:
		{
			if (!initialize_now)
				HX8369b_panel_draw(lcd_handle);
			ret = HX8369b_panel_draw_black(lcd_handle);
			if (ret != 0)
				return -1;

			break;
		}
		case MIPI_DSI_DELAY:
			msleep(cmdset[loop].size);
			break;

		case MIPI_DSI_END:
			loop = -2;
			break;
#ifdef CONFIG_LDI_MDNIE_SUPPORT
		case MIPI_DSI_PANEL_MDNIE:
		{
			set_mdnie_enable();
			break;
		}
#endif
		default:
			printk(KERN_ALERT "Undefine command err!\n");
			return -1;
		}
		loop++;
	}

	return 0;
}

static void mipi_display_reset(void)
{
	int ret;
	printk(KERN_INFO "%s\n", __func__);

	/* Already power supply */
	if (power_supplied) {
		printk(KERN_ALERT "Already power supply!\n");
                alreadypower = 1;
		goto out;
	}

	ret = regulator_enable(power_ldo_1v8);
	if (ret)
		pr_info("enabling vlcd_1v8 failed, err %d", ret);
	ret = regulator_enable(power_ldo_3v);
	if (ret)
		pr_info("enabling vlcd_3v failed, err %d", ret);

out:
	power_supplied = true;
}

static void gpio_reset(void)
{
	usleep_range(1000, 1050);
	gpio_direction_output(reset_gpio, 1);
	usleep_range(1000, 1050);
	gpio_direction_output(reset_gpio, 0);
	usleep_range(500, 550);
	gpio_direction_output(reset_gpio, 1);
	msleep(200);
}

static void mipi_display_power_off(void)
{
	printk(KERN_INFO "%s\n", __func__);

	/* Already not power supply */
	if (!power_supplied) {
		printk(KERN_ALERT "Already not power supply!\n");
		goto out;
	}

	/* GPIO control */
	gpio_direction_output(reset_gpio, 0);
	msleep(20);
	regulator_disable(power_ldo_3v);
	regulator_disable(power_ldo_1v8);
	msleep(25);

       alreadypower = 0;

out:
	power_supplied = false;
}


static int HX8369b_panel_init(unsigned int mem_size)
{
	void *screen_handle;
	screen_disp_start_lcd start_lcd;
	screen_disp_stop_lcd disp_stop_lcd;
	screen_disp_set_lcd_if_param set_lcd_if_param;
	screen_disp_set_address set_address;
	screen_disp_delete disp_delete;
	screen_disp_set_rt_standby set_rt_standby;
	int ret = 0;
	int retry_count = HX8369b_INIT_RETRY_COUNT;
	unsigned char read_data[60];

#ifdef HX8369b_POWAREA_MNG_ENABLE
	void *system_handle;
	system_pmg_param powarea_start_notify;
	system_pmg_delete pmg_delete;
#endif

	printk(KERN_INFO "%s\n", __func__);

	initialize_now = true;

#ifdef HX8369b_POWAREA_MNG_ENABLE
	printk(KERN_INFO "Start A4LC power area\n");
	system_handle = system_pwmng_new();

	/* Notifying the Beginning of Using Power Area */
	powarea_start_notify.handle		= system_handle;
	powarea_start_notify.powerarea_name	= RT_PWMNG_POWERAREA_A4LC;
	ret = system_pwmng_powerarea_start_notify(&powarea_start_notify);
	if (ret != SMAP_LIB_PWMNG_OK)
		printk(KERN_ALERT "system_pwmng_powerarea_start_notify err!\n");
	pmg_delete.handle = system_handle;
	system_pwmng_delete(&pmg_delete);
#endif

	screen_handle =  screen_display_new();



	/* Setting peculiar to panel */
	set_lcd_if_param.handle			= screen_handle;
	set_lcd_if_param.port_no		= irq_portno;
	set_lcd_if_param.lcd_if_param		= &r_mobile_lcd_if_param;
	set_lcd_if_param.lcd_if_param_mask	= &r_mobile_lcd_if_param_mask;
	ret = screen_display_set_lcd_if_parameters(&set_lcd_if_param);
	if (ret != SMAP_LIB_DISPLAY_OK) {
		printk(KERN_ALERT "disp_set_lcd_if_parameters err!\n");
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
		printk(KERN_ALERT "disp_set_address err!\n");
		goto out;
	}

	/* Start a display to LCD */
	start_lcd.handle	= screen_handle;
	start_lcd.output_mode	= RT_DISPLAY_LCD1;
	ret = screen_display_start_lcd(&start_lcd);
	if (ret != SMAP_LIB_DISPLAY_OK) {
		printk(KERN_ALERT "disp_start_lcd err!\n");
		goto out;
	}

retry:
	is_dsi_read_enabled = 1;

	/* read LCD panel ID */
    	panel_dsi_read(MIPI_DSI_DCS_READ, 0x04, 4, &read_data[0]);
	pr_info("read_data(RDID0) = %02X\n", read_data[0]);
	pr_info("read_data(RDID1) = %02X\n", read_data[1]);
	pr_info("read_data(RDID2) = %02X\n", read_data[2]);

	if(read_data[1]==188 || read_data[1]==190 || read_data[1]==191) {
		exist_lcd = 1;
	} else {
		exist_lcd = 0;
		gpio_set_value(47, 0);
		pr_info("[LCD] LCD panel does not exist !! \n");
	}
	/* read LCD panel ID */

	/* Transmit DSI command peculiar to a panel */
	if (retry_count == HX8369b_INIT_RETRY_COUNT)
		ret = panel_specific_cmdset(screen_handle, initialize_cmdset_1);
	else {
		ret = panel_specific_cmdset(screen_handle, initialize_cmdset);
		/* enable rt_standby which was disabled before jump to retry */
		set_rt_standby.handle	= screen_handle;
		set_rt_standby.rt_standby	= ENABLE_RT_STANDBY;
		screen_display_set_rt_standby(&set_rt_standby);
	}
	/* if error in inititalize_cmdset */
	if (ret != 0) {
		printk(KERN_ALERT "panel_specific_cmdset err!\n");
		is_dsi_read_enabled = 0;

		if (retry_count == 0) {
			printk(KERN_ALERT "retry count 0!!!!\n");

			mipi_display_power_off();

			disp_stop_lcd.handle		= screen_handle;
			disp_stop_lcd.output_mode	= RT_DISPLAY_LCD1;
			ret = screen_display_stop_lcd(&disp_stop_lcd);
			if (ret != SMAP_LIB_DISPLAY_OK)
				printk(KERN_ALERT "display_stop_lcd err!\n");

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
				printk(KERN_ALERT "display_stop_lcd err!\n");
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
				printk(KERN_ALERT "disp_start_lcd err!\n");
				goto out;
			}

			set_rt_standby.handle	= screen_handle;
			set_rt_standby.rt_standby	= DISABLE_RT_STANDBY;
			screen_display_set_rt_standby(&set_rt_standby);

			gpio_reset();
			goto retry;
		}
	}

	pr_info(KERN_DEBUG "Panel initialized with Video mode\n");

	printk(KERN_DEBUG"HX8369b_panel_init DSI0PCKCR : 0x%0x DSI0PHYCR:0x%0x\n",readl(DSI0PCKCR),readl(DSI0PHYCR));
	
#if defined(CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG)
	esd_duration = DURATION_TIME;
	schedule_delayed_work(&esd_check_work, msecs_to_jiffies(esd_duration));
#endif /* CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG */

#if defined(CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG) || defined(CONFIG_FB_LCD_ESD)
	esd_check_flag = ESD_CHECK_ENABLE;
#endif /* CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG or CONFIG_FB_LCD_ESD */

#if defined(CONFIG_FB_LCD_ESD)
	ret = request_irq(esd_detect_irq, lcd_esd_irq_handler,
			IRQF_TRIGGER_HIGH | IRQF_ONESHOT, esd_devname, &esd_irq_requested);
	if (ret != 0)
		printk(KERN_ALERT "request_irq err! =%d\n", ret);
	else
		esd_irq_requested = true;
#endif /* CONFIG_FB_LCD_ESD */

out:
	disp_delete.handle = screen_handle;
	screen_display_delete(&disp_delete);

	initialize_now = false;

	return ret;
}

static int HX8369b_panel_suspend(void)
{
	void *screen_handle;
	screen_disp_stop_lcd disp_stop_lcd;
	screen_disp_start_lcd start_lcd;
	screen_disp_delete disp_delete;
	int ret;

#ifdef HX8369b_POWAREA_MNG_ENABLE
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

	printk(KERN_INFO "%s\n", __func__);

	screen_handle =  screen_display_new();

	is_dsi_read_enabled = 0;

	/* Stop a display to LCD */
	disp_stop_lcd.handle		= screen_handle;
	disp_stop_lcd.output_mode	= RT_DISPLAY_LCD1;
	ret = screen_display_stop_lcd(&disp_stop_lcd);
	if (ret != SMAP_LIB_DISPLAY_OK)
		printk(KERN_ALERT "display_stop_lcd err!\n");

	start_lcd.handle	= screen_handle;
	start_lcd.output_mode	= RT_DISPLAY_LCD1;
	ret = screen_display_start_lcd(&start_lcd);
	if (ret != SMAP_LIB_DISPLAY_OK)
		printk(KERN_ALERT "disp_start_lcd err!\n");

	/* Transmit DSI command peculiar to a panel */
	ret = panel_specific_cmdset(screen_handle, demise_cmdset);
	if (ret != 0) {
		printk(KERN_ALERT "panel_specific_cmdset err!\n");
		/* continue */
	}

	/* Stop a display to LCD */
	disp_stop_lcd.handle		= screen_handle;
	disp_stop_lcd.output_mode	= RT_DISPLAY_LCD1;
	ret = screen_display_stop_lcd(&disp_stop_lcd);
	if (ret != SMAP_LIB_DISPLAY_OK)
		printk(KERN_ALERT "display_stop_lcd err!\n");

	printk(KERN_DEBUG"SUSPEND DSI0PCKCR : 0x%0x DSI0PHYCR:0x%0x\n",readl(DSI0PCKCR),readl(DSI0PHYCR));

	mipi_display_power_off();

	disp_delete.handle = screen_handle;
	screen_display_delete(&disp_delete);

#ifdef HX8369b_POWAREA_MNG_ENABLE
	printk(KERN_INFO "End A4LC power area\n");
	system_handle = system_pwmng_new();

	/* Notifying the Beginning of Using Power Area */
	powarea_end_notify.handle		= system_handle;
	powarea_end_notify.powerarea_name	= RT_PWMNG_POWERAREA_A4LC;
	ret = system_pwmng_powerarea_end_notify(&powarea_end_notify);
	if (ret != SMAP_LIB_PWMNG_OK)
		printk(KERN_ALERT "system_pwmng_powerarea_end_notify err!\n");

	pmg_delete.handle = system_handle;
	system_pwmng_delete(&pmg_delete);
#endif

#if defined(CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG) || defined(CONFIG_FB_LCD_ESD)
	mutex_unlock(&esd_check_mutex);
#endif /* CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG or CONFIG_FB_LCD_ESD */


	return 0;
}

static int HX8369b_panel_resume(void)
{
	void *screen_handle;
	screen_disp_start_lcd start_lcd;
	screen_disp_stop_lcd disp_stop_lcd;
	screen_disp_delete disp_delete;
	screen_disp_set_rt_standby set_rt_standby;
	unsigned char read_data[60];
	int retry_count_dsi;
	int ret = 0;
	int retry_count = HX8369b_INIT_RETRY_COUNT;

#ifdef HX8369b_POWAREA_MNG_ENABLE
	void *system_handle;
	system_pmg_param powarea_start_notify;
	system_pmg_delete pmg_delete;
#endif

#if defined(CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG) || defined(CONFIG_FB_LCD_ESD)
	/* Wait for end of check ESD */
	mutex_lock(&esd_check_mutex);
#endif /* CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG or CONFIG_FB_LCD_ESD */

	printk(KERN_INFO "%s\n", __func__);

#ifdef HX8369b_POWAREA_MNG_ENABLE
	printk(KERN_INFO "Start A4LC power area\n");
	system_handle = system_pwmng_new();

	/* Notifying the Beginning of Using Power Area */
	powarea_start_notify.handle		= system_handle;
	powarea_start_notify.powerarea_name	= RT_PWMNG_POWERAREA_A4LC;
	ret = system_pwmng_powerarea_start_notify(&powarea_start_notify);
	if (ret != SMAP_LIB_PWMNG_OK)
		printk(KERN_ALERT "system_pwmng_powerarea_start_notify err!\n");
	pmg_delete.handle = system_handle;
	system_pwmng_delete(&pmg_delete);
#endif

retry:

	screen_handle =  screen_display_new();

	/* LCD panel reset */
	mipi_display_reset();

	/* Start a display to LCD */
	start_lcd.handle	= screen_handle;
	start_lcd.output_mode	= RT_DISPLAY_LCD1;
	ret = screen_display_start_lcd(&start_lcd);
	if (ret != SMAP_LIB_DISPLAY_OK) {
		printk(KERN_ALERT "disp_start_lcd err!\n");
		goto out;
	}

	/* RT Standby Mode (DISABLE_RT_STANDBY, ENABLE_RT_STANDBY) */
	set_rt_standby.handle	= screen_handle;
	set_rt_standby.rt_standby	= DISABLE_RT_STANDBY;
	screen_display_set_rt_standby(&set_rt_standby);

	gpio_reset();

	is_dsi_read_enabled = 1;

	retry_count_dsi = HX8369b_INIT_RETRY_COUNT;
	panel_dsi_read(MIPI_DSI_DCS_READ, 0x04, 4, &read_data[0]);
	pr_info("read_data(RDID0) = %02X\n", read_data[0]);
	pr_info("read_data(RDID1) = %02X\n", read_data[1]);
	pr_info("read_data(RDID2) = %02X\n", read_data[2]);

        if(0 == alreadypower){
	     if(read_data[1]==188 || read_data[1]==190 || read_data[1]==191) {
		exist_lcd = 1;
	} else {
		exist_lcd = 0;
		gpio_set_value(47, 0);
		pr_info("[LCD] LCD panel does not exist !! \n");
	}
        }

	/* Transmit DSI command peculiar to a panel */
	ret = panel_specific_cmdset(screen_handle, initialize_cmdset);
	/* RT Standby Mode (DISABLE_RT_STANDBY, ENABLE_RT_STANDBY) */
	set_rt_standby.handle	= screen_handle;
	set_rt_standby.rt_standby	= ENABLE_RT_STANDBY;
	screen_display_set_rt_standby(&set_rt_standby);
	/* if error in initialize_cmdset */
	if (ret != 0) {
		printk(KERN_ALERT "panel_specific_cmdset err!\n");
		is_dsi_read_enabled = 0;
		if (retry_count == 0) {
			printk(KERN_ALERT "retry count 0!!!!\n");

			mipi_display_power_off();

			disp_stop_lcd.handle		= screen_handle;
			disp_stop_lcd.output_mode	= RT_DISPLAY_LCD1;
			ret = screen_display_stop_lcd(&disp_stop_lcd);
			if (ret != SMAP_LIB_DISPLAY_OK)
				printk(KERN_ALERT "display_stop_lcd err!\n");

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
				printk(KERN_ALERT "display_stop_lcd err!\n");

			disp_delete.handle = screen_handle;
			screen_display_delete(&disp_delete);
			goto retry;
		}
	}

	/* Resume frame rate */
	lcdfreq_resume();

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
			IRQF_TRIGGER_HIGH | IRQF_ONESHOT, esd_devname, &esd_irq_requested);
	if (ret != 0)
		printk(KERN_ALERT "request_irq err! =%d\n", ret);
	else
		esd_irq_requested = true;

#endif /* CONFIG_FB_LCD_ESD */

out:
	disp_delete.handle = screen_handle;
	screen_display_delete(&disp_delete);

#if defined(CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG) || defined(CONFIG_FB_LCD_ESD)
	mutex_unlock(&esd_check_mutex);
#endif /* CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG or CONFIG_FB_LCD_ESD */
	printk(KERN_DEBUG"RESUME Done  DSI0PCKCR : 0x%0x DSI0PHYCR:0x%0x\n",readl(DSI0PCKCR),readl(DSI0PHYCR));


	return ret;
}

static int HX8369b_panel_probe(struct fb_info *info,
			    struct fb_panel_hw_info hw_info)
{
	struct platform_device *pdev;
	struct resource *res_irq_port;
	int ret;

	printk(KERN_INFO "%s\n", __func__);

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
		printk(KERN_ALERT "panel_irq_port is NULL!!\n");
		return -ENODEV;
	}
	irq_portno = res_irq_port->start;
	power_ldo_3v = regulator_get(NULL, "vlcd_3v");
	power_ldo_1v8 = regulator_get(NULL, "vlcd_1v8");

	if (power_ldo_3v == NULL || power_ldo_1v8 == NULL) {
		printk(KERN_ERR "regulator_get failed\n");
		return -ENODEV;
	}
	ret = regulator_enable(power_ldo_1v8);
	if (ret) {
		pr_err("regulator_enable failed\n");
		return -ENODEV;
	}
	ret = regulator_enable(power_ldo_3v);
	if (ret) {
		pr_err("regulator_enable failed\n");
		return -ENODEV;
	}
	power_supplied = true;

	printk(KERN_INFO "PMIC        : for panel power\n");
	printk(KERN_INFO "GPIO_PORT%d : for panel reset\n", reset_gpio);
	printk(KERN_INFO "IRQ%d       : for panel te\n", irq_portno);

	common_fb_info = info;
	is_dsi_read_enabled = 0;

	/* clear internal info */
	memset(&lcd_info_data, 0, sizeof(lcd_info_data));

	/* register sysfs for LCD */
	lcd_info_data.ld = lcd_device_register("panel",
						&pdev->dev,
						&lcd_info_data,
						&HX8369b_lcd_ops);
	if (IS_ERR(lcd_info_data.ld)){
		printk(KERN_INFO "lcd_device_register() error\n");
		return PTR_ERR(lcd_info_data.ld);
	}

	/* register sysfs for LCD frequency control */
	ret = HX8369b_lcd_frequency_register(info->device);
	if (ret < 0)
		goto out;

	lcd_info_data.power = FB_BLANK_UNBLANK;

	ret = device_create_file(&lcd_info_data.ld->dev, &dev_attr_lcd_type);
	if (ret < 0)
		pr_err("failed to add sysfs entries, %d\n", __LINE__);

#if defined(CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG) || defined(CONFIG_FB_LCD_ESD)
	esd_check_flag = ESD_CHECK_DISABLE;
	mutex_init(&esd_check_mutex);
#endif /* CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG or CONFIG_FB_LCD_ESD */

#if defined(CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG)
	INIT_DELAYED_WORK(&esd_check_work, HX8369b_panel_esd_check_work);
#endif /* CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG */

#if defined(CONFIG_FB_LCD_ESD)
	/* get resource info from platform_device */
	res_irq_port	= platform_get_resource_byname(pdev,
							IORESOURCE_MEM,
							"panel_esd_irq_port");
	if (!res_irq_port) {
		printk(KERN_ALERT "panel_esd_irq_port is NULL!!\n");
		return -ENODEV;
	}
	esd_irq_portno = res_irq_port->start;
	/* GPIO control */
	gpio_request(esd_irq_portno, NULL);
	gpio_direction_input(esd_irq_portno);
	gpio_pull_off_port(esd_irq_portno);

	printk(KERN_INFO "GPIO_PORT%d : for ESD detect\n", esd_irq_portno);

	lcd_wq = create_workqueue("lcd_esd_irq_wq");
	INIT_WORK(&esd_detect_work, lcd_esd_detect);
	esd_detect_irq = gpio_to_irq(esd_irq_portno);
	printk(KERN_INFO "IRQ%d       : for ESD detect\n", esd_detect_irq);

#endif /* CONFIG_FB_LCD_ESD */

	return 0;

out:
	/* unregister sysfs for LCD */
	lcd_device_unregister(lcd_info_data.ld);

	return ret;
}

static int HX8369b_panel_remove(struct fb_info *info)
{
	printk(KERN_INFO "%s\n", __func__);

#if defined(CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG) || defined(CONFIG_FB_LCD_ESD)
	esd_check_flag = ESD_CHECK_DISABLE;

	/* Wait for end of check to power mode state */
	mutex_lock(&esd_check_mutex);
	mutex_unlock(&esd_check_mutex);
	mutex_destroy(&esd_check_mutex);
#endif /* CONFIG_LCD_ESD_RECOVERY_BY_CHECK_REG or CONFIG_FB_LCD_ESD */

	gpio_free(reset_gpio);

	/* unregister sysfs for LCD frequency control */
	HX8369b_lcd_frequency_unregister();

	/* unregister sysfs for LCD */
	lcd_device_unregister(lcd_info_data.ld);

	return 0;
}

static int HX8369b_panel_shutdown(void)
{
	pr_debug("%s\n", __func__);

    #if defined(CONFIG_FB_LCD_ESD)
	free_irq(esd_detect_irq, &esd_irq_requested);
	gpio_free(esd_irq_portno);
    #endif /* CONFIG_FB_LCD_ESD test*/

	gpio_set_value(47, 0);
	mdelay(3);

	mipi_display_power_off();

	return 0;
}

static struct fb_panel_info HX8369b_panel_info(void)
{
	printk(KERN_INFO "%s\n", __func__);

	r_mobile_info.buff_address = g_fb_start;
	return r_mobile_info;
}

struct fb_panel_func r_mobile_hx8369b_panel_func(int panel)
{

	struct fb_panel_func panel_func;

	printk(KERN_INFO "%s\n", __func__);

	memset(&panel_func, 0, sizeof(struct fb_panel_func));

/* e.g. support (panel=RT_DISPLAY_LCD1) */

	if (panel == RT_DISPLAY_LCD1) {
		panel_func.panel_init    = HX8369b_panel_init;
		panel_func.panel_suspend = HX8369b_panel_suspend;
		panel_func.panel_resume  = HX8369b_panel_resume;
		panel_func.panel_probe   = HX8369b_panel_probe;
		panel_func.panel_remove  = HX8369b_panel_remove;
		panel_func.panel_shutdown= HX8369b_panel_shutdown;
		panel_func.panel_info    = HX8369b_panel_info;
	}

	return panel_func;
}
