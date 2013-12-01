/* drivers/input/misc/kxtik.c - KXTIK accelerometer driver
 *
 * Copyright (C) 2011 Kionix, Inc.
 * Written by Kuching Tan <kuchingtan@kionix.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input/kxtik.h>
#include <linux/version.h>
#include <linux/earlysuspend.h>
#include <linux/wakelock.h>

#define NAME			"kxtik"
#define G_MAX			8000
/* OUTPUT REGISTERS */
#define XOUT_L			0x06
#define WHO_AM_I		0x0F
/* CONTROL REGISTERS */
#define INT_REL			0x1A
#define CTRL_REG1		0x1B
#define INT_CTRL1		0x1E
#define DATA_CTRL		0x21
/* CONTROL REGISTER 1 BITS */
#define PC1_OFF			0x7F
#define PC1_ON			(1 << 7)
/* INPUT_ABS CONSTANTS */
#define FUZZ			3
#define FLAT			3
/* RESUME STATE INDICES */
#define RES_DATA_CTRL		0
#define RES_CTRL_REG1		1
#define RES_INT_CTRL1		2
#define RESUME_ENTRIES		3

/*
 * The following table lists the maximum appropriate poll interval for each
 * available output data rate.
 */
static const struct {
	unsigned int cutoff;
	u8 mask;
} kxtik_odr_table[] = {
	{ 3,	ODR800F },
	{ 5,	ODR400F },
	{ 10,	ODR200F },
	{ 20,	ODR100F },
	{ 40,	ODR50F  },
	{ 80,	ODR25F  },
	{ 0,	ODR12_5F},
};

struct kxtik_data {
	struct i2c_client *client;
	struct kxtik_platform_data pdata;
	struct input_dev *input_dev;
	struct delayed_work poll_work;
	struct workqueue_struct *workqueue;
	unsigned int poll_interval;
	unsigned int poll_delay;
	u8 shift;
	u8 ctrl_reg1;
	u8 data_ctrl;
	struct early_suspend    early_suspend;
	int enable_cnt;
};
static struct wake_lock kxtik_wake_lock;
static int is_early_suspend = 0;

#ifdef CONFIG_HAS_EARLYSUSPEND
static void kxtik_earlysuspend(struct early_suspend *handler);
static void kxtik_lateresume(struct early_suspend *handler);
#endif

static int kxtik_i2c_read(struct kxtik_data *tik, u8 addr, u8 *data, int len)
{
	struct i2c_msg msgs[] = {
		{
			.addr = tik->client->addr,
			.flags = tik->client->flags,
			.len = 1,
			.buf = &addr,
		},
		{
			.addr = tik->client->addr,
			.flags = tik->client->flags | I2C_M_RD,
			.len = len,
			.buf = data,
		},
	};

	return i2c_transfer(tik->client->adapter, msgs, 2);
}

static void kxtik_report_acceleration_data(struct kxtik_data *tik)
{
	s16 acc_data[3]; /* Data bytes from hardware xL, xH, yL, yH, zL, zH */
	s16 x, y, z;
	int err;
	struct input_dev *input_dev = tik->input_dev;

	mutex_lock(&input_dev->mutex);
	err = kxtik_i2c_read(tik, XOUT_L, (u8 *)acc_data, 6);
	mutex_unlock(&input_dev->mutex);
	if (err < 0)
		dev_err(&tik->client->dev, "accelerometer data read failed\n");

	x = ((s16) le16_to_cpu(acc_data[tik->pdata.axis_map_x])) >> tik->shift;
	y = ((s16) le16_to_cpu(acc_data[tik->pdata.axis_map_y])) >> tik->shift;
	z = ((s16) le16_to_cpu(acc_data[tik->pdata.axis_map_z])) >> tik->shift;

#if 0
	pr_info("Accel: x = %d, y = %d, z = %d\n", \
		 tik->pdata.negate_x ? -x : x,  \
		 tik->pdata.negate_y ? -y : y,  \
		 tik->pdata.negate_z ? -z : z);
#endif

	input_report_abs(tik->input_dev, ABS_X, tik->pdata.negate_x ? -x : x);
	input_report_abs(tik->input_dev, ABS_Y, tik->pdata.negate_y ? -y : y);
	input_report_abs(tik->input_dev, ABS_Z, tik->pdata.negate_z ? -z : z);
	input_sync(tik->input_dev);
}

