#include <linux/sysdev.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/serial_core.h>
#include <linux/io.h>

#include <plat/map-base.h>
#include <plat/gpio-cfg.h>

#include <mach/regs-gpio.h>
#include <linux/gpio.h>

#include <mach/regs-irq.h>
#include <linux/irq.h>

static ssize_t reset_status_show(struct sysdev_class *class, struct sysdev_class_attribute * attr, char *buf)
{
        int tmp = gpio_get_value(EXYNOS4_GPF2(6));
        return sprintf(buf, "%s\n", tmp==0?"0":"1");
}

static ssize_t reset_status_store(struct sysdev_class *class, struct sysdev_class_attribute * attr,
                                const char *buf, size_t count)
{
        if(strncmp(buf, "1", 1)==0){
               // s3c_gpio_setpin(EXYNOS4_GPC1(1),1);//bt_reset_n  high
                gpio_set_value(EXYNOS4_GPF2(6),1);//bt_reset_n  high
		return count;
        }

        if(strncmp(buf, "0", 1)==0){
                //s3c_gpio_setpin(EXYNOS4_GPC1(1),0);//bt_reset_n  low
                gpio_set_value(EXYNOS4_GPF2(6),0);//bt_reset_n  low
                return count;
        }

        return count;
}

static ssize_t shutdown_status_show(struct sysdev_class *class, struct sysdev_class_attribute * attr, char *buf)
{
        int tmp = gpio_get_value(EXYNOS4_GPF2(5));
        return sprintf(buf, "%s\n", tmp==0?"0":"1");
}

static ssize_t shutdown_status_store(struct sysdev_class *class, struct sysdev_class_attribute * attr,
                                const char *buf, size_t count)
{
        if(strncmp(buf, "1", 1)==0){
		//s3c_gpio_setpin(EXYNOS4_GPC1(0),1);//bt_shutdown high to power on
		gpio_set_value(EXYNOS4_GPF2(5),1);//bt_shutdown high to power on
                return count;
        }

        if(strncmp(buf, "0", 1)==0){
	//	s3c_gpio_setpin(EXYNOS4_GPC1(0),0);//bt_shutdown low to power off
		gpio_set_value(EXYNOS4_GPF2(5),0);//bt_shutdown low to power off
                return count;
        }

        return count;
}

static SYSDEV_CLASS_ATTR(reset, 0644, reset_status_show, reset_status_store);
static SYSDEV_CLASS_ATTR(shutdown, 0644, shutdown_status_show, shutdown_status_store);

static struct sysdev_class_attribute *mbluetooth_attributes[] = {
        &attr_reset,
	&attr_shutdown,
        NULL
};

static struct sysdev_class modulebluetooth_class = {
        .name = "bluetoothlenovo",
};

static int __init modulebluetooth_init(void)
{
        u32 err;
	struct sysdev_class_attribute **attr;
        int res;

        res=sysdev_class_register(&modulebluetooth_class);
        if (unlikely(res)) {
                return res;
        }

        for (attr = mbluetooth_attributes; *attr; attr++) {
                res = sysdev_class_create_file(&modulebluetooth_class, *attr);
                if (res)
                        goto out_unreg;
        }

        s3c_gpio_cfgpin(EXYNOS4_GPA0(0), S3C_GPIO_SFN(2));//rxd
                s3c_gpio_setpull(EXYNOS4_GPA0(0), S3C_GPIO_PULL_NONE);
        s3c_gpio_cfgpin(EXYNOS4_GPA0(1), S3C_GPIO_SFN(2));//txd
                s3c_gpio_setpull(EXYNOS4_GPA0(1), S3C_GPIO_PULL_NONE);
        s3c_gpio_cfgpin(EXYNOS4_GPA0(2), S3C_GPIO_SFN(2));//n_cts
                s3c_gpio_setpull(EXYNOS4_GPA0(2), S3C_GPIO_PULL_NONE);
        s3c_gpio_cfgpin(EXYNOS4_GPA0(3), S3C_GPIO_SFN(2)); //n_rts
                s3c_gpio_setpull(EXYNOS4_GPA0(3), S3C_GPIO_PULL_NONE);

        err = gpio_request(EXYNOS4_GPF2(5), "GPF2");//bt_shutdown
        if (err) {
                printk(KERN_INFO "gpio request bluetooth bt_shutdown error : %d\n", err);
        } else {
                s3c_gpio_cfgpin(EXYNOS4_GPF2(5), S3C_GPIO_OUTPUT);
                s3c_gpio_setpull(EXYNOS4_GPF2(5), S3C_GPIO_PULL_NONE);
        }
        err = gpio_request(EXYNOS4_GPF2(6), "GPF2");//bt_rest
        if (err) {
                printk(KERN_INFO "gpio request bluetooth bt_reset error : %d\n", err);
        } else {
                s3c_gpio_cfgpin(EXYNOS4_GPF2(6), S3C_GPIO_OUTPUT);
                s3c_gpio_setpull(EXYNOS4_GPF2(6), S3C_GPIO_PULL_NONE);
        }
	return 0;

out_unreg:
        for (; attr >= mbluetooth_attributes; attr--)
                sysdev_class_remove_file(&modulebluetooth_class, *attr);
        sysdev_class_unregister(&modulebluetooth_class);

        return res;
}
module_init(modulebluetooth_init);
