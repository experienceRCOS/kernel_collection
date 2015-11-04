/*
 * drivers/video/r-mobile/panel/panel_nt35590.c
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

/* #define NT35590_DRAW_BLACK_KERNEL */

#define PANEL_NT35590

#define NT35590_POWAREA_MNG_ENABLE

#ifdef NT35590_POWAREA_MNG_ENABLE
#include <rtapi/system_pwmng.h>
#endif

/* panel size (mm) : unused for NT35590 Panel */
#define R_MOBILE_M_PANEL_SIZE_WIDTH	54
#define R_MOBILE_M_PANEL_SIZE_HEIGHT	95


#define R_MOBILE_M_PANEL_PIXEL_WIDTH	 720
#define R_MOBILE_M_PANEL_PIXEL_HEIGHT	 1280
#define R_MOBILE_M_PANEL_LEFT_MARGIN	 100
#define R_MOBILE_M_PANEL_RIGHT_MARGIN	 8
#define R_MOBILE_M_PANEL_HSYNC_LEN	 7
#define R_MOBILE_M_PANEL_UPPER_MARGIN	 8
#define R_MOBILE_M_PANEL_LOWER_MARGIN	 10
#define R_MOBILE_M_PANEL_VSYNC_LEN	 5
#define R_MOBILE_M_PANEL_PIXCLOCK	 15927
#define R_MOBILE_M_PANEL_H_TOTAL	 836
#define R_MOBILE_M_PANEL_V_TOTAL	 1303
/*register values for 500 mbps /lane @ 4 lanes */
#define LCD_DSITCKCR			0x00000007
#define LCD_DSI0PCKCR			0x00000022
#define LCD_DSI0PHYCR			0x2A800015
#define LCD_SYSCONF			0x00000F0F
#define LCD_TIMSET0			0x4C2C6332
#define LCD_TIMSET1			0x000900A2
#define LCD_DSICTRL			0x00000001
#define LCD_VMCTR1			0x0002003E
#define LCD_VMCTR2			0x00000718
#define LCD_VMLEN1			0x08700000
#define LCD_VMLEN2			0x013A000C
#define LCD_VMLEN3			0x00000000
#define LCD_VMLEN4			0x00000000
#define LCD_DTCTR			0x00000006
#define LCD_MLDHCNR			0x005A0068
#define LCD_MLDHSYNR			0x0001005B
#define LCD_MLDHAJR			0x00040000
#define LCD_MLDVLNR			0x00020040
#define LCD_MLDVSYNR			0x0005050B
#define LCD_MLDMT1R			0x0400000B
#define LCD_LDDCKR			0x00020040
#define LCD_MLDDCKPAT1R			0x00000000
#define LCD_MLDDCKPAT2R			0x00000000
#define LCD_PHYTEST			0x0000038C


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

#define NT35590_INIT_RETRY_COUNT 3

#define POWER_IS_ON(pwr)	((pwr) <= FB_BLANK_NORMAL)
static int NT35590_panel_suspend(void);
static int NT35590_panel_resume(void);
static void mipi_display_reset(void);
static void mipi_display_power_off(void);
static int NT35590_panel_draw(void *screen_handle);
static int panel_dsi_read(int id, int reg, int len, char *buf);

