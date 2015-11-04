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


#include <linux/memblock.h>
#include <mach/setup-u2fb.h>
#include <mach/ramdump.h>
#include <mach/memory-r8a7373.h>
static phys_addr_t u2mem_reserve(phys_addr_t, unsigned int, phys_addr_t);

/* These two globals should be removed once we start passing the complete
 * information through pdata/DT by consolidating pdata/DT for rtboot and fb.
 * +
 * Since this is used in panel driver, the info has to be passed from fb to
 * panel through a struct and SHOULD be addressed during planned code
 * restructure */
phys_addr_t g_fb_start;
unsigned int g_fb_sz;

int __init setup_u2fb_reserve(struct screen_info *screen)
{
	g_fb_sz = screen->height * screen->stride * BYTES_PER_PIXEL;
	g_fb_sz = PAGE_ALIGN(g_fb_sz) * NO_OF_BUFFERS;
	g_fb_start = u2mem_reserve((phys_addr_t)NULL, g_fb_sz, SZ_128K);
	if (!g_fb_start)
		return -ENOMEM;
	register_ramdump_split("Framebuffer0", g_fb_start,
			g_fb_start + (g_fb_sz / 2) - 1);
	register_ramdump_split("Framebuffer1", g_fb_start + (g_fb_sz / 2),
			g_fb_start + g_fb_sz - 1);
	return 0;
}

static phys_addr_t __init u2mem_reserve(phys_addr_t base, unsigned int size,
		phys_addr_t align)
{
	if (NULL == (void *)base) {
		base = memblock_alloc(size, align);
		if (!base) {
			pr_err("Failed to alloc memory size=0x%x align=0x%x\n",
					size, align);
			goto err_alloc;
		}
		if (memblock_free(base, size) < 0) {
			pr_err("Failed to free memory, base=0x%x size=0x%x\n",
					base, size);
			goto err_free;
		}
	}
	if (memblock_remove(base, size) < 0) {
		pr_err("Failed to reserve memory, base=0x%x size=0x%x\n",
				base, size);
		goto err_remove;
	}
	pr_err("%s: reserve addr=0x%x size=0x%x\n", __func__,
			base, size);
	return base;
err_remove:
err_free:
err_alloc:
	return 0;
}


