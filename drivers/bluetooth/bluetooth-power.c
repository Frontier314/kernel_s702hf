/* Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
/*
 * Bluetooth Power Switch Module
 * controls power to external Bluetooth device
 * with interface to power management device
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/rfkill.h>

//davied add for bt
#include <linux/io.h>
#include <mach/regs-gpio.h>
#include <plat/gpio-cfg.h>
#include <mach/gpio.h>
//#define EXYNOS4_GPC1CONPDN  (S5P_VA_GPIO+0x90)
//#define EXYNOS4_GPA0CONPDN  (S5P_VA_GPIO+0x10)
//#define EXYNOS4_GPA0PUDPDN  (S5P_VA_GPIO+0x14)

#if defined(CONFIG_BT_HCIUART_PS)
unsigned int bt_state_for_hostwakeup = 0;
static unsigned int bt_power_state = 0;
#endif
//

static int bluetooth_toggle_radio(void *data, bool blocked)
{
	int ret;
	int (*power_control)(int enable);

	power_control = data;
	ret = (*power_control)(!blocked);
	return ret;
}

static const struct rfkill_ops bluetooth_power_rfkill_ops = {
	.set_block = bluetooth_toggle_radio,
};

static int bluetooth_power_rfkill_probe(struct platform_device *pdev)
{
	struct rfkill *rfkill;
	int ret;

	rfkill = rfkill_alloc("bt_power", &pdev->dev, RFKILL_TYPE_BLUETOOTH,
			      &bluetooth_power_rfkill_ops,
			      pdev->dev.platform_data);

	if (!rfkill) {
		printk(KERN_DEBUG
			"%s: rfkill allocate failed\n", __func__);
		return -ENOMEM;
	}

	/* force Bluetooth off during init to allow for user control */
	rfkill_init_sw_state(rfkill, 1);

	ret = rfkill_register(rfkill);
	if (ret) {
		printk(KERN_DEBUG
			"%s: rfkill register failed=%d\n", __func__,
			ret);
		rfkill_destroy(rfkill);
		return ret;
	}

	platform_set_drvdata(pdev, rfkill);

	return 0;
}

static void bluetooth_power_rfkill_remove(struct platform_device *pdev)
{
	struct rfkill *rfkill;

	rfkill = platform_get_drvdata(pdev);
	if (rfkill)
		rfkill_unregister(rfkill);
	rfkill_destroy(rfkill);
	platform_set_drvdata(pdev, NULL);
}

static int __init bt_power_probe(struct platform_device *pdev)
{
	int ret = 0;

	printk(KERN_DEBUG "%s\n", __func__);

	if (!pdev->dev.platform_data) {
		printk(KERN_ERR "%s: platform data not initialized\n",
				__func__);
		return -ENOSYS;
	}

	ret = bluetooth_power_rfkill_probe(pdev);

	return ret;
}

static int __devexit bt_power_remove(struct platform_device *pdev)
{
	printk(KERN_DEBUG "%s\n", __func__);

	bluetooth_power_rfkill_remove(pdev);

	return 0;
}

//#define s5p_gpio_set_conpdn  s5p_gpio_set_pd_cfg



