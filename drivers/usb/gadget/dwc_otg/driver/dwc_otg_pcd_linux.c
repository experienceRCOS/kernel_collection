 /* ==========================================================================
  * $File: //dwh/usb_iip/dev/software/otg/linux/drivers/dwc_otg_pcd_linux.c $
  * $Revision: #13 $
  * $Date: 2010/03/09 $
  * $Change: 1458980 $
  *
  * Synopsys HS OTG Linux Software Driver and documentation (hereinafter,
  * "Software") is an Unsupported proprietary work of Synopsys, Inc. unless
  * otherwise expressly agreed to in writing between Synopsys and you.
  *
  * The Software IS NOT an item of Licensed Software or Licensed Product under
  * any End User Software License Agreement or Agreement for Licensed Product
  * with Synopsys or any supplement thereto. You are permitted to use and
  * redistribute this Software in source and binary forms, with or without
  * modification, provided that redistributions of source code must retain this
  * notice. You may not view, use, disclose, copy or distribute this file or
  * any information contained herein except pursuant to this license grant from
  * Synopsys. If you do not agree with this notice, including the disclaimer
  * below, then you are not authorized to use the Software.
  *
  * THIS SOFTWARE IS BEING DISTRIBUTED BY SYNOPSYS SOLELY ON AN "AS IS" BASIS
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  * ARE HEREBY DISCLAIMED. IN NO EVENT SHALL SYNOPSYS BE LIABLE FOR ANY DIRECT,
  * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
  * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
  * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
  * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
  * DAMAGE.
  * =================================== */
#ifndef DWC_HOST_ONLY

/** @file
 * This file implements the Peripheral Controller Driver.
 *
 * The Peripheral Controller Driver (PCD) is responsible for
 * translating requests from the Function Driver into the appropriate
 * actions on the DWC_otg controller. It isolates the Function Driver
 * from the specifics of the controller by providing an API to the
 * Function Driver.
 *
 * The Peripheral Controller Driver for Linux will implement the
 * Gadget API, so that the existing Gadget drivers can be used.
 * (Gadget Driver is the Linux terminology for a Function Driver.)
 *
 * The Linux Gadget API is defined in the header file
 * <code><linux/usb_gadget.h></code>.  The USB EP operations API is
 * defined in the structure <code>usb_ep_ops</code> and the USB
 * Controller API is defined in the structure
 * <code>usb_gadget_ops</code>.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/string.h>
#include <linux/dma-mapping.h>
#include <linux/version.h>

#ifdef LM_INTERFACE
#include <mach/lm.h>
#include <mach/irqs.h>
#else
#include <linux/platform_device.h>
#endif
#include <linux/io.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28)
#include <linux/usb_ch9.h>
#include <linux/usb_gadget.h>
#else
#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#endif

#include "dwc_otg_pcd_if.h"
#include "dwc_otg_pcd.h"
#include "dwc_otg_driver.h"
#include "dwc_otg_dbg.h"

static struct gadget_wrapper {
	dwc_otg_pcd_t *pcd;

	struct usb_gadget gadget;
	struct usb_gadget_driver *driver;

	struct usb_ep ep0;
	struct usb_ep in_ep[16];
	struct usb_ep out_ep[16];

} *gadget_wrapper;

static int _disconnect(dwc_otg_pcd_t *pcd);

/* check if request buffer is 4byte aligned */
static inline int is_req_aligned(struct usb_request *req)
{
	return !((int)req->buf & 0x3UL);
}

/* Display the contents of the buffer */
extern void dump_msg(const u8 *buf, unsigned int length);
/**
 * Get the dwc_otg_pcd_ep_t* from usb_ep* pointer - NULL in case
 * if the endpoint is not found
 */
static struct dwc_otg_pcd_ep *ep_from_handle(dwc_otg_pcd_t *pcd, void *handle)
{
	int i;
	if (pcd->ep0.priv == handle)
		return &pcd->ep0;

	for (i = 0; i < MAX_EPS_CHANNELS - 1; i++) {
		if (pcd->in_ep[i].priv == handle)
			return &pcd->in_ep[i];
		if (pcd->out_ep[i].priv == handle)
			return &pcd->out_ep[i];
	}

	return NULL;
}

/* USB Endpoint Operations */
/*
 * The following sections briefly describe the behavior of the Gadget
 * API endpoint operations implemented in the DWC_otg driver
 * software. Detailed descriptions of the generic behavior of each of
 * these functions can be found in the Linux header file
 * include/linux/usb_gadget.h.
 *
 * The Gadget API provides wrapper functions for each of the function
 * pointers defined in usb_ep_ops. The Gadget Driver calls the wrapper
 * function, which then calls the underlying PCD function. The
 * following sections are named according to the wrapper
 * functions. Within each section, the corresponding DWC_otg PCD
 * function name is specified.
 *
 */

/**
 * This function is called by the Gadget Driver for each EP to be
 * configured for the current configuration (SET_CONFIGURATION).
 *
 * This function initializes the dwc_otg_ep_t data structure, and then
 * calls dwc_otg_ep_activate.
 */