static void kxtik_poll_work(struct work_struct *work)
{
	struct kxtik_data *tik = container_of((struct delayed_work *)work,	struct kxtik_data, poll_work);
       wake_lock(&kxtik_wake_lock);

	kxtik_report_acceleration_data(tik);
       if(!is_early_suspend)
	    queue_delayed_work(tik->workqueue, &tik->poll_work, tik->poll_delay);

       wake_unlock(&kxtik_wake_lock);
}

static int kxtik_update_g_range(struct kxtik_data *tik, u8 new_g_range)
{
	switch (new_g_range) {
	case KXTIK_G_2G:
		tik->shift = 4;
		break;
	case KXTIK_G_4G:
		tik->shift = 3;
		break;
	case KXTIK_G_8G:
		tik->shift = 2;
		break;
	default:
		return -EINVAL;
	}

	tik->ctrl_reg1 &= 0xe7;
	tik->ctrl_reg1 |= new_g_range;

	return 0;
}

static int kxtik_update_odr(struct kxtik_data *tik, unsigned int poll_interval)
{
	int err;
	int i;

	/* Use the lowest ODR that can support the requested poll interval */
	for (i = 0; i < ARRAY_SIZE(kxtik_odr_table); i++) {
		tik->data_ctrl = kxtik_odr_table[i].mask;
		if (poll_interval < kxtik_odr_table[i].cutoff)
			break;
	}

	err = i2c_smbus_write_byte_data(tik->client, CTRL_REG1, 0);
	if (err < 0)
		return err;

	err = i2c_smbus_write_byte_data(tik->client, DATA_CTRL, tik->data_ctrl);
	if (err < 0)
		return err;

	err = i2c_smbus_write_byte_data(tik->client, CTRL_REG1, tik->ctrl_reg1);
	if (err < 0)
		return err;

	return 0;
}

static int kxtik_device_power_on(struct kxtik_data *tik)
{
	if (tik->pdata.power_on)
		return tik->pdata.power_on();

	return 0;
}

static void kxtik_device_power_off(struct kxtik_data *tik)
{
	int err;

	tik->ctrl_reg1 &= PC1_OFF;
	err = i2c_smbus_write_byte_data(tik->client, CTRL_REG1, tik->ctrl_reg1);
	if (err < 0)
		dev_err(&tik->client->dev, "soft power off failed\n");

	if (tik->pdata.power_off)
		tik->pdata.power_off();
}

static int kxtik_enable(struct kxtik_data *tik)
{
	int err;
	unsigned long flags;

	err = kxtik_device_power_on(tik);
	if (err < 0)
		return err;

	/* ensure that PC1 is cleared before updating control registers */
	err = i2c_smbus_write_byte_data(tik->client, CTRL_REG1, 0);
	if (err < 0)
		return err;

	err = kxtik_update_g_range(tik, tik->pdata.g_range);
	if (err < 0)
		return err;

	/* turn on outputs */
	tik->ctrl_reg1 |= PC1_ON;
	err = i2c_smbus_write_byte_data(tik->client, CTRL_REG1, tik->ctrl_reg1);
	if (err < 0)
		return err;

	err = kxtik_update_odr(tik, tik->poll_interval);
	if (err < 0)
		return err;

	spin_lock_irqsave(&tik->input_dev->mutex.wait_lock, flags);
	queue_delayed_work(tik->workqueue, &tik->poll_work, 0);
	spin_unlock_irqrestore(&tik->input_dev->mutex.wait_lock, flags);

	return 0;
}

static void kxtik_disable(struct kxtik_data *tik)
{
	unsigned long flags;

	spin_lock_irqsave(&tik->input_dev->mutex.wait_lock, flags);
	__cancel_delayed_work(&tik->poll_work);
	spin_unlock_irqrestore(&tik->input_dev->mutex.wait_lock, flags);

	kxtik_device_power_off(tik);
}

static int kxtik_input_open(struct input_dev *input)
{
	struct kxtik_data *tik = input_get_drvdata(input);

	return kxtik_enable(tik);
}

static void kxtik_input_close(struct input_dev *dev)
{
	struct kxtik_data *tik = input_get_drvdata(dev);

	kxtik_disable(tik);
}

static void __devinit kxtik_init_input_device(struct kxtik_data *tik,
					      struct input_dev *input_dev)
{
	__set_bit(EV_ABS, input_dev->evbit);
	input_set_abs_params(input_dev, ABS_X, -G_MAX, G_MAX, FUZZ, FLAT);
	input_set_abs_params(input_dev, ABS_Y, -G_MAX, G_MAX, FUZZ, FLAT);
	input_set_abs_params(input_dev, ABS_Z, -G_MAX, G_MAX, FUZZ, FLAT);