static int bt_suspend_mode(struct platform_device *dev, pm_message_t state)
{
        int tmp;
#if defined(CONFIG_BT_HCIUART_PS)
	if(bt_power_state){
#if 0
        	tmp=__raw_readl(EXYNOS4_GPC1CONPDN);  //output bt_reset and bt_shutdown 1 and output bt_wakeup 0
        	tmp |= (0x1<<0) | (0x1 <<2);
        	tmp &= ~((0x1<<1) | (0x1<<3) | (0x1<<4) | (0x1<<5));
        	__raw_writel(tmp, EXYNOS4_GPC1CONPDN);
		printk(KERN_INFO "bt suspend here EXYNOS4_GPC1CONPDN %x = %x\n",S5PV310_GPC1CONPDN,tmp);

        	tmp=__raw_readl(EXYNOS4_GPA0CONPDN);   //enable bt_uart rx cts input and pull high enable tx rts output 1
        	tmp |= (0x1<<1) | (0x1<<2) | (0x1<<5) | (0x1 <<6);
        	tmp &= ~((0x1<<0) | (0x1<<3) | (0x1<<4) | (0x1<<7));
        	__raw_writel(tmp, EXYNOS4_GPA0CONPDN);
		printk(KERN_INFO "bt suspend here EXYNOS4_GPA0CONPDN %x = %x\n",S5PV310_GPA0CONPDN,tmp);

        	tmp=__raw_readl(EXYNOS4_GPA0PUDPDN);
        	tmp |= (0x1<<1) |(0x01<<5);
        	tmp &= ~((0x1<<0) | (0x1<<2) | (0x1<<3) |(0x1 <<4) | (0x1<<6) | (0x1<<7));
        	__raw_writel(tmp, EXYNOS4_GPA0PUDPDN);
		printk(KERN_INFO "bt suspend here EXYNOS4_GPA0PUDPDN %x = %x\n",S5PV310_GPA0PUDPDN,tmp);
#endif
 		/*output bt_reset and bt_shutdown 1 and output bt_wakeup 0*/
		//return 0;
		s5p_gpio_set_conpdn(EXYNOS4_GPF2(5),S5P_GPIO_OUTPUT1);
		s5p_gpio_set_conpdn(EXYNOS4_GPF2(6),S5P_GPIO_OUTPUT1);
		s5p_gpio_set_conpdn(EXYNOS4_GPF2(4),S5P_GPIO_OUTPUT0);

		s5p_gpio_set_conpdn(EXYNOS4_GPA0(0),S5P_GPIO_INPUT);
		s5p_gpio_set_conpdn(EXYNOS4_GPA0(1),S5P_GPIO_OUTPUT1);
		s5p_gpio_set_conpdn(EXYNOS4_GPA0(2),S5P_GPIO_INPUT);
		s5p_gpio_set_conpdn(EXYNOS4_GPA0(3),S5P_GPIO_OUTPUT1);

		/*enable bt_uart rx cts input and pull high enable tx rts output 1*/
		s5p_gpio_set_pudpdn(EXYNOS4_GPA0(0), S5P_GPIO_PULL_DOWN_ENABLE);
		s5p_gpio_set_pudpdn(EXYNOS4_GPA0(1), S5P_GPIO_PULL_UP_DOWN_DISABLE);
		s5p_gpio_set_pudpdn(EXYNOS4_GPA0(2), S5P_GPIO_PULL_DOWN_ENABLE);
		s5p_gpio_set_pudpdn(EXYNOS4_GPA0(3), S5P_GPIO_PULL_UP_DOWN_DISABLE);

		bt_state_for_hostwakeup = 1; //if the value is not set when host_wakeup interrupt will not action
	}
#endif
	return 0;
}

static int bt_resume_back(struct platform_device *dev)
{
#if defined(CONFIG_BT_HCIUART_PS)
	if(bt_power_state){
		printk(KERN_INFO"bt resume back !!!!!\n");
		bt_state_for_hostwakeup = 0;	
	}
#endif
	return 0;
}

static struct platform_driver bt_power_driver = {
	.probe = bt_power_probe,
	.remove = __devexit_p(bt_power_remove),
	.driver = {
		.name = "bt_power",
		.owner = THIS_MODULE,
	},
	.suspend = bt_suspend_mode,
	.resume = bt_resume_back,
};

static int __init bluetooth_power_init(void)
{
	int ret;

	printk(KERN_DEBUG "%s\n", __func__);
	ret = platform_driver_register(&bt_power_driver);
	return ret;
}

static void __exit bluetooth_power_exit(void)
{
	printk(KERN_DEBUG "%s\n", __func__);
	platform_driver_unregister(&bt_power_driver);
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("shenzhou Bluetooth power control driver");
MODULE_VERSION("1.30");

module_init(bluetooth_power_init);
module_exit(bluetooth_power_exit);