static int ep_enable(struct usb_ep *usb_ep,
		     const struct usb_endpoint_descriptor *ep_desc)
{
	int retval;

	DWC_DEBUGPL(DBG_PCDV, "%s(%p,%p)\n", __func__, usb_ep, ep_desc);

	if (!usb_ep || !ep_desc || ep_desc->bDescriptorType != USB_DT_ENDPOINT) {
		DWC_WARN("%s, bad ep or descriptor\n", __func__);
		return -EINVAL;
	}
	if (usb_ep == &gadget_wrapper->ep0) {
		DWC_WARN("%s, bad ep(0)\n", __func__);
		return -EINVAL;
	}

	/* Check FIFO size? */
	if (!ep_desc->wMaxPacketSize) {
		DWC_WARN("%s, bad %s maxpacket\n", __func__, usb_ep->name);
		return -ERANGE;
	}

	if (!gadget_wrapper->driver ||
	    gadget_wrapper->gadget.speed == USB_SPEED_UNKNOWN) {
		DWC_WARN("%s, bogus device state\n", __func__);
		return -ESHUTDOWN;
	}

	/* Delete after check - MAS */
#if 0
	nat = (uint32_t)ep_desc->wMaxPacketSize;
	pr_alert("%s: nat (before) =%d\n", __func__, nat);
	nat = (nat >> 11) & 0x03;
	pr_alert("%s: nat (after) =%d\n", __func__, nat);
#endif
	retval = dwc_otg_pcd_ep_enable(gadget_wrapper->pcd,
				       (const uint8_t *)ep_desc,
				       (void *)usb_ep);
	if (retval) {
		DWC_WARN("dwc_otg_pcd_ep_enable failed\n");
		return -EINVAL;
	}

	usb_ep->maxpacket = le16_to_cpu(ep_desc->wMaxPacketSize);

	return 0;
}

/**
 * This function is called when an EP is disabled due to disconnect or
 * change in configuration. Any pending requests will terminate with a
 * status of -ESHUTDOWN.
 *
 * This function modifies the dwc_otg_ep_t data structure for this EP,
 * and then calls dwc_otg_ep_deactivate.
 */
static int ep_disable(struct usb_ep *usb_ep)
{
	int retval;

	DWC_DEBUGPL(DBG_PCDV, "%s(%p)\n", __func__, usb_ep);
	if (!usb_ep) {
		DWC_DEBUGPL(DBG_PCD, "%s, %s not enabled\n", __func__,
			    usb_ep ? usb_ep->name : NULL);
		return -EINVAL;
	}

	retval = dwc_otg_pcd_ep_disable(gadget_wrapper->pcd, usb_ep);
	if (retval)
		retval = -EINVAL;

	usb_ep->maxpacket = MAX_PACKET_SIZE;

	return retval;
}

/**
 * This function allocates a request object to use with the specified
 * endpoint.
 *
 * @param ep The endpoint to be used with with the request
 * @param gfp_flags the GFP_* flags to use.
 */
static struct usb_request *dwc_otg_pcd_alloc_request(struct usb_ep *ep,
						     gfp_t gfp_flags)
{
	struct usb_request *usb_req;

	DWC_DEBUGPL(DBG_PCDV, "%s(%p,%d)\n", __func__, ep, gfp_flags);
	if (0 == ep) {
		DWC_WARN("%s() %s\n", __func__, "Invalid EP!\n");
		return 0;
	}
	usb_req = kmalloc(sizeof(*usb_req), gfp_flags);
	if (0 == usb_req) {
		DWC_WARN("%s() %s\n", __func__, "request allocation failed!\n");
		return 0;
	}
	memset(usb_req, 0, sizeof(*usb_req));
	usb_req->dma = DWC_INVALID_DMA_ADDR;

	return usb_req;
}

/**
 * This function frees a request object.
 *
 * @param ep The endpoint associated with the request
 * @param req The request being freed
 */
static void dwc_otg_pcd_free_request(struct usb_ep *ep, struct usb_request *req)
{
	DWC_DEBUGPL(DBG_PCDV, "%s(%p,%p)\n", __func__, ep, req);

	if (0 == ep || 0 == req) {
		DWC_WARN("%s() %s\n", __func__,
			 "Invalid ep or req argument!\n");
		return;
	}

	/* free dma buffer if it's allocated but not freed
	 * for example, req queued but killed w/o completion
	 */
	if ((req->dma != DWC_INVALID_DMA_ADDR) && !is_req_aligned(req))
		kfree(phys_to_virt(req->dma));

	kfree(req);
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28)
/**
 * This function allocates an I/O buffer to be used for a transfer
 * to/from the specified endpoint.
 *
 * @param usb_ep The endpoint to be used with with the request
 * @param bytes The desired number of bytes for the buffer
 * @param dma Pointer to the buffer's DMA address; must be valid
 * @param gfp_flags the GFP_* flags to use.
 * @return address of a new buffer or null is buffer could not be allocated.
 */
static void *dwc_otg_pcd_alloc_buffer(struct usb_ep *usb_ep, unsigned bytes,
				      dma_addr_t *dma, gfp_t gfp_flags)
{
	void *buf;
	dwc_otg_pcd_t *pcd = 0;

	pcd = gadget_wrapper->pcd;

	DWC_DEBUGPL(DBG_PCDV, "%s(%p,%d,%p,%0x)\n", __func__, usb_ep, bytes,
		    dma, gfp_flags);

	/* Check dword alignment */
	if ((bytes & 0x3UL) != 0) {
		DWC_WARN("%s() Buffer size is not a multiple of"
			 "DWORD size (%d)", __func__, bytes);
	}

	buf = dma_alloc_coherent(NULL, bytes, dma, gfp_flags);

	/* Check dword alignment */
	if (((int)buf & 0x3UL) != 0) {
		DWC_WARN("%s() Buffer is not DWORD aligned (%p)",
			 __func__, buf);
	}

	return buf;
}