static struct fb_panel_info r_mobile_info = {
	.pixel_width	= R_MOBILE_M_PANEL_PIXEL_WIDTH,
	.pixel_height	= R_MOBILE_M_PANEL_PIXEL_HEIGHT,
	.size_width	= R_MOBILE_M_PANEL_SIZE_WIDTH,
	.size_height	= R_MOBILE_M_PANEL_SIZE_HEIGHT,
	.pixclock       = R_MOBILE_M_PANEL_PIXCLOCK,
	.left_margin    = R_MOBILE_M_PANEL_LEFT_MARGIN,
	.right_margin   = R_MOBILE_M_PANEL_RIGHT_MARGIN,
	.upper_margin   = R_MOBILE_M_PANEL_UPPER_MARGIN,
	.lower_margin   = R_MOBILE_M_PANEL_LOWER_MARGIN,
	.hsync_len      = R_MOBILE_M_PANEL_HSYNC_LEN,
	.vsync_len      = R_MOBILE_M_PANEL_VSYNC_LEN,
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
/* panel Specific*/
#define MIPI_DSI_NT35590_GAMMA		(0x02)
#define TE_SCAN_LINE 1279

static char exit_sleep[2] = {0x11, 0x00};
static char display_on[2] = {0x29, 0x00};
static char display_off[2] = {0x28, 0x00};
static char enter_sleep[2] = {0x10, 0x00};

/*Panel Manufacturer commands */
static char  entrycmd3[] = {0xFF , 0xEE};
static char  oscoff[] = {0x26 , 0x08};
static char  entrycmd1[] = {0xFF , 0x00};
static char  oscon[] = {0x26 , 0x00};

static char stesl[] = { 0x44, (TE_SCAN_LINE & 0xFF00) >> 8,
	(TE_SCAN_LINE & 0xFF) };

static char teon[2] = {0x35, 0x00};
static char cmd1_regs[]  = {0xFF , 0x00}; /*basic display control setting*/
static char cmd2_page0[] = {0xFF , 0x01}; /*pwr,mtp,gamma setting*/
static char cmd2_page1[] = {0xFF , 0x02}; /*gamma only*/
static char cmd2_page2[] = {0xFF , 0x03}; /*Image enhancement*/
static char cmd2_page3[] = {0xFF , 0x04};  /*CABC*/
static char cmd2_page4[] = {0xFF , 0x05};  /*display LTPS timing*/
static char reload_reg[] = {0xFB , 0x01};  /*reload page */

/* set VDD Level */
static char vdd_regff[] = {0xFF , 0xEE};
static char vdd_reg12[] = {0x12 , 0x50};
static char vdd_reg13[] = {0x13 , 0x02};
static char vdd_reg6A[] = {0x6A , 0x60};
static char vdd_reg05[] = {0x05 , 0x59};
/* no of lanes 0x2= 3 lanes , 0x3= 4lanes*/
static char  setmipilane[] = {0xBA , 0x03};
static char  setdsimode[] = {0xC2 , 0x08};

/*Brightness control */
static char  wrdisbv[] = {0x51 , 0xFF};
static char  wrctrld[] = {0x53 , 0x2C};
static char  wrpwrsave[] = {0x55 , 0x00};

/*cmd2 page 0 power  as per Novatek reference*/
static char pwrctrl1[] = {0x00, 0x4A};
static char pwrctrl2[] = {0x01, 0x33};
static char pwrctrl3[] = {0x02, 0x53};
static char pwrctrl4[] = {0x03, 0x55};
static char pwrctrl5[] = {0x04, 0x55};
static char pwrctrl6[] = {0x05, 0x33};
static char pwrctrl7[] = {0x06, 0x22};
static char pwrctrl8[] = {0x08, 0x56};
static char pwrctrl9[] = {0x09, 0x8F};
/* no description about 0x36 in data manual / appnote */
static char regnodesc1[] = {0x36, 0x73};
static char regctrl1[] = {0x0B, 0xCF};
static char regctrl2[] = {0x0C, 0xCF};
static char regctrl3[] = {0x0D, 0x2F};
static char regctrl4[] = {0x0E, 0x29};
static char regctrl7[] = {0x11, 0x86};
static char regctrl8[] = {0x12, 0x03};
static char mipictrl1[] = {0x71, 0x2C};
static char gatedrv[] = {0x6F, 0x03};
/* no description about 0x0F in data manual / appnote */
/* NVT engineer cmd*/
static char regnodesc2[] = {0x0F, 0x0A};

/*cmd2 page 4 -display register */
static char  lptsctrl1[] = {0x01, 0x00};
static char  lptsctrl2[] = {0x02, 0x82};
static char  lptsctrl3[] = {0x03, 0x82};
static char  lptsctrl4[] = {0x04, 0x82};
static char  lptsctrl5[] = {0x05, 0x30};
static char  lptsctrl6[] = {0x06, 0x33};
static char  c2pg4reg7[] = {0x07, 0x01};
static char  c2pg4reg8[] = {0x08, 0x00};
static char  c2pg4reg9[] = {0x09, 0x46};
static char  c2pg4regA[] = {0x0A, 0x46};
static char  c2pg4regD[] = {0x0D, 0x0B};
static char  c2pg4regE[] = {0x0E, 0x1D};
static char  c2pg4regF[] = {0x0F, 0x08};
static char  c2pg4reg10[] = {0x10, 0x53};
static char  c2pg4reg11[] = {0x11, 0x00};
static char  c2pg4reg12[] = {0x12, 0x00};
static char  c2pg4reg14[] = {0x14, 0x01};
static char  c2pg4reg15[] = {0x15, 0x00};
static char  c2pg4reg16[] = {0x16, 0x05};
static char  c2pg4reg17[] = {0x17, 0x00};
static char  c2pg4reg19[] = {0x19, 0x7F};
static char  c2pg4reg1A[] = {0x1A, 0xFF};
static char  c2pg4reg1B[] = {0x1B, 0x0F};
static char  c2pg4reg1C[] = {0x1C, 0x00};
static char  c2pg4reg1D[] = {0x1D, 0x00};
static char  c2pg4reg1E[] = {0x1E, 0x00};
static char  c2pg4reg1F[] = {0x1F, 0x07};
static char  partialctrl[] = {0x20, 0x00};
static char  displayctrl1[] = {0x21, 0x00};
static char  displayctrl2[] = {0x22, 0x55};
static char  displayctrl3[] = {0x23, 0x4D};
static char  displayctrl4[] = {0x2D, 0x02};
static char  c2pg4reg83[] = {0x83, 0x01};
static char  c2pg4reg9E[] = {0x9E, 0x58};
static char  c2pg4reg9F[] = {0x9F, 0x6A};
static char  c2pg4regA0[] = {0xA0, 0x01};
static char  slpmode[] = {0xA2, 0x10};
static char  porchctrl1[] = {0xBB, 0x0A};
static char  porchctrl2[] = {0xBC, 0x0A};
/*NVT Engineer command*/
static char  c2pg4reg28[] = {0x28, 0x01};
static char  c2pg4reg2F[] = {0x2F, 0x02};
static char  c2pg4reg32[] = {0x32, 0x08};
static char  c2pg4reg33[] = {0x33, 0xB8};
static char  c2pg4reg36[] = {0x36, 0x01};
static char  c2pg4reg37[] = {0x37, 0x00};
static char  c2pg4reg43[] = {0x43, 0x00};
static char  c2pg4reg4B[] = {0x4B, 0x21};
static char  c2pg4reg4C[] = {0x4C, 0x03};
static char  c2pg4reg50[] = {0x50, 0x21};
static char  c2pg4reg51[] = {0x51, 0x03};
static char  c2pg4reg58[] = {0x58, 0x21};
static char  c2pg4reg59[] = {0x59, 0x03};
static char  c2pg4reg5D[] = {0x5D, 0x21};
static char  c2pg4reg5E[] = {0x5E, 0x03};
static char  c2pg4reg6C[] = {0x6C, 0x00};
static char  c2pg4reg6D[] = {0x6D, 0x00};


static char nt35590_gamma1[] = {
	/* R+*/ 0x75,
	0x00, 0x98, 0x00, 0xAF, 0x00,
	0xD1, 0x00, 0xE9, 0x00, 0xFE,
	0x01, 0x10, 0x01, 0x20, 0x01,
	0x2E, 0x01, 0x3B, 0x01, 0x65,
	0x01, 0x88, 0x01, 0xBD, 0x01,
	0xE7, 0x02, 0x27, 0x02, 0x59,
	0x02, 0x5B, 0x02, 0x87, 0x02,
	0xB6, 0x02, 0xD5, 0x02, 0xFD,
	0x03, 0x19, 0x03, 0x40, 0x03,
	0x4C, 0x03, 0x59, 0x03, 0x67,
	0x03, 0x78, 0x03, 0x8A, 0x03,
	0xA8, 0x03, 0xB8, 0x03, 0xBE,

	0x00, 0x98, 0x00, 0xAF, 0x00,
	0xD1, 0x00, 0xE9, 0x00, 0xFE,
	0x01, 0x10, 0x01, 0x20, 0x01,
	0x2E, 0x01, 0x3B, 0x01, 0x65,
	0x01, 0x88, 0x01, 0xBD, 0x01,
	0xE7, 0x02, 0x27, 0x02, 0x59,
	0x02, 0x5B, 0x02, 0x87, 0x02,
	0xB6, 0x02, 0xD5, 0x02, 0xFD,
	0x03, 0x19, 0x03, 0x40, 0x03,
	0x4C, 0x03, 0x59, 0x03, 0x67,
	0x03, 0x78, 0x03, 0x8A, 0x03,
	0xA8, 0x03, 0xB8, 0x03, 0xBE,

	0x00, 0x98, 0x00, 0xAF, 0x00,
	0xD1, 0x00, 0xE9, 0x00, 0xFE,
	0x01, 0x10};

/*  switch page */
static char page_switch[] = {0xFF, 0x02};

static char nt35590_gamma2[] = {0x00,
	0x01, 0x20, 0x01, 0x2E, 0x01,
	0x3B, 0x01, 0x65, 0x01, 0x88,
	0x01, 0xBD, 0x01, 0xE7, 0x02,
	0x27, 0x02, 0x59, 0x02, 0x5B,
	0x02, 0x87, 0x02, 0xB6, 0x02,
	0xD5, 0x02, 0xFD, 0x03, 0x19,
	0x03, 0x40, 0x03, 0x4C, 0x03,
	0x59, 0x03, 0x67, 0x03, 0x78,
	0x03, 0x8A, 0x03, 0xA8, 0x03,
	0xB8, 0x03, 0xBE,
	/* G - */
	0x00, 0x98, 0x00, 0xAF, 0x00,
	0xD1, 0x00, 0xE9, 0x00, 0xFE,
	0x01, 0x10, 0x01, 0x20, 0x01,
	0x2E, 0x01, 0x3B, 0x01, 0x65,
	0x01, 0x88, 0x01, 0xBD, 0x01,
	0xE7, 0x02, 0x27, 0x02, 0x59,
	0x02, 0x5B, 0x02, 0x87, 0x02,
	0xB6, 0x02, 0xD5, 0x02, 0xFD,
	0x03, 0x19, 0x03, 0x40, 0x03,
	0x4C, 0x03, 0x59, 0x03, 0x67,
	0x03, 0x78, 0x03, 0x8A, 0x03,
	0xA8, 0x03, 0xB8, 0x03, 0xBE,
	/* B + */
	0x00, 0x98, 0x00, 0xAF, 0x00,
	0xD1, 0x00, 0xE9, 0x00, 0xFE,
	0x01, 0x10, 0x01, 0x20, 0x01,
	0x2E, 0x01, 0x3B, 0x01, 0x65,
	0x01, 0x88, 0x01, 0xBD, 0x01,
	0xE7, 0x02, 0x27, 0x02, 0x59,
	0x02, 0x5B, 0x02, 0x87, 0x02,
	0xB6, 0x02, 0xD5, 0x02, 0xFD,
	0x03, 0x19, 0x03, 0x40, 0x03,
	0x4C, 0x03, 0x59, 0x03, 0x67,
	0x03, 0x78, 0x03, 0x8A, 0x03,
	0xA8, 0x03, 0xB8, 0x03, 0xBE,
	/* B - */
	0x00, 0x98, 0x00, 0xAF, 0x00,
	0xD1, 0x00, 0xE9, 0x00, 0xFE,
	0x01, 0x10, 0x01, 0x20, 0x01,
	0x2E, 0x01, 0x3B, 0x01, 0x65,
	0x01, 0x88, 0x01, 0xBD, 0x01,
	0xE7, 0x02, 0x27, 0x02, 0x59,
	0x02, 0x5B, 0x02, 0x87, 0x02,
	0xB6, 0x02, 0xD5, 0x02, 0xFD,
	0x03, 0x19, 0x03, 0x40, 0x03,
	0x4C, 0x03, 0x59, 0x03, 0x67,
	0x03, 0x78, 0x03, 0x8A, 0x03,
	0xA8, 0x03, 0xB8, 0x03, 0xBE
};

static const struct specific_cmdset pwrinit_cmdset1[] = {
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, entrycmd3, sizeof(entrycmd3) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, oscoff, sizeof(oscoff) },
	{ MIPI_DSI_END,   NULL,     0                }
};
static const struct specific_cmdset pwrinit_cmdset2[] = {
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, oscon, sizeof(oscon) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, entrycmd1, sizeof(entrycmd1) },
	{ MIPI_DSI_END,   NULL,     0                }
};

