/*
 * drivers/video/r-mobile/panel/panel_nt35516.c
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

#include <video/sh_mobile_lcdc.h>

#include <rtapi/screen_display.h>

#include <linux/platform_device.h>
#include <linux/fb.h>

#include <linux/regulator/consumer.h>
#include <linux/lcd.h>

#include <mach/memory-r8a7373.h>

#include "panel_common.h"

/* #define NT35516_DRAW_BLACK_KERNEL */

#define NT35516_POWAREA_MNG_ENABLE
/* #define NT35516_GED_ORG */
#define QHD_COMMAND_MODE 1
/* #define NT35516_SWITCH_FRAMERATE_40HZ */

#ifdef NT35516_POWAREA_MNG_ENABLE
#include <rtapi/system_pwmng.h>
#endif

#define BRCM
/* panel size (mm) */
#define R_MOBILE_M_PANEL_SIZE_WIDTH		54
#define R_MOBILE_M_PANEL_SIZE_HEIGHT	95

#ifdef BRCM
/*
	.phys_width = 54,
	.phys_height = 95,

	.hs = 46,
	.hbp = 46,
	.hfp = 46,
	.vs = 20,
	.vbp = 20,
	.vfp = 20,

*/
#define R_MOBILE_M_PANEL_PIXEL_WIDTH	 540
#define R_MOBILE_M_PANEL_PIXEL_HEIGHT	 960
#define R_MOBILE_M_PANEL_LEFT_MARGIN	 46
#define R_MOBILE_M_PANEL_RIGHT_MARGIN	 46
#define R_MOBILE_M_PANEL_HSYNC_LEN	 46
#define R_MOBILE_M_PANEL_UPPER_MARGIN	 20
#define R_MOBILE_M_PANEL_LOWER_MARGIN	 20
#define R_MOBILE_M_PANEL_VSYNC_LEN	 20
#define R_MOBILE_M_PANEL_PIXCLOCK	 24103
#define R_MOBILE_M_PANEL_H_TOTAL	 678
#define R_MOBILE_M_PANEL_V_TOTAL	 1020
#define LCD_DSITCKCR		0x00000007
#define LCD_DSI0PCKCR		0x0000002E
#define LCD_DSI0PHYCR		0x2A800018
#define LCD_SYSCONF		0x00000F07
#define LCD_TIMSET0		0x4C2C6332
#define LCD_TIMSET1		0x00080092
#define LCD_DSICTRL		0x00000001
#define LCD_VMCTR1		0x0001003E
#define LCD_VMCTR2		0x00000718
#define LCD_VMLEN1		0x06540000
#define LCD_VMLEN2		0x010A007E
#define LCD_VMLEN3		0x00000000
#define LCD_VMLEN4		0x00000000
#define LCD_DTCTR		0x00000007
#define LCD_MLDHCNR		0x00430054
#define LCD_MLDHSYNR		0x00050049
#define LCD_MLDHAJR		0x04060602
#define LCD_MLDVLNR		0x03C003FC
#define LCD_MLDVSYNR		0x001403D5
#define LCD_MLDMT1R		0x0400000B
#define LCD_LDDCKR		0x00010040
#define LCD_MLDDCKPAT1R		0x00000000
#define LCD_MLDDCKPAT2R		0x00000000
#define LCD_PHYTEST		0x0000038C

#endif

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

#define NT35516_INIT_RETRY_COUNT 3

#define POWER_IS_ON(pwr)	((pwr) <= FB_BLANK_NORMAL)
static int NT35516_panel_suspend(void);
static int NT35516_panel_resume(void);
static void mipi_display_reset(void);
static void mipi_display_power_off(void);
static int NT35516_panel_draw(void *screen_handle);
static int panel_dsi_read(int id, int reg, int len, char *buf);

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

#define TE_SCAN_LINE 960

static char exit_sleep[2] = {0x11, 0x00};
static char display_on[2] = {0x29, 0x00};
static char display_off[2] = {0x28, 0x00};
static char enter_sleep[2] = {0x10, 0x00};

