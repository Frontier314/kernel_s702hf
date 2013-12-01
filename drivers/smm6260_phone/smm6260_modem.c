/*
 * Modem control driver
 *
 * Copyright (C) 2010 Samsung Electronics Co.Ltd
 * Author: Suchang Woo <suchang.woo@samsung.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#define DEBUG

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/ioctl.h>
#include <linux/uaccess.h>
#include <linux/miscdevice.h>
#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#endif
#include <mach/regs-gpio.h>
#include <mach/gpio.h>
#include <mach/modem.h>
#include <linux/modemctl.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <plat/gpio-cfg.h>
#include <linux/poll.h>
#include <linux/io.h>

extern void usb_host_phy_init(void);
extern void usb_host_phy_off(void);
extern void usb_host_phy_suspend(void);
extern int usb_host_phy_resume(void);
enum {
	HOST_WAKEUP_LOW = 1,
	HOST_WAKEUP_WAIT_RESET,
} HOST_WAKEUP_STATE;

enum {
	MODEM_EVENT_RESET,
	MODEM_EVENT_CRASH,
	MODEM_EVENT_DUMP,
	MODEM_EVENT_CONNECT,
} MODEM_EVENT_TYPE;

/* FIXME: Don't use this except pm */
static struct modemctl *global_mc;
extern int acm_init(void);
extern int smd_init(void);
extern void  acm_exit(void);
extern void smd_exit(void);
extern int acm_request_resume(void);
extern int  s5pv210_ehci_power(int value);
static int modem_boot_enumeration(struct modemctl *mc);
static int modem_main_enumeration(struct modemctl *mc);


extern void s3c_otg_device_power_control(int is_active);
int mc_reconnect_gpio(void)
{
	struct modemctl *mc = global_mc;

	if (!mc)
		return -EFAULT;
	printk("TRY Reconnection.................................\n");

	gpio_set_value(mc->gpio_active_state, 0);
	msleep(10);
	gpio_set_value(mc->gpio_ipc_slave_wakeup, 1);
	msleep(10);
	gpio_set_value(mc->gpio_ipc_slave_wakeup, 0);
	msleep(10);
	gpio_set_value(mc->gpio_active_state, 1);

	return 0;
}
EXPORT_SYMBOL_GPL(mc_reconnect_gpio);
int mc_is_suspend_request(void)
{

	printk(KERN_DEBUG "%s:suspend requset val=%d\n", __func__,
		gpio_get_value(global_mc->gpio_suspend_request));
	return gpio_get_value(global_mc->gpio_suspend_request);
}
EXPORT_SYMBOL_GPL(mc_is_suspend_request);
int mc_is_reset(void){//haozz
	struct modemctl *mc = global_mc;
	if (!mc)
		return 0;
	return mc->cpreset_flag;
}
int mc_is_on(void)
{
	struct modemctl *mc = global_mc;
	if (!mc)
		return 0;
	return !mc_is_reset()&&mc->boot_done;
}
EXPORT_SYMBOL_GPL(mc_is_on);
void mc_hostwakeup_enable(bool enable)
{
	struct modemctl *mc = global_mc;
	if (!mc)
		return;
	/*diog.zhao,0614*/
	if(mc_is_on()&&NULL != mc->irq[1]){
	if(enable)
		enable_irq(mc->irq[1]);
	else
		disable_irq(mc->irq[1]);
		}
}
EXPORT_SYMBOL_GPL(mc_hostwakeup_enable);
int mc_prepare_resume(int ms_time)
{
	int val;
	struct completion done;
	struct modemctl *mc = global_mc;

	if (!mc)
		return -EFAULT;
	/*before do ipc, we should check if modem is on or not.*/
	if(!mc_is_on())
		return -ENODEV;
	val = gpio_get_value(mc->gpio_ipc_slave_wakeup);
	if (val) {
		//gpio_set_value(mc->gpio_ipc_slave_wakeup, 0);
		smm6260_set_slave_wakeup(0);
		dev_info(mc->dev, "svn SLAV_WUP:reset\n");
	}
	val = gpio_get_value(mc->gpio_ipc_host_wakeup);
	if (val == HOST_WUP_LEVEL) {
		dev_info(mc->dev, "svn HOST_WUP:high!\n");
		return MC_HOST_HIGH;
	}

	init_completion(&done);
	mc->l2_done = &done;
	//gpio_set_value(mc->gpio_ipc_slave_wakeup, 1);
	smm6260_set_slave_wakeup(1);
	dev_dbg(mc->dev, "AP>>CP:  SLAV_WUP:1,%d\n",
		gpio_get_value(mc->gpio_ipc_slave_wakeup));

	if (!wait_for_completion_timeout(&done, ms_time)) {
			/*before do ipc, we should check if modem is on or not.*/
			if(!mc_is_on())
				return -ENODEV;
		val = gpio_get_value(mc->gpio_ipc_host_wakeup);
		if (val == HOST_WUP_LEVEL) {
			dev_err(mc->dev, "maybe complete late.. %d\n", ms_time);
			mc->l2_done = NULL;
			return MC_SUCCESS;
		}
		dev_err(mc->dev, "Modem wakeup timeout %d\n", ms_time);
		//gpio_set_value(mc->gpio_ipc_slave_wakeup, 0);
		smm6260_set_slave_wakeup(0);
		dev_dbg(mc->dev, "AP>>CP:  SLAV_WUP:0,%d\n",
			gpio_get_value(mc->gpio_ipc_slave_wakeup));
		mc->l2_done = NULL;
		return MC_HOST_TIMEOUT;
	}
	return MC_SUCCESS;
}
EXPORT_SYMBOL_GPL(mc_prepare_resume);