/**
 * This function frees an I/O buffer that was allocated by alloc_buffer.
 *
 * @param usb_ep the endpoint associated with the buffer
 * @param buf address of the buffer
 * @param dma The buffer's DMA address
 * @param bytes The number of bytes of the buffer
 */
static void dwc_otg_pcd_free_buffer(struct usb_ep *usb_ep, void *buf,
				    dma_addr_t dma, unsigned bytes)
{
	dwc_otg_pcd_t *pcd = 0;

	pcd = gadget_wrapper->pcd;

	DWC_DEBUGPL(DBG_PCDV, "%s(%p,%0x,%d)\n", __func__, buf, dma, bytes);

	dma_free_coherent(NULL, bytes, buf, dma);
}
#endif

/**
 * This function is used to submit an I/O Request to an EP.
 *
 *	- When the request completes the request's completion callback
 *	  is called to return the request to the driver.
 *	- An EP, except control EPs, may have multiple requests
 *	  pending.
 *	- Once submitted the request cannot be examined or modified.
 *	- Each request is turned into one or more packets.
 *	- A BULK EP can queue any amount of data; the transfer is
 *	  packetized.
 *	- Zero length Packets are specified with the request 'zero'
 *	  flag.
 */
static int ep_queue(struct usb_ep *usb_ep, struct usb_request *usb_req,
		    gfp_t gfp_flags)
{
	dwc_otg_pcd_t *pcd;
	struct dwc_otg_pcd_ep *ep = NULL;
	int retval = 0, is_isoc_ep = 0;
	void *buf = NULL;

	DWC_DEBUGPL(DBG_PCDV, "%s(%p,%p,%d)\n",
		    __func__, usb_ep, usb_req, gfp_flags);
	if (!usb_req || !usb_req->complete || !usb_req->buf) {
		DWC_WARN("bad params\n");
		return -EINVAL;
	}

	if (!usb_ep) {
		DWC_WARN("bad ep\n");
		return -EINVAL;
	}

	pcd = gadget_wrapper->pcd;
	if (!gadget_wrapper->driver ||
	    gadget_wrapper->gadget.speed == USB_SPEED_UNKNOWN) {
		DWC_DEBUGPL(DBG_PCDV, "gadget.speed=%d\n",
			    gadget_wrapper->gadget.speed);
		DWC_WARN("bogus device state\n");
		return -ESHUTDOWN;
	}

	DWC_DEBUGPL(DBG_PCD, "%s queue req %p, len %d buf %p\n",
		    usb_ep->name, usb_req, usb_req->length, usb_req->buf);

	usb_req->status = -EINPROGRESS;
	usb_req->actual = 0;

	ep = ep_from_handle(pcd, usb_ep);
	if (!ep) {
		DWC_ERROR("bad endpoint\n");
		return -EINVAL;
	}

	is_isoc_ep = (ep->dwc_ep.type == DWC_OTG_EP_TYPE_ISOC) ? 1 : 0;

#if defined(PCI_INTERFACE)
#error	"need to take care cache coherence"
#else
	/* Turn on this assert to test for unaligned buffers
	   BUG_ON (usb_req->dma != DWC_INVALID_DMA_ADDR);
	 */

	/*
	 * DWC OTG DMA engine only accepts 4byte-aligned address
	 * allocate 4byte-aligned dma buffer if needed
	 */
	if (!is_req_aligned(usb_req)) {
		buf = kmalloc(usb_req->length, gfp_flags);
		if (!buf) {
			DWC_WARN("Can't allocate aligned DMA buffer\n");
			return -ENOMEM;
		}

		usb_req->dma = virt_to_phys(buf);
		if (ep->dwc_ep.is_in)
			memcpy(buf, usb_req->buf, usb_req->length);
	} else {
		buf = usb_req->buf;
		usb_req->dma = virt_to_phys(usb_req->buf);
	}

	dma_sync_single_for_device(NULL,
				   usb_req->dma,
				   usb_req->length,
				   ep->dwc_ep.
				   is_in ? DMA_TO_DEVICE : DMA_FROM_DEVICE);

#endif

	retval = dwc_otg_pcd_ep_queue(pcd, usb_ep, buf, usb_req->dma,
				      usb_req->length, usb_req->zero, usb_req,
				      gfp_flags == GFP_ATOMIC ? 1 : 0);
	if (retval)
		return -EINVAL;

	return 0;
}

/**
 * This function cancels an I/O request from an EP.
 */
static int ep_dequeue(struct usb_ep *usb_ep, struct usb_request *usb_req)
{
	DWC_DEBUGPL(DBG_PCDV, "%s(%p,%p)\n", __func__, usb_ep, usb_req);

	if (!usb_ep || !usb_req) {
		DWC_WARN("bad argument\n");
		return -EINVAL;
	}
	if (!gadget_wrapper->driver ||
	    gadget_wrapper->gadget.speed == USB_SPEED_UNKNOWN) {
		DWC_WARN("bogus device state\n");
		return -ESHUTDOWN;
	}
	if (dwc_otg_pcd_ep_dequeue(gadget_wrapper->pcd, usb_ep, usb_req))
		return -EINVAL;

	return 0;
}

