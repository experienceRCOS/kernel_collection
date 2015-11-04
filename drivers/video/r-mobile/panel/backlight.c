/* linux/drivers/video/samsung/s6e88a0_mipi_lcd.c
 *
 * Samsung SoC MIPI LCD driver.
 *
 * Copyright (c) 2012 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/ctype.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/backlight.h>
#include <linux/lcd.h>
#include <linux/rtc.h>
#include <linux/reboot.h>
#include <rtapi/screen_display.h>
#include "s6e88a0_param.h"
#include "dynamic_aid_s6e88a0.h"

#define MIN_BRIGHTNESS		0
#define MAX_BRIGHTNESS		255
#define DEFAULT_BRIGHTNESS		143
#define FB_BLANK_NORMAL 1
#define FB_BLANK_UNBLANK 1
#define FB_BLANK_POWERDOWN 1
/*#define EINVAL -1*/
#define POWER_IS_ON(pwr)		(pwr <= FB_BLANK_NORMAL)
#define LEVEL_IS_HBM(level)		(level >= 6)

#define MAX_GAMMA			300
#define DEFAULT_GAMMA_LEVEL		GAMMA_143CD

#define LDI_ID_REG			0x04
#define LDI_ID_LEN			4
#define LDI_MTP_REG			0xC8
#define LDI_MTP_LEN			(GAMMA_PARAM_SIZE - 1)
#define LDI_ELVSS_REG			0xB6
#define LDI_ELVSS_LEN			(ELVSS_PARAM_SIZE - 1)	/* ELVSS + HBM V151 ~ V35 + ELVSS 17th */

#define LDI_HBM_REG			0xB5
#define LDI_HBM_LEN			28	/* HBM V255 + HBM ELVSS + white coordination + manufacture date */

#define MIPI_DSI_SET_MAX_RETURN_PACKET	(0x37)
/*#define SMART_DIMMING_DEBUG*/
#ifdef SMART_DIMMING_DEBUG
#define smtd_dbg(format, arg...)	printk(format, ##arg)
#else
#define smtd_dbg(format, arg...)
#endif

struct lcd_info {
	unsigned int			bl;
	unsigned int			auto_brightness;
	unsigned int			acl_enable;
	unsigned int			siop_enable;
	unsigned int			current_acl;
	unsigned int			current_bl;
	unsigned int			current_elvss;
	unsigned int			ldi_enable;
	unsigned int			power;
	struct mutex			lock;
	struct mutex			bl_lock;

	struct device			*dev;
	struct lcd_device		*ld;
	struct backlight_device		*bd;
	unsigned char			id[LDI_ID_LEN];
	unsigned char			**gamma_table;
	unsigned char			**elvss_table;

	struct dynamic_aid_param_t	daid;
	unsigned char			aor[GAMMA_MAX][ARRAY_SIZE(SEQ_AOR_CONTROL)];
	unsigned int			connected;

	unsigned int			coordinate[2];

};

static const unsigned int candela_table[GAMMA_MAX] = {
	10,	11,	12,	13,	14,	15,	16,	17,
	19,	20,	21,	22,	24,	25,	27,	29,
	30,	32,	34,	37,	39,	41,	44,	47,
	50,	53,	56,	60,	64,	68,	72,	77,
	82,	87,	93,	98,	105,	111,	119,	126,
	134,	143,	152,	162,	172,	183,	195,	207,
	220,	234,	249,	265,	282,	300,	400
};

extern void (*lcd_early_suspend)(void);
extern void (*lcd_late_resume)(void);


int lcd_cmd_write(unsigned int data_id, const unsigned char *data0, unsigned int data_size)
{
	return 0;
}