	input_dev->name = "kxtik_accel";
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &tik->client->dev;
}

static int __devinit kxtik_setup_input_device(struct kxtik_data *tik)
{
	struct input_dev *input_dev;
	int err;

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&tik->client->dev, "input device allocate failed\n");
		return -ENOMEM;
	}

	tik->input_dev = input_dev;

	input_dev->open = kxtik_input_open;
	input_dev->close = kxtik_input_close;
	input_set_drvdata(input_dev, tik);

	kxtik_init_input_device(tik, input_dev);

	err = input_register_device(tik->input_dev);
	if (err) {
		dev_err(&tik->client->dev,
			"unable to register input polled device %s: %d\n",
			tik->input_dev->name, err);
		input_free_device(tik->input_dev);
		return err;
	}

	return 0;
}

/*
 * When IRQ mode is selected, we need to provide an interface to allow the user
 * to change the output data rate of the part.  For consistency, we are using
 * the set_poll method, which accepts a poll interval in milliseconds, and then
 * calls update_odr() while passing this value as an argument.  In IRQ mode, the
 * data outputs will not be read AT the requested poll interval, rather, the
 * lowest ODR that can support the requested interval.  The client application
 * will be responsible for retrieving data from the input node at the desired
 * interval.
 */

/* Returns currently selected poll interval (in ms) */
static ssize_t kxtik_get_poll(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kxtik_data *tik = i2c_get_clientdata(client);

	return sprintf(buf, "%d\n", tik->poll_interval);
}

/* Allow users to select a new poll interval (in ms) */
static ssize_t kxtik_set_poll(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kxtik_data *tik = i2c_get_clientdata(client);
	struct input_dev *input_dev = tik->input_dev;
	unsigned int interval;

	#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,35))
	int error;
	error = kstrtouint(buf, 10, &interval);
	if (error < 0)
		return error;
	#else
	interval = (unsigned int)simple_strtoul(buf, NULL, 10);
	#endif

	/* Lock the device to prevent races with open/close (and itself) */
	mutex_lock(&input_dev->mutex);

	/*
	 * Set current interval to the greater of the minimum interval or
	 * the requested interval
	 */
	tik->poll_interval = max(interval, tik->pdata.min_interval);
	tik->poll_delay = msecs_to_jiffies(tik->poll_interval);

	kxtik_update_odr(tik, tik->poll_interval);

	mutex_unlock(&input_dev->mutex);

	return count;
}

/* Allow users to enable/disable the device */
static ssize_t kxtik_set_enable(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct kxtik_data *tik = i2c_get_clientdata(client);
	struct input_dev *input_dev = tik->input_dev;
	unsigned int enable;

	#if (LINUX_VERSION_CODE > KERNEL_VERSION(2,6,35))
	int error;
	error = kstrtouint(buf, 10, &enable);
	if (error < 0)
		return error;
	#else
	enable = (unsigned int)simple_strtoul(buf, NULL, 10);
	#endif

	/* Lock the device to prevent races with open/close (and itself) */
	mutex_lock(&input_dev->mutex);

	if(enable)
	{
		tik->enable_cnt++;
		kxtik_enable(tik);
	}
	else
	{
		if(tik->enable_cnt)
			tik->enable_cnt--;

		kxtik_disable(tik);
	}

	mutex_unlock(&input_dev->mutex);

	return count;
}

static DEVICE_ATTR(poll, S_IRUGO|S_IWUSR, kxtik_get_poll, kxtik_set_poll);
static DEVICE_ATTR(enable, S_IWUSR, NULL, kxtik_set_enable);

static struct attribute *kxtik_attributes[] = {
	&dev_attr_poll.attr,
	&dev_attr_enable.attr,
	NULL
};

static struct attribute_group kxtik_attribute_group = {
	.attrs = kxtik_attributes
};

static int __devinit kxtik_verify(struct kxtik_data *tik)
{
	int retval;

	retval = kxtik_device_power_on(tik);
	if (retval < 0)
		return retval;

	retval = i2c_smbus_read_byte_data(tik->client, WHO_AM_I);
	if (retval < 0) {
		dev_err(&tik->client->dev, "read err int source\n");
		goto out;
	}

	retval = retval != 0x05 ? -EIO : 0;

out:
	kxtik_device_power_off(tik);
	return retval;
}