/**
 * usb_ep_set_halt stalls an endpoint.
 *
 * usb_ep_clear_halt clears an endpoint halt and resets its data
 * toggle.
 *
 * Both of these functions are implemented with the same underlying
 * function. The behavior depends on the value argument.
 *
 * @param[in] usb_ep the Endpoint to halt or clear halt.
 * @param[in] value
 *	- 0 means clear_halt.
 *	- 1 means set_halt,
 *	- 2 means clear stall lock flag.
 *	- 3 means set  stall lock flag.
 */
static int ep_halt(struct usb_ep *usb_ep, int value)
{
	int retval = 0;

	DWC_DEBUGPL(DBG_PCD, "HALT %s %d\n", usb_ep->name, value);

	if (!usb_ep) {
		DWC_WARN("bad ep\n");
		return -EINVAL;
	}

	retval = dwc_otg_pcd_ep_halt(gadget_wrapper->pcd, usb_ep, value);
	if (retval == -DWC_E_AGAIN)
		return -EAGAIN;
	else if (retval)
		retval = -EINVAL;

	return retval;
}

static struct usb_ep_ops dwc_otg_pcd_ep_ops = {
	.enable = ep_enable,
	.disable = ep_disable,

	.alloc_request = dwc_otg_pcd_alloc_request,
	.free_request = dwc_otg_pcd_free_request,

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 28)
	.alloc_buffer = dwc_otg_pcd_alloc_buffer,
	.free_buffer = dwc_otg_pcd_free_buffer,
#endif

	.queue = ep_queue,
	.dequeue = ep_dequeue,

	.set_halt = ep_halt,
	.fifo_status = 0,
	.fifo_flush = 0,

};

/*	Gadget Operations */
/**
 * The following gadget operations will be implemented in the DWC_otg
 * PCD. Functions in the API that are not described below are not
 * implemented.
 *
 * The Gadget API provides wrapper functions for each of the function
 * pointers defined in usb_gadget_ops. The Gadget Driver calls the
 * wrapper function, which then calls the underlying PCD function. The
 * following sections are named according to the wrapper functions
 * (except for ioctl, which doesn't have a wrapper function). Within
 * each section, the corresponding DWC_otg PCD function name is
 * specified.
 *
 */

/**
 *Gets the USB Frame number of the last SOF.
 */
static int get_frame_number(struct usb_gadget *gadget)
{
	struct gadget_wrapper *d;

	DWC_DEBUGPL(DBG_PCDV, "%s(%p)\n", __func__, gadget);

	if (gadget == 0)
		return -ENODEV;

	d = container_of(gadget, struct gadget_wrapper, gadget);
	return dwc_otg_pcd_get_frame_number(d->pcd);
}

/**
 * Controls how much current to draw from VBUS
 */
static int vbus_draw(struct usb_gadget *gadget, unsigned int mA)
{
	struct gadget_wrapper *d;

	DWC_DEBUGPL(DBG_PCDV, "%s(%p)\n", __func__, gadget);

	if (gadget == 0)
		return -ENODEV;
	else
		d = container_of(gadget, struct gadget_wrapper, gadget);

	/* Save requested max Vbus current draw */
	d->pcd->core_if->vbus_ma = mA;

	/* Schedule a work item to set the max current to draw on Vbus */
	DWC_WORKQ_SCHEDULE(d->pcd->core_if->wq_otg, w_vbus_draw,
			   d->pcd->core_if, "set max vbus current draw");
	return 0;
}

/**
 * Controls pullup which lets the host detect that a USB device is attached.
 * On implies activate pullup, i.e. remove disconnect condition.
 */
static int pullup(struct usb_gadget *gadget, int is_on)
{
	struct gadget_wrapper *d;

	DWC_DEBUGPL(DBG_PCDV, "%s(%p)\n", __func__, gadget);

	if (gadget == 0)
		return -ENODEV;
	else
		d = container_of(gadget, struct gadget_wrapper, gadget);

	d->pcd->core_if->gadget_pullup_on = is_on;

#ifdef CONFIG_USB_OTG_UTILS
	/* Need a defined transceiver's state before controlling pullup */
	if (gadget_wrapper->pcd->core_if->xceiver->state != OTG_STATE_UNDEFINED)
#endif
	{
		dwc_otg_pcd_disconnect(d->pcd, is_on ? false : true);
		if (is_on == 0) {
			DWC_DEBUGPL(DBG_PCDV, "usbd][pullup] fops=0x%x\n", 
				d->pcd->fops);
			_disconnect(d->pcd);
		}
	}

	return 0;
}

#ifdef CONFIG_USB_PCD_SETTINGS
static int pcd_start_clean(struct usb_gadget *gadget, int is_start)
{
	struct gadget_wrapper *d;

	pr_info("%s = %d\n", __func__, is_start);

	if (gadget == 0)
		return -ENODEV;
	else
		d = container_of(gadget, struct gadget_wrapper, gadget);

	d->pcd->core_if->gadget_pullup_on = is_start;

#ifdef CONFIG_USB_OTG_UTILS
	/* Need a defined transceiver's state before controlling pullup */
	if (gadget_wrapper->pcd->core_if->xceiver->state != OTG_STATE_UNDEFINED)
#endif
		dwc_otg_pcd_start_clean(d->pcd, is_start);

	return 0;
}
#endif

