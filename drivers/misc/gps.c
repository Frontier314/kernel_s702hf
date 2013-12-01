#include <linux/sysdev.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/serial_core.h>
#include <linux/io.h>
#include <linux/platform_device.h>

#include <plat/map-base.h>
#include <plat/gpio-cfg.h>

#include <mach/regs-gpio.h>
#include <linux/gpio.h>
#include <linux/mfd/max77686.h>

/* gps power state  */
#define GP_EN EXYNOS4_GPF3(4)
#define GP_RESET EXYNOS4_GPF3(5)
#define GP_RTSn EXYNOS4_GPA1(3)
static volatile int gps_state;

int check_gps_status(void)
{
	return gps_state;
}
EXPORT_SYMBOL(check_gps_status);

static ssize_t reset_status_show(struct sysdev_class *class, struct sysdev_class_attribute * attr, char *buf)
{
	int tmp = gpio_get_value(GP_RESET);
        return sprintf(buf, "%s\n", tmp==0?"0":"1");
}

static ssize_t reset_status_store(struct sysdev_class *class, struct sysdev_class_attribute * attr,
                                const char *buf, size_t count)
{
        if(strncmp(buf, "1", 1)==0){
                gpio_set_value(GP_RESET,1);//gps_reset_n  high
		return count;
        }

        if(strncmp(buf, "0", 1)==0){
                gpio_set_value(GP_RESET,0);//gps_reset_n  low
                return count;
        }

        return count;
}

static ssize_t regpu_status_show(struct sysdev_class *class, struct sysdev_class_attribute * attr, char *buf)
{
	int tmp = gpio_get_value(GP_EN);
        return sprintf(buf, "%s\n", tmp==0?"0":"1");
}

static void gps_regulator(int on)
{
	static int  reg_state = 0;
	struct regulator * gps32k;

	gps32k = regulator_get(NULL, "gps_32khz");

	if(IS_ERR(gps32k)) {
		pr_err("%s: failed to get %s\n", __func__, "gps_32khz");
		return;
	}

	if (on && !reg_state) {
		regulator_enable(gps32k);
		reg_state = 1;
	} else if (!on && reg_state) {
		regulator_disable(gps32k);
		reg_state = 0;
	}

	regulator_put(gps32k);
	return;
}

static ssize_t regpu_status_store(struct sysdev_class *class, struct sysdev_class_attribute * attr,
                                const char *buf, size_t count)
{
	if(strncmp(buf, "1", 1)==0) {
	       	//gps_regulator(1);
		gpio_set_value(GP_EN, 1);//gps_regpu high to power on
		gps_state = 1;
	} else if(strncmp(buf, "0", 1)==0) {
		gpio_set_value(GP_EN, 0);//gps_regpu low to power off
		//gps_regulator(0);
		gps_state = 0;
	}

        return count;
}

static SYSDEV_CLASS_ATTR(reset, 0664, reset_status_show, reset_status_store);
static SYSDEV_CLASS_ATTR(regpu, 0664, regpu_status_show, regpu_status_store);

static struct sysdev_class_attribute *mgps_attributes[] = {
        &attr_reset,
	 &attr_regpu,
        NULL
};

static struct sysdev_class modulegps_class = {
        .name = "gpslenovo",
};