//#ifdef CONFIG_SEC_DEBUG
/*
 * HSIC CP uploas scenario -
 * 1. CP send Crash message
 * 2. Rild save the ram data to file via HSIC
 * 3. Rild call the kernel_upload() for AP ram dump
 */
static void enumeration(struct modemctl *mc)
{
	 gpio_set_value(mc->gpio_active_state, 0);
}
//#else
//static void enumeration(struct modemctl *mc) {}
//#endif

static int modem_on(struct modemctl *mc)
{
	dev_info(mc->dev, "%s\n", __func__);
	if (!mc->ops || !mc->ops->modem_on)
		return -ENXIO;
        s3c_otg_device_power_control(0);
	//disable_irq(mc->irq[2]);
	mc->ops->modem_on();
	//enable_irq(mc->irq[2]);
	s3c_otg_device_power_control(1);
	return 0;
}
static int modem_on_normal(struct modemctl *mc)
{
	dev_info(mc->dev, "%s\n", __func__);
	if (!mc->ops || !mc->ops->modem_on)
		return -ENXIO;
	//disable_irq(mc->irq[2]);
	mc->ops->modem_on();
	//enable_irq(mc->irq[2]);
	return 0;
}

static int modem_off(struct modemctl *mc)
{
	dev_info(mc->dev, "%s\n", __func__);
	if (!mc->ops || !mc->ops->modem_off)
		return -ENXIO;
	//disable_irq(mc->irq[2]);
	mc->ops->modem_off();
	//enable_irq(mc->irq[2]);	
	return 0;
}

static int modem_reset(struct modemctl *mc)
{
	dev_info(mc->dev, "%s\n", __func__);
	if (!mc->ops || !mc->ops->modem_reset)
		return -ENXIO;
	//disable_irq(mc->irq[2]);
	mc->ops->modem_reset();
	//enable_irq(mc->irq[2]);
	return 0;
}

static int modem_boot(struct modemctl *mc)
{
	dev_info(mc->dev, "%s\n", __func__);
	if (!mc->ops || !mc->ops->modem_boot)
		return -ENXIO;

	mc->ops->modem_boot();

	return 0;
}

static int modem_get_active(struct modemctl *mc)
{
	dev_info(mc->dev, "%s\n", __func__);
	if (!mc->gpio_active_state || !mc->gpio_cp_reset)
		return -ENXIO;

	dev_info(mc->dev, "cp %d phone %d\n",
			gpio_get_value(mc->gpio_cp_reset),
			gpio_get_value(mc->gpio_active_state));

	if (gpio_get_value(mc->gpio_cp_reset))
		return gpio_get_value(mc->gpio_active_state);

	return 0;
}

static ssize_t show_control(struct device *d,
		struct device_attribute *attr, char *buf)
{
	char *p = buf;
	struct modemctl *mc = dev_get_drvdata(d);
	struct modem_ops *ops = mc->ops;

	if (ops) {
		if (ops->modem_on)
			p += sprintf(p, "on ");
		if (ops->modem_off)
			p += sprintf(p, "off ");
		if (ops->modem_reset)
			p += sprintf(p, "reset ");
		if (ops->modem_boot)
			p += sprintf(p, "boot ");
	} else {
		p += sprintf(p, "(No ops)");
	}

	p += sprintf(p, "\n");
	return p - buf;
}