#ifdef CONFIG_USB_DWC_OTG_LPM
static int test_lpm_enabled(struct usb_gadget *gadget)
{
	struct gadget_wrapper *d;

	d = container_of(gadget, struct gadget_wrapper, gadget);

	return dwc_otg_pcd_is_lpm_enabled(d->pcd);
}
#endif

/**
 * Initiates Session Request Protocol (SRP) to wakeup the host if no
 * session is in progress. If a session is already in progress, but
 * the device is suspended, remote wakeup signaling is started.
 *
 */
static int wakeup(struct usb_gadget *gadget)
{
	struct gadget_wrapper *d;

	DWC_DEBUGPL(DBG_PCDV, "%s(%p)\n", __func__, gadget);

	if (gadget == 0)
		return -ENODEV;
	else
		d = container_of(gadget, struct gadget_wrapper, gadget);

	dwc_otg_pcd_wakeup(d->pcd);
	return 0;
}

/**
 * Sets device self-powered state.
 *
 */
static int set_selfpowered(struct usb_gadget *gadget, int is_selfpowered)
{
	struct gadget_wrapper *d;

	DWC_DEBUGPL(DBG_PCDV, "%s(%p)\n", __func__, gadget);

	if (gadget == 0)
		return -ENODEV;
	else
		d = container_of(gadget, struct gadget_wrapper, gadget);

	d->pcd->self_powered = is_selfpowered ? 1 : 0;
	return 0;
}


static int dwc_udc_start(struct usb_gadget *gadget,
					struct usb_gadget_driver *driver)
{
	DWC_DEBUGPL(DBG_PCD, "probing gadget driver '%s'\n",
		    driver->driver.name);

	if (!driver || driver->max_speed == USB_SPEED_UNKNOWN ||
	    !driver->disconnect || !driver->setup) {
		DWC_DEBUGPL(DBG_PCDV, "EINVAL\n");
		return -EINVAL;
	}
	if (gadget_wrapper == 0) {
		DWC_DEBUGPL(DBG_PCDV, "ENODEV\n");
		return -ENODEV;
	}
	if (gadget_wrapper->driver != 0) {
		DWC_DEBUGPL(DBG_PCDV, "EBUSY (%p)\n", gadget_wrapper->driver);
		return -EBUSY;
	}

	/* hook up the driver */
	gadget_wrapper->driver = driver;
	gadget->dev.driver = &driver->driver;

#ifdef CONFIG_USB_OTG_UTILS
#ifndef CONFIG_USB_OTG
	if (!(gadget_wrapper->pcd->core_if->xceiver->otg->default_a))
#else
	if (!(gadget_wrapper->pcd->core_if->xceiver->otg->default_a) &&
	    !(gadget_wrapper->pcd->core_if->core_params->otg_supp_enable))
#endif
	{
		/* Init the core */

		DWC_WORKQ_SCHEDULE(gadget_wrapper->pcd->core_if->wq_otg,
				   w_init_core, gadget_wrapper->pcd->core_if,
				   "init core");
	}
#endif /* CONFIG_USB_OTG_UTILS */

#ifdef CONFIG_USB_OTG_UTILS
	if (gadget_wrapper->pcd->core_if->xceiver->otg->set_peripheral)
		otg_set_peripheral(gadget_wrapper->pcd->core_if->xceiver->otg,
				   gadget);
#endif

	return 0;

}

/**
 * This function unregisters a gadget driver
 *
 * @param driver The driver being unregistered
 */
static int dwc_udc_stop(struct usb_gadget *gadget,
					struct usb_gadget_driver *driver)
{
	/*DWC_DEBUGPL(DBG_PCDV,"%s(%p)\n", __func__, _driver); */

	if (gadget_wrapper == 0) {
		DWC_DEBUGPL(DBG_ANY, "%s Return(%d): s_pcd==0\n", __func__,
			    -ENODEV);
		return -ENODEV;
	}
	if (driver == 0 || driver != gadget_wrapper->driver) {
		DWC_DEBUGPL(DBG_ANY, "%s Return(%d): driver?\n", __func__,
			    -EINVAL);
		return -EINVAL;
	}

	dwc_otg_disable_global_interrupts(gadget_wrapper->pcd->core_if);
	/* Gadget about to unbound, disable connection to USB host */
	dwc_otg_pcd_disconnect(gadget_wrapper->pcd, true);
	/* Reflect our current connect status */
	gadget_wrapper->pcd->core_if->gadget_pullup_on = false;

	dwc_otg_pcd_stop(gadget_wrapper->pcd);

	dwc_otg_enable_global_interrupts(gadget_wrapper->pcd->core_if);

	/* Schedule a work item to shutdown the core */
	DWC_WORKQ_SCHEDULE(gadget_wrapper->pcd->core_if->wq_otg,
			   w_shutdown_core, gadget_wrapper->pcd->core_if,
			   "Shutdown core");
	gadget->dev.driver = NULL;
	gadget_wrapper->driver = 0;

#ifdef CONFIG_USB_OTG_UTILS
	if (gadget_wrapper->pcd->core_if->xceiver->otg->set_peripheral)
		otg_set_peripheral(gadget_wrapper->pcd->core_if->xceiver->otg, NULL);
#endif