static char colmod[2] = {0x3A, 0x77};
/* Power Setting Sequence */
static char maucctr3[] = { 0xFF, 0xAA, 0x55, 0x25, 0x01 };
static char maucctrd3[] = { 0xF3, 0x02, 0x03, 0x07, 0x45, 0x88, 0xD4, 0x0D };
static char maucctrd4[] = { 0xF4, 0x40, 0x48, 0x00, 0x00, 0x40 };
static char maucctr0[] = { 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x00 };
static char rgbctr[] = { 0xB0, 0x00, 0x0C, 0x40, 0x3C, 0x3C };
static char dopctr[] = { 0xB1, 0xFC, 0x00 };
static char sdhdtctr[] = { 0xB6, 0x08 };
static char gseqctr[] = { 0xB7, 0x00, 0x00 };
static char sdvpctr[] = { 0xBA, 0x01 };
static char invctr[] = { 0xBC, 0x00 };
static char dpfrctr1[] = { 0xBD, 0x01, 0x41, 0x10, 0x37, 0x01 };
static char dpfrctr[] = { 0xCC, 0x03, 0x00, 0x00 };
static char maucctr1[] = { 0xF0, 0x55, 0xAA, 0x52, 0x08, 0x01 };
static char setavdd[] = { 0xB0, 0x0A, 0x0A, 0x0A };
static char bt1ctr[] = { 0xB6, 0x44, 0x44, 0x44 };
static char setavee[] = { 0xB1, 0x0A, 0x0A, 0x0A };
static char bt2ctr[] = { 0xB7, 0x24, 0x24, 0x24 };
static char setvcl[] = { 0xB2, 0x03, 0x03, 0x03 };
static char bt3ctr[] = { 0xB8, 0x30, 0x30, 0x30 };
static char setvgh[] = { 0xB3, 0x0D, 0x0D, 0x0D };
static char bt4ctr[] = { 0xB9, 0x24, 0x24, 0x24 };
static char setvglx[] = { 0xB4, 0x0A, 0x0A, 0x0A };
static char bt5ctr[] = { 0xBA, 0x24, 0x24, 0x24 };
static char setxxx[] = { 0xB5, 0x07, 0x07, 0x07 };
static char setvgp[] = { 0xBC, 0x00, 0x78, 0x00 };
static char setvgn[] = { 0xBD, 0x00, 0x78, 0x00 };
/*0x45:: proposed by vendor to remove ghost image*/
static char setvcmoff[] = { 0xBE, 0x45 };
/* Gamma Setting Sequence */
static char gmprctr1[] = { 0xD1, 0x00, 0x43, 0x00, 0x73, 0x00, 0xAE, 0x00,
		0xD8, 0x00, 0xF0, 0x01, 0x19, 0x01, 0x3A, 0x01, 0x60 };
static char gmprctr2[] = { 0xD2, 0x01, 0x7F, 0x01, 0xB0, 0x01, 0xD6, 0x02,
		0x13, 0x02, 0x44, 0x02, 0x45, 0x02, 0x72, 0x02, 0xA3 };
static char gmprctr3[] = { 0xD3, 0x02, 0xC0, 0x02, 0xEA, 0x03, 0x07, 0x03,
		0x32, 0x03, 0x4E, 0x03, 0x77, 0x03, 0x91, 0x03, 0xB9 };
static char gmprctr4[] = { 0xD4, 0x03, 0xF4, 0x03, 0xFF };
static char gmpgctr1[] = { 0xD5, 0x00, 0x43, 0x00, 0x73, 0x00, 0xAE, 0x00,
		0xD8, 0x00, 0xF0, 0x01, 0x19, 0x01, 0x3A, 0x01, 0x60 };
static char gmpgctr2[] = { 0xD6, 0x01, 0x7F, 0x01, 0xB0, 0x01, 0xD6, 0x02,
		0x13, 0x02, 0x44, 0x02, 0x45, 0x02, 0x72, 0x02, 0xA3 };
static char gmpgctr3[] = { 0xD7, 0x02, 0xC0, 0x02, 0xEA, 0x03, 0x07, 0x03,
		0x32, 0x03, 0x4E, 0x03, 0x77, 0x03, 0x91, 0x03, 0xB9 };
static char gmpgctr4[] = { 0xD8, 0x03, 0xF4, 0x03, 0xFF };
static char gmpbctr1[] = { 0xD9, 0x00, 0x43, 0x00, 0x73, 0x00, 0xAE, 0x00,
		0xD8, 0x00, 0xF0, 0x01, 0x19, 0x01, 0x3A, 0x01, 0x60 };