static ssize_t store_control(struct device *d,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct modemctl *mc = dev_get_drvdata(d);

	if (!strncmp(buf, "on", 2)) {
		modem_on(mc);
		return count;
	}

	if (!strncmp(buf, "off", 3)) {
		modem_off(mc);
		return count;
	}

	if (!strncmp(buf, "reset", 5)) {
		modem_reset(mc);
		return count;
	}

	if (!strncmp(buf, "boot", 4)) {
		modem_boot(mc);
		return count;
	}

	if (!strncmp(buf, "renum", 6)) {
		enumeration(mc);
		return count;
	}
	if (!strncmp(buf, "phon", 4)) {
		gpio_set_value(mc->gpio_phone_on, 0);
		mdelay(1);
		gpio_set_value(mc->gpio_phone_on, 1);
		return count;
	}
	if (!strncmp(buf, "gsw=0", 5)) {
		gpio_set_value(mc->gpio_ipc_slave_wakeup, 0);
		return count;
	}
	if (!strncmp(buf, "gsw=1", 5)) {
		gpio_set_value(mc->gpio_ipc_slave_wakeup, 1);
		return count;
	}
	if (!strncmp(buf, "gat=0", 5)) {
		gpio_set_value(mc->gpio_active_state, 0);
		return count;
	}
	if (!strncmp(buf, "gat=1", 5)) {
		gpio_set_value(mc->gpio_active_state, 1);
		return count;
	}	
	return count;
}

static ssize_t show_status(struct device *d,
		struct device_attribute *attr, char *buf)
{
	char *p = buf;
	struct modemctl *mc = dev_get_drvdata(d);

	p += sprintf(p, "%d\n", modem_get_active(mc));

	return p - buf;
}

static ssize_t show_wakeup(struct device *d,
		struct device_attribute *attr, char *buf)
{
	struct modemctl *mc = dev_get_drvdata(d);
	int count = 0;

	if (!mc->gpio_ipc_host_wakeup)
		return -ENXIO;

	count += sprintf(buf + count, "%d\n",
			mc->wakeup_flag);

	return count;
}

static ssize_t store_wakeup(struct device *d,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct modemctl *mc = dev_get_drvdata(d);

	if (!strncmp(buf, "reset", 5)) {
		mc->wakeup_flag = HOST_WAKEUP_WAIT_RESET;
		dev_info(mc->dev, "%s: wakup_flag %d\n",
			__func__, mc->wakeup_flag);
		return count;
	}
	return 0;

}

static ssize_t show_debug(struct device *d,
		struct device_attribute *attr, char *buf)
{
	char *p = buf;
	int i;
	struct modemctl *mc = dev_get_drvdata(d);

	for (i = 0; i < ARRAY_SIZE(mc->irq); i++) {
		if (mc->irq[i])
			p += sprintf(p, "Irq %d: %d\n", i, mc->irq[i]);
	}

	p += sprintf(p, "GPIO ----\n");

	if (mc->gpio_phone_on)
		p += sprintf(p, "\t%3d %d : phone on\n", mc->gpio_phone_on,
				gpio_get_value(mc->gpio_phone_on));
	if (mc->gpio_phone_active)
		p += sprintf(p, "\t%3d %d : phone active\n",
				mc->gpio_phone_active,
				gpio_get_value(mc->gpio_phone_active));
	if (mc->gpio_pda_active)
		p += sprintf(p, "\t%3d %d : pda active\n", mc->gpio_pda_active,
				gpio_get_value(mc->gpio_pda_active));
	if (mc->gpio_cp_reset)
		p += sprintf(p, "\t%3d %d : CP reset\n", mc->gpio_cp_reset,
				gpio_get_value(mc->gpio_cp_reset));
	if (mc->gpio_usim_boot)
		p += sprintf(p, "\t%3d %d : USIM boot\n", mc->gpio_usim_boot,
				gpio_get_value(mc->gpio_usim_boot));
	if (mc->gpio_flm_sel)
		p += sprintf(p, "\t%3d %d : FLM sel\n", mc->gpio_flm_sel,
				gpio_get_value(mc->gpio_flm_sel));
	if (mc->gpio_ipc_host_wakeup)
		p += sprintf(p, "\t%3d %d : hw sel\n", mc->gpio_ipc_host_wakeup,
				gpio_get_value(mc->gpio_ipc_host_wakeup));
	if (mc->gpio_ipc_slave_wakeup)
		p += sprintf(p, "\t%3d %d : sw sel\n", mc->gpio_ipc_slave_wakeup,
				gpio_get_value(mc->gpio_ipc_slave_wakeup));
	if (mc->gpio_cp_reset_int)
		p += sprintf(p, "\t%3d %d : cp silent reset int\n", mc->gpio_cp_reset_int,
				gpio_get_value(mc->gpio_cp_reset_int));	
	p += sprintf(p, "Support types ---\n");

	return p - buf;
}