int lcd_cmd_read(unsigned int data_id, unsigned int data0, unsigned int req_size, u8 *rx_buf)
{
	return 0;
}
#define DCS_LONG_WR		(0x39)
#define DCS_WR_1_PARA	(0x15)
#define DCS_WR_NO_PARA	(0x05)
#define MIPI_DSI_DCS_READ		(0x06)
static int _s6e88a0_write(struct lcd_info *lcd,  unsigned char *seq, int len)
{
	int size;
	unsigned char *wbuf;
	int ret = 0;
	screen_disp_delete disp_delete;
	screen_disp_write_dsi_long  write_dsi_l;
	screen_disp_write_dsi_short write_dsi_s;	
	void *screen_handle;
	if (!lcd->connected)
		return 0;
	
	mutex_lock(&lcd->lock);

	size = len;
	wbuf = seq;
	screen_handle = screen_display_new();
	

	if (size == 1)
	{
			printk(KERN_INFO "panel_cmdset SHORT Write\n");
			write_dsi_s.handle         = screen_handle;
			write_dsi_s.output_mode    = RT_DISPLAY_LCD1;
			write_dsi_s.data_id        = DCS_WR_NO_PARA;
			write_dsi_s.reg_address    = wbuf[0];
			write_dsi_s.write_data     = 0x00;
			write_dsi_s.reception_mode = RT_DISPLAY_RECEPTION_ON;
			ret = screen_display_write_dsi_short_packet(
					&write_dsi_s);
			if (ret != SMAP_LIB_DISPLAY_OK) {
				printk(KERN_ALERT
				       "disp_write_dsi_short err %d!\n", ret);
				return -1;
			}
//		ret = lcd_cmd_write(DCS_WR_NO_PARA, wbuf[0], 0);
	}
	else if (size == 2)
	{
			printk(KERN_INFO
			       "panel_cmdset SHORT Write with param\n");
			write_dsi_s.handle         = screen_handle;
			write_dsi_s.output_mode    = RT_DISPLAY_LCD1;
			write_dsi_s.data_id        = DCS_WR_1_PARA;
			write_dsi_s.reg_address    = wbuf[0];
			write_dsi_s.write_data     = wbuf[1];
			write_dsi_s.reception_mode = RT_DISPLAY_RECEPTION_ON;
			ret = screen_display_write_dsi_short_packet(
					&write_dsi_s);
			if (ret != SMAP_LIB_DISPLAY_OK) {
				printk(KERN_ALERT
				       "disp_write_dsi_short err %d!\n", ret);
				return -1;
			}
	
//		ret = lcd_cmd_write(DCS_WR_1_PARA, wbuf[0], wbuf[1]);
		}
	else
	{
			printk(KERN_INFO "panel_cmdset LONG Write\n");
			write_dsi_l.handle         = screen_handle;
			write_dsi_l.output_mode    = RT_DISPLAY_LCD1;
			write_dsi_l.data_id        = DCS_LONG_WR;
			write_dsi_l.data_count     = size;
			write_dsi_l.write_data     = wbuf;
			write_dsi_l.reception_mode	= RT_DISPLAY_RECEPTION_ON;
			write_dsi_l.send_mode = RT_DISPLAY_SEND_MODE_LP;
			ret = screen_display_write_dsi_long_packet(
					&write_dsi_l);
			if (ret != SMAP_LIB_DISPLAY_OK) {
				printk(KERN_ALERT
				       "display_write_dsi_long err %d!\n", ret);
				return -1;
			}	
//		ret = lcd_cmd_write(DCS_LONG_WR, (unsigned int)wbuf, size);
		}

	disp_delete.handle = screen_handle;
	screen_display_delete(&disp_delete);	
	mutex_unlock(&lcd->lock);

	return ret;
}

static int s6e88a0_write(struct lcd_info *lcd,  unsigned char *seq, int len)
{
	int ret = 0;
	int retry_cnt = 1;

retry:
	ret = _s6e88a0_write(lcd, seq, len);
	if (ret) {
		if (retry_cnt) {
			printk( "%s: retry: %d\n", __func__, retry_cnt);
			retry_cnt--;
			goto retry;
		} else
			printk( "%s: 0x%02x\n", __func__, seq[1]);
	}

	return ret;
}

