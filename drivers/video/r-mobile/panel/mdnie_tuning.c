/* linux/drivers/video/r-mobile/panel/mdnie_tunning.c
 *
 * Register interface file for Samsung mDNIe driver
 *
 * Copyright (c) 2011 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/ctype.h>
#include <linux/uaccess.h>

#ifdef __KERNEL__
#include <linux/mutex.h>
#include <linux/fb.h>
#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#include <linux/earlysuspend.h>
#endif
#endif

#include "mdnie.h"
#include "mdnie_table.h"

static unsigned char mdnie_data[200];
extern int mdnie_send_sequence(struct mdnie_info *mdnie,
	const struct specific_cmdset *cmdset);

static const struct specific_cmdset tuning_cmdset[] = {
	{ MIPI_DSI_DCS_LONG_WRITE,  mdnie_data,
					sizeof(mdnie_data) },
	{ MIPI_DSI_END,             NULL,      0                }
};

static int parse_text(char *src, int len)
{
	int i, count, ret;
	int index = 0;
	char *str_line[200];
	char *sstart;
	char *c;
	unsigned int data1;
	int total=1;

	pr_info("parse_text START\n");
	c = src;
	count = 0;
	sstart = c;

	for (i = 0; i < len; i++, c++) {
		char a = *c;
		if (a == '\r' || a == '\n') {
			if (c > sstart) {
				str_line[count] = sstart;
				count++;
			}
			else if(a == '\r'){
				if (c > sstart) {
					str_line[count] = sstart;
					//count++;
				}

				for (i = total; i < count; i++) {
						ret = sscanf(str_line[i], "0x%x\n", &data1);
					mdnie_data[index++] = (unsigned char)data1;
					pr_info("mdnie_data[%d] : %x\n", index-1, mdnie_data[index-1]);
					total++;
				}
				mdnie_data[index++] = END_SEQ;				
				mdnie_send_sequence(g_mdnie, tuning_cmdset);				
				index=0;
			}
			*c = '\0';
			sstart = c+1;
		}
		
	}

	if (c > sstart) {
		str_line[count] = sstart;
		//count++;
	}

	for (i = total; i < (count-1); i++) {
			ret = sscanf(str_line[i], "0x%x\n", &data1);
		mdnie_data[index++] = (unsigned char)data1;
		pr_info("mdnie_data[%d] : %x\n", index-1, mdnie_data[index-1]);
	}

	return index;
}


int mdnie_txtbuf_to_parsing(char const *pfilepath)
{
	struct file *filp;
	char	*dp;
	long	l;
	loff_t  pos;
	int     ret, num;
	mm_segment_t fs;

	fs = get_fs();
	set_fs(get_ds());

	pr_info("%s:", pfilepath);

	if (!pfilepath) {
		pr_err(KERN_ERR "Error : mdnie_txtbuf_to_parsing has invalid filepath.\n");
		goto parse_err;
	}

	filp = filp_open(pfilepath, O_RDONLY, 0);

	if (IS_ERR(filp)) {
		pr_err("file open error:%d\n", (s32)filp);
		goto parse_err;
	}

	l = filp->f_path.dentry->d_inode->i_size;
	dp = kmalloc(l+10, GFP_KERNEL);
	if (dp == NULL) {
		pr_info("Out of Memory!\n");
		filp_close(filp, current->files);
		goto parse_err;
	}
	pos = 0;
	memset(dp, 0, l);
	ret = vfs_read(filp, (char __user *)dp, l, &pos);

	if (ret != l) {
		pr_info("<CMC623> Failed to read file (ret = %d)\n", ret);
		kfree(dp);
		filp_close(filp, current->files);
		goto parse_err;
	}

	filp_close(filp, current->files);
	set_fs(fs);
	num = parse_text(dp, l);
	if (!num) {
		pr_err("Nothing to parse!\n");
		kfree(dp);
		goto parse_text_err;
	}

	mdnie_data[num] = END_SEQ;

/*
	mdnie_send_sequence(g_mdnie, mdnie_data);
*/
	mdnie_send_sequence(g_mdnie, tuning_cmdset);
	kfree(dp);

	num = num / 2;
	return num;

parse_err:
	set_fs(fs);
parse_text_err:
	return -EPERM;
}