	DWC_DEBUGPL(DBG_ANY, "unregistered driver '%s'\n", driver->driver.name);
	return 0;
}


static const struct usb_gadget_ops dwc_otg_pcd_ops = {
	.get_frame = get_frame_number,
	.vbus_draw = vbus_draw,
	.pullup = pullup,
	.wakeup = wakeup,
#ifdef CONFIG_USB_DWC_OTG_LPM
	.lpm_support = test_lpm_enabled,
#endif
	.set_selfpowered = set_selfpowered,
	.udc_start = dwc_udc_start,
	.udc_stop = dwc_udc_stop,
};

static int _setup(dwc_otg_pcd_t *pcd, uint8_t *bytes)
{
	int retval = -DWC_E_NOT_SUPPORTED;
	if (gadget_wrapper->driver && gadget_wrapper->driver->setup) {
		retval = gadget_wrapper->driver->setup(&gadget_wrapper->gadget,
						       (struct usb_ctrlrequest
							*)bytes);
	}

	if (retval == -ENOTSUPP)
		retval = -DWC_E_NOT_SUPPORTED;
	else if (retval < 0)
		retval = -DWC_E_INVALID;

	return retval;
}

static int _complete(dwc_otg_pcd_t *pcd, void *ep_handle,
		     void *req_handle, int32_t status, uint32_t actual)
{
	struct usb_request *req = (struct usb_request *)req_handle;
	struct dwc_otg_pcd_ep *ep = NULL;
	enum dma_data_direction dir;

	if (req && req->complete) {
		switch (status) {
		case -DWC_E_SHUTDOWN:
			req->status = -ESHUTDOWN;
			break;
		case -DWC_E_RESTART:
			req->status = -ECONNRESET;
			break;
		case -DWC_E_INVALID:
			req->status = -EINVAL;
			break;
		case -DWC_E_TIMEOUT:
			req->status = -ETIMEDOUT;
			break;
		default:
			req->status = status;
		}
		req->actual = actual;
		ep = ep_from_handle(pcd, ep_handle);

#if defined(PCI_INTERFACE)
#error	"need to take care cache coherence"
#else
		/*
		 * for control pipe, the complete callback may be delayed by 1 packet
		 * so direction of current packet doesn't apply
		 * use DMA_FROM_DEVICE for conservativeness
		 */
		if (ep->dwc_ep.type == UE_CONTROL)
			dir = DMA_FROM_DEVICE;
		else
			dir =
			    ep->dwc_ep.is_in ? DMA_TO_DEVICE : DMA_FROM_DEVICE;

		dma_sync_single_for_cpu(NULL, req->dma, req->length, dir);

		/* if 4byte-aligned dma buffer is ever used */
		if (!is_req_aligned(req)) {
			void *buf = phys_to_virt(req->dma);
			if (!ep->dwc_ep.is_in)
				memcpy(req->buf, buf, req->length);

			kfree(buf);
		}

		/* reset dma to invalid value */
		req->dma = DWC_INVALID_DMA_ADDR;

#endif
		req->complete(ep_handle, req);
	}

	return 0;
}

static int _connect(dwc_otg_pcd_t *pcd, int speed)
{
	gadget_wrapper->gadget.speed = speed;
	return 0;
}

static int _disconnect(dwc_otg_pcd_t *pcd)
{
	if (gadget_wrapper->driver && gadget_wrapper->driver->disconnect)
		gadget_wrapper->driver->disconnect(&gadget_wrapper->gadget);

	return 0;
}

static int _resume(dwc_otg_pcd_t *pcd)
{
	if (gadget_wrapper->driver && gadget_wrapper->driver->resume)
		gadget_wrapper->driver->resume(&gadget_wrapper->gadget);

	return 0;
}

static int _suspend(dwc_otg_pcd_t *pcd)
{
	if (gadget_wrapper->driver && gadget_wrapper->driver->suspend)
		gadget_wrapper->driver->suspend(&gadget_wrapper->gadget);

	return 0;
}

/**
 * This function updates the otg values in the gadget structure.
 */
static int _hnp_changed(dwc_otg_pcd_t *pcd)
{

	if (!gadget_wrapper->gadget.is_otg)
		return 0;

	gadget_wrapper->gadget.b_hnp_enable = get_b_hnp_enable(pcd);
	gadget_wrapper->gadget.a_hnp_support = get_a_hnp_support(pcd);
	gadget_wrapper->gadget.a_alt_hnp_support = get_a_alt_hnp_support(pcd);
	return 0;
}

static int _reset(dwc_otg_pcd_t *pcd)
{
		return 0;
}

#ifdef DWC_UTE_CFI
static int _cfi_setup(dwc_otg_pcd_t *pcd, void *cfi_req)
{
	int retval = -DWC_E_INVALID;
	if (gadget_wrapper->driver->cfi_feature_setup) {
		retval =
		    gadget_wrapper->driver->cfi_feature_setup(&gadget_wrapper->
							      gadget,
							      (struct
							       cfi_usb_ctrlrequest
							       *)cfi_req);
	}

	return retval;
}
#endif