static const struct specific_cmdset initialize_cmdset[] = {
	/* Panel Power Setting*/
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, vdd_regff,  sizeof(vdd_regff)},
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, reload_reg,  sizeof(reload_reg)},
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, vdd_reg12,  sizeof(vdd_reg12)},
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, vdd_reg13,  sizeof(vdd_reg13)},
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, vdd_reg6A,  sizeof(vdd_reg6A)},
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, vdd_reg05,  sizeof(vdd_reg05)},
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, cmd2_page0, sizeof(cmd2_page0)},
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, reload_reg,  sizeof(reload_reg)},
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, pwrctrl1,   sizeof(pwrctrl1)  },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, pwrctrl2,  sizeof(pwrctrl2)  },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, pwrctrl3,   sizeof(pwrctrl3)   },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, pwrctrl4,   sizeof(pwrctrl4)   },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, pwrctrl5,   sizeof(pwrctrl5)   },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, pwrctrl6,   sizeof(pwrctrl6)   },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, pwrctrl7,   sizeof(pwrctrl7)},
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, pwrctrl8,   sizeof(pwrctrl8)   },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, pwrctrl9,   sizeof(pwrctrl9)   },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, regnodesc1,   sizeof(regnodesc1) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, regctrl1, sizeof(regctrl1) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, regctrl2, sizeof(regctrl2) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, regctrl3,  sizeof(regctrl3)  },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, regctrl4,  sizeof(regctrl4)  },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, regctrl7,  sizeof(regctrl7)  },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, regctrl8, sizeof(regctrl8) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, mipictrl1, sizeof(mipictrl1) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, gatedrv, sizeof(gatedrv) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, regnodesc2, sizeof(regnodesc2) },
	/* LPTS timing*/
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, cmd2_page4, sizeof(cmd2_page4) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, reload_reg, sizeof(reload_reg) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, lptsctrl1, sizeof(lptsctrl1) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, lptsctrl2, sizeof(lptsctrl2) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, lptsctrl3, sizeof(lptsctrl3) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, lptsctrl4, sizeof(lptsctrl4) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, lptsctrl5, sizeof(lptsctrl5) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, lptsctrl6, sizeof(lptsctrl6) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg7, sizeof(c2pg4reg7) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg8, sizeof(c2pg4reg8) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg9, sizeof(c2pg4reg9) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4regA, sizeof(c2pg4regA) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4regD, sizeof(c2pg4regD) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4regE, sizeof(c2pg4regE) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4regF, sizeof(c2pg4regF) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg10, sizeof(c2pg4reg10) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg11, sizeof(c2pg4reg11) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg12, sizeof(c2pg4reg12) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg14, sizeof(c2pg4reg14) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg15, sizeof(c2pg4reg15) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg16, sizeof(c2pg4reg16) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg17, sizeof(c2pg4reg17) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg19, sizeof(c2pg4reg19) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg1A, sizeof(c2pg4reg1A) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg1B, sizeof(c2pg4reg1B) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg1C, sizeof(c2pg4reg1C) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg1D, sizeof(c2pg4reg1D) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg1E, sizeof(c2pg4reg1E) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg1F, sizeof(c2pg4reg1F) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, partialctrl,  sizeof(partialctrl) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, displayctrl1, sizeof(displayctrl1) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, displayctrl2, sizeof(displayctrl2) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, displayctrl3, sizeof(displayctrl3) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, displayctrl4, sizeof(displayctrl4) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg83, sizeof(c2pg4reg83) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg9E, sizeof(c2pg4reg9E) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg9F, sizeof(c2pg4reg9F) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4regA0, sizeof(c2pg4regA0) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, slpmode,    sizeof(slpmode) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, porchctrl1, sizeof(porchctrl1) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, porchctrl2, sizeof(porchctrl2) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg28, sizeof(c2pg4reg28) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg2F, sizeof(c2pg4reg2F) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg32, sizeof(c2pg4reg32) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg33, sizeof(c2pg4reg33) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg36, sizeof(c2pg4reg36) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg37, sizeof(c2pg4reg37) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg43, sizeof(c2pg4reg43) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg4B, sizeof(c2pg4reg4B) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg4C, sizeof(c2pg4reg4C) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg50, sizeof(c2pg4reg50) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg51, sizeof(c2pg4reg51) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg58, sizeof(c2pg4reg58) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg59, sizeof(c2pg4reg59) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg5D, sizeof(c2pg4reg5D) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg5E, sizeof(c2pg4reg5E) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg6C, sizeof(c2pg4reg6C) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, c2pg4reg6D, sizeof(c2pg4reg6D) },
	{ MIPI_DSI_DCS_LONG_WRITE,	nt35590_gamma1,	sizeof(nt35590_gamma1)},
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, page_switch,	sizeof(page_switch)},
	{ MIPI_DSI_DCS_LONG_WRITE,	nt35590_gamma2,	sizeof(nt35590_gamma2)},
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, cmd1_regs,   sizeof(cmd1_regs)  },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, reload_reg, sizeof(reload_reg) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, setmipilane, sizeof(setmipilane)},
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, setdsimode, sizeof(setdsimode) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, teon, sizeof(teon)},
	{ MIPI_DSI_DCS_LONG_WRITE, stesl, sizeof(stesl)},
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, cmd1_regs,   sizeof(cmd1_regs)  },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, reload_reg, sizeof(reload_reg) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, cmd2_page0, sizeof(cmd2_page0) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, reload_reg, sizeof(reload_reg) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, cmd2_page1, sizeof(cmd2_page1) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, reload_reg, sizeof(reload_reg) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, cmd2_page2, sizeof(cmd2_page1) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, reload_reg, sizeof(reload_reg) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, cmd2_page3, sizeof(cmd2_page3) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, reload_reg, sizeof(reload_reg) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, cmd2_page4, sizeof(cmd2_page4) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, reload_reg, sizeof(reload_reg) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, cmd1_regs,   sizeof(cmd1_regs) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, reload_reg, sizeof(reload_reg) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, exit_sleep,   sizeof(exit_sleep) },
	{ MIPI_DSI_DELAY, NULL, 120 },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, wrdisbv, sizeof(wrdisbv) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, wrctrld, sizeof(wrctrld) },
	{ MIPI_DSI_DCS_SHORT_WRITE_PARAM, wrpwrsave, sizeof(wrpwrsave) },
	{ MIPI_DSI_DCS_SHORT_WRITE, display_on,   sizeof(display_on) },
	{ MIPI_DSI_BLACK, NULL, 0 },
	{ MIPI_DSI_END, NULL, 0 }
};

