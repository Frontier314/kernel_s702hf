//
// cp_led.c
//

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/timer.h>
#include <plat/gpio-cfg.h>
#include <linux/gpio.h>

static int state = 0;
static void control_led(int value)
{
	if (value) {	//led on
		gpio_direction_output(EXYNOS4_GPX0(6), 1);
	} else {	//led off
		gpio_direction_output(EXYNOS4_GPX0(6), 0);
	}
}

static int set_led(const char *val, struct kernel_param *kp)
{
	int value;
	int ret = param_set_int(val, kp);

	if(ret < 0)
	{
		printk(KERN_ERR"Errored active vibrator");
		return -EINVAL;
	}
	value = *((int*)kp->arg); 

//	printk(KERN_ERR "%s: state = %d\n", __func__, value);
	control_led(value);	
	return 0;
}

static int __devinit cp_led_init(void)
{
	int err = 0;

	s3c_gpio_cfgpin(EXYNOS4_GPX0(6), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(EXYNOS4_GPX0(6), S3C_GPIO_PULL_NONE);

	err = gpio_request(EXYNOS4_GPX0(6), "cp led");
	if (err) {
		printk("Fail to request cp_led indicate gpio :%d\n", err);
	}

	return 0;
}

static void __exit cp_led_exit(void)
{
	gpio_free(EXYNOS4_GPX0(6));
}

module_init(cp_led_init);
module_exit(cp_led_exit);

module_param_call(set_led_state, set_led, param_get_int, &state, 0600);
MODULE_AUTHOR("<liya@lenovomobile.com>");
MODULE_DESCRIPTION("download cp led driver");
MODULE_LICENSE("GPL");