static int _s6e88a0_read(struct lcd_info *lcd, const u8 addr, u16 count, u8 *buf)
{
	int ret = 0;
	void *screen_handle;
	screen_disp_read_dsi_short read_dsi_s;
	screen_disp_write_dsi_short write_dsi_s;	
	screen_disp_delete disp_delete;
	if (!lcd->connected)
		return ret;

	
	screen_handle =  screen_display_new();

	mutex_lock(&lcd->lock);
    	printk(KERN_INFO "%s\n", __func__ );
	//if (lcd_cmd_read)
	{
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
		printk( "disp_write_dsi_short err!\n");
		}
	}
	
	read_dsi_s.handle		= screen_handle;
	read_dsi_s.output_mode		= RT_DISPLAY_LCD1;
	read_dsi_s.data_id		= MIPI_DSI_DCS_READ;
	read_dsi_s.reg_address		= addr;
	read_dsi_s.write_data		= 0;
	read_dsi_s.data_count		= count;
	read_dsi_s.read_data		= &buf[0];
	/*printk(" values are %0x %0x %0x \n",read_dsi_s.data_id,read_dsi_s.reg_address,read_dsi_s.data_count);*/
	ret = screen_display_read_dsi_short_packet(&read_dsi_s);
	   	printk(KERN_INFO "\n screen_display_read_dsi_short_packet %d\n",ret );

	if (ret != SMAP_LIB_DISPLAY_OK)
		printk( "disp_dsi_read err! ret = %d\n", ret);
		
//		ret = lcd_cmd_read(addr, count, buf);
		

	mutex_unlock(&lcd->lock);
	disp_delete.handle = screen_handle;
	screen_display_delete(&disp_delete);
	return ret;
}

static int s6e88a0_read(struct lcd_info *lcd, const u8 addr, u8 *buf, u16 count, u8 retry_cnt)
{
	int ret = 0;

retry:
	ret = _s6e88a0_read(lcd, addr, count, buf);
	if (ret) {
		if (retry_cnt) {
			printk( "%s: retry: %d\n", __func__, retry_cnt);
			retry_cnt--;
			goto retry;
		} else
			printk( "%s: 0x%02x\n", __func__, addr);
	}

	return ret;
}

static void s6e88a0_read_id(struct lcd_info *lcd, u8 *buf)
{
	int ret = 0;
	printk(KERN_INFO "s6e88a0_read_id \n" );

	ret = s6e88a0_read(lcd, LDI_ID_REG, buf, LDI_ID_LEN, 1);
	printk(KERN_INFO "\ns6e88a0_read %d \n",*buf );

								
	if (ret < 0) {
		lcd->connected = 0;
		printk( "panel is not connected well\n");
	}
}

static int s6e88a0_read_mtp(struct lcd_info *lcd, u8 *buf)
{
	int ret, i;

	ret = s6e88a0_read(lcd, LDI_MTP_REG, buf, LDI_MTP_LEN, 1);

	smtd_dbg("s6e88a0_read_mtp %02xh %d \n", LDI_MTP_REG,LDI_MTP_LEN);
	for (i = 0; i < LDI_MTP_LEN; i++)
		//smtd_dbg("%02dth value is %02x\n", i+1, (int)buf[i]);
		smtd_dbg("0x%02x, ",  (int)buf[i]);
	printk("\n");

	return ret;
}

static int s6e88a0_read_elvss(struct lcd_info *lcd, u8 *buf)
{
	int ret, i;

	ret = s6e88a0_read(lcd, LDI_ELVSS_REG, buf, LDI_ELVSS_LEN, 1);

	smtd_dbg("s6e88a0_read_elvss %02xh %d\n", LDI_ELVSS_REG,LDI_ELVSS_LEN);
	for (i = 0; i < LDI_ELVSS_LEN; i++)
		//smtd_dbg("%02dth value is %02x\n", i+1, (int)buf[i]);
		smtd_dbg("0x%02x, ", (int)buf[i]);
	printk("\n");

	return ret;
}

static void s6e88a0_read_coordinate(struct lcd_info *lcd, u8 *buf)
{
	int ret = 0, i;

	ret = s6e88a0_read(lcd, LDI_HBM_REG, buf, LDI_HBM_LEN, 1);

	if (ret < 0)
		dev_err(&lcd->ld->dev, "%s failed\n", __func__);

	lcd->coordinate[0] = buf[19] << 8 | buf[20];	/* X */
	lcd->coordinate[1] = buf[21] << 8 | buf[22];	/* Y */

	smtd_dbg("s6e88a0_read_coordinate %02xh %d\n", LDI_HBM_REG,LDI_HBM_LEN);
	for (i = 0; i < LDI_HBM_LEN; i++)
		//smtd_dbg("%02dth value is %02x\n", i+1, (int)buf[i]);
		smtd_dbg("0x%02x, ",(int)buf[i]);
	printk("\n");

}