static char gmpbctr2[] = { 0xDD, 0x01, 0x7F, 0x01, 0xB0, 0x01, 0xD6, 0x02,
		0x13, 0x02, 0x44, 0x02, 0x45, 0x02, 0x72, 0x02, 0xA3 };
static char gmpbctr3[] = { 0xDE, 0x02, 0xC0, 0x02, 0xEA, 0x03, 0x07, 0x03,
		0x32, 0x03, 0x4E, 0x03, 0x77, 0x03, 0x91, 0x03, 0xB9 };
static char gmpbctr4[] = { 0xDF, 0x03, 0xF4, 0x03, 0xFF };

static char gmnrctr1[] = { 0xE0, 0x00, 0x43, 0x00, 0x73, 0x00, 0xAE, 0x00,
		0xD8, 0x00, 0xF0, 0x01, 0x19, 0x01, 0x3A, 0x01, 0x60 };
static char gmnrctr2[] = { 0xE1, 0x01, 0x7F, 0x01, 0xB0, 0x01, 0xD6, 0x02,
		0x13, 0x02, 0x44, 0x02, 0x45, 0x02, 0x72, 0x02, 0xA3 };
static char gmnrctr3[] = { 0xE2, 0x02, 0xC0, 0x02, 0xEA, 0x03, 0x07, 0x03,
		0x32, 0x03, 0x4E, 0x03, 0x77, 0x03, 0x91, 0x03, 0xB9 };
static char gmnrctr4[] = { 0xE3, 0x03, 0xF4, 0x03, 0xFF };

static char gmngctr1[] = { 0xE4, 0x00, 0x43, 0x00, 0x73, 0x00, 0xAE, 0x00,
		0xD8, 0x00, 0xF0, 0x01, 0x19, 0x01, 0x3A, 0x01, 0x60 };
static char gmngctr2[] = { 0xE5, 0x01, 0x7F, 0x01, 0xB0, 0x01, 0xD6, 0x02,
		0x13, 0x02, 0x44, 0x02, 0x45, 0x02, 0x72, 0x02, 0xA3 };
static char gmngctr3[] = { 0xE6, 0x02, 0xC0, 0x02, 0xEA, 0x03, 0x07, 0x03,
		0x32, 0x03, 0x4E, 0x03, 0x77, 0x03, 0x91, 0x03, 0xB9 };
static char gmngctr4[] = { 0xE7, 0x03, 0xF4, 0x03, 0xFF };

static char gmnbctr1[] = { 0xE8, 0x00, 0x43, 0x00, 0x73, 0x00, 0xAE, 0x00,
		0xD8, 0x00, 0xF0, 0x01, 0x19, 0x01, 0x3A, 0x01, 0x60 };
static char gmnbctr2[] = { 0xE9, 0x01, 0x7F, 0x01, 0xB0, 0x01, 0xD6, 0x02,
		0x13, 0x02, 0x44, 0x02, 0x45, 0x02, 0x72, 0x02, 0xA3 };
static char gmnbctr3[] = { 0xEA, 0x02, 0xC0, 0x02, 0xEA, 0x03, 0x07, 0x03,
		0x32, 0x03, 0x4E, 0x03, 0x77, 0x03, 0x91, 0x03, 0xB9 };
static char gmnbctr4[] = { 0xEB, 0x03, 0xF4, 0x03, 0xFF };
/* Set Tearing Effect Scan Line */
#ifdef QHD_COMMAND_MODE
static char stesl[] = { 0x44, (TE_SCAN_LINE & 0xFF00) >> 8,
			(TE_SCAN_LINE & 0xFF) };