static int __init gps_probe(struct platform_device *pdev)
{
	u32 err;

	s3c_gpio_cfgpin(EXYNOS4_GPA1(0), S3C_GPIO_SFN(2));//rxd
                s3c_gpio_setpull(EXYNOS4_GPA1(0), S3C_GPIO_PULL_NONE);
        s3c_gpio_cfgpin(EXYNOS4_GPA1(1), S3C_GPIO_SFN(2));//txd
                s3c_gpio_setpull(EXYNOS4_GPA1(1), S3C_GPIO_PULL_NONE);
        s3c_gpio_cfgpin(EXYNOS4_GPA1(2), S3C_GPIO_SFN(2));//n_cts
                s3c_gpio_setpull(EXYNOS4_GPA1(2), S3C_GPIO_PULL_NONE);
        s3c_gpio_cfgpin(EXYNOS4_GPA1(3), S3C_GPIO_SFN(2)); //n_rts
                s3c_gpio_setpull(EXYNOS4_GPA1(3), S3C_GPIO_PULL_NONE);

        err = gpio_request(GP_RESET, "GPF3");//gps_reset_n
        if (err) {
                printk(KERN_INFO "gpio request gps reset_n error : %d\n", err);
        } else {
                s3c_gpio_cfgpin(GP_RESET, S3C_GPIO_OUTPUT);
                s3c_gpio_setpull(GP_RESET, S3C_GPIO_PULL_NONE);
        }
        err = gpio_request(GP_EN, "GPF3");//gps_regpu
        if (err) {
                printk(KERN_INFO "gpio request gps gps_regpu error : %d\n", err);
        } else {
                s3c_gpio_cfgpin(GP_EN, S3C_GPIO_OUTPUT);
                s3c_gpio_setpull(GP_EN, S3C_GPIO_PULL_NONE);
        }

        gpio_set_value(GP_RESET, 1);  //enable n_reset
        gpio_set_value(GP_EN, 0);  //disable regpu
        gps_state = 0;
        
        gps_regulator(1); //Broadcom: Volans commands that the RTC clock should be always enabled.

        return 0;
}

static int __devexit gps_remove(struct platform_device *pdev)
{
        gpio_set_value(GP_RESET, 0);  //disable n_reset
        return 0;
}

static int gps_suspend_mode(struct platform_device *dev, pm_message_t state)
{
	/* shaoguodong added */
	int val = 0;

	if (gps_state)
		val = 1;
	s5p_gpio_set_pd_cfg(GP_EN, val);
	s5p_gpio_set_pd_cfg(GP_RTSn, val);
	printk(KERN_INFO"gps suspend !!!!!\n");

	return 0;
}

static int gps_resume_back(struct platform_device *dev)
{
	//pr_info("gps resume back !!!!!\n");
	/*s3c_gpio_cfgpin(EXYNOS4_GPA1(0), S3C_GPIO_SFN(2));//rxd
	        s3c_gpio_setpull(EXYNOS4_GPA1(0), S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(EXYNOS4_GPA1(1), S3C_GPIO_SFN(2));//txd
	        s3c_gpio_setpull(EXYNOS4_GPA1(1), S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(EXYNOS4_GPA1(2), S3C_GPIO_SFN(2));//n_cts
	        s3c_gpio_setpull(EXYNOS4_GPA1(2), S3C_GPIO_PULL_NONE);
	s3c_gpio_cfgpin(EXYNOS4_GPA1(3), S3C_GPIO_SFN(2)); //n_rts
	        s3c_gpio_setpull(EXYNOS4_GPA1(3), S3C_GPIO_PULL_NONE);*/

	return 0;
}

static struct platform_driver gps_driver = {
        .probe = gps_probe,
        .remove = __devexit_p(gps_remove),
        .driver = {
                .name = "gps_device",
                .owner = THIS_MODULE,
        },
        .suspend = gps_suspend_mode,
        .resume = gps_resume_back,
};

static int __init modulegps_init(void)
{

        struct sysdev_class_attribute **attr;
        int res;

        res=sysdev_class_register(&modulegps_class);
        if (unlikely(res)) {
                return res;
        }

        for (attr = mgps_attributes; *attr; attr++) {
                res = sysdev_class_create_file(&modulegps_class, *attr);
                if (res)
                        goto out_unreg;
        }

	res = platform_driver_register(&gps_driver);

	return res;

out_unreg:
        for (; attr >= mgps_attributes; attr--)
                sysdev_class_remove_file(&modulegps_class, *attr);
        sysdev_class_unregister(&modulegps_class);

        return res;
}

static void __exit modulegps_exit(void)
{
        platform_driver_unregister(&gps_driver);
}

module_init(modulegps_init);
module_exit(modulegps_exit);
