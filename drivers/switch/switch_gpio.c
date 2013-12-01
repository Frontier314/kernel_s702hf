/*
 *  drivers/switch/switch_gpio.c
 *
 * Copyright (C) 2008 Google, Inc.
 * Author: Mike Lockwood <lockwood@android.com>
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/nvm.h>
#include <linux/slab.h>
#include <linux/switch.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>


static int switch_gpio_active = 0;
int get_board_id(void);
int board_id=1;

struct gpio_switch_data {
	struct switch_dev sdev;
	unsigned gpio;
	const char *name_on;
	const char *name_off;
	const char *state_on;
	const char *state_off;
	int irq;
	struct work_struct work;
};

static void gpio_switch_work(struct work_struct *work)
{
	int state;
	struct gpio_switch_data	*data =
		container_of(work, struct gpio_switch_data, work);

	state = gpio_get_value(data->gpio);
	switch_set_state(&data->sdev, state);
}

static irqreturn_t gpio_irq_handler(int irq, void *dev_id)
{
	struct gpio_switch_data *switch_data =
	    (struct gpio_switch_data *)dev_id;

	schedule_work(&switch_data->work);
	return IRQ_HANDLED;
}

static ssize_t switch_gpio_print_state(struct switch_dev *sdev, char *buf)
{
	struct gpio_switch_data	*switch_data =
		container_of(sdev, struct gpio_switch_data, sdev);
	const char *state;
	if (switch_get_state(sdev))
		state = switch_data->state_on;
	else
		state = switch_data->state_off;

	if (state)
		return sprintf(buf, "%s\n", state);
	return -1;
}

static int gpio_switch_probe(struct platform_device *pdev)
{
	struct gpio_switch_platform_data *pdata = pdev->dev.platform_data;
	struct gpio_switch_data *switch_data;
	int ret = 0;

	if (!pdata)
		return -EBUSY;

	switch_data = kzalloc(sizeof(struct gpio_switch_data), GFP_KERNEL);
	if (!switch_data)
		return -ENOMEM;

	switch_data->sdev.name = pdata->name;
	switch_data->gpio = pdata->gpio;
	switch_data->name_on = pdata->name_on;
	switch_data->name_off = pdata->name_off;
	switch_data->state_on = pdata->state_on;
	switch_data->state_off = pdata->state_off;
	switch_data->sdev.print_state = switch_gpio_print_state;

    ret = switch_dev_register(&switch_data->sdev);
	if (ret < 0)
		goto err_switch_dev_register;

	ret = gpio_request(switch_data->gpio, pdev->name);
	if (ret < 0)
		goto err_request_gpio;

	ret = gpio_direction_input(switch_data->gpio);
	if (ret < 0)
		goto err_set_gpio_input;

	INIT_WORK(&switch_data->work, gpio_switch_work);

	switch_data->irq = gpio_to_irq(switch_data->gpio);
	if (switch_data->irq < 0) {
		ret = switch_data->irq;
		goto err_detect_irq_num_failed;
	}

	ret = request_irq(switch_data->irq, gpio_irq_handler,
			  IRQF_TRIGGER_LOW, pdev->name, switch_data);
	if (ret < 0)
		goto err_request_irq;

	/* Perform initial detection */
	gpio_switch_work(&switch_data->work);

	return 0;

err_request_irq:
err_detect_irq_num_failed:
err_set_gpio_input:
	gpio_free(switch_data->gpio);
err_request_gpio:
    switch_dev_unregister(&switch_data->sdev);
err_switch_dev_register:
	kfree(switch_data);

	return ret;
}

static int __devexit gpio_switch_remove(struct platform_device *pdev)
{
	struct gpio_switch_data *switch_data = platform_get_drvdata(pdev);

	cancel_work_sync(&switch_data->work);
	gpio_free(switch_data->gpio);
    switch_dev_unregister(&switch_data->sdev);
	kfree(switch_data);

	return 0;
}

static struct platform_driver gpio_switch_driver = {
	.probe		= gpio_switch_probe,
	.remove		= __devexit_p(gpio_switch_remove),
	.driver		= {
		.name	= "switch-gpio",
		.owner	= THIS_MODULE,
	},
};

static int __init gpio_switch_init(void)
{
	return platform_driver_register(&gpio_switch_driver);
}

static void __exit gpio_switch_exit(void)
{
	platform_driver_unregister(&gpio_switch_driver);
}

#define D2H_FLAG_DEFAULT 	0xFFFFFFFF
#define D2H_FLAG_ACTIVE 	0xAABBCCDD

enum {
	D2H_DEBUG 		= 	0,
	D2H_HDSET,
	D2H_DEBUG_NOFLAG,
	D2H_HDSET_NOFLAG,
};