static char teon[2] = {0x35, 0x00};
static const struct specific_cmdset initialize_cmdset[] = {
	{ MIPI_DSI_DCS_LONG_WRITE,  maucctr3,  sizeof(maucctr3) },
	{ MIPI_DSI_DCS_LONG_WRITE,  maucctrd3,   sizeof(maucctrd3)  },
	{ MIPI_DSI_DCS_LONG_WRITE,  maucctrd4,    sizeof(maucctrd4)   },

	{ MIPI_DSI_DCS_LONG_WRITE,  maucctr0,   sizeof(maucctr0)  },
	{ MIPI_DSI_DCS_LONG_WRITE,  rgbctr,    sizeof(rgbctr)   },
	{ MIPI_DSI_DCS_LONG_WRITE,  dopctr,    sizeof(dopctr)   },
	{ MIPI_DSI_DCS_LONG_WRITE,  sdhdtctr,    sizeof(sdhdtctr)   },
	{ MIPI_DSI_DCS_LONG_WRITE,  gseqctr,    sizeof(gseqctr)   },
	{ MIPI_DSI_DCS_LONG_WRITE,  sdvpctr, sizeof(sdvpctr)},
	{ MIPI_DSI_DCS_LONG_WRITE,  invctr,    sizeof(invctr)   },
	{ MIPI_DSI_DCS_LONG_WRITE,  dpfrctr1,    sizeof(dpfrctr1)   },
	{ MIPI_DSI_DCS_LONG_WRITE,  dpfrctr,    sizeof(dpfrctr)   },

	{ MIPI_DSI_DCS_LONG_WRITE,  maucctr1,  sizeof(maucctr1) },
	{ MIPI_DSI_DCS_LONG_WRITE,  setavdd,  sizeof(setavdd) },
	{ MIPI_DSI_DCS_LONG_WRITE,  bt1ctr,   sizeof(bt1ctr)  },
	{ MIPI_DSI_DCS_LONG_WRITE,  setavee,   sizeof(setavee)  },
	{ MIPI_DSI_DCS_LONG_WRITE,  bt2ctr,   sizeof(bt2ctr)  },

	{ MIPI_DSI_DCS_LONG_WRITE,  setvcl,  sizeof(setvcl) },
	{ MIPI_DSI_DCS_LONG_WRITE,  bt3ctr,  sizeof(bt3ctr) },
	{ MIPI_DSI_DCS_LONG_WRITE,  setvgh,  sizeof(setvgh) },
	{ MIPI_DSI_DCS_LONG_WRITE,  bt4ctr,  sizeof(bt4ctr) },
	{ MIPI_DSI_DCS_LONG_WRITE,  setvglx,  sizeof(setvglx) },
	{ MIPI_DSI_DCS_LONG_WRITE,  bt5ctr,  sizeof(bt5ctr) },
	{ MIPI_DSI_DCS_LONG_WRITE,  setxxx,  sizeof(setxxx) },
	{ MIPI_DSI_DCS_LONG_WRITE,  setvgp,  sizeof(setvgp) },
	{ MIPI_DSI_DCS_LONG_WRITE,  setvgn,  sizeof(setvgn) },
	{ MIPI_DSI_DCS_LONG_WRITE,  setvcmoff,  sizeof(setvcmoff) },

	{ MIPI_DSI_DCS_LONG_WRITE,  gmprctr1,  sizeof(gmprctr1) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmprctr2,  sizeof(gmprctr2) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmprctr3,  sizeof(gmprctr3) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmprctr4,  sizeof(gmprctr4) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmpgctr1,  sizeof(gmpgctr1) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmpgctr2,  sizeof(gmpgctr2) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmpgctr3,  sizeof(gmpgctr3) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmpgctr4,  sizeof(gmpgctr4) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmpbctr1,  sizeof(gmpbctr1) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmpbctr2,  sizeof(gmpbctr2) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmpbctr3,  sizeof(gmpbctr3) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmpbctr4,  sizeof(gmpbctr4) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmnrctr1,  sizeof(gmnrctr1) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmnrctr2,  sizeof(gmnrctr2) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmnrctr3,  sizeof(gmnrctr3) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmnrctr4,  sizeof(gmnrctr4) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmngctr1,  sizeof(gmngctr1) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmngctr2,  sizeof(gmngctr2) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmngctr3,  sizeof(gmngctr3) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmngctr4,  sizeof(gmngctr4) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmnbctr1,  sizeof(gmnbctr1) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmnbctr2,  sizeof(gmnbctr2) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmnbctr3,  sizeof(gmnbctr3) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmnbctr4,  sizeof(gmnbctr4) },

	{ MIPI_DSI_DCS_LONG_WRITE,  colmod,    sizeof(colmod)	},
	{MIPI_DSI_DCS_LONG_WRITE, teon,  sizeof(teon)},
	{MIPI_DSI_DCS_LONG_WRITE, stesl , sizeof(stesl) },
	{MIPI_DSI_DCS_SHORT_WRITE, exit_sleep, sizeof(exit_sleep)},
	{MIPI_DSI_DELAY,           NULL,      120              },
	{ MIPI_DSI_BLACK,           NULL,      0                },
	{ MIPI_DSI_DCS_SHORT_WRITE, display_on,    sizeof(display_on)   },
	{ MIPI_DSI_END,             NULL,      0                }
};
#else
static const struct specific_cmdset initialize_cmdset[] = {
	{ MIPI_DSI_DCS_LONG_WRITE,  maucctr3,  sizeof(maucctr3) },
	{ MIPI_DSI_DCS_LONG_WRITE,  maucctrd3,   sizeof(maucctrd3)  },
	{ MIPI_DSI_DCS_LONG_WRITE,  maucctrd4,    sizeof(maucctrd4)   },

	{ MIPI_DSI_DCS_LONG_WRITE,  maucctr0,   sizeof(maucctr0)  },
	{ MIPI_DSI_DCS_LONG_WRITE,  rgbctr,    sizeof(rgbctr)   },
	{ MIPI_DSI_DCS_LONG_WRITE,  dopctr,    sizeof(dopctr)   },
	{ MIPI_DSI_DCS_LONG_WRITE,  sdhdtctr,    sizeof(sdhdtctr)   },
	{ MIPI_DSI_DCS_LONG_WRITE,  gseqctr,    sizeof(gseqctr)   },
	{ MIPI_DSI_DCS_LONG_WRITE,  sdvpctr, sizeof(sdvpctr)},
	{ MIPI_DSI_DCS_LONG_WRITE,  invctr,    sizeof(invctr)   },
	{ MIPI_DSI_DCS_LONG_WRITE,  dpfrctr1,    sizeof(dpfrctr1)   },
	{ MIPI_DSI_DCS_LONG_WRITE,  dpfrctr,    sizeof(dpfrctr)   },

	{ MIPI_DSI_DCS_LONG_WRITE,  maucctr1,  sizeof(maucctr1) },
	{ MIPI_DSI_DCS_LONG_WRITE,  setavdd,  sizeof(setavdd) },
	{ MIPI_DSI_DCS_LONG_WRITE,  bt1ctr,   sizeof(bt1ctr)  },
	{ MIPI_DSI_DCS_LONG_WRITE,  setavee,   sizeof(setavee)  },
	{ MIPI_DSI_DCS_LONG_WRITE,  bt2ctr,   sizeof(bt2ctr)  },

	{ MIPI_DSI_DCS_LONG_WRITE,  setvcl,  sizeof(setvcl) },
	{ MIPI_DSI_DCS_LONG_WRITE,  bt3ctr,  sizeof(bt3ctr) },
	{ MIPI_DSI_DCS_LONG_WRITE,  setvgh,  sizeof(setvgh) },
	{ MIPI_DSI_DCS_LONG_WRITE,  bt4ctr,  sizeof(bt4ctr) },
	{ MIPI_DSI_DCS_LONG_WRITE,  setvglx,  sizeof(setvglx) },
	{ MIPI_DSI_DCS_LONG_WRITE,  bt5ctr,  sizeof(bt5ctr) },
	{ MIPI_DSI_DCS_LONG_WRITE,  setxxx,  sizeof(setxxx) },
	{ MIPI_DSI_DCS_LONG_WRITE,  setvgp,  sizeof(setvgp) },
	{ MIPI_DSI_DCS_LONG_WRITE,  setvgn,  sizeof(setvgn) },
	{ MIPI_DSI_DCS_LONG_WRITE,  setvcmoff,  sizeof(setvcmoff) },

	{ MIPI_DSI_DCS_LONG_WRITE,  gmprctr1,  sizeof(gmprctr1) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmprctr2,  sizeof(gmprctr2) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmprctr3,  sizeof(gmprctr3) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmprctr4,  sizeof(gmprctr4) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmpgctr1,  sizeof(gmpgctr1) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmpgctr2,  sizeof(gmpgctr2) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmpgctr3,  sizeof(gmpgctr3) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmpgctr4,  sizeof(gmpgctr4) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmpbctr1,  sizeof(gmpbctr1) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmpbctr2,  sizeof(gmpbctr2) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmpbctr3,  sizeof(gmpbctr3) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmpbctr4,  sizeof(gmpbctr4) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmnrctr1,  sizeof(gmnrctr1) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmnrctr2,  sizeof(gmnrctr2) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmnrctr3,  sizeof(gmnrctr3) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmnrctr4,  sizeof(gmnrctr4) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmngctr1,  sizeof(gmngctr1) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmngctr2,  sizeof(gmngctr2) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmngctr3,  sizeof(gmngctr3) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmngctr4,  sizeof(gmngctr4) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmnbctr1,  sizeof(gmnbctr1) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmnbctr2,  sizeof(gmnbctr2) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmnbctr3,  sizeof(gmnbctr3) },
	{ MIPI_DSI_DCS_LONG_WRITE,  gmnbctr4,  sizeof(gmnbctr4) },
	{ MIPI_DSI_DCS_LONG_WRITE,  colmod,    sizeof(colmod)	},
	{MIPI_DSI_DCS_SHORT_WRITE, exit_sleep, sizeof(exit_sleep)},
	{MIPI_DSI_DELAY,           NULL,      120              },

	{ MIPI_DSI_BLACK,           NULL,      0                },
	{ MIPI_DSI_DELAY, NULL, 20 },
	{ MIPI_DSI_DCS_SHORT_WRITE, display_on,    sizeof(display_on)   },
	{ MIPI_DSI_END,             NULL,      0                }
};