static ssize_t show_usbctl(struct device *d,
		struct device_attribute *attr, char *buf)
{
	struct modemctl *mc = dev_get_drvdata(d);
	int count = 0;
	int val;

	val = gpio_get_value(GPIO_USB_SWITCH_CTRL);
	count += sprintf(buf, "%s\n", val? "pc2cp":"pc2ap");

	return count;
}

/*s3 usb switch control
 *pc2ap: pc --> ap otg
 *pc2cp: pc --> smm6260 modem
*/
static ssize_t store_usbctl(struct device *d,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct modemctl *mc = dev_get_drvdata(d);

#if 0
	/* added to test poll function, will be removed*/
	if (!strncmp(buf, "pc2ap", 5)) {
		if (!work_pending(&mc->cpreset_work))
			schedule_work(&mc->cpreset_work);		
		return count;
	}
#endif

	if (!strncmp(buf, "pc2ap", 5)) {
		gpio_direction_output(GPIO_USB_SWITCH_CTRL, 0);
		return count;
	}

	if (!strncmp(buf, "pc2cp", 5)) {
		gpio_direction_output(GPIO_USB_SWITCH_CTRL, 1);
		return count;
	}
	if (!strncmp(buf, "cdind", 5)) {//haozz add
	//	gpio_direction_output(GPIO_CP_COREDUMP_IND, 1);
		printk("haozz coredump output 0");
		return count;
	}	
	

	return 0;
}

static DEVICE_ATTR(control, 0664, show_control, store_control);
static DEVICE_ATTR(status, S_IRUGO, show_status, NULL);
static DEVICE_ATTR(wakeup, 0664, show_wakeup, store_wakeup);
static DEVICE_ATTR(debug, S_IRUGO, show_debug, NULL);
static DEVICE_ATTR(usbctl, 0664, show_usbctl, store_usbctl);

static struct attribute *modem_attributes[] = {
	&dev_attr_control.attr,
	&dev_attr_status.attr,
	&dev_attr_wakeup.attr,
	&dev_attr_debug.attr,
	&dev_attr_usbctl.attr,
	NULL
};

static const struct attribute_group modem_group = {
	.name = NULL,
	.attrs = modem_attributes,
};

void crash_event(int type)
{
	char *envs[2] = { NULL, NULL };

	if (!global_mc)
		return;
#if 0
	switch (type)
	{
	case MODEM_EVENT_RESET:
		envs[0] = "MAILBOX=cp_reset";
		kobject_uevent_env(&global_mc->dev->kobj, KOBJ_OFFLINE, envs);
		wake_up_interruptible(&global_mc->wq);
		printk("%s: MODEM_EVENT_RESET\n", __func__);
		break;
	case MODEM_EVENT_DUMP:	
		envs[0] = "MAILBOX=cp_dump";
		kobject_uevent_env(&global_mc->dev->kobj, KOBJ_OFFLINE, envs);
		wake_up_interruptible(&global_mc->wq);
		printk("%s: MODEM_EVENT_DUMP\n", __func__);
		break;
	case MODEM_EVENT_CONNECT:
		envs[0] = "MAILBOX=cp_connect";
		kobject_uevent_env(&global_mc->dev->kobj, KOBJ_ONLINE, envs);
		wake_up_interruptible(&global_mc->wq);
		printk("%s: MODEM_EVENT_CONNECT\n", __func__);
		break;
	case MODEM_EVENT_CRASH:
		envs[0] = "MAILBOX=cp_crash";
		kobject_uevent_env(&global_mc->dev->kobj, KOBJ_OFFLINE, envs);
		wake_up_interruptible(&global_mc->wq);
		printk("%s: MODEM_EVENT_CRASH\n", __func__);
		break;
	}
#else
	switch (type){
	case MODEM_EVENT_RESET:
		global_mc->cpreset_flag = 1;
		wake_up_interruptible(&global_mc->wq);
		break;
	case MODEM_EVENT_CRASH:
		global_mc->cpcrash_flag = 1;
		wake_up_interruptible(&global_mc->wq);
		break;
	}
#endif
}

/*
 * mc_work - PHONE_ACTIVE irq
 *  After
 *	CP Crash : PHONE_ACTIVE(L) + CP_DUMP_INT(H) (0xC9)
 *	CP Reset : PHONE_ACTIVE(L) + CP_DUMP_INT(L) (0xC7)
 *  Before
 *	CP Crash : PHONE_ACTIVE(L) + SUSPEND_REQUEST(H) (0xC9)
 *	CP Reset : PHONE_ACTIVE(L) + SUSPEND_REQUEST(L) (0xC7)
 */
 #if 0
