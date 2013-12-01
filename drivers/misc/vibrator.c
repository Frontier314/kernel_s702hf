/* include/asm/mach-msm/htc_pwrsink.h
 *
 * Copyright (C) 2008 HTC Corporation.
 * Copyright (C) 2007 Google, Inc.
 * Copyright (c) 2011 Code Aurora Forum. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/hrtimer.h>
#include <linux/sched.h>
#include <plat/exynos4.h>
#include <plat/gpio-cfg.h>
#include <linux/workqueue.h>
#include "../staging/android/timed_output.h"

#include <asm/gpio.h>
#include <linux/mutex.h>


/*
#include <mach/msm_rpcrouter.h>

#define PM_LIBPROG      0x30000061
#if (CONFIG_MSM_AMSS_VERSION == 6220) || (CONFIG_MSM_AMSS_VERSION == 6225)
#define PM_LIBVERS      0xfb837d0b
#else
#define PM_LIBVERS      0x10001
#endif

#define HTC_PROCEDURE_SET_VIB_ON_OFF	21
#define PMIC_VIBRATOR_LEVEL	(3000)
*/
//static struct work_struct work_vibrator_on;
static struct work_struct work_vibrator_off;
static struct hrtimer vibe_timer;
static struct mutex vib_lock;
/*
#ifdef CONFIG_PM8XXX_RPC_VIBRATOR
static void set_pmic_vibrator(int on)
{
	int rc;

	rc = pmic_vib_mot_set_mode(PM_VIB_MOT_MODE__MANUAL);
	if (rc) {
		pr_err("%s: Vibrator set mode failed", __func__);
		return;
	}

	if (on)
		rc = pmic_vib_mot_set_volt(PMIC_VIBRATOR_LEVEL);
	else
		rc = pmic_vib_mot_set_volt(0);

	printk(KERN_ALERT"=== seoul vibrator ===1 %s", __func__);
	if (rc)
		pr_err("%s: Vibrator set voltage level failed", __func__);
}
#else
static void set_pmic_vibrator(int on)
{
	static struct msm_rpc_endpoint *vib_endpoint;
	struct set_vib_on_off_req {
		struct rpc_request_hdr hdr;
		uint32_t data;
	} req;
	printk(KERN_ALERT"=== seoul vibrator ===2 %s", __func__);

	if (!vib_endpoint) {
		vib_endpoint = msm_rpc_connect(PM_LIBPROG, PM_LIBVERS, 0);
		if (IS_ERR(vib_endpoint)) {
			printk(KERN_ERR "init vib rpc failed!\n");
			vib_endpoint = 0;
			return;
		}
	}


	if (on)
		req.data = cpu_to_be32(PMIC_VIBRATOR_LEVEL);
	else
		req.data = cpu_to_be32(0);

	msm_rpc_call(vib_endpoint, HTC_PROCEDURE_SET_VIB_ON_OFF, &req,
		sizeof(req), 5 * HZ);
}
#endif
*/

static void set_pmic_vibrator(int on)
{
	if (on)
		gpio_set_value(EXYNOS4_GPZ(6), 1);
	else
		gpio_set_value(EXYNOS4_GPZ(6), 0);
}

/*
static void pmic_vibrator_on(struct work_struct *work)
{
	set_pmic_vibrator(1);
}
*/
static void pmic_vibrator_off(struct work_struct *work)
{
	set_pmic_vibrator(0);
}

/*
static void timed_vibrator_on(struct timed_output_dev *sdev)
{
	schedule_work(&work_vibrator_on);
}
*/
static void timed_vibrator_off(struct timed_output_dev *sdev)
{
	schedule_work(&work_vibrator_off);
}

static void vibrator_enable(struct timed_output_dev *dev, int value)
{
	mutex_lock(&vib_lock);
	hrtimer_cancel(&vibe_timer);
	cancel_work_sync(&work_vibrator_off);

	if (value == 0)
//		timed_vibrator_off(dev);
		set_pmic_vibrator(0);
	
	else {
		if(value > 0){
		value = (value > 15000 ? 15000 : value);

	//	timed_vibrator_on(dev);
		set_pmic_vibrator(1);

		hrtimer_start(&vibe_timer,
			      ktime_set(value / 1000, (value % 1000) * 1000000),
			      HRTIMER_MODE_REL);
		}else 
			printk(KERN_ERR "vibrator had been written a wrong value %d\n",value);
	}
	mutex_unlock(&vib_lock);
}

static int vibrator_get_time(struct timed_output_dev *dev)
{
	if (hrtimer_active(&vibe_timer)) {
		ktime_t r = hrtimer_get_remaining(&vibe_timer);
		struct timeval t = ktime_to_timeval(r);
		return t.tv_sec * 1000 + t.tv_usec / 1000;
	}
	return 0;
}

static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
	timed_vibrator_off(NULL);
	return HRTIMER_NORESTART;
}

static struct timed_output_dev pmic_vibrator = {
	.name = "vibrator",
	.get_time = vibrator_get_time,
	.enable = vibrator_enable,
};

void __init msm_init_pmic_vibrator(void)
{
	int err;
	err = gpio_request(EXYNOS4_GPZ(6), "vib enable");
	if(err)
		printk(KERN_ERR "Failed to request GPIO%d vib_enable err=%d\n",
				 EXYNOS4_GPZ(6), err);

	gpio_direction_output(EXYNOS4_GPZ(6), 0);
//	INIT_WORK(&work_vibrator_on, pmic_vibrator_on);
	INIT_WORK(&work_vibrator_off, pmic_vibrator_off);

	hrtimer_init(&vibe_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vibe_timer.function = vibrator_timer_func;
	
	mutex_init(&vib_lock);
	timed_output_dev_register(&pmic_vibrator);
}
module_init(msm_init_pmic_vibrator);
MODULE_DESCRIPTION("timed output pmic vibrator device");
MODULE_LICENSE("GPL");