static const struct dwc_otg_pcd_function_ops fops = {
	.complete = _complete,
	.setup = _setup,
	.disconnect = _disconnect,
	.connect = _connect,
	.resume = _resume,
	.suspend = _suspend,
	.hnp_changed = _hnp_changed,
	.reset = _reset,
#ifdef DWC_UTE_CFI
	.cfi_setup = _cfi_setup,
#endif
};

/**
 * This function is the top level PCD interrupt handler.
 */
static irqreturn_t dwc_otg_pcd_irq(int irq, void *dev)
{
	dwc_otg_pcd_t *pcd = dev;
	int32_t retval = IRQ_NONE;

	retval = dwc_otg_pcd_handle_intr(pcd);
	if (retval != 0)
		S3C2410X_CLEAR_EINTPEND();

	return IRQ_RETVAL(retval);
}

/**
 * This function initialized the usb_ep structures to there default
 * state.
 *
 * @param d Pointer on gadget_wrapper.
 */
void gadget_add_eps(struct gadget_wrapper *d)
{
	static const char *names[] = {

		"ep0",
		"ep1in",
		"ep2in",
		"ep3in",
		"ep4in",
		"ep5in",
		"ep6in",
		"ep7in",
		"ep8in",
		"ep9in",
		"ep10in",
		"ep11in",
		"ep12in",
		"ep13in",
		"ep14in",
		"ep15in",
		"ep1out",
		"ep2out",
		"ep3out",
		"ep4out",
		"ep5out",
		"ep6out",
		"ep7out",
		"ep8out",
		"ep9out",
		"ep10out",
		"ep11out",
		"ep12out",
		"ep13out",
		"ep14out",
		"ep15out"
	};

	int i;
	struct usb_ep *ep;
	int8_t dev_endpoints;

	DWC_DEBUGPL(DBG_PCDV, "%s\n", __func__);

	INIT_LIST_HEAD(&d->gadget.ep_list);
	d->gadget.ep0 = &d->ep0;
	d->gadget.speed = USB_SPEED_UNKNOWN;

	INIT_LIST_HEAD(&d->gadget.ep0->ep_list);

	/**
	 * Initialize the EP0 structure.
	 */
	ep = &d->ep0;

	/* Init the usb_ep structure. */
	ep->name = names[0];
	ep->ops = (struct usb_ep_ops *)&dwc_otg_pcd_ep_ops;

	/**
	 * @todo NGS: What should the max packet size be set to
	 * here?  Before EP type is set?
	 */
	ep->maxpacket = MAX_PACKET_SIZE;
	dwc_otg_pcd_ep_enable(d->pcd, NULL, ep);

	list_add_tail(&ep->ep_list, &d->gadget.ep_list);

	/**
	 * Initialize the EP structures.
	 */
	dev_endpoints = d->pcd->core_if->dev_if->num_in_eps;

	for (i = 0; i < dev_endpoints; i++) {
		ep = &d->in_ep[i];

		/* Init the usb_ep structure. */
		ep->name = names[d->pcd->in_ep[i].dwc_ep.num];
		ep->ops = (struct usb_ep_ops *)&dwc_otg_pcd_ep_ops;

		/**
		 * @todo NGS: What should the max packet size be set to
		 * here?  Before EP type is set?
		 */
		ep->maxpacket = MAX_PACKET_SIZE;
		list_add_tail(&ep->ep_list, &d->gadget.ep_list);
	}

	dev_endpoints = d->pcd->core_if->dev_if->num_out_eps;

	for (i = 0; i < dev_endpoints; i++) {
		ep = &d->out_ep[i];

		/* Init the usb_ep structure. */
		ep->name = names[15 + d->pcd->out_ep[i].dwc_ep.num];
		ep->ops = (struct usb_ep_ops *)&dwc_otg_pcd_ep_ops;

		/**
		 * @todo NGS: What should the max packet size be set to
		 * here?  Before EP type is set?
		 */
		ep->maxpacket = MAX_PACKET_SIZE;

		list_add_tail(&ep->ep_list, &d->gadget.ep_list);
	}

	/* remove ep0 from the list.  There is a ep0 pointer. */
	list_del_init(&d->ep0.ep_list);

	d->ep0.maxpacket = MAX_EP0_SIZE;
}

/**
 * This function releases the Gadget device.
 * required by device_unregister().
 *
 * @todo Should this do something?	Should it free the PCD?
 */
static void dwc_otg_pcd_gadget_release(struct device *dev)
{
	DWC_DEBUGPL(DBG_PCDV, "%s(%p)\n", __func__, dev);
}

static struct gadget_wrapper *alloc_wrapper(
#ifdef LM_INTERFACE
						   struct lm_device *_dev
#elif defined (PCI_INTERFACE)
						   struct pci_dev *_dev
#else
						   struct platform_device *_dev
#endif
)
{
	static char pcd_name[] = "dwc_otg_pcd";

#ifdef LM_INTERFACE
	dwc_otg_device_t *otg_dev = lm_get_drvdata(_dev);
#elif defined (PCI_INTERFACE)
	dwc_otg_device_t *otg_dev = pci_get_drvdata(_dev);
#else
	dwc_otg_device_t *otg_dev = platform_get_drvdata(_dev);
#endif

	struct gadget_wrapper *d;

	d = dwc_alloc(sizeof(*d));
	if (d == NULL)
		return NULL;

	memset(d, 0, sizeof(*d));

	d->gadget.name = pcd_name;
	d->pcd = otg_dev->pcd;
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 30)
	strcpy(d->gadget.dev.bus_id, "gadget");
