/******************************************************************************/
/*                                                                            */
/*  Copyright 2014  Broadcom Corporation                                      */
/*                                                                            */
/*     Unless you and Broadcom execute a separate written software license    */
/*     agreement governing use of this software, this software is licensed to */
/*     you under the terms of the GNU General Public License version 2        */
/*     (the GPL), available at                                                */
/*                                                                            */
/*          http://www.broadcom.com/licenses/GPLv2.php                        */
/*                                                                            */
/*     with the following added to such license:                              */
/*                                                                            */
/*     As a special exception, the copyright holders of this software give you*/
/*     permission to link this software with independent modules, and to copy */
/*     and distribute the resulting executable under terms of your choice,    */
/*     provided that you also meet, for each linked independent module, the   */
/*     terms and conditions of the license of that module. An independent     */
/*     module is a module which is not derived from this software.            */
/*     The special exception does not apply to any modifications of the       */
/*     software.                                                              */
/*                                                                            */
/*     Notwithstanding the above, under no circumstances may you combine this */
/*     software in any way with any other Broadcom software provided under a  */
/*     license other than the GPL, without Broadcom's express prior written   */
/*     consent.                                                               */
/*                                                                            */
/******************************************************************************/


#ifndef __SETUP_U2FB_H
#define __SETUP_U2FB_H

#include <linux/platform_data/rt_boot_pdata.h>

int u2fb_reserve(void);
#ifdef CONFIG_FB_SH_MOBILE_LCDC
int setup_u2fb_reserve(struct screen_info *);
#else
#define setup_u2fb_reserve(a) 0
#endif

/* These parameters should be obtained from DT/pdata. Since there exists no such
 * means by which board gets these info, this should be addressed once the info
 * is passed from pdata/DT. Currently panel dictates all params */
#define BYTES_PER_PIXEL 4
#define NO_OF_BUFFERS 2

#endif