static const struct specific_cmdset demise_cmdset[] = {

	{MIPI_DSI_DCS_SHORT_WRITE, display_off, sizeof(display_off)},
	{MIPI_DSI_DCS_SHORT_WRITE, enter_sleep, sizeof(enter_sleep)},
	{MIPI_DSI_DELAY, NULL, 120 },
	{MIPI_DSI_END, NULL, 0 }
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

static int NT35590_power_on(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	ret = NT35590_panel_resume();

	return ret;
}

static int NT35590_power_off(struct lcd_info *lcd)
{
	int ret = 0;

	dev_info(&lcd->ld->dev, "%s\n", __func__);

	ret = NT35590_panel_suspend();

	return ret;
}

static int NT35590_power(struct lcd_info *lcd, int power)
{
	int ret = 0;

	if (POWER_IS_ON(power) && !POWER_IS_ON(lcd->power))
		ret = NT35590_power_on(lcd);
	else if (!POWER_IS_ON(power) && POWER_IS_ON(lcd->power))
		ret = NT35590_power_off(lcd);

	if (!ret)
		lcd->power = power;

	return ret;
}

static int NT35590_set_power(struct lcd_device *ld, int power)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	if (power != FB_BLANK_UNBLANK && power != FB_BLANK_POWERDOWN &&
			power != FB_BLANK_NORMAL) {
		dev_err(&lcd->ld->dev, "power value should be 0, 1 or 4.\n");
		return -EINVAL;
	}

	return NT35590_power(lcd, power);
}