#else
	d->gadget.dev.init_name = "gadget";
#endif
	d->gadget.dev.parent = &_dev->dev;
	d->gadget.dev.release = dwc_otg_pcd_gadget_release;
	d->gadget.ops = &dwc_otg_pcd_ops;
	d->gadget.max_speed = USB_SPEED_HIGH;
	/*d->gadget.is_dualspeed = dwc_otg_pcd_is_dualspeed(otg_dev->pcd);*/
	d->gadget.is_otg = dwc_otg_pcd_is_otg(otg_dev->pcd);
	/*d->gadget.otg_version = dwc_otg_pcd_is_otg20(otg_dev->pcd);*/
	/*d->gadget.is_lpm = dwc_otg_pcd_is_lpm_enabled(otg_dev->pcd);*/

	d->driver = 0;
	return d;
}

static void free_wrapper(struct gadget_wrapper *d)
{
	if (d->driver) {
		/* should have been done already by driver model core */
		DWC_WARN("driver '%s' is still registered\n",
			 d->driver->driver.name);
		usb_gadget_unregister_driver(d->driver);
	}

	device_unregister(&d->gadget.dev);
	dwc_free(d);
}

/**
 * This function initialized the PCD portion of the driver.
 *
 */
int pcd_init(
#ifdef LM_INTERFACE
		    struct lm_device *_dev
#elif defined(PCI_INTERFACE)
		    struct pci_dev *_dev
#else
		    struct platform_device *_dev
#endif
)
{
#ifdef LM_INTERFACE
	dwc_otg_device_t *otg_dev = lm_get_drvdata(_dev);
#elif defined(PCI_INTERFACE)
	dwc_otg_device_t *otg_dev = pci_get_drvdata(_dev);
#else
	dwc_otg_device_t *otg_dev = platform_get_drvdata(_dev);
#endif

	int retval = 0;

	DWC_DEBUGPL(DBG_PCDV, "%s(%p)\n", __func__, _dev);

	otg_dev->pcd = dwc_otg_pcd_init(otg_dev->core_if);

	if (!otg_dev->pcd) {
		DWC_ERROR("dwc_otg_pcd_init failed\n");
		return -ENOMEM;
	}
	/* Postpone any connections to USB host until a gadget is attached */
	dwc_otg_pcd_disconnect(otg_dev->pcd, true);

	gadget_wrapper = alloc_wrapper(_dev);

	/*
	 * Initialize EP structures
	 */
	gadget_add_eps(gadget_wrapper);
	/*
	 * Setup interupt handler
	 */
#if defined(LM_INTERFACE) || defined(PCI_INTERFACE)
	DWC_DEBUGPL(DBG_ANY, "registering handler for irq%d\n", _dev->irq);
	retval = request_irq(_dev->irq, dwc_otg_pcd_irq,
			     IRQF_SHARED, gadget_wrapper->gadget.name,
			     otg_dev->pcd);
	if (retval != 0) {
		DWC_ERROR("request of irq%d failed\n", _dev->irq);
		free_wrapper(gadget_wrapper);
		return -EBUSY;
	}
#else
	DWC_DEBUGPL(DBG_ANY, "registering handler for irq%d\n",
		    platform_get_irq(_dev, 0));
	retval =
	    request_irq(platform_get_irq(_dev, 0), dwc_otg_pcd_irq, IRQF_SHARED,
			gadget_wrapper->gadget.name, otg_dev->pcd);
	if (retval != 0) {
		DWC_ERROR("request of irq%d failed\n",
			  platform_get_irq(_dev, 0));
		free_wrapper(gadget_wrapper);
		return -EBUSY;
	}
#endif
	dwc_otg_pcd_start(gadget_wrapper->pcd, &fops);
	usb_add_gadget_udc(&_dev->dev, &gadget_wrapper->gadget);
	return retval;
}

/**
 * Cleanup the PCD.
 */
void pcd_remove(
#ifdef LM_INTERFACE
		       struct lm_device *_dev
#elif defined(PCI_INTERFACE)
		       struct pci_dev *_dev
#else
		       struct platform_device *_dev
#endif
)
{
#ifdef LM_INTERFACE
	dwc_otg_device_t *otg_dev = lm_get_drvdata(_dev);
#elif defined(PCI_INTERFACE)
	dwc_otg_device_t *otg_dev = pci_get_drvdata(_dev);
#else
	dwc_otg_device_t *otg_dev = platform_get_drvdata(_dev);
#endif
	dwc_otg_pcd_t *pcd = otg_dev->pcd;

	DWC_DEBUGPL(DBG_PCDV, "%s(%p)\n", __func__, _dev);
	usb_del_gadget_udc(&gadget_wrapper->gadget);
	/*
	 * Free the IRQ
	 */
#if defined(LM_INTERFACE) || defined(PCI_INTERFACE)
	free_irq(_dev->irq, pcd);
#else
	free_irq(platform_get_irq(_dev, 0), pcd);
#endif
	dwc_otg_pcd_remove(otg_dev->pcd);
	free_wrapper(gadget_wrapper);
	otg_dev->pcd = 0;

}
#endif /* DWC_HOST_ONLY */