static void mc_work(struct work_struct *work_arg)
{
	struct modemctl *mc = container_of(work_arg, struct modemctl,
		work.work);
	int error;
	int cpdump_int;
	char *envs[2] = { NULL, NULL };

	error = modem_get_active(mc);
	if (error < 0) {
		dev_err(mc->dev, "Not initialized\n");
		return;
	}

	cpdump_int = gpio_get_value(mc->gpio_cp_reset_int);
	dev_info(mc->dev, "PHONE ACTIVE: %d CP_DUMP_INT: %d\n",
		error, cpdump_int);

	envs[0] = cpdump_int ? "MAILBOX=cp_exit" : "MAILBOX=cp_reset";

	if (error && gpio_get_value(global_mc->gpio_phone_on)) {
		mc->cpcrash_flag = 0;
		mc->boot_done = 1;
		crash_event(MODEM_EVENT_CONNECT);
		wake_unlock(&mc->reset_lock);
	} else if (mc->boot_done) {
		if (modem_get_active(mc)) {
			wake_unlock(&mc->reset_lock);
			return;
		}
		if (mc->cpcrash_flag > 3) {
			dev_info(mc->dev, "send uevnet to RIL [cpdump_int:%d]\n",
				cpdump_int);
			crash_event(MODEM_EVENT_CRASH);
		} else {
			mc->cpcrash_flag++;
			schedule_delayed_work(&mc->work, msecs_to_jiffies(50));
		}
	}
}
#endif

static irqreturn_t modem_resume_thread(int irq, void *dev_id)
{
	struct modemctl *mc = (struct modemctl *)dev_id;
	int val = gpio_get_value(mc->gpio_ipc_host_wakeup);
	int err;

	dev_err(mc->dev, "CP>>AP:  HOST_WUP:%d\n", val);
	//if(mc_get_ignore_cpirq())
		//return IRQ_HANDLED;
	if (val != HOST_WUP_LEVEL) {
		//gpio_set_value(mc->gpio_ipc_slave_wakeup, 0);
		smm6260_set_slave_wakeup(0);
		mc->debug_cnt = 0;
		return IRQ_HANDLED;
	}


	if (val == HOST_WUP_LEVEL) {
		if (mc->l2_done) {
			complete(mc->l2_done);
			mc->l2_done = NULL;
		}
		err = acm_request_resume();
		if (err < 0)
			dev_err(mc->dev, "request resume failed: %d\n", err);
		mc->debug_cnt++;
	}
	if (mc->debug_cnt > 30) {
		dev_err(mc->dev, "Abnormal Host wakeup -- over 30times");
		//disable_irq(irq);
		mc->debug_cnt = 0;
		if(mc->boot_done)
		{
			//mc->cpcrash_flag = 1;
			crash_event(MODEM_EVENT_CRASH);
		}		
		/*crash_event(SVNET_ERROR_CRASH);*/
		/*panic("HSIC Disconnected");*/
		//msleep(1000);
		//enable_irq(irq);
	}


	if (!val
		&& mc->wakeup_flag == HOST_WAKEUP_WAIT_RESET) {
		mc->wakeup_flag = HOST_WAKEUP_LOW;
		dev_err(mc->dev, "%s: wakeup flag (%d)\n",
			__func__, mc->wakeup_flag);
	}

	return IRQ_HANDLED;
}

static irqreturn_t modem_irq_handler(int irq, void *dev_id)
{
	struct modemctl *mc = (struct modemctl *)dev_id;
	if(mc->boot_done)
	{
		int val = gpio_get_value(mc->gpio_suspend_request);
		//dev_info(mc->dev, "%s: CP_REQUEST_SUSPEND_INT:%d\n", __func__, val);
		//wake_lock_timeout(&mc->reset_lock, HZ*30);
		//if (!work_pending(&mc->work.work))
		//	schedule_delayed_work(&mc->work, msecs_to_jiffies(100));
		/*schedule_work(&mc->work);*/
	}
	return IRQ_HANDLED;
}

static void mc_cpreset_worker(struct work_struct *work)
{
	struct modemctl *mc = container_of(work, struct modemctl, cpreset_work);
	int val = gpio_get_value(mc->gpio_cp_reset_int);

	dev_info(mc->dev, "%s: CP_RESET_INT:%d\n", __func__, val);

	if((mc->boot_done)&&(mc->cpreset_flag != 1))
	{
		mc->cpreset_flag = 1;
		wake_up(&mc->wq);
		//crash_event(MODEM_EVENT_RESET);
	}
}