#endif

static const struct specific_cmdset initialize_cmdset_1[] = {
	{ MIPI_DSI_BLACK, NULL, 0 },
	{ MIPI_DSI_END, NULL, 0 }
};

static const struct specific_cmdset demise_cmdset[] = {

	{MIPI_DSI_DCS_SHORT_WRITE, display_off, sizeof(display_off)},
	{MIPI_DSI_DCS_SHORT_WRITE, enter_sleep, sizeof(enter_sleep)},
	{MIPI_DSI_DELAY,           NULL,      120              },
	{MIPI_DSI_END,             NULL,      0                }
};

static int is_dsi_read_enabled;
static int power_supplied;

static struct fb_info *common_fb_info;
static int panel_specific_cmdset(void *lcd_handle,
				   const struct specific_cmdset *cmdset);

struct lcd_info {
	struct mutex		lock;	/* Lock for change frequency */
	struct device		*dev;	/* Hold device of LCD */
	struct device_attribute	*attr;	/* Hold attribute info */
	struct lcd_device	*ld;	/* LCD device info */

	unsigned int			ldi_enable;
	unsigned int			power;
};

static struct lcd_info lcd_info_data;

static int initialize_now;


#ifdef NT35516_GED_ORG
static int lcd_get_power(struct lcd_device *ld)
{
#if 0
	struct lcd_info *lcd = lcd_get_data(ld);

	return lcd->power;
#endif
	return true;
}


