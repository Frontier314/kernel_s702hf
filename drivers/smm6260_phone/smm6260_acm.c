/*
 * smm6260_acm.c
 *
 * Copyright (c) 2011 
 *
 * USB Abstract Control Model driver for USB modems and ISDN adapters
 *
 * base on cdc-acm driver
 *
 * ChangeLog:
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#undef DEBUG
#undef VERBOSE_DEBUG

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/usb.h>
#include <linux/usb/cdc.h>
#include <asm/byteorder.h>
#include <asm/unaligned.h>
#include <linux/list.h>
#include <linux/pm_runtime.h>
#include <linux/suspend.h>
#include <linux/modemctl.h>
#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#endif
#include "smm6260_acm.h"
#include "smm6260_smd.h"
#include "mach/modem.h"

#define ACM_CLOSE_TIMEOUT	15	/* seconds to let writes drain */

/*
 * Version Information
 */
#define DRIVER_VERSION "v0.01"
#define DRIVER_AUTHOR "WenBin Wu"
#define DRIVER_DESC " Abstract Control Model driver for Intel USB HSIC modems"

static struct usb_driver acm_usb_driver;
//static struct tty_driver *acm_tty_driver;
static struct acm *acm_table[ACM_INTERFACES];
static struct acm_ch *acm_channel[ACM_INTERFACES];
struct acm share_acm;
extern int smm6260_is_cp_wakeup_ap(void);


static DEFINE_MUTEX(open_mutex);

#define ACM_CONNECTED(acm)	(acm && acm->dev)
#define ACM_READY(acm)	(acm && acm->dev && acm->count)
#define ACM_NO_MAIN	1
#define VERBOSE_DEBUG

#ifdef VERBOSE_DEBUG
#define verbose	1
#else
#define verbose	0
#endif


#ifdef CONFIG_HAS_WAKELOCK
enum {
	ACM_WLOCK_RUNTIME,
	ACM_WLOCK_DORMANCY,
} ACM_WLOCK_TYPE;

#define ACM_DEFAULT_WAKE_TIME (6*HZ)
#define ACM_SUSPEND_UNLOCK_DELAY (5*HZ)

static inline void acm_wake_lock_init(struct acm *acm)
{
	wake_lock_init(&acm->pm_lock, WAKE_LOCK_SUSPEND, "cdc-acm");
	wake_lock_init(&acm->dormancy_lock, WAKE_LOCK_SUSPEND, "acm-dormancy");
	acm->wake_time = ACM_DEFAULT_WAKE_TIME;
}

static inline void acm_wake_lock_destroy(struct acm *acm)
{
	wake_lock_destroy(&acm->pm_lock);
	wake_lock_destroy(&acm->dormancy_lock);
}

static inline void _wake_lock(struct acm *acm, int type)
{
	if (acm->usb_connected)
		switch (type) {
		case ACM_WLOCK_DORMANCY:
			wake_lock(&acm->dormancy_lock);
			break;
		case ACM_WLOCK_RUNTIME:
		default:
			wake_lock(&acm->pm_lock);
			break;
		}
}

static inline void _wake_unlock(struct acm *acm, int type)
{
	if (acm)
		switch (type) {
		case ACM_WLOCK_DORMANCY:
			wake_unlock(&acm->dormancy_lock);
			break;
		case ACM_WLOCK_RUNTIME:
		default:
			wake_unlock(&acm->pm_lock);
			break;
		}
}

static inline void _wake_lock_timeout(struct acm *acm, int type)
{
	switch (type) {
	case ACM_WLOCK_DORMANCY:
		wake_lock_timeout(&acm->dormancy_lock, acm->wake_time);
		break;
	case ACM_WLOCK_RUNTIME:
	default:
		wake_lock_timeout(&acm->pm_lock, ACM_SUSPEND_UNLOCK_DELAY);
	}
}

static inline void _wake_lock_settime(struct acm *acm, long time)
{
	if (acm)
		acm->wake_time = time;
}

static inline long _wake_lock_gettime(struct acm *acm)
{
	return acm ? acm->wake_time : ACM_DEFAULT_WAKE_TIME;
}
#else
#define _wake_lock_init(acm) do { } while (0)
#define _wake_lock_destroy(acm) do { } while (0)
#define _wake_lock(acm, type) do { } while (0)
#define _wake_unlock(acm, type) do { } while (0)
#define _wake_lock_timeout(acm, type) do { } while (0)
#define _wake_lock_settime(acm, time) do { } while (0)
#define _wake_lock_gettime(acm) (0)
#endif

#define wake_lock_pm(acm)	_wake_lock(acm, ACM_WLOCK_RUNTIME)
#define wake_lock_data(acm)	_wake_lock(acm, ACM_WLOCK_DORMANCY)
#define wake_unlock_pm(acm)	_wake_unlock(acm, ACM_WLOCK_RUNTIME)
#define wake_unlock_data(acm)	_wake_unlock(acm, ACM_WLOCK_DORMANCY)
#define wake_lock_timeout_pm(acm) _wake_lock_timeout(acm, ACM_WLOCK_RUNTIME)
#define wake_lock_timeout_data(acm) _wake_lock_timeout(acm, ACM_WLOCK_DORMANCY)


/*
 * Functions for ACM control messages.
 */

static int acm_ctrl_msg(struct acm *acm, int request, int value,
							void *buf, int len)
{
	int retval = usb_control_msg(acm->dev, usb_sndctrlpipe(acm->dev, 0),
		request, USB_RT_ACM, value,
		acm->control->altsetting[0].desc.bInterfaceNumber,
		buf, len, 5000);
	dev_info(&acm->control->dev, "acm_control_msg: rq: 0x%02x val: %#x len: %#x result: %d\n",
						request, value, len, retval);
	return retval < 0 ? retval : 0;
}

/* devices aren't required to support these requests.
 * the cdc acm descriptor tells whether they do...
 */
#define acm_set_control(acm, control) \
	acm_ctrl_msg(acm, USB_CDC_REQ_SET_CONTROL_LINE_STATE, control, NULL, 0)
#define acm_set_line(acm, line) \
	acm_ctrl_msg(acm, USB_CDC_REQ_SET_LINE_CODING, 0, line, sizeof *(line))
#define acm_send_break(acm, ms) \
	acm_ctrl_msg(acm, USB_CDC_REQ_SEND_BREAK, ms, NULL, 0)