static irqreturn_t modem_cpreset_irq(int irq, void *dev_id)
{
	struct modemctl *mc = (struct modemctl *)dev_id;
	int val = gpio_get_value(mc->gpio_cp_reset_int);

	printk("%s: CP_RESET_INT:%d\n", __func__, val);

	if (!work_pending(&mc->cpreset_work))
		schedule_work(&mc->cpreset_work);

	return IRQ_HANDLED;
}

static void _free_all(struct modemctl *mc)
{
	int i;

	if (mc) {
		if (mc->ops)
			mc->ops = NULL;

		if (mc->group)
			sysfs_remove_group(&mc->dev->kobj, mc->group);

		for (i = 0; i < ARRAY_SIZE(mc->irq); i++) {
			if (mc->irq[i])
				free_irq(mc->irq[i], mc);
		}

		kfree(mc);
	}
}
int modem_open (struct inode *inode, struct file *file)
{
	return 0;
}
int modem_close (struct inode *inode, struct file *file)
{
	return 0;
}
int modem_ioctl (struct file *filp, unsigned int cmd, unsigned long arg)
{
	if(!global_mc)
		return -1;
	switch(cmd)
	{
		case MODEM_POWER_MAIN_CMD:
			modem_main_enumeration(global_mc);
			break;
		case MODEM_POWER_FLASH_CMD:
			modem_boot_enumeration(global_mc);
			break;
		case MODEM_POWER_OFF_CMD:
			modem_off(global_mc);
			break;
		case MODEM_POWER_ON_CMD:
			modem_on(global_mc);
			break;
		case MODEM_POWER_RESET_CMD:
			modem_reset(global_mc);
			break;		
	}
	return 0;
}

static ssize_t modem_read (struct file *filp, char __user * buffer, size_t count, loff_t * offset)
{
	int flag = 0;
	
	printk("%s: call\n", __func__);
	if(!global_mc)
		return -EFAULT;
	
	wait_event_interruptible(global_mc->wq, (global_mc->cpcrash_flag || global_mc->cpreset_flag || global_mc->cpdump_flag));
	
	flag = (global_mc->cpcrash_flag << CRASH_STATE_OFFSET) |\
		(global_mc->cpreset_flag << RESET_STATE_OFFSET) |\
		(global_mc->cpdump_flag << DUMP_STATE_OFFSET);
	printk("%s: modem event = 0x%x   1\n", __func__, flag);
	if(copy_to_user(buffer, &flag, sizeof(flag)))
		return -EFAULT;
	global_mc->boot_done =0;
	global_mc->cpcrash_flag =0;
	global_mc->cpreset_flag =0;
	global_mc->cpdump_flag =0 ;	
	printk("%s: modem event = 0x%x   2\n", __func__, flag);
	return 1;
}
static ssize_t modem_write (struct file *filp, const char __user *buffer, size_t count, loff_t *offset)
{
	if(!global_mc)
		return -1;

	if(count >= 4 && !strncmp(buffer, "main", 4))
	{
		modem_main_enumeration(global_mc);
	}
	if(count >= 5 && !strncmp(buffer, "flash", 5))
	{
		modem_boot_enumeration(global_mc);
	}
	if(count >= 3 && !strncmp(buffer, "off", 3))
	{
		modem_on(global_mc);
	}
	if(count >= 2 && !strncmp(buffer, "on", 2))
	{
		modem_off(global_mc);
	}	
	if(count >= 5 && !strncmp(buffer, "reset", 5))
	{
		modem_reset(global_mc);
	}	
	return count;
}

static unsigned int modem_poll(struct file *file, poll_table * wait)
{
	unsigned int mask = 0;

	poll_wait(file, &global_mc->wq, wait);
	if(global_mc->cpreset_flag == 1 || global_mc->cpcrash_flag == 1){
		mask |= POLLHUP;
		global_mc->cpreset_flag = 0;
		global_mc->cpcrash_flag = 0;
		global_mc->boot_done = 0;
	}

	return mask;
}

static struct file_operations modem_file_ops = {
	.owner = THIS_MODULE,
	.open = modem_open,
	.release = modem_close,
	.read = modem_read,
	.write = modem_write,
	.poll = modem_poll,
 	.unlocked_ioctl = modem_ioctl,
};

static struct miscdevice modem_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "modemctl",
	.fops = &modem_file_ops
};
#ifdef CONFIG_USB_EHCI_S5P
extern int s5p_ehci_power(int value);
#else
int s5p_ehci_power(int value)
{
	return 0;
}
#endif
static int modem_boot_enumeration(struct modemctl *mc)
{
//	struct completion done;
#if CONFIG_HAS_WAKELOCK
	wake_lock(&mc->reset_lock);	
#endif
	mc->boot_done =0;
	mc->cpcrash_flag =0;
	mc->cpreset_flag =0;
	mc->cpdump_flag =0;
	s5p_ehci_power(0);
	s5p_ehci_power(2);
	modem_on(mc);
 	mc->boot_done =1;
#if CONFIG_HAS_WAKELOCK
	wake_unlock(&mc->reset_lock);	
#endif
	return 0;	
}