static struct lcd_ops NT35516_lcd_ops = {
	.set_power = lcd_set_power,
	.get_power = lcd_get_power,
};

#else /* NT35516_GED_ORG */
static int NT35516_power_on(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	ret = NT35516_panel_resume();

	return ret;
}

static int NT35516_power_off(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	ret = NT35516_panel_suspend();

	return ret;
}

static int NT35516_power(struct lcd_info *lcd, int power)
{
	int ret = 0;

	if (POWER_IS_ON(power) && !POWER_IS_ON(lcd->power))
		ret = NT35516_power_on(lcd);
	else if (!POWER_IS_ON(power) && POWER_IS_ON(lcd->power))
		ret = NT35516_power_off(lcd);

	if (!ret)
		lcd->power = power;

	return ret;
}

static int NT35516_set_power(struct lcd_device *ld, int power)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	if (power != FB_BLANK_UNBLANK && power != FB_BLANK_POWERDOWN &&
		power != FB_BLANK_NORMAL) {
		dev_err(&lcd->ld->dev, "power value should be 0, 1 or 4.\n");
		return -EINVAL;
	}

	return NT35516_power(lcd, power);
}

static int NT35516_get_power(struct lcd_device *ld)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	return lcd->power;
}

static struct lcd_ops NT35516_lcd_ops = {
	.set_power = NT35516_set_power,
	.get_power = NT35516_get_power,
};