#ifdef CONFIG_PM_RUNTIME
int acm_request_resume(void)
{
	struct acm *acm=acm_table[0];
	struct acm *parent_acm;
	struct device *dev;
	int err=0;

	/*before do ipc, we should check if modem is on or not.*/
	if(!mc_is_on())
		return 0;
	if (!ACM_CONNECTED(acm))
		return 0;

	parent_acm = acm->parent;
	dev = &acm->dev->dev;

	if (parent_acm->dpm_suspending) {
		parent_acm->skip_hostwakeup = 1;
		dev_dbg(dev,  "%s: suspending skip host wakeup\n",
			__func__);
		return 0;
	}
	
	usb_mark_last_busy(acm->dev);

	if (parent_acm->resume_debug >= 1) {
		dev_dbg(dev,  "%s: resumeing, return\n", __func__);
		return 0;
	}

	if (dev->power.status != DPM_OFF) {
		wake_lock_pm(parent_acm);
		dev_dbg(dev, "%s:run time resume\n", __func__);
		parent_acm->resume_debug = 1;
		err = pm_runtime_resume(dev);
		if (!err && dev->power.timer_expires == 0
			&& dev->power.request_pending == false) {
			dev_dbg(dev, "%s:run time idle\n", __func__);
			pm_runtime_idle(dev);
		}
		parent_acm->resume_debug = 0;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(acm_request_resume);
static int acm_initiated_resume(struct acm *acm)
{
	int err;
	struct usb_device *udev = acm->dev;
	struct acm *parent_acm;	
	if (udev) {
		struct device *dev = &udev->dev;
		int spin = 10, spin2 = 30;
		int host_wakeup_done = 0;
		int _host_high_cnt = 0, _host_timeout_cnt = 0;
		parent_acm = acm->parent;
retry:
		switch (dev->power.runtime_status) {
		case RPM_SUSPENDED:
			modem_data_trace(&udev->dev,"resume %d,%d\n",
				parent_acm->dpm_suspending,host_wakeup_done);
			if (parent_acm->dpm_suspending || host_wakeup_done) {
				dev_info(&udev->dev,
					"DPM Suspending, spin:%d\n", spin2);
				if (spin2-- == 0) {
					dev_err(&udev->dev,
					"dpm resume timeout\n");
					return -ETIMEDOUT;
				}
				msleep(30);
				goto retry;
			}
			err = mc_prepare_resume(500);
			switch (err) {
			case MC_SUCCESS:
				host_wakeup_done = 1;
				_host_timeout_cnt = 0;
				_host_high_cnt = 0;
				goto retry; /*wait until RPM_ACTIVE states*/

			case MC_HOST_TIMEOUT:
				_host_timeout_cnt++;
				break;

			case MC_HOST_HIGH:
				_host_high_cnt++;
				break;
			default:
				return err;
			}
			if (spin2-- == 0) {
				dev_info(&udev->dev,
				"svn initiated resume, RPM_SUSPEND timeout\n");
				crash_event(0);
				return -ETIMEDOUT;
			}
			msleep(20);
			goto retry;

		case RPM_SUSPENDING:
			dev_info(&udev->dev,
				"RPM Suspending, spin:%d\n", spin);
			if (spin-- == 0) {
				dev_err(&udev->dev,
				"Modem suspending timeout\n");
				return -ETIMEDOUT;
			}
			msleep(100);
			goto retry;
		case RPM_RESUMING:
			dev_info(&udev->dev,
				"RPM Resuming, spin:%d\n", spin2);
			if (spin2-- == 0) {
				dev_err(&udev->dev,
				"Modem resume timeout\n");
				return -ETIMEDOUT;
			}
			msleep(50);
			goto retry;
		case RPM_ACTIVE:
			dev_dbg(&udev->dev,
				"RPM Active, spin:%d\n", spin2);			
			break;
		default:
			dev_info(&udev->dev,
				"RPM EIO, spin:%d\n", spin2);				
			return -EIO;
		}
	}
	return 0;
}
static void acm_runtime_start(struct work_struct *work)
{
	struct acm *acm =
		container_of(work, struct acm, pm_runtime_work.work);
	struct device *dev, *ppdev;

	dev = &acm->dev->dev;
	if (acm->dev && dev->parent) {
		ppdev = dev->parent->parent;
		/*enable runtime feature - once after boot*/
		dev_info(dev, "ACM Runtime PM Start!!\n");
		//usb_enable_autosuspend(acm->dev);
		//pm_runtime_allow(dev);
		
		pm_runtime_allow(ppdev); /*ehci*/
		//should try usb_enable_autosuspend(acm->dev); in next week

		if (usb_autopm_get_interface(acm->control) >= 0)
			acm->control->needs_remote_wakeup = 0;
		usb_autopm_put_interface(acm->control);

	}
}
#else
static int acm_initiated_resume(struct acm *acm) {return 0; }
static void acm_runtime_start(struct work_struct *work){return;};
#endif


/*check the acm interface driver status after resume*/
static void acm_post_resume_work(struct work_struct *work)
{
	struct acm *acm=acm_table[0];
	struct acm *parent_acm =
		container_of(work, struct acm, post_resume_work);
	struct device *dev;
	int spin = 10;
	int err;

	if (!ACM_CONNECTED(acm))
		return;
	dev = &acm->dev->dev;
	if (parent_acm->skip_hostwakeup && smm6260_is_cp_wakeup_ap()) {
		dev_info(dev,
			"post resume host skip=%d, host gpio=%d, rpm_stat=%d",
			parent_acm->skip_hostwakeup, smm6260_is_cp_wakeup_ap(),
			dev->power.runtime_status);
retry:
		switch (dev->power.runtime_status) {
		case RPM_SUSPENDED:
			parent_acm->resume_debug = 1;
			err = pm_runtime_resume(dev);
			if (!err && dev->power.timer_expires == 0
				&& dev->power.request_pending == false) {
				dev_dbg(dev, "%s:run time idle\n",  __func__);
				pm_runtime_idle(dev);
			}
			parent_acm->resume_debug = 0;
			break;
		case RPM_SUSPENDING:
			if (spin--) {
				dev_err(dev, "usbsvn suspending when resum spin=%d\n", spin);
				msleep(20);
				goto retry;
			}
		case RPM_RESUMING:
		case RPM_ACTIVE:
			break;
		}
		parent_acm->skip_hostwakeup = 0;
	}
}


/*
 * Write buffer management.
 * All of these assume proper locks taken by the caller.
 */

static int acm_wb_alloc(struct acm *acm)
{
	int i, wbn;
	struct acm_wb *wb;

	wbn = 0;
	i = 0;
	for (;;) {
		wb = &acm->wb[wbn];
		if (!wb->use) {
			wb->use = 1;
			return wbn;
		}
		wbn = (wbn + 1) % ACM_NW;
		if (++i >= ACM_NW)
			return -1;
	}
}

static int acm_wb_is_avail(struct acm *acm)
{
	int i, n;
	unsigned long flags;

	n = ACM_NW;
	spin_lock_irqsave(&acm->write_lock, flags);
	for (i = 0; i < ACM_NW; i++)
		n -= acm->wb[i].use;
	spin_unlock_irqrestore(&acm->write_lock, flags);
	return n;
}

/*
 * Finish write. Caller must hold acm->write_lock
 */
static void acm_write_done(struct acm *acm, struct acm_wb *wb)
{
	struct acm_ch *ch;

	ch = acm_channel[acm->cid];
	wb->use = 0;
	acm->transmitting--;
	if(ch && ch ->wr_cb)
		ch ->wr_cb(ch->priv);
	usb_autopm_put_interface(acm->control);
}

/*
 * Poke write.
 *
 * the caller is responsible for locking
 */

static int acm_start_wb(struct acm *acm, struct acm_wb *wb)
{
	int rc;

	acm->transmitting++;

	wb->urb->transfer_buffer = wb->buf;
	wb->urb->transfer_dma = wb->dmah;
	wb->urb->transfer_buffer_length = wb->len;
	wb->urb->dev = acm->dev;

	rc = usb_submit_urb(wb->urb, GFP_ATOMIC);
	if (rc < 0) {
		dbg("usb_submit_urb(write bulk) failed: %d", rc);
		acm_write_done(acm, wb);
	}
	usb_mark_last_busy(acm->dev);
	return rc;
}

static int acm_write_start(struct acm *acm, int wbn)
{
	unsigned long flags;
	struct acm_wb *wb = &acm->wb[wbn];
	int rc;
	/*before do ipc, we should check if modem is on or not.*/
	if(!mc_is_on())
		return -ENODEV;
	rc = acm_initiated_resume(acm);
	if(rc<0)
		return rc;
	
	spin_lock_irqsave(&acm->write_lock, flags);
	if (!acm->dev) {
		wb->use = 0;
		spin_unlock_irqrestore(&acm->write_lock, flags);
		return -ENODEV;
	}

	dbg("%s susp_count: %d", __func__, acm->susp_count);
	usb_autopm_get_interface_async(acm->control);
	if (acm->susp_count) {
		if (!acm->delayed_wb)
			acm->delayed_wb = wb;
		else
			usb_autopm_put_interface(acm->control);
		spin_unlock_irqrestore(&acm->write_lock, flags);
		return 0;	/* A white lie */
	}
	usb_mark_last_busy(acm->dev);

	rc = acm_start_wb(acm, wb);
	spin_unlock_irqrestore(&acm->write_lock, flags);
	wake_lock_timeout_data(acm->parent);
	return rc;

}
/*
 * attributes exported through sysfs
 */
static ssize_t show_caps
(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct usb_interface *intf = to_usb_interface(dev);
	struct acm *acm = usb_get_intfdata(intf);

	return sprintf(buf, "%d", acm->ctrl_caps);
}
static DEVICE_ATTR(bmCapabilities, S_IRUGO, show_caps, NULL);

/*
 * Interrupt handlers for various ACM device responses
 */

/* control interface reports status changes with "interrupt" transfers */
static void acm_ctrl_irq(struct urb *urb)
{
	struct acm *acm = urb->context;
	struct usb_cdc_notification *dr = (struct usb_cdc_notification *)acm->ctrl_buffer;
	unsigned char *data;
	int newctrl;
	int retval;
	int status = urb->status;
	
	dev_info(&acm->dev->dev, "%s - urb status: %d\n", __func__, status);
	switch (status) {
	case 0:
		/* success */
		break;
	case -ECONNRESET:
	case -ENOENT:
	case -ESHUTDOWN:
		/* this urb is terminated, clean up */
		dev_info(&acm->dev->dev, "%s - urb shutting down with status: %d\n", __func__, status);
		return;
	default:
		dev_info(&acm->dev->dev, "%s - nonzero urb status received: %d\n", __func__, status);
		goto exit;
	}

	if (!ACM_READY(acm))
		goto exit;

	data = (unsigned char *)(dr + 1);
	dev_info(&acm->dev->dev, "%s: request type --- %d\n", __func__, dr->bmRequestType);
	switch (dr->bNotificationType) {
	case USB_CDC_NOTIFY_NETWORK_CONNECTION:
		printk("USB_CDC_NOTIFY_NETWORK_CONNECTION: %s network\n", dr->wValue ?
					"connected to" : "disconnected from");
		break;

	case USB_CDC_NOTIFY_SERIAL_STATE:
		printk("%s: USB_CDC_NOTIFY_SERIAL_STATE\n", __func__);
		newctrl = get_unaligned_le16(data);

		if((acm->ctrlin & ~newctrl & ACM_CTRL_DCD))
		{
			printk("#################################################calling hangup");
		}
		acm->ctrlin = newctrl;

		printk("input control lines: dcd%c dsr%c break%c ring%c framing%c parity%c overrun%c\n",
			acm->ctrlin & ACM_CTRL_DCD ? '+' : '-',
			acm->ctrlin & ACM_CTRL_DSR ? '+' : '-',
			acm->ctrlin & ACM_CTRL_BRK ? '+' : '-',
			acm->ctrlin & ACM_CTRL_RI  ? '+' : '-',
			acm->ctrlin & ACM_CTRL_FRAMING ? '+' : '-',
			acm->ctrlin & ACM_CTRL_PARITY ? '+' : '-',
			acm->ctrlin & ACM_CTRL_OVERRUN ? '+' : '-');
		break;
	default:
		printk("unknown notification %d received: index %d len %d data0 %d data1 %d",
			dr->bNotificationType, dr->wIndex,
			dr->wLength, data[0], data[1]);
		break;
	}
exit:
	usb_mark_last_busy(acm->dev);
	retval = usb_submit_urb(urb, GFP_ATOMIC);
	if (retval)
		dev_err(&urb->dev->dev, "%s - usb_submit_urb failed with "
			"result %d", __func__, retval);
}

/* data interface returns incoming bytes, or we got unthrottled */
static void acm_read_bulk(struct urb *urb)
{
	struct acm_rb *buf;
	struct acm_ru *rcv = urb->context;
	struct acm *acm = rcv->instance;
	int status = urb->status;

	modem_data_trace(&acm->data->dev,"r-b%d\n", status);
	
	if (!ACM_READY(acm)) {
		dev_dbg(&acm->data->dev, "Aborting, acm not ready");
		return;
	}
	usb_mark_last_busy(acm->dev);
	wake_lock_timeout_data(acm->parent);
	if (status)
		dev_dbg(&acm->data->dev, "bulk rx status %d\n", status);

	//printk("r-%x,%x,%d\n",urb->actual_length,urb->transfer_buffer_length,status);
	setstamp(7);//acm write 7
	//printstamp();
	buf = rcv->buffer;
	buf->size = urb->actual_length;

	if (likely(status == 0)) {
		spin_lock(&acm->read_lock);
		acm->processing++;
		list_add_tail(&rcv->list, &acm->spare_read_urbs);
		list_add_tail(&buf->list, &acm->filled_read_bufs);
		dev_dbg(&acm->data->dev,"add %p to filled_read_bufs \n", buf);
		spin_unlock(&acm->read_lock);
	} else {
		/* we drop the buffer due to an error */
		spin_lock(&acm->read_lock);
		list_add_tail(&rcv->list, &acm->spare_read_urbs);
		list_add(&buf->list, &acm->spare_read_bufs);
		spin_unlock(&acm->read_lock);
		/* nevertheless the tasklet must be kicked unconditionally
		so the queue cannot dry up */
	}
	if (likely(!acm->susp_count))
		tasklet_schedule(&acm->urb_task);
}

static void acm_rx_tasklet(unsigned long _acm)
{
	struct acm *acm = (void *)_acm;
	struct acm_ch *ch;
	struct acm_rb *buf;
	//struct tty_struct *tty;
	struct acm_ru *rcv;
	unsigned long flags;
	//unsigned char throttled;
	int free;
	int ret;
	int i, max;
	
	modem_data_trace(&acm->data->dev,"rx_task\n");
	if (!ACM_READY(acm)) {
		printk("acm_rx_tasklet: ACM not ready\n");
		return;
	}
	ch = acm_channel[acm->cid];
	
next_buffer:
	spin_lock_irqsave(&acm->read_lock, flags);
	if (list_empty(&acm->filled_read_bufs)) {
		spin_unlock_irqrestore(&acm->read_lock, flags);
		goto urbs;
	}
	buf = list_entry(acm->filled_read_bufs.next,
			 struct acm_rb, list);
	list_del(&buf->list);
	spin_unlock_irqrestore(&acm->read_lock, flags);

	dev_dbg(&acm->data->dev,"acm_rx_tasklet: procesing buf 0x%p, size = %d\n", buf, buf->size);

	if (ch->rx_cb) {
	#if MODEM_DATA_DEBUG
		if(acm->cid > 1)
		{
			dev_info(&acm->data->dev, "Receive Buffer:\n");
			max =  20;
			if (buf->size<20)
				max = buf->size;

			for(i=0; i<max; i++)
			{
				printk("[0x%02x]", buf->base[i]);
			}
			printk("\n");
		}
	#endif
		ret = ch->rx_cb(ch->priv, buf->base, buf->size, 0, &free);
		if (ret <=0 && buf->size) {/*buf->size maybe 0 length*/
			spin_lock_irqsave(&acm->read_lock, flags);
			list_add(&buf->list, &acm->filled_read_bufs);
			spin_unlock_irqrestore(&acm->read_lock, flags);
			
			usb_mark_last_busy(acm->dev);
			dev_dbg(&acm->data->dev,"acm_rx_tasklet: rx_cb process fail insert buffer to head\n");
			goto urbs;
		}
	}

	spin_lock_irqsave(&acm->read_lock, flags);
	list_add(&buf->list, &acm->spare_read_bufs);
	spin_unlock_irqrestore(&acm->read_lock, flags);
	usb_mark_last_busy(acm->dev);
	goto next_buffer;

urbs:
	dev_dbg(&acm->data->dev,"acm_rx_tasklet: fill urb\n");
	spin_lock_irqsave(&acm->read_lock, flags);
	while (!list_empty(&acm->spare_read_bufs)) {
		
		if (list_empty(&acm->spare_read_urbs)) {
			acm->processing = 0;
			spin_unlock_irqrestore(&acm->read_lock, flags);
			usb_mark_last_busy(acm->dev);
			return;
		}
		rcv = list_entry(acm->spare_read_urbs.next,
				 struct acm_ru, list);
		list_del(&rcv->list);

		buf = list_entry(acm->spare_read_bufs.next,
				 struct acm_rb, list);
		list_del(&buf->list);
		spin_unlock_irqrestore(&acm->read_lock, flags);
		
		rcv->buffer = buf;

		usb_fill_bulk_urb(rcv->urb, acm->dev,
					  acm->rx_endpoint,
					  buf->base,
					  acm->readsize,
					  acm_read_bulk, rcv);
		rcv->urb->transfer_dma = buf->dma;
		rcv->urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;

		dev_dbg(&acm->data->dev,"acm_rx_tasklet: fill read buffer %p, acm->susp_count=%d\n", rcv->buffer, acm->susp_count);
		/* This shouldn't kill the driver as unsuccessful URBs are
		   returned to the free-urbs-pool and resubmited ASAP */
		spin_lock_irqsave(&acm->read_lock, flags);
  		if (acm->susp_count ||
				usb_submit_urb(rcv->urb, GFP_ATOMIC) < 0) {
			list_add(&buf->list, &acm->spare_read_bufs);
			list_add(&rcv->list, &acm->spare_read_urbs);
			acm->processing = 0;
			spin_unlock_irqrestore(&acm->read_lock, flags);
			dev_dbg(&acm->data->dev,"acm_rx_tasklet: fill back urb and exit\n");
			usb_mark_last_busy(acm->dev);
			wake_lock_timeout_data(acm->parent);
			return;
		} 
	}
	acm->processing = 0;
	spin_unlock_irqrestore(&acm->read_lock, flags);
	usb_mark_last_busy(acm->dev);
	wake_lock_timeout_data(acm->parent);
	dev_dbg(&acm->data->dev,"acm_rx_tasklet: fill urb exit\n");	
}
static void acm_kick(struct acm_ch *ch)
{
	struct acm *acm = acm_table[ch->cid];
	
	if (!ACM_READY(acm))
		return;
	if (likely(!acm->susp_count))
		tasklet_schedule(&acm->urb_task);
}

/* data interface wrote those outgoing bytes */
static void acm_write_bulk(struct urb *urb)
{
	struct acm_wb *wb = urb->context;
	struct acm *acm = wb->instance;
	unsigned long flags;

	if (verbose || urb->status
			|| (urb->actual_length != urb->transfer_buffer_length))
		modem_data_trace(&acm->data->dev, "tx%d/%d>%d\n",
			urb->actual_length,
			urb->transfer_buffer_length,
			urb->status);
	//printk("w-%x-%x\n",urb->transfer_buffer_length,urb->actual_length);
	setstamp(6);//acm write 6
	spin_lock_irqsave(&acm->write_lock, flags);
	acm_write_done(acm, wb);
	spin_unlock_irqrestore(&acm->write_lock, flags);
	if (ACM_READY(acm))
		schedule_work(&acm->work);
	else
		wake_up_interruptible(&acm->drain_wait);
	usb_mark_last_busy(acm->dev);
}

static void acm_softint(struct work_struct *work)
{
	struct acm *acm = container_of(work, struct acm, work);

	dev_dbg(&acm->data->dev, "tx work\n");
	if (!ACM_READY(acm))
		return;
}


static void stop_data_traffic(struct acm *acm)
{
	int i;
	dev_dbg(&acm->data->dev, "Entering stop_data_traffic\n");

	tasklet_disable(&acm->urb_task);

	//usb_kill_urb(acm->ctrlurb);
	for (i = 0; i < ACM_NW; i++)
		usb_kill_urb(acm->wb[i].urb);
	for (i = 0; i < acm->rx_buflimit; i++)
		usb_kill_urb(acm->ru[i].urb);

	tasklet_enable(&acm->urb_task);

	//cancel_work_sync(&acm->work);
}

static int acm_open(struct acm_ch *ch)
{
	int rv = -ENODEV;
	int i;
	struct acm *acm;
	
	if (ch->cid>= ACM_INTERFACES)
		goto out;
	else
		rv = 0;
	acm = acm_table[ch->cid];
	if(!acm)
	{
		goto early_bail;
	}
	if (acm->count) {
		goto out;
	}
	/*before do ipc, we should check if modem is on or not.*/
	if(!mc_is_on())
		return -ENODEV;
	mutex_lock(&open_mutex);
	rv = acm_initiated_resume(acm);
	if(rv<0)
		return rv;
	if (usb_autopm_get_interface(acm->control) < 0)
		goto early_bail;
	else
		acm->control->needs_remote_wakeup = 0;
	
	acm->count = 1;

	mutex_lock(&acm->mutex);
#if 0	
	acm_set_control(acm, acm->ctrlout = ACM_CTRL_DTR | ACM_CTRL_RTS);
	
	acm->line.dwDTERate = cpu_to_le32(24000000);
	acm->line.bDataBits = 8;
	acm_set_line(acm, &acm->line);
	
#endif
	
	usb_autopm_put_interface(acm->control);

	spin_lock(&acm->read_lock);
	INIT_LIST_HEAD(&acm->spare_read_urbs);
	INIT_LIST_HEAD(&acm->spare_read_bufs);
	INIT_LIST_HEAD(&acm->filled_read_bufs);
	
	for (i = 0; i < acm->rx_buflimit; i++)
		list_add(&(acm->ru[i].list), &acm->spare_read_urbs);
	for (i = 0; i < acm->rx_buflimit; i++)
		list_add(&(acm->rb[i].list), &acm->spare_read_bufs);
	spin_unlock(&acm->read_lock);

	tasklet_schedule(&acm->urb_task);

	mutex_unlock(&acm->mutex);
	printk("%s: %d open count=%d\n", __func__, acm->cid, acm->count);
	printk("haozz--acm_open=%d\n",ch->cid);

	mutex_unlock(&open_mutex);
	
out:
	
	return rv;
early_bail:
	mutex_unlock(&open_mutex);
	return -EIO;
}

static void acm_down(struct acm *acm, int drain)
{
	int i;

	if (acm->dev) {
		usb_autopm_get_interface(acm->control);
		//acm_set_control(acm, acm->ctrlout = 0);
		/* try letting the last writes drain naturally */
		if (drain) {
			wait_event_interruptible_timeout(acm->drain_wait,
				(ACM_NW == acm_wb_is_avail(acm)) || !acm->dev,
					ACM_CLOSE_TIMEOUT * HZ);
		}
		tasklet_kill(&acm->urb_task);
		for (i = 0; i < ACM_NW; i++)
			usb_kill_urb(acm->wb[i].urb);
		for (i = 0; i < acm->rx_buflimit; i++)
			usb_kill_urb(acm->ru[i].urb);
		acm->control->needs_remote_wakeup = 0;
		usb_autopm_put_interface(acm->control);
	}
}

static void acm_close(struct acm_ch *ch)
{
	struct acm *acm;
	printk("haozz--acm_close=%d\n",ch->cid);
	mutex_lock(&open_mutex);
	acm = acm_table[ch->cid];
	if (!acm || !acm->count)
	{
		mutex_unlock(&open_mutex);	
		return;
	}
	acm->count = 0 ;
	
	acm_down(acm, 1);
	mutex_unlock(&open_mutex);	
	printk("%s: %d open count=%d\n", __func__, acm->cid, acm->count);
}

static int acm_write(struct acm_ch *ch,
					const unsigned char *buf, int count)
{
	struct acm *acm = acm_table[ch->cid];
	int stat;
	unsigned long flags;
	int wbn;
	struct acm_wb *wb;
	int i, max;
	setstamp(4);//acm write 4
	if (!ACM_READY(acm))
		return -EINVAL;

	if(count<0)
		return 0;
	modem_data_trace(&acm->data->dev,"w-%d-%d\n",ch->cid,count);
	spin_lock_irqsave(&acm->write_lock, flags);
	wbn = acm_wb_alloc(acm);
	if (wbn < 0) {
		spin_unlock_irqrestore(&acm->write_lock, flags);
		return 0;
	}
	wb = &acm->wb[wbn];

	count = (count > acm->writesize) ? acm->writesize : count;
	memcpy(wb->buf, buf, count);
#ifdef MODEM_DATA_DEBUG
	if(acm->cid > 1)
	{
		dev_info(&acm->data->dev, "Send Buffer:\n");
		max =  20;
		if (count<20)
			max = count;

		for(i=0; i<max; i++)
		{
			printk("[0x%02x]", buf[i]);
		}
		printk("\n");
	}
#endif
	wb->len = count;
	spin_unlock_irqrestore(&acm->write_lock, flags);

	stat = acm_write_start(acm, wbn);
	if (stat < 0)
	{
		printk("%s,%d\n",__FUNCTION__,__LINE__);
		return stat;
	}
	return count;
}

/*
 * USB probe and disconnect routines.
 */

/* Little helpers: write/read buffers free */
static void acm_write_buffers_free(struct acm *acm)
{
	int i;
	struct acm_wb *wb;
	struct usb_device *usb_dev = interface_to_usbdev(acm->control);

	for (wb = &acm->wb[0], i = 0; i < ACM_NW; i++, wb++)
		usb_free_coherent(usb_dev, acm->writesize, wb->buf, wb->dmah);
}

static void acm_read_buffers_free(struct acm *acm)
{
	struct usb_device *usb_dev = interface_to_usbdev(acm->control);
	int i, n = acm->rx_buflimit;

	for (i = 0; i < n; i++)
		usb_free_coherent(usb_dev, acm->readsize,
				  acm->rb[i].base, acm->rb[i].dma);
}

/* Little helper: write buffers allocate */
static int acm_write_buffers_alloc(struct acm *acm)
{
	int i;
	struct acm_wb *wb;

	for (wb = &acm->wb[0], i = 0; i < ACM_NW; i++, wb++) {
		wb->buf = usb_alloc_coherent(acm->dev, acm->writesize, GFP_KERNEL,
		    &wb->dmah);
		if (!wb->buf) {
			while (i != 0) {
				--i;
				--wb;
				usb_free_coherent(acm->dev, acm->writesize,
				    wb->buf, wb->dmah);
			}
			return -ENOMEM;
		}
	}
	return 0;
}

static int acm_probe(struct usb_interface *intf,
		     const struct usb_device_id *id)
{
	struct usb_cdc_union_desc *union_header = NULL;
	struct usb_cdc_call_mgmt_descriptor *cmd = NULL;
	unsigned char *buffer = intf->altsetting->extra;
	int buflen = intf->altsetting->extralen;
	struct usb_interface *control_interface;
	struct usb_interface *data_interface;
	struct usb_endpoint_descriptor *epctrl = NULL;
	struct usb_endpoint_descriptor *epread = NULL;
	struct usb_endpoint_descriptor *epwrite = NULL;
	struct usb_device *usb_dev = interface_to_usbdev(intf);
	struct usb_device *root_usbdev= to_usb_device(intf->dev.parent->parent);
	struct acm *acm;
	int rv;
	int minor;
	int ctrlsize, readsize;
	u8 *buf;
	u8 ac_management_function = 0;
	u8 call_management_function = 0;
	int call_interface_num = -1;
	int data_interface_num;
	unsigned long quirks;
	int num_rx_buf;
	int i;
	int combined_interfaces = 0;
	const struct usb_host_interface *data_desc;
	const struct usb_host_interface *ctrl_desc;
	int dev_id;

	/* normal quirks */
	quirks = (unsigned long)id->driver_info;
	num_rx_buf = (quirks == SINGLE_RX_URB) ? 1 : ACM_NR;

	/* normal probing*/
	if (!buffer) {
		dev_err(&intf->dev, "Weird descriptor references\n");
		return -EINVAL;
	}
	dev_info(usb_dev->dev.parent->parent, "JOHNLAY 2-1\n");

	if (!buflen) {
		if (intf->cur_altsetting->endpoint &&
				intf->cur_altsetting->endpoint->extralen &&
				intf->cur_altsetting->endpoint->extra) {
			dev_dbg(&intf->dev,
				"Seeking extra descriptors on endpoint\n");
			buflen = intf->cur_altsetting->endpoint->extralen;
			buffer = intf->cur_altsetting->endpoint->extra;
		} else {
			dev_err(&intf->dev,
				"Zero length descriptor references\n");
			return -EINVAL;
		}
	}
	printk("JOHNLAY 3\n");

	while (buflen > 0) {
		if (buffer[1] != USB_DT_CS_INTERFACE) {
			dev_err(&intf->dev, "skipping garbage\n");
			goto next_desc;
		}
		switch (buffer[2]) {
		case USB_CDC_UNION_TYPE: /* we've found it */
			if (union_header) {
				dev_err(&intf->dev, "More than one "
					"union descriptor, skipping ...\n");
				goto next_desc;
			}
			union_header = (struct usb_cdc_union_desc *)buffer;
			dev_info(&intf->dev, "USB_CDC_UNION_TYPE bLength=0x%x\n", union_header->bLength);
			break;
		case USB_CDC_HEADER_TYPE: /* maybe check version */
			dev_info(&intf->dev, "USB_CDC_HEADER_TYPE\n");
			break; /* for now we ignore it */
		case USB_CDC_ACM_TYPE:
			ac_management_function = buffer[3];
			dev_info(&intf->dev, "USB_CDC_ACM_TYPE Capabilities=0x%x\n", ac_management_function);
			break;
		case USB_CDC_CALL_MANAGEMENT_TYPE:
			cmd = (struct usb_cdc_call_mgmt_descriptor *)buffer;
			dev_info(&intf->dev, "USB_CDC_CALL_MANAGEMENT_TYPE Capabilities=0x%x\n", cmd->bmCapabilities);
			break;
		default:
			/* there are LOTS more CDC descriptors that
			 * could legitimately be found here.
			 */
			dev_dbg(&intf->dev, "Ignoring descriptor: "
					"type %02x, length %d\n",
					buffer[2], buffer[0]);
			break;
		}
next_desc:
		buflen -= buffer[0];
		buffer += buffer[0];
	}
	if(!union_header)
		return -EINVAL;
	data_interface= usb_ifnum_to_if(usb_dev, union_header->bSlaveInterface0);
	control_interface = usb_ifnum_to_if(usb_dev, union_header->bMasterInterface0);
	if (!data_interface)
		return -ENODEV;
	if (!control_interface)
		return -ENODEV;
	data_desc = data_interface->altsetting;
	ctrl_desc = control_interface->altsetting;
	/* To detect usb device order probed */
	dev_id = intf->altsetting->desc.bInterfaceNumber / 2;
	dev_info(&root_usbdev->dev,  "%s: probe dev_id=%d, num_altsetting=%d\n", __func__, dev_id, intf->num_altsetting);
	epctrl = &ctrl_desc->endpoint[0].desc;
	/* Endpoints */
	if (usb_pipein(data_desc->endpoint[0].desc.bEndpointAddress)) {
		epread = &data_desc->endpoint[0].desc;
		epwrite = &data_desc->endpoint[1].desc;
		dev_info(&data_interface->dev,"%s: usb_pipein = 0\n", __func__);
	} else {
		epread = &data_desc->endpoint[1].desc;
		epwrite = &data_desc->endpoint[0].desc;
		dev_info(&data_interface->dev,"%s: usb_pipein = 1\n", __func__);
	}
// LSI 
	dev_info(&data_interface->dev,"epread number : 0x%x, max read=%d ", epread->bEndpointAddress, epread->wMaxPacketSize);
	dev_info(&data_interface->dev,"eprwrite number : 0x%x, max write=%d\n", epwrite->bEndpointAddress, epwrite->wMaxPacketSize);
	dev_info(&control_interface->dev,"eprwrite number : 0x%x, max write=%d\n", epwrite->bEndpointAddress, epwrite->wMaxPacketSize);
	dev_info(&control_interface->dev,"eprwrite number : 0x%x, max write=%d\n", epwrite->bEndpointAddress, epwrite->wMaxPacketSize);
	
	printk("interfaces are valid\n");
	for (minor = 0; minor < ACM_INTERFACES && acm_table[minor]; minor++);
	
	if (minor == ACM_INTERFACES) {
		dev_err(&intf->dev, "no more free acm devices\n");
		return -ENODEV;
	}

	acm = kzalloc(sizeof(struct acm), GFP_KERNEL);
	if (acm == NULL) {
		dev_dbg(&intf->dev, "out of memory (acm kzalloc)\n");
		goto alloc_fail;
	}

	ctrlsize = le16_to_cpu(epctrl->wMaxPacketSize);
	readsize = le16_to_cpu(epread->wMaxPacketSize) *
				(quirks == SINGLE_RX_URB ? 1 : 4);
	acm->combined_interfaces = combined_interfaces;
	acm->writesize = le16_to_cpu(epwrite->wMaxPacketSize*8) ;
	acm->control = control_interface;
	acm->data = data_interface;
	acm->minor = minor;
	acm->cid = minor;
	acm->count = 0;
	acm->dev = usb_dev;
	acm->ctrl_caps = ac_management_function;
	if (quirks & NO_CAP_LINE)
		acm->ctrl_caps &= ~USB_CDC_CAP_LINE;
	acm->ctrlsize = ctrlsize;
	acm->readsize = readsize;
	acm->rx_buflimit = num_rx_buf;
	acm->urb_task.func = acm_rx_tasklet;
	acm->urb_task.data = (unsigned long) acm;
	INIT_WORK(&acm->work, acm_softint);
	init_waitqueue_head(&acm->drain_wait);
	spin_lock_init(&acm->throttle_lock);
	spin_lock_init(&acm->write_lock);
	spin_lock_init(&acm->read_lock);

	mutex_init(&acm->mutex);
	acm->rx_endpoint = usb_rcvbulkpipe(usb_dev, epread->bEndpointAddress);
	acm->ctrl_endpoint = usb_rcvintpipe(usb_dev, epctrl->bEndpointAddress);
	acm->tx_endpoint = usb_sndbulkpipe(usb_dev, epwrite->bEndpointAddress);	
	acm->is_int_ep = usb_endpoint_xfer_int(epctrl);
	
	if (acm->is_int_ep)
	{
		acm->bInterval = epctrl->bInterval;	
		printk("%s: is_int_ep bInterval=%d\n", __func__, acm->bInterval);
	}else{
		printk("%s: epint isn't a interrupt endpoint\n", __func__);
	}

	usb_dev->autosuspend_delay = msecs_to_jiffies(500);      /* 200ms */
	root_usbdev->autosuspend_delay = msecs_to_jiffies(500); // 400 is temporary value
	buf = usb_alloc_coherent(usb_dev, ctrlsize, GFP_KERNEL, &acm->ctrl_dma);
	if (!buf) {
		dev_dbg(&intf->dev, "out of memory (ctrl buffer alloc)\n");
		goto alloc_fail2;
	}
	acm->ctrl_buffer = buf;

	if (acm_write_buffers_alloc(acm) < 0) {
		dev_dbg(&intf->dev, "out of memory (write buffer alloc)\n");
		goto alloc_fail4;
	}

	acm->ctrlurb = usb_alloc_urb(0, GFP_KERNEL);
	if (!acm->ctrlurb) {
		dev_dbg(&intf->dev, "out of memory (ctrlurb kmalloc)\n");
		goto alloc_fail5;
	}
	for (i = 0; i < num_rx_buf; i++) {
		struct acm_ru *rcv = &(acm->ru[i]);

		rcv->urb = usb_alloc_urb(0, GFP_KERNEL);
		if (rcv->urb == NULL) {
			dev_dbg(&intf->dev,
				"out of memory (read urbs usb_alloc_urb)\n");
			goto alloc_fail6;
		}

		rcv->urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
		rcv->instance = acm;
	}
	for (i = 0; i < num_rx_buf; i++) {
		struct acm_rb *rb = &(acm->rb[i]);

		rb->base = usb_alloc_coherent(acm->dev, readsize,
				GFP_KERNEL, &rb->dma);
		if (!rb->base) {
			dev_dbg(&intf->dev,
				"out of memory (read bufs usb_alloc_coherent)\n");
			goto alloc_fail7;
		}
	}
	for (i = 0; i < ACM_NW; i++) {
		struct acm_wb *snd = &(acm->wb[i]);

		snd->urb = usb_alloc_urb(0, GFP_KERNEL);
		if (snd->urb == NULL) {
			dev_dbg(&intf->dev,
				"out of memory (write urbs usb_alloc_urb)");
			goto alloc_fail8;
		}

		usb_fill_bulk_urb(snd->urb, usb_dev,acm->tx_endpoint,
			NULL, acm->writesize, acm_write_bulk, snd);
		dev_info(&usb_dev->dev, "%s: BULK endpoint\n", __func__);

		snd->urb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
		snd->instance = acm;
	}

	usb_set_intfdata(intf, acm);

	i = device_create_file(&intf->dev, &dev_attr_bmCapabilities);
	if (i < 0)
		goto alloc_fail8;
#if 0
	//DTR
	acm_set_control(acm, acm->ctrlout = ACM_CTRL_DTR | ACM_CTRL_RTS);

	acm->line.dwDTERate = cpu_to_le32(24000000);
	acm->line.bDataBits = 8;
	acm_set_line(acm, &acm->line);

	usb_fill_int_urb(acm->ctrlurb, acm->dev,
			 acm->ctrl_endpoint,
			 acm->ctrl_buffer, acm->ctrlsize, acm_ctrl_irq, acm,
			 /* works around buggy devices */
			 acm->bInterval ? acm->bInterval : 0xff);
	acm->ctrlurb->transfer_flags |= URB_NO_TRANSFER_DMA_MAP;
	acm->ctrlurb->transfer_dma = acm->ctrl_dma;
#endif	
	dev_info(&intf->dev, "ttyACM%d: USB ACM device\n", minor);
#if 0
	printk("%s: id=%d, ctrlsize=%d, bInterval=%d\n", __func__, acm->cid,acm->ctrlsize, acm->bInterval);
	rv = usb_submit_urb(acm->ctrlurb, GFP_KERNEL);
	if (rv) {
		printk("usb_submit_urb(ctrl irq) failed %d\n", rv);
		goto alloc_fail8;
	}
#endif

	usb_driver_claim_interface(&acm_usb_driver, data_interface, acm);
	//usb_driver_claim_interface(&acm_usb_driver, control_interface, acm);
	usb_set_intfdata(control_interface, acm);
//	pm_suspend_ignore_children(&data_interface->dev, true);
//	pm_suspend_ignore_children(&intf->dev, true);
//	usb_get_intf(control_interface);
	usb_enable_autosuspend(acm->dev);
	INIT_DELAYED_WORK(&acm->pm_runtime_work, acm_runtime_start);

	share_acm.usb_connected = 1;
	acm->parent = &share_acm;
	acm_table[minor] = acm;
	
	if(minor==ACM_INTERFACES-1)
		schedule_delayed_work(&acm->pm_runtime_work, msecs_to_jiffies(500));
	printk("JOHNLAY OK\n");
	return 0;
alloc_fail8:
	for (i = 0; i < ACM_NW; i++)
		usb_free_urb(acm->wb[i].urb);
alloc_fail7:
	acm_read_buffers_free(acm);
alloc_fail6:
	for (i = 0; i < num_rx_buf; i++)
		usb_free_urb(acm->ru[i].urb);
	usb_free_urb(acm->ctrlurb);
alloc_fail5:
	acm_write_buffers_free(acm);
alloc_fail4:
	usb_free_coherent(usb_dev, ctrlsize, acm->ctrl_buffer, acm->ctrl_dma);
alloc_fail2:
	kfree(acm);
alloc_fail:
	return -ENOMEM;
}

static void acm_disconnect(struct usb_interface *intf)
{
	struct acm *acm = usb_get_intfdata(intf);
	struct usb_device *usb_dev = interface_to_usbdev(intf);

	/* sibling interface is already cleaning up */
	if (!acm)
		return;
	
	mutex_lock(&open_mutex);
	device_remove_file(&acm->control->dev, &dev_attr_bmCapabilities);
	acm->dev = NULL;
	usb_set_intfdata(acm->control, NULL);
	usb_set_intfdata(acm->data, NULL);

	stop_data_traffic(acm);

	acm_write_buffers_free(acm);
	usb_free_coherent(usb_dev, acm->ctrlsize, acm->ctrl_buffer,
			  acm->ctrl_dma);
	acm_read_buffers_free(acm);

	if (!acm->combined_interfaces)
		usb_driver_release_interface(&acm_usb_driver, intf == acm->control ?
					acm->data : acm->control);
	wake_unlock_pm(acm->parent);
	kfree(acm);	
	acm_table[acm->minor] = NULL;
	share_acm.usb_connected = 0;
	share_acm.resume_debug = 0;
	share_acm.dpm_suspending = 0;
	share_acm.skip_hostwakeup = 0;

	mutex_unlock(&open_mutex);
	printk("%s: ACM disconnect!\n",__func__);
}
static int acm_is_opened(struct acm_ch *ch)
{
	struct acm *acm = acm_table[ch->cid];

	return ACM_READY(acm);
}
static int acm_is_connected(struct acm_ch *ch)
{
	struct acm *acm = acm_table[ch->cid];

	return ACM_CONNECTED(acm);
}
static int acm_get_write_size(struct acm_ch *ch)
{
	struct acm *acm = acm_table[ch->cid];
	
	if(ACM_READY(acm))
	{
		return acm->writesize ? acm->writesize : 512;
	}
	
	return 512;
}
#ifdef CONFIG_PM
static int acm_suspend(struct usb_interface *intf, pm_message_t message)
{
	struct acm *acm = usb_get_intfdata(intf);
	int cnt;
	int i;
	wake_lock_timeout_pm(acm->parent);	
	dev_dbg(&intf->dev, "%s: interface number%d\n", __func__,intf->altsetting->desc.bInterfaceNumber);
	if (message.event & PM_EVENT_AUTO) {
		int b;

		spin_lock_irq(&acm->read_lock);
		spin_lock(&acm->write_lock);
		b = acm->processing + acm->transmitting;
		spin_unlock(&acm->write_lock);
		spin_unlock_irq(&acm->read_lock);
		if (b)
			return -EBUSY;
	}

	spin_lock_irq(&acm->read_lock);
	spin_lock(&acm->write_lock);
	cnt = acm->susp_count++;
	spin_unlock(&acm->write_lock);
	spin_unlock_irq(&acm->read_lock);

	if (cnt)
		return 0;
	/*
	we treat opened interfaces differently,
	we must guard against open
	*/
	mutex_lock(&acm->mutex);
	if (acm->count)
		stop_data_traffic(acm);

	mutex_unlock(&acm->mutex);
	return 0;
}

static int acm_resume(struct usb_interface *intf)
{
	struct acm *acm = usb_get_intfdata(intf);
	struct acm_wb *wb;
	int rv = 0;
	int cnt;

	dev_dbg(&intf->dev, "%s\n", __func__);

	wake_lock_pm(acm->parent);
	spin_lock_irq(&acm->read_lock);
	acm->susp_count -= 1;
	cnt = acm->susp_count;
	spin_unlock_irq(&acm->read_lock);

	if (cnt)
		return 0;

	mutex_lock(&acm->mutex);
	if (acm->count) {
		spin_lock_irq(&acm->write_lock);
		if (acm->delayed_wb) {
			wb = acm->delayed_wb;
			acm->delayed_wb = NULL;
			spin_unlock_irq(&acm->write_lock);
			acm_start_wb(acm, wb);
		} else {
			spin_unlock_irq(&acm->write_lock);
		}

		/*
		 * delayed error checking because we must
		 * do the write path at all cost
		 */
		if (rv < 0)
			goto err_out;

		tasklet_schedule(&acm->urb_task);
	}

err_out:
	mutex_unlock(&acm->mutex);
	return rv;
}

static int acm_reset_resume(struct usb_interface *intf)
{
	struct acm *acm = usb_get_intfdata(intf);
//	struct tty_struct *tty;
	int ret = 0;

	dev_dbg(&intf->dev, "%s\n", __func__);
	mutex_lock(&acm->mutex);
	if(acm->cid==ACM_INTERFACES-1 && acm->data == intf)
		schedule_delayed_work(&acm->pm_runtime_work, msecs_to_jiffies(500));
	mutex_unlock(&acm->mutex);
	ret = acm_resume(intf);
	return ret;
}

#endif /* CONFIG_PM */

#define INTEL_BOOTROM_ACM_INFO(x) \
		USB_DEVICE_AND_INTERFACE_INFO(0x058b, x, \
		USB_CLASS_COMM, USB_CDC_SUBCLASS_ACM, \
		USB_CDC_PROTO_NONE)
		
#define INTEL_PSI_ACM_INFO(x) \
		USB_DEVICE_AND_INTERFACE_INFO(0x058b, x, \
		USB_CLASS_COMM, USB_CDC_SUBCLASS_ACM, \
		USB_CDC_PROTO_NONE)
		
#define INTEL_MAIN_ACM_INFO(x) \
		USB_DEVICE_AND_INTERFACE_INFO(0x01519, x, \
		USB_CLASS_COMM, USB_CDC_SUBCLASS_ACM, \
		USB_CDC_ACM_PROTO_AT_V25TER)		
/*
 * USB driver structure.
 */

static const struct usb_device_id acm_ids[] = {
	/* quirky and broken devices */
//	{ INTEL_BOOTROM_ACM_INFO(0x0041),
//	  .driver_info = ACM_NO_MAIN,
//	},/*intel acm bootrom hsic device*/
//	{ INTEL_PSI_ACM_INFO(0x0015), 
//	  .driver_info = ACM_NO_MAIN,
//	 },/* intel ACM PSI hsic device*/
	{ INTEL_MAIN_ACM_INFO(0x0020),},/*intel acm main hsic device*/
	{ }

};

MODULE_DEVICE_TABLE(usb, acm_ids);

static struct usb_driver acm_usb_driver = {
	.name =		"cdc_acm",
	.probe =	acm_probe,
	.disconnect =	acm_disconnect,
#ifdef CONFIG_PM
	.suspend =	acm_suspend,
	.resume =	acm_resume,
	.reset_resume =	acm_reset_resume,
#endif
	.id_table =	acm_ids,
#ifdef CONFIG_PM
	.supports_autosuspend = 1,
#endif
};

/*
 * Init / exit.
 */
static int acm_notifier_event(struct notifier_block *this,
		unsigned long event, void *ptr)
{
	struct acm *acm= &share_acm;
	switch (event) {
	case PM_SUSPEND_PREPARE:
		acm->dpm_suspending = 1;
		printk("%s: PM_SUSPEND_PREPARE\n", __func__);
		return NOTIFY_OK;
	case PM_POST_SUSPEND:
		acm->dpm_suspending = 0;
		if (acm->usb_connected)
			schedule_work(&acm->post_resume_work);
		printk("%s: PM_POST_SUSPEND\n", __func__);
		return NOTIFY_OK;
	}
	return NOTIFY_DONE;
}

static struct notifier_block acm_pm_notifier = {
	.notifier_call = acm_notifier_event,
};
struct acm_ch *acm_register(unsigned int cid, void (**low_notify)(struct acm_ch *),  int (*rxcb)(void*, char *,int, int, int *), void(*wrcb)(void*), void *priv )
{
	if(cid < ACM_INTERFACES)
	{
		*low_notify = acm_kick;
		acm_channel[cid]->rx_cb = rxcb;
		acm_channel[cid]->wr_cb = wrcb;
		acm_channel[cid]->cid = cid;
		acm_channel[cid]->priv = priv;
		acm_channel[cid]->open = acm_open;
		acm_channel[cid]->close = acm_close;
		acm_channel[cid]->write = acm_write;
		acm_channel[cid]->is_opened = acm_is_opened;
		acm_channel[cid]->is_connected = acm_is_connected;		
		acm_channel[cid]->get_write_size = acm_get_write_size;
		return acm_channel[cid];
	}
	return NULL;
}
int __init acm_init(void)
{
	int i;
	int retval;

	for(i=0; i<ACM_INTERFACES;i++)
	{
		acm_channel[i] = kzalloc(sizeof(struct acm_ch), GFP_KERNEL);
		acm_table[i] = NULL;
	}
	
	acm_wake_lock_init(&share_acm);
	share_acm.resume_debug = 0;
	share_acm.dpm_suspending = 0;
	share_acm.skip_hostwakeup = 0;
	share_acm.usb_connected = 0;
	share_acm.parent = NULL;
	INIT_WORK(&share_acm.post_resume_work, acm_post_resume_work);
	
	retval = usb_register(&acm_usb_driver);
	if (retval) {
		acm_wake_lock_destroy(&share_acm);
		return retval;
	}
	
	register_pm_notifier(&acm_pm_notifier);
	printk(KERN_INFO KBUILD_MODNAME ": " DRIVER_VERSION ":"
	       DRIVER_DESC "\n");

	return 0;
}

void __exit acm_exit(void)
{
	int i;
	
	usb_deregister(&acm_usb_driver);
	unregister_pm_notifier(&acm_pm_notifier);
	for(i=0; i<ACM_INTERFACES;i++)
	{
		kfree(acm_channel[i]);
	}
	wake_unlock_pm(&share_acm);
	acm_wake_lock_destroy(&share_acm);	
}

MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");


