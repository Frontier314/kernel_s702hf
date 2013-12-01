/* linux/arch/arm/mach-s5pv210/herring-btlpm.c
 * Copyright (C) 2010 Samsung Electronics. All rights reserved.
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
 * 2012-4-28, Samsung SSCR shaoguodong, for lenovo S3 bt
 */

#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/hrtimer.h>
#include <linux/irq.h>
#include <asm/mach-types.h>
#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <plat/gpio-cfg.h>
#include <linux/module.h>

#define GPIO_BT_WAKE EXYNOS4_GPF2(4)
#define IRQ_BT_HOST_WAKE IRQ_EINT(24)
#define GPIO_BT_HOST_WAKE EXYNOS4_GPX3(0)
#define GPIO_BT_RST EXYNOS4_GPF2(6)
#define GPIO_LEVEL_LOW 0
#define GPIO_LEVEL_HIGH 1

static struct herring_bt_lpm {
 struct hrtimer bt_lpm_timer;
 ktime_t bt_lpm_delay;
} bt_lpm;

static volatile int bt_state;
static struct wake_lock rfkill_wake_lock;

int check_bt_status(void)
{
	return bt_state;
}
EXPORT_SYMBOL(check_bt_status);

static enum hrtimer_restart bt_enter_lpm(struct hrtimer *timer)
{
 printk("[BT] bt enter sleep....\n");

 gpio_set_value(GPIO_BT_WAKE, 0);
 bt_state = 0;
 return HRTIMER_NORESTART;
}

/**
 * when ap enter lpa
 * uart can not be waked up
 * so we should tell bt,
 * do not send data to me,
 * or the data will missing
 * AND KERNEL WILL PANIC
 */
void bt_uart_rts_ctrl(int flag)
{
	if(flag) {
		/* BT RTS Set to HIGH */
		s3c_gpio_cfgpin(EXYNOS4_GPA0(3), S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(EXYNOS4_GPA0(3), S3C_GPIO_PULL_NONE);
		gpio_set_value(EXYNOS4_GPA0(3), 1);
	} else {
		/* BT RTS Set to LOW */
		s3c_gpio_cfgpin(EXYNOS4_GPA0(3), S3C_GPIO_OUTPUT);
		gpio_set_value(EXYNOS4_GPA0(3), 0);

		s3c_gpio_cfgpin(EXYNOS4_GPA0(3), S3C_GPIO_SFN(2));
		s3c_gpio_setpull(EXYNOS4_GPA0(3), S3C_GPIO_PULL_NONE);
	}
}
EXPORT_SYMBOL(bt_uart_rts_ctrl);

/* uart data on going  */
void herring_bt_uart_wake_peer(struct uart_port *port)
{
 if (!bt_lpm.bt_lpm_timer.function)
  return;
 bt_state = 1;
 hrtimer_try_to_cancel(&bt_lpm.bt_lpm_timer);
 gpio_set_value(GPIO_BT_WAKE, 1);
 hrtimer_start(&bt_lpm.bt_lpm_timer, bt_lpm.bt_lpm_delay, HRTIMER_MODE_REL);
}
EXPORT_SYMBOL(herring_bt_uart_wake_peer);

static irqreturn_t bt_host_wake_irq_handler(int irq, void *dev_id)
{
 printk("[BT] bt_host_wake_irq_handler start\n");
 bt_state = 1;
 wake_lock_timeout(&rfkill_wake_lock, 5*HZ);

 return IRQ_HANDLED;
}


static int __init bt_lpm_init(void)
{
 int ret;
 int irq;

 wake_lock_init(&rfkill_wake_lock, WAKE_LOCK_SUSPEND, "bt_host_wake");
 bt_state = 0;

 ret = gpio_request(GPIO_BT_WAKE, "gpio_bt_wake");
 if (ret) {
  printk(KERN_ERR "Failed to request gpio_bt_wake control\n");
  return 0;
 }

 gpio_direction_output(GPIO_BT_WAKE, GPIO_LEVEL_LOW);

 irq = IRQ_BT_HOST_WAKE;

	ret = request_irq(irq, bt_host_wake_irq_handler,
			IRQF_TRIGGER_RISING,
			"bt_host_wake_irq_handler", NULL);

 if (ret < 0) {
  pr_err("[BT] Request_irq failed\n");
  return -1;
 }

 enable_irq_wake(irq);

 hrtimer_init(&bt_lpm.bt_lpm_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
 bt_lpm.bt_lpm_delay = ktime_set(1, 0); /* 1 sec */
 bt_lpm.bt_lpm_timer.function = bt_enter_lpm;
 return 0;
}
device_initcall(bt_lpm_init);