static int modem_main_enumeration(struct modemctl *mc)
{
	struct completion done;
	unsigned long timeout;
#if CONFIG_HAS_WAKELOCK
	wake_lock(&mc->reset_lock);
#endif
	mc->boot_done =0;
	mc->cpcrash_flag =0;
	mc->cpreset_flag =0;
	mc->cpdump_flag =0 ;
	s3c_otg_device_power_control(0);//haozz for extenal modem power crash
	s5p_ehci_power(0);//power off hsic and remove modem devices for skip bootrom flash program
	modem_off(mc);
	msleep(100);
#if 1
//	smm6260_set_active_state(1);
	enable_irq(mc->irq[1]);
	modem_on_normal(mc);//power on modem
	msleep(100);	//waiting for the first time HSIC_HOST_WAKEUP go high 
	init_completion(&done);
	mc->l2_done = &done;
	printk("\n-------%%%%%%% wait no more than 20 s\n");
	timeout = wait_for_completion_timeout(&done, 20*HZ);//waiting for the second time HSIC_HOST_WAKEUP go high //wait modem power on
	mc->l2_done = NULL;
//	smm6260_set_active_state(0);
#else
	msleep(100);	
	modem_on(mc);
	s5p_ehci_power(1);
	msleep(100);
	s5p_ehci_power(0);
#endif
	//msleep(200);	//for ELPM initialization time
	s5p_ehci_power(1);//power on hsic
	s3c_otg_device_power_control(1);//haozz for extenal modem power crash
	if(timeout > 0){
		printk("Success: modem OK, time left: %d\n", timeout);
		mc->boot_done = 1;
	}else{
		printk("Error: fail to boot modem, time left: %d\n", timeout);
		/*must disable this irq, otherwise CP keeps interrupting AP*/
		free_irq(mc->irq[2], mc);
	}
	
#if CONFIG_HAS_WAKELOCK
	wake_unlock(&mc->reset_lock);
#endif
	return 0;
}
static irqreturn_t modem_cpreset_out_irq(int irq, void *dev_id)
{
        struct modemctl *mc = (struct modemctl *)dev_id;
	 int val = gpio_get_value(GPIO_CP_ABNORMAL_RESET_INT);

	 printk("\n\n%s: CP_RESET_OUT_INT:%d\n\n", __func__, val);
	if((mc->boot_done)&&(mc->cpreset_flag != 1))
	{
		mc->cpreset_flag = 1;
		wake_up(&mc->wq);
	}
	return IRQ_HANDLED;
}
static int __devinit modem_probe(struct platform_device *pdev)
{
	struct modem_platform_data *pdata = pdev->dev.platform_data;
	struct device *dev = &pdev->dev;
	struct modemctl *mc;
	int irq;
	int error;
	if (!pdata) {
		dev_err(dev, "No platform data\n");
		return -EINVAL;
	}

	mc = kzalloc(sizeof(struct modemctl), GFP_KERNEL);
	if (!mc) {
		dev_err(dev, "Failed to allocate device\n");
		return -ENOMEM;
	}

	mc->gpio_phone_on = pdata->gpio_phone_on;
	mc->gpio_phone_active = pdata->gpio_phone_active;
	mc->gpio_pda_active = pdata->gpio_pda_active;
	mc->gpio_cp_reset = pdata->gpio_cp_reset;
	mc->gpio_cp_req_reset = pdata->gpio_cp_req_reset;
	mc->gpio_ipc_slave_wakeup = pdata->gpio_ipc_slave_wakeup;
	mc->gpio_ipc_host_wakeup = pdata->gpio_ipc_host_wakeup;
	mc->gpio_suspend_request = pdata->gpio_suspend_request;
	mc->gpio_active_state = pdata->gpio_active_state;
	mc->gpio_usim_boot = pdata->gpio_usim_boot;
	mc->gpio_flm_sel = pdata->gpio_flm_sel;
	//mc->gpio_cp_dump_int = pdata->gpio_cp_dump_int;
	mc->gpio_cp_reset_int = pdata->gpio_cp_reset_int;
	mc->ops = &pdata->ops;
	mc->dev = dev;
	dev_set_drvdata(mc->dev, mc);

	error = sysfs_create_group(&mc->dev->kobj, &modem_group);
	if (error) {
		dev_err(dev, "Failed to create sysfs files\n");
		goto fail;
	}
	mc->group = &modem_group;

//	INIT_DELAYED_WORK(&mc->work, mc_work);
	INIT_WORK(&mc->cpreset_work, mc_cpreset_worker);
#if CONFIG_HAS_WAKELOCK
	wake_lock_init(&mc->reset_lock, WAKE_LOCK_SUSPEND, "modemctl");
	//wake_lock(&mc->reset_lock);
//This should be disable at first for some kernel page access error
#endif
	init_waitqueue_head(&mc->wq);
	mc->ops->modem_cfg();
#if 0
	irq = gpio_to_irq(pdata->gpio_suspend_request);

	error = request_irq(irq, modem_irq_handler,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			"phone_request_suspend", mc);
	if (error) {
		dev_err(dev, "Active Failed to allocate an interrupt(%d)\n", irq);
		goto fail;
	}
	mc->irq[0] = irq;
	enable_irq_wake(irq);
#endif
        /*add CP_RESET_OUT int detect function*/
		 irq=s5p_register_gpio_interrupt(GPIO_CP_ABNORMAL_RESET_INT);
		 printk("irq num =%d\n",irq);
		 //irq = gpio_to_irq(GPIO_CP_ABNORMAL_RESET_INT);
		 error = request_irq(irq, modem_cpreset_out_irq,
						IRQF_TRIGGER_FALLING,
		 		 		 "CP_RESET_OUT_INT", mc);
		 if (error) {
		 		 dev_err(dev, "Failed to allocate an interrupt(%d)\n", irq);
		 		 goto fail;
		 	}
	irq = gpio_to_irq(pdata->gpio_ipc_host_wakeup);

	error = request_threaded_irq(irq, NULL, modem_resume_thread,
			IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING,
			"IPC_HOST_WAKEUP", mc);

	if (error) {
		dev_err(dev, "Resume thread Failed to allocate an interrupt(%d)\n", irq);
		goto fail;
	}
	mc->irq[1] = irq;
	//enable_irq_wake(irq);
#if 1
	irq = gpio_to_irq(pdata->gpio_cp_reset_int);
	error = request_threaded_irq(irq, NULL, modem_cpreset_irq,
			IRQF_TRIGGER_RISING ,               //    IRQF_TRIGGER_FALLING,
			"CP_RESET_INT", mc);
	if (error) {
		dev_err(dev, "Failed to allocate an interrupt(%d)\n", irq);
		goto fail;
	}
	
	mc->irq[2] = irq;
	enable_irq_wake(irq);
#endif
	mc->debug_cnt = 0;

	device_init_wakeup(&pdev->dev, pdata->wakeup);
	platform_set_drvdata(pdev, mc);
	global_mc = mc;

	error = misc_register(&modem_miscdev);
	if(error)
	{
		dev_err(dev, "Failed to register modem control device\n");
		goto fail;
	}

	return 0;

fail:
	_free_all(mc);
	return error;
}

