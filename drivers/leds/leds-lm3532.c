/*
 * lm3532.c - Backlight driver IC
 * Author : lvxin1@lenovo.com 
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
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/sysfs.h>
#include <linux/leds.h>
#include <linux/gpio.h>
#include <linux/earlysuspend.h>

#include <asm/uaccess.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/mutex.h>

#include "leds-lm3532.h"
#define GPIO_LED 	(34)

#ifdef BLK_DEBUG
#define blk_dbg(fmt, args...)	\
	printk(KERN_INFO "%s:"fmt, __func__, ## args)
#else
#define blk_dbg(fmt, args...)
#endif

static int lm3532_early_suspend(struct early_suspend *h);
static int lm3532_late_resume(struct early_suspend *h);
struct i2c_client *lm3532_client = NULL;
static DEFINE_MUTEX(lm3532_mutex);
int g_SetValue;

static s32 lm3532_write(struct i2c_client *client, u8 addr, u8 value)
{
	s32 ret;
	ret = i2c_smbus_write_byte_data(client, addr, value);
	if (ret < 0) {
		blk_dbg("LM3532 device write error!\n");
	}
	return ret;
}

static void lm3532_read(struct i2c_client *client, u8 addr, u8 *value)
{
	s32 ret;
	ret = i2c_smbus_read_byte_data(client,addr);
	if (ret < 0) {
		blk_dbg("I2C Read Failed!\n");
	} else {
		*value = ret & (0xff);
	}
	 return;
}

/*Device configuration*/
static int  lm3532_device_configuration(struct lm3532_data *driver_data)
{
	struct i2c_client *client = driver_data->client;
	/* ILED1 & ILED2 are all controlled by Control A PWM and Brightness Registers */
	lm3532_write(client, OUTPUT_CONFIGURATION, 0x00);

	/* Setting Startup/shutdown Ramp Rate */
	lm3532_write(client, STAR_SHUT_RAMP_RATE, SHUTDN_RAMP_2p048ms_STEP | START_RAMP_2p048ms_STEP);

	/* Run time Ramp Rate */
	lm3532_write(client, RUN_TIME_RAMP_RATE, RT_RAMPDN_2p048ms_STEP | RT_RAMPUP_2p048ms_STEP);

	/* Control A PWM Register*/
	lm3532_write(client, CONTL_A_PWM, PWM_CHANNEL_SEL_PWM2 | PWM_INPUT_POLARITY_HIGH | PWM_ZONE_0_EN);

	/* Control A Brightness Configuration */
	lm3532_write(client, CONTL_A_BRIGHT, I2C_CURRENT_CONTROL | LINEAR_MAPPING | CONTROL_ZONE_TARGET_0);
	/* Control A Full-Scale Current */
	lm3532_write(client, CONTL_A_FULL_SCALE_CURRENT, CNTL_FS_CURRENT_20P2mA);

	/* Feedback */
	lm3532_write(client, FEEDBACK_EN, FEEDBACK_ENABLE_ILED1 | FEEDBACK_ENABLE_ILED2);

	/* Control Enable */
	lm3532_write(client, CONTL_EN, ENABLE_CNTL_A);

	/* Default using Control A Zone 0 Target */
	lm3532_write(client, CONTL_A_ZONE_TARGET0, 0x00);
	driver_data->current_value = 0;
	return 0;
}

static int lm3532_detect(struct i2c_client *client)
{
	u8 val;

	lm3532_read(client, OUTPUT_CONFIGURATION, &val);
	if (val != 0xE4) {
		blk_dbg("LM3532 is not detected!, Output_Config:[0x10] = 0x%x,\n", val);
		return -ENODEV;
	} else if (val == 0xE4) {
		blk_dbg("LM3532 is detected\n");
	}
	return 0;
}
void lm3532_brightness_set(int value)
{
	struct lm3532_data *driver_data = i2c_get_clientdata(lm3532_client);

	blk_dbg("set brightness, value = %d\n", value);
	value == 1 ? (value = 0) : (value = ((value * 255)/100));

	blk_dbg("actual result : %d\n", value);
#if 0
	if (value) 
		gpio_direction_output(GPIO_LED, 1);
	else
		gpio_direction_output(GPIO_LED, 0);
#endif
	lm3532_write(driver_data->client, CONTL_A_ZONE_TARGET0, value);
	driver_data->current_value = value;
        g_SetValue = value;
	return;
}
EXPORT_SYMBOL(lm3532_brightness_set);

int lm3532_brightness_get(void)
{
	struct lm3532_data *driver_data = i2c_get_clientdata(lm3532_client);
	return driver_data->current_value;
}
EXPORT_SYMBOL(lm3532_brightness_get);

static void lm3532_brightness_ledclass_set(struct led_classdev *led_cdev, enum led_brightness value)
{
	unsigned int total_time = 0;
	signed int nsteps;
	struct lm3532_data *driver_data;

	driver_data = container_of(led_cdev, struct lm3532_data, led_dev);

	if (!driver_data->initialized) {
		blk_dbg("LM3532 not Initialized\n");
		return;
	}
	if (strstr(led_cdev->name, "lcd-backlight"))
		blk_dbg("Led Char device name matched\n");

	/* Calculate number of steps for ramping */
	nsteps = value - driver_data->current_value;
	if (nsteps < 0)
		nsteps = -nsteps;

	mutex_lock(&lm3532_mutex);
	lm3532_write(driver_data->client, CONTL_A_ZONE_TARGET0, value);
	driver_data->current_value = value;
	mutex_unlock(&lm3532_mutex);

	return;
}