static void debug2hs_set_flag(int on)
{
	int flag = D2H_FLAG_DEFAULT;

	if(0 == on) {
		// switch headset to debug
		flag = D2H_FLAG_ACTIVE;
	}

	write_nvm(NVM_HSFLAG, (char *)&flag, sizeof(int));
}
int get_board_id(void)
{
	unsigned int id = 0;
#if 0
	int ret;
	ret = gpio_request(S5PV210_MP05(6), "MP05");
	if (ret < 0)
	{
		printk(KERN_ERR"fail to request S5PV210_MP05(6)\n ");
		return ret;
	}
	s3c_gpio_cfgpin(S5PV210_MP05(6), S3C_GPIO_INPUT);//output
	s3c_gpio_setpull(S5PV210_MP05(6), S3C_GPIO_PULL_NONE);
	id=gpio_get_value(S5PV210_MP05(6));
	gpio_free(S5PV210_MP05(6));
#endif
	return id;
}
static void debug_switch_to_hs(unsigned int on)
{
	int ret;

	printk(KERN_INFO"------------%s: on = %d\n", __FUNCTION__, on);
	if(on)
	{
		ret = gpio_request(EXYNOS4_GPF2(7), "GPF2_7_SW_DBG_HS");
		if (ret < 0)
		{
			printk(KERN_ERR"fail to request EXYNOS4_GPF2(7)\n ");
			return;
		}
		s3c_gpio_cfgpin(EXYNOS4_GPF2(7), S3C_GPIO_OUTPUT);//output
        s3c_gpio_setpull(EXYNOS4_GPF2(7), S3C_GPIO_PULL_NONE);
		if(board_id){//PVT
			gpio_set_value(EXYNOS4_GPF2(7), 0);
			printk(KERN_ALERT"------------%s: The board id is PVT & MP on state = %d\n", __FUNCTION__,
					gpio_get_value(EXYNOS4_GPF2(7)));
		}
		else{		//DVT1&DVT2
			gpio_set_value(EXYNOS4_GPF2(7), 1);
			printk(KERN_ALERT"------------%s: The board id is DVT1 & DVT2 on state = %d\n", __FUNCTION__,
					gpio_get_value(EXYNOS4_GPF2(7)));
		}
		gpio_free(EXYNOS4_GPF2(7));
	}
	else
	{
		ret = gpio_request(EXYNOS4_GPF2(7), "GPJ4");
		if (ret < 0)
		{
			printk(KERN_ERR"fail to request EXYNOS4_GPF2(7)\n ");
			return;
		}
		s3c_gpio_cfgpin(EXYNOS4_GPF2(7), S3C_GPIO_OUTPUT);//output
		s3c_gpio_setpull(EXYNOS4_GPF2(7), S3C_GPIO_PULL_NONE);
		if(board_id){//PVT
			gpio_set_value(EXYNOS4_GPF2(7), 1);
			printk(KERN_ALERT"------------%s: The board id is PVT & MP off state = %d\n", __FUNCTION__,
					 gpio_get_value(EXYNOS4_GPF2(7)));
		}
		else{		//DVT1&DVT2
			gpio_set_value(EXYNOS4_GPF2(7), 0);
			printk(KERN_ALERT"------------%s: The board id is DVT1 & DVT2 on state = %d\n", __FUNCTION__,
					gpio_get_value(EXYNOS4_GPF2(7)));
		}
		gpio_free(EXYNOS4_GPF2(7));
	}
	return;
}

void debug2hs_early_set(int v)
{
	board_id=get_board_id();
	switch_gpio_active = v;

	debug_switch_to_hs(switch_gpio_active%2);
}

static int debug2hs_set(const char *val, struct kernel_param *kp)
{
    param_set_int(val, kp);

    switch(switch_gpio_active)
    {
        case D2H_DEBUG:
        case D2H_HDSET:
            debug2hs_set_flag(switch_gpio_active);
            debug_switch_to_hs(switch_gpio_active);
            break;
        case D2H_DEBUG_NOFLAG:
        case D2H_HDSET_NOFLAG:
            debug_switch_to_hs(switch_gpio_active%2);
            break;
        default:
            break;
    }

    return 0;
}

module_param_call(switch_gpio_active, debug2hs_set, param_get_int, &switch_gpio_active, S_IRUGO | S_IWUSR);

module_init(gpio_switch_init);
module_exit(gpio_switch_exit);

MODULE_AUTHOR("Mike Lockwood <lockwood@android.com>");
MODULE_DESCRIPTION("GPIO Switch driver");
MODULE_LICENSE("GPL");
