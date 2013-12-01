/*
 * Wakeup assist driver
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#include <linux/err.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <mach/regs-pmu.h>
#include <linux/suspend.h>

/* jeff, regulator, TODO: it will remove later, because these regulator should be control by driver*/
#if defined(CONFIG_MACH_STUTTGART) && defined(CONFIG_REGULATOR)
#include <linux/regulator/machine.h>
#endif
/* end */

#define DEV_NAME "wakeup_assist"

static int wakeup_assist_keycode[] = { KEY_POWER };

extern int regulator_suspend_prepare(suspend_state_t state);

static int __devinit wakeup_assist_probe(struct platform_device *pdev)
{
#if 0
	int error;
	struct input_dev *input_dev;

	input_dev = input_allocate_device();

	if (!input_dev)
		return -ENOMEM;

	input_dev->name = DEV_NAME;
	input_dev->id.bustype = BUS_HOST;
	input_dev->dev.parent = &pdev->dev;

	input_dev->evbit[0] = BIT_MASK(EV_KEY);

	input_set_capability(input_dev, EV_MSC, MSC_SCAN);

	input_dev->keycode = wakeup_assist_keycode;
	input_dev->keycodesize = sizeof(wakeup_assist_keycode[0]);
	input_dev->keycodemax = ARRAY_SIZE(wakeup_assist_keycode);

	__set_bit(wakeup_assist_keycode[0], input_dev->keybit);
	__clear_bit(KEY_RESERVED, input_dev->keybit);

	error = input_register_device(input_dev);
	if (error) {
		input_free_device(input_dev);
		return error;
	}

	platform_set_drvdata(pdev, input_dev);
#endif
	return 0;
}

static int __devexit wakeup_assist_remove(struct platform_device *pdev)
{
#if 0
	struct input_dev *input_dev = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);
	input_unregister_device(input_dev);
	input_free_device(input_dev);
#endif
	return 0;
}

/* jeff, enable regulator, TODO: it will remove later, because these regulator should be control by driver*/
static int wakeup_assist_suspend(struct device *dev)
{
	regulator_suspend_prepare(PM_SUSPEND_MEM);
}
/*end*/

static int wakeup_assist_resume(struct device *dev)
{
#if 0 //jeff
	struct platform_device *pdev = to_platform_device(dev);
	struct input_dev *input_dev = platform_get_drvdata(pdev);

	if (readl(S5P_WAKEUP_STAT) & 0x1) {
		input_report_key(input_dev, wakeup_assist_keycode[0], 0x2);
		input_sync(input_dev);
		input_report_key(input_dev, wakeup_assist_keycode[0], 0x0);
		input_sync(input_dev);
	}
#endif
/* jeff, enable regulator, TODO: it will remove later, because these regulator should be control by driver*/
#if defined(CONFIG_MACH_STUTTGART) && defined(CONFIG_REGULATOR)
	regulator_suspend_finish();
#endif
/* end */
	return 0;
}

static const struct dev_pm_ops wakeup_assist_pm_ops = {
	.suspend		= wakeup_assist_suspend,
	.resume		= wakeup_assist_resume,
};

static struct platform_driver wakeup_assist_driver = {
	.probe		= wakeup_assist_probe,
	.remove		= __devexit_p(wakeup_assist_remove),
	.driver		= {
		.name	= DEV_NAME,
		.owner	= THIS_MODULE,
		.pm	= &wakeup_assist_pm_ops,
	},
};

static int __init wakeup_assist_init(void)
{
	return platform_driver_register(&wakeup_assist_driver);
}
module_init(wakeup_assist_init);

static void __exit wakeup_assist_exit(void)
{
	platform_driver_unregister(&wakeup_assist_driver);
}
module_exit(wakeup_assist_exit);

MODULE_DESCRIPTION("Wakeup assist driver");
MODULE_AUTHOR("Eunki Kim <eunki_kim@samsung.com>");
MODULE_LICENSE("GPL");