static int __devinit kxtik_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	const struct kxtik_platform_data *pdata = client->dev.platform_data;
	struct kxtik_data *tik;
	int err;

	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_I2C | I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&client->dev, "client is not i2c capable\n");
		return -ENXIO;
	}

	if (!pdata) {
		dev_err(&client->dev, "platform data is NULL; exiting\n");
		return -EINVAL;
	}

	tik = kzalloc(sizeof(*tik), GFP_KERNEL);
	if (!tik) {
		dev_err(&client->dev,
			"failed to allocate memory for module data\n");
		return -ENOMEM;
	}

	tik->client = client;
	tik->pdata = *pdata;

	if (pdata->init) {
		err = pdata->init();
		if (err < 0)
			goto err_free_mem;
	}

	err = kxtik_verify(tik);
	if (err < 0) {
		dev_err(&client->dev, "device not recognized\n");
		goto err_pdata_exit;
	}

	i2c_set_clientdata(client, tik);

	err = kxtik_setup_input_device(tik);
	if (err)
		goto err_pdata_exit;
#ifdef CONFIG_HAS_EARLYSUSPEND
	printk("==register_early_suspend =\n");
	tik->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB - 50;
	tik->early_suspend.suspend = kxtik_earlysuspend;
	tik->early_suspend.resume = kxtik_lateresume;
	register_early_suspend(&tik->early_suspend);
#endif

	tik->ctrl_reg1 = tik->pdata.res_12bit | tik->pdata.g_range;
	tik->poll_interval = tik->pdata.poll_interval;
	tik->poll_delay = msecs_to_jiffies(tik->poll_interval);

	wake_lock_init(&kxtik_wake_lock, WAKE_LOCK_SUSPEND, "kxtik_wake_lock");

	tik->workqueue = create_workqueue("KXTIK Workqueue");
	INIT_DELAYED_WORK(&tik->poll_work, kxtik_poll_work);

	err = sysfs_create_group(&client->dev.kobj, &kxtik_attribute_group);
	if (err) {
		dev_err(&client->dev, "sysfs create failed: %d\n", err);
		goto err_destroy_input;
	}

	return 0;

err_destroy_input:
	input_unregister_device(tik->input_dev);
	destroy_workqueue(tik->workqueue);
err_pdata_exit:
	if (tik->pdata.exit)
		tik->pdata.exit();
err_free_mem:
	kfree(tik);
	return err;
}

static int __devexit kxtik_remove(struct i2c_client *client)
{
	struct kxtik_data *tik = i2c_get_clientdata(client);

	wake_lock_destroy(&kxtik_wake_lock);
	unregister_early_suspend(&tik->early_suspend);
	sysfs_remove_group(&client->dev.kobj, &kxtik_attribute_group);
	input_unregister_device(tik->input_dev);
	destroy_workqueue(tik->workqueue);

	if (tik->pdata.exit)
		tik->pdata.exit();

	kfree(tik);

	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void kxtik_earlysuspend(struct early_suspend *handler)
{
	struct kxtik_data *tik = container_of(handler, struct kxtik_data,
		early_suspend);
	struct input_dev *input_dev = tik->input_dev;
	printk("start ========%s=======\n", __func__);
       is_early_suspend = 1;

	if (input_dev->users)
		kxtik_disable(tik);

	printk("end ========%s=======\n", __func__);
}

static void kxtik_lateresume(struct early_suspend *handler)
{
	struct kxtik_data *tik = container_of(handler, struct kxtik_data,
		early_suspend);
	struct input_dev *input_dev = tik->input_dev;
	printk("start ========%s=======\n", __func__);

       is_early_suspend = 0;

	if (input_dev->users && tik->enable_cnt)
		kxtik_enable(tik);

	printk("end ========%s=======\n", __func__);
}
#endif
static const struct i2c_device_id kxtik_id[] = {
	{ NAME, 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, kxtik_id);

static struct i2c_driver kxtik_driver = {
	.driver = {
		.name	= NAME,
		.owner	= THIS_MODULE,
	},
	.probe		= kxtik_probe,
	.remove		= __devexit_p(kxtik_remove),
	.id_table	= kxtik_id,
};

static int __init kxtik_init(void)
{
	return i2c_add_driver(&kxtik_driver);
}
module_init(kxtik_init);

static void __exit kxtik_exit(void)
{
	i2c_del_driver(&kxtik_driver);
}
module_exit(kxtik_exit);

MODULE_DESCRIPTION("KXTIK accelerometer driver");
MODULE_AUTHOR("Kuching Tan <kuchingtan@kionix.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.6.1");