static int get_backlight_level_from_brightness(int brightness)
{
	int backlightlevel;

	switch (brightness) {
	case 0 ... 10:
		backlightlevel = GAMMA_10CD;
		break;
	case 11:
		backlightlevel = GAMMA_11CD;
		break;
	case 12:
		backlightlevel = GAMMA_12CD;
		break;
	case 13:
		backlightlevel = GAMMA_13CD;
		break;
	case 14:
		backlightlevel = GAMMA_14CD;
		break;
	case 15:
		backlightlevel = GAMMA_15CD;
		break;
	case 16:
		backlightlevel = GAMMA_16CD;
		break;
	case 17 ... 18:
		backlightlevel = GAMMA_17CD;
		break;
	case 19:
		backlightlevel = GAMMA_19CD;
		break;
	case 20:
		backlightlevel = GAMMA_20CD;
		break;
	case 21:
		backlightlevel = GAMMA_21CD;
		break;
	case 22 ... 23:
		backlightlevel = GAMMA_22CD;
		break;
	case 24:
		backlightlevel = GAMMA_24CD;
		break;
	case 25 ... 26:
		backlightlevel = GAMMA_25CD;
		break;
	case 27 ... 28:
		backlightlevel = GAMMA_27CD;
		break;
	case 29:
		backlightlevel = GAMMA_29CD;
		break;
	case 30 ... 31:
		backlightlevel = GAMMA_30CD;
		break;
	case 32 ... 33:
		backlightlevel = GAMMA_32CD;
		break;
	case 34 ... 36:
		backlightlevel = GAMMA_34CD;
		break;
	case 37 ... 38:
		backlightlevel = GAMMA_37CD;
		break;
	case 39 ... 40:
		backlightlevel = GAMMA_39CD;
		break;
	case 41 ... 43:
		backlightlevel = GAMMA_41CD;
		break;
	case 44 ... 46:
		backlightlevel = GAMMA_44CD;
		break;
	case 47 ... 49:
		backlightlevel = GAMMA_47CD;
		break;
	case 50 ... 52:
		backlightlevel = GAMMA_50CD;
		break;
	case 53 ... 55:
		backlightlevel = GAMMA_53CD;
		break;
	case 56 ... 59:
		backlightlevel = GAMMA_56CD;
		break;
	case 60 ... 63:
		backlightlevel = GAMMA_60CD;
		break;
	case 64 ... 67:
		backlightlevel = GAMMA_64CD;
		break;
	case 68 ... 71:
		backlightlevel = GAMMA_68CD;
		break;
	case 72 ... 76:
		backlightlevel = GAMMA_72CD;
		break;
	case 77 ... 81:
		backlightlevel = GAMMA_77CD;
		break;
	case 82 ... 86:
		backlightlevel = GAMMA_82CD;
		break;
	case 87 ... 92:
		backlightlevel = GAMMA_87CD;
		break;
	case 93 ... 97:
		backlightlevel = GAMMA_93CD;
		break;
	case 98 ... 104:
		backlightlevel = GAMMA_98CD;
		break;
	case 105 ... 110:
		backlightlevel = GAMMA_105CD;
		break;
	case 111 ... 118:
		backlightlevel = GAMMA_111CD;
		break;
	case 119 ... 125:
		backlightlevel = GAMMA_119CD;
		break;
	case 126 ... 133:
		backlightlevel = GAMMA_126CD;
		break;
	case 134 ... 142:
		backlightlevel = GAMMA_134CD;
		break;
	case 143 ... 151:
		backlightlevel = GAMMA_143CD;
		break;
	case 152 ... 161:
		backlightlevel = GAMMA_152CD;
		break;
	case 162 ... 171:
		backlightlevel = GAMMA_162CD;
		break;
	case 172 ... 182:
		backlightlevel = GAMMA_172CD;
		break;
	case 183 ... 194:
		backlightlevel = GAMMA_183CD;
		break;
	case 195 ... 206:
		backlightlevel = GAMMA_195CD;
		break;
	case 207 ... 219:
		backlightlevel = GAMMA_207CD;
		break;
	case 220 ... 233:
		backlightlevel = GAMMA_220CD;
		break;
	case 234 ... 248:
		backlightlevel = GAMMA_234CD;
		break;
	case 249:
		backlightlevel = GAMMA_249CD;
		break;
	case 250 ... 251:
		backlightlevel = GAMMA_265CD;
		break;
	case 252 ... 253:
		backlightlevel = GAMMA_282CD;
		break;
	case 254 ... 255:
		backlightlevel = GAMMA_300CD;
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
	char *temp=lcd->gamma_table[lcd->bl];
	for(i=0;i< GAMMA_PARAM_SIZE; i++)
	{
		smtd_dbg("0x%0x, ",*temp++);
	}
#endif
	s6e88a0_write(lcd, lcd->gamma_table[lcd->bl], GAMMA_PARAM_SIZE);

	return 0;
}

static int s6e88a0_aid_parameter_ctl(struct lcd_info *lcd, u8 force)
{
	if (force)
		goto aid_update;
	else if (lcd->aor[lcd->bl][0x04] !=  lcd->aor[lcd->current_bl][0x04])
		goto aid_update;
	else if (lcd->aor[lcd->bl][0x05] !=  lcd->aor[lcd->current_bl][0x05])
		goto aid_update;
	else
		goto exit;

aid_update:
	s6e88a0_write(lcd, lcd->aor[lcd->bl], AID_PARAM_SIZE);

exit:
	s6e88a0_write(lcd, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	return 0;
}

static int s6e88a0_set_acl(struct lcd_info *lcd, u8 force)
{
	int ret = 0, level;
	char *temp;

	level = ACL_STATUS_40P;
    
	if (lcd->siop_enable || LEVEL_IS_HBM(lcd->auto_brightness))
		goto acl_update;

	if (!lcd->acl_enable)
		level = ACL_STATUS_0P;

acl_update:
	temp = (unsigned char *)ACL_CUTOFF_TABLE[level];
	if (force || lcd->current_acl != ACL_CUTOFF_TABLE[level][1]) {
		ret = s6e88a0_write(lcd, temp, ACL_PARAM_SIZE);
		lcd->current_acl = ACL_CUTOFF_TABLE[level][1];
		dev_info(&lcd->ld->dev, "acl: %d, auto_brightness: %d\n", lcd->current_acl, lcd->auto_brightness);
	}

	if (!ret)
		ret = -EPERM;

	return ret;
}

static int s6e88a0_set_elvss(struct lcd_info *lcd, u8 force)
{
	int ret = 0, elvss_level = 0;
	u32 candela = candela_table[lcd->bl];

	switch (candela) {
	case 0 ... 105:
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
	case 283 ... 299:
		elvss_level = ELVSS_STATUS_300;
		break;
	case 400:
		elvss_level = ELVSS_STATUS_HBM;
		break;
	default:
		elvss_level = ELVSS_STATUS_300;
		break;
	}

	if (force || lcd->current_elvss != elvss_level) {
		ret = s6e88a0_write(lcd, lcd->elvss_table[elvss_level], ELVSS_PARAM_SIZE);
		lcd->current_elvss = elvss_level;
	}

	if (!ret) {
		ret = -EPERM;
		goto elvss_err;
	}

elvss_err:
	return ret;
}

static void init_dynamic_aid(struct lcd_info *lcd)
{
	lcd->daid.vreg = VREG_OUT_X1000;
	lcd->daid.iv_tbl = index_voltage_table;
	lcd->daid.iv_max = IV_MAX;
	lcd->daid.mtp = kzalloc(IV_MAX * CI_MAX * sizeof(int), GFP_KERNEL);
	lcd->daid.gamma_default = gamma_default;
	lcd->daid.formular = gamma_formula;
	lcd->daid.vt_voltage_value = vt_voltage_value;

	lcd->daid.ibr_tbl = index_brightness_table;
	lcd->daid.ibr_max = IBRIGHTNESS_MAX;
	lcd->daid.br_base = brightness_base_table;
	lcd->daid.gc_tbls = gamma_curve_tables;
	lcd->daid.gc_lut = gamma_curve_lut;
	lcd->daid.offset_gra = offset_gradation;
	lcd->daid.offset_color = (const struct rgb_t(*)[])offset_color;
}

static void init_mtp_data(struct lcd_info *lcd, const u8 *mtp_data)
{
	int i, c, j;
	int *mtp;

	mtp = lcd->daid.mtp;

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
	for (i = 0, j = 0; i <= IV_MAX; i++)
		for (c = 0; c < CI_MAX; c++, j++)
			smtd_dbg("%d, ",mtp_data[j]);

	for (i = 0, j = 0; i <= IV_MAX; i++)
		for (c = 0; c < CI_MAX; c++, j++)
			smtd_dbg("%d, ",mtp[j]);

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
	lcd->gamma_table = kzalloc(GAMMA_MAX * sizeof(u8 *), GFP_KERNEL);
	if (!lcd->gamma_table) {
		pr_err("failed to allocate gamma table 2\n");
		ret = -ENOMEM;
		goto err_alloc_gamma_table2;
	}

	for (i = 0; i < GAMMA_MAX; i++) {
		lcd->gamma_table[i] = kzalloc(GAMMA_PARAM_SIZE * sizeof(u8), GFP_KERNEL);
		if (!lcd->gamma_table[i]) {
			pr_err("failed to allocate gamma 2\n");
			ret = -ENOMEM;
			goto err_alloc_gamma2;
		}
		lcd->gamma_table[i][0] = 0xCA;
	}

	/* calculate gamma table */
	init_mtp_data(lcd, mtp_data);
	dynamic_aid(lcd->daid, gamma_table);

	/* relocate gamma order */
	for (i = 0; i < GAMMA_MAX; i++) {
		/* Brightness table */
		v = IV_MAX - 1;
		pgamma = &gamma_table[i][v * CI_MAX];
		for (c = 0, j = 1; c < CI_MAX; c++, pgamma++) {
			if (*pgamma & 0x100)
				lcd->gamma_table[i][j++] = 1;
			else
				lcd->gamma_table[i][j++] = 0;

			lcd->gamma_table[i][j++] = *pgamma & 0xff;
		}

		for (v = IV_MAX - 2; v >= 0; v--) {
			pgamma = &gamma_table[i][v * CI_MAX];
			for (c = 0; c < CI_MAX; c++, pgamma++)
				lcd->gamma_table[i][j++] = *pgamma;
		}

		smtd_dbg("candela_table %03d: ", candela_table[i]);
		for (v = 0; v < GAMMA_PARAM_SIZE; v++)
			smtd_dbg("%03d ", lcd->gamma_table[i][v]);
		smtd_dbg("\n");
	}
	/* free local gamma table */
	for (i = 0; i < IBRIGHTNESS_MAX; i++)
		kfree(gamma_table[i]);
	kfree(gamma_table);

	return 0;

err_alloc_gamma2:
	while (i > 0) {
		kfree(lcd->gamma_table[i-1]);
		i--;
	}
	kfree(lcd->gamma_table);
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
	int i, j;

	for (i = 0; i < GAMMA_MAX; i++) {
		memcpy(lcd->aor[i], SEQ_AOR_CONTROL, ARRAY_SIZE(SEQ_AOR_CONTROL));
		lcd->aor[i][4] = aor_cmd[i][0];
		lcd->aor[i][5] = aor_cmd[i][1];

		for (j = 0; j < ARRAY_SIZE(SEQ_AOR_CONTROL); j++)
			smtd_dbg("%02X ", lcd->aor[i][j]);
		smtd_dbg("\n");
	}

	return 0;
}

static int init_elvss_table(struct lcd_info *lcd, u8 *elvss_data)
{
	int i, j, ret;

	lcd->elvss_table = kzalloc(ELVSS_STATUS_MAX * sizeof(u8 *), GFP_KERNEL);

	if (IS_ERR_OR_NULL(lcd->elvss_table)) {
		pr_err("failed to allocate elvss table\n");
		ret = -ENOMEM;
		goto err_alloc_elvss_table;
	}

	for (i = 0; i < ELVSS_STATUS_MAX; i++) {
		lcd->elvss_table[i] = kzalloc(ELVSS_PARAM_SIZE * sizeof(u8), GFP_KERNEL);
		if (IS_ERR_OR_NULL(lcd->elvss_table[i])) {
			pr_err("failed to allocate elvss\n");
			ret = -ENOMEM;
			goto err_alloc_elvss;
		}

		for (j = 0; j < LDI_ELVSS_LEN; j++)
			lcd->elvss_table[i][j+1] = elvss_data[j];

		lcd->elvss_table[i][0] = 0xB6;
		lcd->elvss_table[i][1] = 0x28;
		lcd->elvss_table[i][2] = ELVSS_TABLE[i];
	}

	return 0;

err_alloc_elvss:
	/* should be kfree elvss with k */
	while (i > 0) {
		kfree(lcd->elvss_table[i-1]);
		i--;
	}
	kfree(lcd->elvss_table);
err_alloc_elvss_table:
	return ret;
}

static int init_hbm_parameter(struct lcd_info *lcd, u8 *elvss_data, u8 *hbm_data)
{
	

	/* CA 1~6 = B5 13~18 */
	lcd->gamma_table[GAMMA_HBM][1] = hbm_data[12];
	lcd->gamma_table[GAMMA_HBM][2] = hbm_data[13];
	lcd->gamma_table[GAMMA_HBM][3] = hbm_data[14];
	lcd->gamma_table[GAMMA_HBM][4] = hbm_data[15];
	lcd->gamma_table[GAMMA_HBM][5] = hbm_data[16];
	lcd->gamma_table[GAMMA_HBM][6] = hbm_data[17];

	/* CA 7~9 = B5 26~28 */
	lcd->gamma_table[GAMMA_HBM][7] = hbm_data[25];
	lcd->gamma_table[GAMMA_HBM][8] = hbm_data[26];
	lcd->gamma_table[GAMMA_HBM][9] = hbm_data[27];

	/* CA 10~21 = B6 3~14 */
	lcd->gamma_table[GAMMA_HBM][10] = elvss_data[2];
	lcd->gamma_table[GAMMA_HBM][11] = elvss_data[3];
	lcd->gamma_table[GAMMA_HBM][12] = elvss_data[4];
	lcd->gamma_table[GAMMA_HBM][13] = elvss_data[5];
	lcd->gamma_table[GAMMA_HBM][14] = elvss_data[6];
	lcd->gamma_table[GAMMA_HBM][15] = elvss_data[7];
	lcd->gamma_table[GAMMA_HBM][16] = elvss_data[8];
	lcd->gamma_table[GAMMA_HBM][17] = elvss_data[9];
	lcd->gamma_table[GAMMA_HBM][18] = elvss_data[10];
	lcd->gamma_table[GAMMA_HBM][19] = elvss_data[11];
	lcd->gamma_table[GAMMA_HBM][20] = elvss_data[12];
	lcd->gamma_table[GAMMA_HBM][21] = elvss_data[13];

	/* B6 17th = B5 19th */
	lcd->elvss_table[ELVSS_STATUS_HBM][17] = hbm_data[18];

	return 0;
}

static int update_brightness(struct lcd_info *lcd, u8 force)
{

	mutex_lock(&lcd->bl_lock);


	if ((force) || ((lcd->ldi_enable) && (lcd->current_bl != lcd->bl))) {
		s6e88a0_gamma_ctl(lcd);
		s6e88a0_aid_parameter_ctl(lcd, force);
		s6e88a0_set_acl(lcd, force);
		s6e88a0_set_elvss(lcd, force);

		lcd->current_bl = lcd->bl;

	}

	mutex_unlock(&lcd->bl_lock);

	return 0;
}
static struct lcd_info *g_lcd;
int s6e88a0_set_brightness(int brightness)
{
	int ret = 0;
	struct lcd_info *lcd = g_lcd;
	u32 max_brightness;
	max_brightness = 255;
	/* dev_info(&lcd->ld->dev, "%s: brightness=%d\n", __func__, bd->props.brightness); */
	if(g_lcd == NULL)
	{
		return 0;
		}
	if(lcd->connected)
	{
		lcd->bl = get_backlight_level_from_brightness(brightness);
		if (LEVEL_IS_HBM(lcd->auto_brightness) && (brightness == max_brightness))
			lcd->bl = GAMMA_HBM;


		if (lcd->ldi_enable) {
			ret = update_brightness(lcd, 0);
			if (ret < 0) {
				return -EINVAL;
			}
		}
	}

	return ret;
}

static int s6e88a0_ldi_init(struct lcd_info *lcd)
{
	int ret = 0;

	msleep(5);

	s6e88a0_write(lcd, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	s6e88a0_write(lcd, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));

	s6e88a0_write(lcd, SEQ_SRC_LATCH, ARRAY_SIZE(SEQ_SRC_LATCH));
	s6e88a0_write(lcd, SEQ_AVDD, ARRAY_SIZE(SEQ_AVDD));
	s6e88a0_write(lcd, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));

	msleep(20);

	update_brightness(lcd, 1);

	if (lcd->id[2] == 0x03)
		s6e88a0_write(lcd, SEQ_PANEL_CONDITION_SET_03, ARRAY_SIZE(SEQ_PANEL_CONDITION_SET_03));
	else
		s6e88a0_write(lcd, SEQ_PANEL_CONDITION_SET, ARRAY_SIZE(SEQ_PANEL_CONDITION_SET));

	msleep(120);

	s6e88a0_write(lcd, SEQ_AVC, ARRAY_SIZE(SEQ_AVC));
	s6e88a0_write(lcd, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	s6e88a0_write(lcd, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));

	return ret;
}




int brt_init(void);
int brt_init(void)
{
	int ret = 0, i, j;
	struct lcd_info *lcd;
	u8 mtp_data[LDI_MTP_LEN] = {0,};
	u8 elvss_data[LDI_ELVSS_LEN] = {0,};
	u8 hbm_data[LDI_HBM_LEN] = {0,};
	
	printk(KERN_INFO "%s\n", __func__);
	
	lcd = kzalloc(sizeof(struct lcd_info), GFP_KERNEL);
	
	if (!lcd) {
		pr_err("failed to allocate for lcd\n");
		ret = -ENOMEM;

	}

	g_lcd = lcd;



	lcd->bl = DEFAULT_GAMMA_LEVEL;
	lcd->current_bl = lcd->bl;
	lcd->acl_enable = 0;
	lcd->current_acl = 0;
	lcd->power = FB_BLANK_UNBLANK;
	lcd->ldi_enable = 1;
	lcd->auto_brightness = 0;
	lcd->connected = 1;
	lcd->siop_enable = 0;


	mutex_init(&lcd->lock);
	mutex_init(&lcd->bl_lock);

	s6e88a0_read_id(lcd, lcd->id);
	
	if (lcd->connected)
		msleep(20);
	if (1)
		{
	s6e88a0_read_mtp(lcd, mtp_data);
	s6e88a0_read_elvss(lcd, elvss_data);
	s6e88a0_read_coordinate(lcd, hbm_data);

	init_dynamic_aid(lcd);
	ret = init_gamma_table(lcd, mtp_data);
	ret += init_aid_dimming_table(lcd, mtp_data);
	ret += init_elvss_table(lcd, elvss_data);
	ret += init_hbm_parameter(lcd, elvss_data, hbm_data);


	for (i = 0; i < GAMMA_MAX; i++) {
		smtd_dbg("%03d: ", candela_table[i]);
		for (j = 0; j < GAMMA_PARAM_SIZE; j++)
			smtd_dbg("%02X, ", lcd->gamma_table[i][j]);
		smtd_dbg("\n");
	}

	for (i = 0; i < ELVSS_STATUS_MAX; i++) {
		for (j = 0; j < ELVSS_PARAM_SIZE; j++)
			smtd_dbg("%02X, ", lcd->elvss_table[i][j]);
		smtd_dbg("\n");
		
	}
	if(0)
			s6e88a0_ldi_init(lcd);	
	}
	return 0;
}