static int lm3532_probe(struct i2c_client *client, const struct i2c_device_id *devid)
{
	int ret = 0;
	struct lm3532_data *driver_data;
	struct led_classdev *lm3532_led_dev;


	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		blk_dbg("%s : %d, lm3532_probe!\n", __func__, __LINE__);
		ret = -ENODEV;
		return ret;
	}
	/* Initialize lm3532_data structure */
	driver_data = (struct lmj3532_data *)kzalloc(sizeof(struct lm3532_data), GFP_KERNEL);
	if (driver_data == NULL) {
		blk_dbg("lm3532_data kzalloc failed!\n");
		return -ENOMEM;
	}
	memset(driver_data, 0, sizeof(*driver_data));
	driver_data->client = client;
	driver_data->initialized = 0;
	driver_data->current_value = 255;
	driver_data->do_ramp = 1;

	lm3532_client = client;

	driver_data->early_suspend.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING - 2;
	driver_data->early_suspend.suspend = lm3532_early_suspend;
	driver_data->early_suspend.resume = lm3532_late_resume;
	register_early_suspend(&driver_data->early_suspend);

	i2c_set_clientdata(client, driver_data);
	driver_data->led_dev.name = "lcd-backlight";
	driver_data->led_dev.brightness_set = lm3532_brightness_ledclass_set;

	ret = led_classdev_register(&client->dev, &driver_data->led_dev);
	if (ret) {
		blk_dbg("led_classdev_register %s failed\n", LM3532_LED_NAME);
		kfree(driver_data);
		return ret;
	}
	/*
	 * Request and Config HWEN gpio
	 */
	ret = gpio_request(HWEN, NULL);
	if (ret) {
		blk_dbg("HWEN request failed!\n");
		return ret;
	}
#if 0
	ret = gpio_request(GPIO_LED, "gpio_keyboard");
	if (ret) {
		blk_dbg("LED request failed!\n");
		return -ret;
	}
#endif
	gpio_direction_output(HWEN, 0);	//Output low, default the Backlight IC is power down
	/*Power On*/
	gpio_direction_output(HWEN, 1);

	/* Read Output Configuration Register default value */
	ret = lm3532_detect(client);
	if (ret < 0) {
		blk_dbg("LM3532 device communication failed\n");
		return -ENODEV;
	}

	ret = lm3532_device_configuration(driver_data);
	if (!ret)
		driver_data->initialized = 1;
	lm3532_brightness_set(100);//tmp yan
	return 0;
}

static int lm3532_remove(struct i2c_client *client)
{
	struct lm3532_data *driver_data = i2c_get_clientdata(client);
	led_classdev_unregister(&driver_data->led_dev);
	kfree(driver_data);
	gpio_free(HWEN);
	return 0;
}

#if 0

#ifdef CONFIG_PM

static int lm3532_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct lm3532_data *driver_data = i2c_get_clientdata(client);
	blk_dbg("Called the pm message suspend!\n");
        led_classdev_suspend(&driver_data->led_dev);
        gpio_direction_output(HWEN, 0);
	return 0;
}

static int lm3532_resume(struct i2c_client *client)
{
	struct lm3532_data *driver_data = i2c_get_clientdata(client);
        
        blk_dbg("called resuming!\n");
        led_classdev_resume(&driver_data->led_dev);
        gpio_direction_output(HWEN, 1);
        lm3532_device_configuration(driver_data);	
	lm3532_write(driver_data->client, CONTL_A_ZONE_TARGET0, g_SetValue);
        return 0;
}

#else

#define lm3532_suspend          NULL
#define lm3532_resume           NULL

#endif /* CONFIG_PM */
#endif


#ifdef CONFIG_HAS_EARLYSUSPEND
static int lm3532_early_suspend(struct early_suspend *h)
{
	struct lm3532_data *driver_data = i2c_get_clientdata(lm3532_client);
	blk_dbg("Called the pm message suspend!\n");
    led_classdev_suspend(&driver_data->led_dev);
//	gpio_direction_output(GPIO_LED, 0);
    gpio_direction_output(HWEN, 0);
	return 0;
}

static int lm3532_late_resume(struct early_suspend *h)
{
	struct lm3532_data *driver_data = i2c_get_clientdata(lm3532_client);
        
    blk_dbg("called resuming!\n");
    led_classdev_resume(&driver_data->led_dev);
    msleep(50);
    gpio_direction_output(HWEN, 1);
    lm3532_device_configuration(driver_data);	
	lm3532_write(driver_data->client, CONTL_A_ZONE_TARGET0, g_SetValue);
//	gpio_direction_output(GPIO_LED, 1);
    return 0;
}

#endif

static struct i2c_device_id lm3532_idtable[] = {
	{"lm3532", 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, lm3532_idtable);

static struct i2c_driver lm3532_driver = {
	.driver = {
		.name = "lm3532",
	},
	.probe	= lm3532_probe,
	.remove	= __devexit_p(lm3532_remove),
	.id_table	= lm3532_idtable,
#if 0
	.suspend	= lm3532_suspend,
	.resume		= lm3532_resume,
#endif
};

static int __init blk_lm3532_init(void)
{
	printk("zdl lm3532 in \n");
	return i2c_add_driver(&lm3532_driver);
}

static void __exit blk_lm3532_exit(void)
{
	i2c_del_driver(&lm3532_driver);
}

MODULE_AUTHOR("Danel lv <lvxin1@lenovo.com>");
MODULE_DESCRIPTION("LM3532 Backlight driver");
MODULE_LICENSE("GPL");

module_init(blk_lm3532_init);
module_exit(blk_lm3532_exit);