#endif /* NT35516_GED_ORG */

static int panel_dsi_read(int id, int reg, int len, char *buf)
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

static int NT35516_panel_draw(void *screen_handle)
{
	screen_disp_draw disp_draw;
	int ret;

	pr_debug("%s\n", __func__);

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

static int NT35516_panel_draw_black(void *screen_handle)
{
	u32 panel_width  = R_MOBILE_M_PANEL_PIXEL_WIDTH;
	u32 panel_height = R_MOBILE_M_PANEL_PIXEL_HEIGHT;
	screen_disp_draw disp_draw;
	int ret;

	pr_debug("%s\n", __func__);

#ifdef NT35516_DRAW_BLACK_KERNEL
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
#ifdef NT35516_DRAW_BLACK_KERNEL
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
				NT35516_panel_draw(lcd_handle);
			ret = NT35516_panel_draw_black(lcd_handle);
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
	printk(KERN_INFO "%s\n", __func__);

	/* Already power supply */
	if (power_supplied) {
		printk(KERN_ALERT "Already power supply!\n");
		goto out;
	}

	if (regulator_enable(power_ldo_1v8))
		 pr_err("Failed to enable regulator\n");
	usleep_range(1000, 1100);
	if (regulator_enable(power_ldo_3v))
		 pr_err("Failed to enable regulator\n");

	usleep_range(1000, 1100);
	gpio_direction_output(reset_gpio, 1);
	usleep_range(1000, 1100);

	gpio_direction_output(reset_gpio, 0);

	usleep_range(5000, 5500);

	gpio_direction_output(reset_gpio, 1);
	usleep_range(5000, 5500);

out:
	power_supplied = true;
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
	usleep_range(1000, 1100);
	regulator_disable(power_ldo_3v);
	regulator_disable(power_ldo_1v8);
	usleep_range(1000, 1100);
out:
	power_supplied = false;
}


static int NT35516_panel_init(unsigned int mem_size)
{
	void *screen_handle;
	screen_disp_start_lcd start_lcd;
	screen_disp_stop_lcd disp_stop_lcd;
	screen_disp_set_lcd_if_param set_lcd_if_param;
	screen_disp_set_address set_address;
	screen_disp_delete disp_delete;
	int ret = 0;
	int retry_count = NT35516_INIT_RETRY_COUNT;

#ifdef NT35516_POWAREA_MNG_ENABLE
	void *system_handle;
	system_pmg_param powarea_start_notify;
	system_pmg_delete pmg_delete;
#endif

	printk(KERN_INFO "%s\n", __func__);

	initialize_now = true;

#ifdef NT35516_POWAREA_MNG_ENABLE
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

	/* Transmit DSI command peculiar to a panel */
	if (retry_count == NT35516_INIT_RETRY_COUNT)
		ret = panel_specific_cmdset(screen_handle, initialize_cmdset_1);
	else
		ret = panel_specific_cmdset(screen_handle, initialize_cmdset);
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

			/* Start a display to LCD */
			start_lcd.handle	= screen_handle;
			start_lcd.output_mode	= RT_DISPLAY_LCD1;
			ret = screen_display_start_lcd(&start_lcd);
			if (ret != SMAP_LIB_DISPLAY_OK) {
				printk(KERN_ALERT "disp_start_lcd err!\n");
				goto out;
			}

			mipi_display_reset();

			goto retry;
		}
	}

	printk(KERN_DEBUG "Panel initialized with Command mode\n");

out:
	disp_delete.handle = screen_handle;
	screen_display_delete(&disp_delete);

	initialize_now = false;

	return ret;
}

static int NT35516_panel_suspend(void)
{
	void *screen_handle;
	screen_disp_stop_lcd disp_stop_lcd;
	screen_disp_start_lcd start_lcd;
	screen_disp_delete disp_delete;
	int ret;

#ifdef NT35516_POWAREA_MNG_ENABLE
	void *system_handle;
	system_pmg_param powarea_end_notify;
	system_pmg_delete pmg_delete;
#endif

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

	mipi_display_power_off();

	disp_delete.handle = screen_handle;
	screen_display_delete(&disp_delete);

#ifdef NT35516_POWAREA_MNG_ENABLE
	pr_debug("End A4LC power area\n");
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

	return 0;
}