static int NT35590_get_power(struct lcd_device *ld)
{
	struct lcd_info *lcd = lcd_get_data(ld);

	return lcd->power;
}

static struct lcd_ops NT35590_lcd_ops = {
	.set_power = NT35590_set_power,
	.get_power = NT35590_get_power,
};

static int panel_dsi_read(int id, int reg, int len, char *buf)
{
	void *screen_handle;
	screen_disp_write_dsi_short write_dsi_s;
	screen_disp_read_dsi_short read_dsi_s;
	screen_disp_delete disp_delete;
	int ret = 0;

	pr_debug("%s\n", __func__);

	if (!is_dsi_read_enabled) {
		pr_err("sequence error!!\n");
		return -EINVAL;
	}

	if ((len <= 0) || (len > 60) || (buf == NULL)) {
		pr_err("argument error!!\n");
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
		pr_err("disp_write_dsi_short err!\n");
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
		pr_err("disp_dsi_read err! ret = %d\n", ret);

out:
	disp_delete.handle = screen_handle;
	screen_display_delete(&disp_delete);

	return ret;
}

static int NT35590_panel_draw(void *screen_handle)
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
		pr_err("screen_display_draw err!\n");
		return -1;
	}

	return 0;
}

static int NT35590_panel_draw_black(void *screen_handle)
{
	u32 panel_width  = R_MOBILE_M_PANEL_PIXEL_WIDTH;
	u32 panel_height = R_MOBILE_M_PANEL_PIXEL_HEIGHT;
	screen_disp_draw disp_draw;
	int ret;

	pr_debug("%s\n", __func__);

#ifdef NT35590_DRAW_BLACK_KERNEL
	pr_debug("num_registered_fb = %d\n", num_registered_fb);

	if (!num_registered_fb) {
		pr_err("num_registered_fb err!\n");
		return -1;
	}
	if (!registered_fb[0]->fix.smem_start) {
		pr_err(
				"registered_fb[0]->fix.smem_start"
				" is NULL err!\n");
		return -1;
	}
	pr_debug("registerd_fb[0]-> fix.smem_start: %08x\n"
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
#ifdef NT35590_DRAW_BLACK_KERNEL
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

static int panel_specific_cmdset(void *lcd_handle,
		const struct specific_cmdset *cmdset)
{
	int ret;
	int loop = 0;
	screen_disp_write_dsi_short write_dsi_s;
	screen_disp_write_dsi_long  write_dsi_l;
	pr_debug("%s\n", __func__);
	while (0 <= loop) {
		switch (cmdset[loop].cmd) {
		case MIPI_DSI_DCS_LONG_WRITE:
		case MIPI_DSI_GEN_LONG_WRITE:
			pr_debug("panel_cmdset LONG Write\n");
			write_dsi_l.handle         = lcd_handle;
			write_dsi_l.output_mode    = RT_DISPLAY_LCD1;
			write_dsi_l.data_id        = cmdset[loop].cmd;
			write_dsi_l.data_count     = cmdset[loop].size;
			write_dsi_l.write_data     = cmdset[loop].data;
			write_dsi_l.reception_mode = RT_DISPLAY_RECEPTION_OFF;
			write_dsi_l.send_mode      = RT_DISPLAY_SEND_MODE_HS;
			ret = screen_display_write_dsi_long_packet(
					&write_dsi_l);
			if (ret != SMAP_LIB_DISPLAY_OK) {
				pr_err("LONG PKT WR err %d\n", ret);
				return -1;
			}
			break;
		case MIPI_DSI_DCS_SHORT_WRITE_PARAM:
		case MIPI_DSI_GEN_SHORT_WRITE_PARAM:
			pr_debug("SHORT Write with param\n");
			write_dsi_s.handle         = lcd_handle;
			write_dsi_s.output_mode    = RT_DISPLAY_LCD1;
			write_dsi_s.data_id        = cmdset[loop].cmd;
			write_dsi_s.reg_address    = cmdset[loop].data[0];
			write_dsi_s.write_data     = cmdset[loop].data[1];
			write_dsi_s.reception_mode = RT_DISPLAY_RECEPTION_ON;

			ret = screen_display_write_dsi_short_packet(
					&write_dsi_s);
			if (ret != SMAP_LIB_DISPLAY_OK) {
				pr_err("SH PKT WR PARM err %d\n", ret);
				return -1;
			}
			break;
		case MIPI_DSI_DCS_SHORT_WRITE:
		case MIPI_DSI_GEN_SHORT_WRITE:
			pr_debug("panel_cmdset SHORT Write\n");
			write_dsi_s.handle         = lcd_handle;
			write_dsi_s.output_mode    = RT_DISPLAY_LCD1;
			write_dsi_s.data_id        = cmdset[loop].cmd;
			write_dsi_s.reg_address    = cmdset[loop].data[0];
			write_dsi_s.write_data     = 0x00;
			write_dsi_s.reception_mode = RT_DISPLAY_RECEPTION_ON;
			ret = screen_display_write_dsi_short_packet(
					&write_dsi_s);
			if (ret != SMAP_LIB_DISPLAY_OK) {
				pr_err("SH PKT WR err %d\n", ret);
				return -1;
			}
			break;
		case MIPI_DSI_BLACK:
			{
				if (!initialize_now)
					NT35590_panel_draw(lcd_handle);
				ret = NT35590_panel_draw_black(lcd_handle);
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
			pr_err("Undefined command err!\n");
			return -1;
		}
		loop++;
	}

	return 0;
}

static void mipi_display_reset(void)
{
	pr_debug("%s\n", __func__);
	/* Already power supply */
	if (power_supplied) {
		pr_err("Already power supply!\n");
		goto out;
	}

	if (regulator_enable(power_ldo_1v8))
		pr_err("Failed to enable regulator\n");
	usleep_range(1000, 1500);
	if (regulator_enable(power_ldo_3v))
		pr_err("Failed to enable regulator\n");
	gpio_direction_output(reset_gpio, 0);

	usleep_range(5000, 5500);

	gpio_direction_output(reset_gpio, 1);
	usleep_range(5000, 5500);

out:
	power_supplied = true;
}

static void mipi_display_power_off(void)
{
	pr_debug("%s\n", __func__);

	/* Already not power supply */
	if (!power_supplied) {
		pr_err("Already not power supply!\n");
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


static int NT35590_panel_init(unsigned int mem_size)
{
	void *screen_handle;
	screen_disp_start_lcd start_lcd;
	screen_disp_stop_lcd disp_stop_lcd;
	screen_disp_set_lcd_if_param set_lcd_if_param;
	screen_disp_set_address set_address;
	screen_disp_delete disp_delete;
	int ret = 0;
	int retry_count = NT35590_INIT_RETRY_COUNT;

#ifdef NT35590_POWAREA_MNG_ENABLE
	void *system_handle;
	system_pmg_param powarea_start_notify;
	system_pmg_delete pmg_delete;
#endif

	pr_debug("%s\n", __func__);
	initialize_now = true;

#ifdef NT35590_POWAREA_MNG_ENABLE
	pr_debug("Start A4LC power area\n");
	system_handle = system_pwmng_new();

	/* Notifying the Beginning of Using Power Area */
	powarea_start_notify.handle		= system_handle;
	powarea_start_notify.powerarea_name	= RT_PWMNG_POWERAREA_A4LC;
	ret = system_pwmng_powerarea_start_notify(&powarea_start_notify);
	if (ret != SMAP_LIB_PWMNG_OK)
		pr_err("system_pwmng_powerarea_start_notify err!\n");
	pmg_delete.handle = system_handle;
	system_pwmng_delete(&pmg_delete);
#endif

	gpio_direction_output(reset_gpio, 0);
	usleep_range(30000, 31000);

	gpio_direction_output(reset_gpio, 1);
	usleep_range(20000, 21000);

	screen_handle =  screen_display_new();

	/* Setting peculiar to panel */
	set_lcd_if_param.handle		= screen_handle;
	set_lcd_if_param.port_no		= irq_portno;
	set_lcd_if_param.lcd_if_param		= &r_mobile_lcd_if_param;
	set_lcd_if_param.lcd_if_param_mask	= &r_mobile_lcd_if_param_mask;
	pr_debug("calling screen_display_set_lcd_if_parameters\n");
	ret = screen_display_set_lcd_if_parameters(&set_lcd_if_param);
	if (ret != SMAP_LIB_DISPLAY_OK) {
		pr_err("disp_set_lcd_if_parameters err!\n");
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
		pr_err("disp_set_address err!\n");
		goto out;
	}

	/* Start a display to LCD */
	start_lcd.handle	= screen_handle;
	start_lcd.output_mode	= RT_DISPLAY_LCD1;
	pr_debug("calling screen_display_start_lcd\n");
	ret = screen_display_start_lcd(&start_lcd);
	if (ret != SMAP_LIB_DISPLAY_OK) {
		pr_err("disp_start_lcd err!\n");
		goto out;
	}

retry:
	is_dsi_read_enabled = 1;
	/* Transmit DSI command peculiar to a panel */
	ret = panel_specific_cmdset(screen_handle, initialize_cmdset);
	if (ret != 0) {
		pr_err("panel_specific_cmdset err!\n");
		is_dsi_read_enabled = 0;

		if (retry_count == 0) {
			pr_err("retry count 0!!!!\n");

			mipi_display_power_off();

			disp_stop_lcd.handle		= screen_handle;
			disp_stop_lcd.output_mode	= RT_DISPLAY_LCD1;
			ret = screen_display_stop_lcd(&disp_stop_lcd);
			if (ret != SMAP_LIB_DISPLAY_OK)
				pr_err("display_stop_lcd err!\n");

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
				pr_err("display_stop_lcd err!\n");
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
				pr_err("disp_start_lcd err!\n");
				goto out;
			}

			mipi_display_reset();

			goto retry;
		}
	}

	pr_info("Panel initialized with Command mode\n");
out:
	disp_delete.handle = screen_handle;
	screen_display_delete(&disp_delete);

	initialize_now = false;
	return ret;
}

static int NT35590_panel_suspend(void)
{
	void *screen_handle;
	screen_disp_stop_lcd disp_stop_lcd;
	screen_disp_start_lcd start_lcd;
	screen_disp_delete disp_delete;
	int ret;

#ifdef NT35590_POWAREA_MNG_ENABLE
	void *system_handle;
	system_pmg_param powarea_end_notify;
	system_pmg_delete pmg_delete;
#endif

	pr_info("%s\n", __func__);

	screen_handle =  screen_display_new();

	is_dsi_read_enabled = 0;

	/* Stop a display to LCD */
	disp_stop_lcd.handle		= screen_handle;
	disp_stop_lcd.output_mode	= RT_DISPLAY_LCD1;
	ret = screen_display_stop_lcd(&disp_stop_lcd);
	if (ret != SMAP_LIB_DISPLAY_OK)
		pr_err("display_stop_lcd err!\n");

	start_lcd.handle	= screen_handle;
	start_lcd.output_mode	= RT_DISPLAY_LCD1;
	ret = screen_display_start_lcd(&start_lcd);
	if (ret != SMAP_LIB_DISPLAY_OK)
		pr_err("disp_start_lcd err!\n");

	/* Transmit DSI command peculiar to a panel */
	ret = panel_specific_cmdset(screen_handle, demise_cmdset);
	if (ret != 0) {
		pr_err("panel_specific_cmdset err!\n");
		/* continue */
	}

	/* Stop a display to LCD */
	disp_stop_lcd.handle		= screen_handle;
	disp_stop_lcd.output_mode	= RT_DISPLAY_LCD1;
	ret = screen_display_stop_lcd(&disp_stop_lcd);
	if (ret != SMAP_LIB_DISPLAY_OK)
		pr_err("display_stop_lcd err!\n");

	mipi_display_power_off();

	disp_delete.handle = screen_handle;
	screen_display_delete(&disp_delete);

#ifdef NT35590_POWAREA_MNG_ENABLE
	pr_debug("End A4LC power area\n");
	system_handle = system_pwmng_new();

	/* Notifying the Beginning of Using Power Area */
	powarea_end_notify.handle		= system_handle;
	powarea_end_notify.powerarea_name	= RT_PWMNG_POWERAREA_A4LC;
	ret = system_pwmng_powerarea_end_notify(&powarea_end_notify);
	if (ret != SMAP_LIB_PWMNG_OK)
		pr_err("system_pwmng_powerarea_end_notify err!\n");

	pmg_delete.handle = system_handle;
	system_pwmng_delete(&pmg_delete);
#endif

	return 0;
}

static int NT35590_panel_resume(void)
{
	void *screen_handle;
	screen_disp_start_lcd start_lcd;
	screen_disp_stop_lcd disp_stop_lcd;
	screen_disp_delete disp_delete;
	unsigned char read_data[60];
	int retry_count_dsi;
	int ret = 0;
	int retry_count = NT35590_INIT_RETRY_COUNT;

#ifdef NT35590_POWAREA_MNG_ENABLE
	void *system_handle;
	system_pmg_param powarea_start_notify;
	system_pmg_delete pmg_delete;
#endif

	pr_info("%s\n", __func__);

#ifdef NT35590_POWAREA_MNG_ENABLE
	pr_debug("Start A4LC power area\n");
	system_handle = system_pwmng_new();

	/* Notifying the Beginning of Using Power Area */
	powarea_start_notify.handle		= system_handle;
	powarea_start_notify.powerarea_name	= RT_PWMNG_POWERAREA_A4LC;
	ret = system_pwmng_powerarea_start_notify(&powarea_start_notify);
	if (ret != SMAP_LIB_PWMNG_OK)
		pr_err("system_pwmng_powerarea_start_notify err!\n");
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
		pr_err("disp_start_lcd err!\n");
		goto out;
	}

	is_dsi_read_enabled = 1;

	retry_count_dsi = NT35590_INIT_RETRY_COUNT;
	do {
		ret = panel_dsi_read(MIPI_DSI_DCS_READ, 0x04, 4, &read_data[0]);
		if (ret == 0) {
			pr_debug("read_data(RDID0) = %02X\n", read_data[0]);
			pr_debug("read_data(RDID1) = %02X\n", read_data[1]);
			pr_debug("read_data(RDID2) = %02X\n", read_data[2]);
		}

		retry_count_dsi--;

		if (retry_count_dsi == 0) {
			pr_debug("LCD ID or DSI read problem\n");
			break;
		}
	} while (read_data[0] != 0x00 || read_data[1] != 0x80 ||
			read_data[2] != 0x00);

	/* Transmit DSI command peculiar to a panel */
	ret = panel_specific_cmdset(screen_handle, initialize_cmdset);
	if (ret != 0) {
		pr_err("panel_specific_cmdset err!\n");
		is_dsi_read_enabled = 0;
		if (retry_count == 0) {
			pr_err("retry count 0!!!!\n");

			mipi_display_power_off();

			disp_stop_lcd.handle		= screen_handle;
			disp_stop_lcd.output_mode	= RT_DISPLAY_LCD1;
			ret = screen_display_stop_lcd(&disp_stop_lcd);
			if (ret != SMAP_LIB_DISPLAY_OK)
				pr_err("display_stop_lcd err!\n");

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
				pr_err("display_stop_lcd err!\n");

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

static int NT35590_panel_probe(struct fb_info *info,
		struct fb_panel_hw_info hw_info)
{
	struct platform_device *pdev;
	struct resource *res_irq_port;
	int ret;

	pr_debug("%s\n", __func__);
	reset_gpio = hw_info.gpio_reg;

	/* GPIO control */
	gpio_request(reset_gpio, NULL);
	gpio_direction_output(reset_gpio, 1);

	/* fb parent device info to platform_device */
	pdev = to_platform_device(info->device);

	/* get resource info from platform_device */
	res_irq_port	= platform_get_resource_byname(pdev,
			IORESOURCE_MEM,
			"panel_irq_port");
	if (!res_irq_port) {
		pr_err("panel_irq_port is NULL!!\n");
		return -ENODEV;
	}
	irq_portno = res_irq_port->start;
	power_ldo_3v = regulator_get(NULL, "vlcd_3v");
	power_ldo_1v8 = regulator_get(NULL, "vlcd_1v8");

	if (power_ldo_3v == NULL || power_ldo_1v8 == NULL) {
		pr_err("regulator_get failed\n");
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

	pr_debug("PMIC        : for panel power\n");
	pr_debug("GPIO_PORT%d : for panel reset\n", reset_gpio);
	pr_debug("IRQ%d       : for panel te\n", irq_portno);

	common_fb_info = info;
	is_dsi_read_enabled = 0;

	/* clear internal info */
	memset(&lcd_info_data, 0, sizeof(lcd_info_data));

	/* register sysfs for LCD */
	lcd_info_data.ld = lcd_device_register("panel",
			&pdev->dev,
			&lcd_info_data,
			&NT35590_lcd_ops);
	if (IS_ERR(lcd_info_data.ld))
		return PTR_ERR(lcd_info_data.ld);
	lcd_info_data.power = FB_BLANK_UNBLANK;
	return 0;

	/* unregister sysfs for LCD */
	lcd_device_unregister(lcd_info_data.ld);

	pr_debug("NT35590_panel_probe --\n");
	return ret;
}

static int NT35590_panel_remove(struct fb_info *info)
{
	pr_debug("%s\n", __func__);

	gpio_free(reset_gpio);

	/* unregister sysfs for LCD */
	lcd_device_unregister(lcd_info_data.ld);

	return 0;
}

static struct fb_panel_info NT35590_panel_info(void)
{
	pr_debug("%s\n", __func__);

	r_mobile_info.buff_address = g_fb_start;
	return r_mobile_info;
}

struct fb_panel_func r_mobile_nt35590_panel_func(int panel)
{

	struct fb_panel_func panel_func;

	pr_debug("%s\n", __func__);

	memset(&panel_func, 0, sizeof(struct fb_panel_func));
	if (panel == RT_DISPLAY_LCD1) {
		panel_func.panel_init    = NT35590_panel_init;
		panel_func.panel_suspend = NT35590_panel_suspend;
		panel_func.panel_resume  = NT35590_panel_resume;
		panel_func.panel_probe   = NT35590_panel_probe;
		panel_func.panel_remove  = NT35590_panel_remove;
		panel_func.panel_info    = NT35590_panel_info;
	}

	return panel_func;
}