static int __devexit modem_remove(struct platform_device *pdev)
{
	struct modemctl *mc = platform_get_drvdata(pdev);

	flush_work(&mc->work.work);
	flush_work(&mc->cpreset_work);
	platform_set_drvdata(pdev, NULL);
#if CONFIG_HAS_WAKELOCK
	wake_lock_destroy(&mc->reset_lock);
#endif

	misc_deregister(&modem_miscdev);
	_free_all(mc);
	return 0;
}

#ifdef CONFIG_PM
static int modem_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct modemctl *mc = platform_get_drvdata(pdev);

	if (mc->ops && mc->ops->modem_suspend)
		mc->ops->modem_suspend();

	if (device_may_wakeup(dev) && smm6260_is_on())
		enable_irq_wake(mc->irq[1]);

	return 0;
}

static int modem_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct modemctl *mc = platform_get_drvdata(pdev);

	if (device_may_wakeup(dev) && smm6260_is_on())
		disable_irq_wake(mc->irq[1]);

	if (mc->ops && mc->ops->modem_resume)
		mc->ops->modem_resume();

	return 0;
}

static const struct dev_pm_ops modem_pm_ops = {
	.suspend	= modem_suspend,
	.resume		= modem_resume,
};
#endif

static struct platform_driver modem_driver = {
	.probe		= modem_probe,
	.remove		= __devexit_p(modem_remove),
	.driver		= {
		.name	= "smm_modem",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &modem_pm_ops,
#endif
	},
};

static int __init modem_init(void)
{
	platform_driver_register(&modem_driver);
	acm_init();
	smd_init();
	return 0; 
}

static void __exit modem_exit(void)
{
	smd_exit();
	acm_exit();
	platform_driver_unregister(&modem_driver);
}
module_init(modem_init);
module_exit(modem_exit);