static int NT35516_panel_resume(void)
{
	void *screen_handle;
	screen_disp_start_lcd start_lcd;
	screen_disp_stop_lcd disp_stop_lcd;
	screen_disp_delete disp_delete;
	unsigned char read_data[60];
	int retry_count_dsi;
	int ret = 0;
	int retry_count = NT35516_INIT_RETRY_COUNT;

#ifdef NT35516_POWAREA_MNG_ENABLE
	void *system_handle;
	system_pmg_param powarea_start_notify;
	system_pmg_delete pmg_delete;
#endif

	printk(KERN_INFO "%s\n", __func__);

#ifdef NT35516_POWAREA_MNG_ENABLE
	pr_debug("Start A4LC power area\n");
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

	/* LCD panel reset */
	mipi_display_reset();

	screen_handle =  screen_display_new();

	/* Start a display to LCD */
	start_lcd.handle	= screen_handle;
	start_lcd.output_mode	= RT_DISPLAY_LCD1;
	ret = screen_display_start_lcd(&start_lcd);
	if (ret != SMAP_LIB_DISPLAY_OK) {
		printk(KERN_ALERT "disp_start_lcd err!\n");
		goto out;
	}

	is_dsi_read_enabled = 1;

	retry_count_dsi = NT35516_INIT_RETRY_COUNT;
	do {
		ret = panel_dsi_read(MIPI_DSI_DCS_READ, 0x04, 4, &read_data[0]);
		if (ret == 0) {
			pr_debug("read_data(RDID0) = %02X\n",
								read_data[0]);
			pr_debug("read_data(RDID1) = %02X\n",
								read_data[1]);
			pr_debug("read_data(RDID2) = %02X\n",
								read_data[2]);
		}

		retry_count_dsi--;

		if (retry_count_dsi == 0) {
			printk(KERN_DEBUG "retry_count=%d, Diff LCD ID or DSI read problem\n",
							retry_count_dsi);
			break;
		}
	} while (read_data[0] != 0x55 || read_data[1] != 0xBC ||
							read_data[2] != 0xC0);

	/* Transmit DSI command peculiar to a panel */
	ret = panel_specific_cmdset(screen_handle, initialize_cmdset);
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

out:
	disp_delete.handle = screen_handle;
	screen_display_delete(&disp_delete);

	return ret;
}

static int NT35516_panel_probe(struct fb_info *info,
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
						&NT35516_lcd_ops);
	if (IS_ERR(lcd_info_data.ld))
		return PTR_ERR(lcd_info_data.ld);

	lcd_info_data.power = FB_BLANK_UNBLANK;

	return 0;

	/* unregister sysfs for LCD */
	lcd_device_unregister(lcd_info_data.ld);

	return ret;
}

static int NT35516_panel_remove(struct fb_info *info)
{
	printk(KERN_INFO "%s\n", __func__);

	gpio_free(reset_gpio);

	/* unregister sysfs for LCD */
	lcd_device_unregister(lcd_info_data.ld);

	return 0;
}

static struct fb_panel_info NT35516_panel_info(void)
{
	printk(KERN_INFO "%s\n", __func__);

	r_mobile_info.buff_address = g_fb_start;
	return r_mobile_info;
}

struct fb_panel_func r_mobile_nt35516_panel_func(int panel)
{

	struct fb_panel_func panel_func;

	printk(KERN_INFO "%s\n", __func__);

	memset(&panel_func, 0, sizeof(struct fb_panel_func));

/* e.g. support (panel=RT_DISPLAY_LCD1) */

	if (panel == RT_DISPLAY_LCD1) {
		panel_func.panel_init    = NT35516_panel_init;
		panel_func.panel_suspend = NT35516_panel_suspend;
		panel_func.panel_resume  = NT35516_panel_resume;
		panel_func.panel_probe   = NT35516_panel_probe;
		panel_func.panel_remove  = NT35516_panel_remove;
		panel_func.panel_info    = NT35516_panel_info;
	}

	return panel_func;
}
