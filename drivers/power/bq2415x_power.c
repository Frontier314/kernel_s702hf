/*
 * Battery driver for TI bq2415x
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/power/bq2415x_power.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/gpio.h>
#include <plat/gpio-core.h>
#include <plat/gpio-cfg.h>
#include <plat/gpio-cfg-helpers.h>

#define CHG_DEBUG  KERN_INFO // KERN_DEBUG 

static unsigned int poll_interval = 10;  
#define  BQ2415X_RESET_TIMER_INTERVAL		25 //32s timer  

enum BQ2415X_CHIP { BQ24153A, BQ24156A, BQ24158 };

/* BQ2415X regs*/
#define BQ2415X_STAT_CTRL_REG		0x00
#define BQ2415X_CTRL_REG			0x01
#define BQ2415X_CTRL_BATVOL_REG	0x02
#define BQ2415X_VERTION_REG		0x03
#define BQ2415X_BT_FCC_REG			0x04
#define BQ2415X_SCV_EPS_REG		0x05
#define BQ2415X_SAFE_LIMI_REG		0x06

struct bq2415x_chip {
	struct i2c_client		*client;
	struct delayed_work		work;
	struct power_supply		ac;
	struct power_supply		usb;	
	struct bq2415x_platform_data	*pdata;

	int 			irq_stat;
	int 			irq_chg_det;

	unsigned		vbus_online:1;
	struct delayed_work status_work;
	struct delayed_work reset_timer_work;
	
	unsigned		ac_online:1;
	unsigned		usb_online:1;
	unsigned		chg_mode:2;
	unsigned		batt_detect:1;	/* detecing MB by ID pin */
	unsigned		topoff_threshold:2;
	unsigned		fast_charge:3;
	
	int (*set_charger) (int);
};


static unsigned 	g_vbus_online_cache = 0;
static unsigned 	g_usb_udc_status = 0; //
static struct wake_lock vbus_wake_lock;
static unsigned 	g_bq2415x_initial = 0;

static int bq2415x_write_reg(struct i2c_client *client, int reg, u8 value)
{
	int ret;

	//printk(CHG_DEBUG "%s > reg:0x%02x, val:0x%02x\n",__func__, reg,value);
	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
	return ret;
}

static int bq2415x_read_reg(struct i2c_client *client, int reg)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
	//printk(CHG_DEBUG "%s > reg:0x%02x, val:0x%02x\n",__func__, reg, ret);
	return ret;
}


static void bq2415x_init_charger(struct bq2415x_chip *chip)
{
	struct i2c_client	*client = chip->client;
	int val;

	printk(CHG_DEBUG "%s \n",__func__);

	/* init some paramater*/


}

static void bq2145x_charge_reset_timer(struct work_struct *work)
{
	struct bq2415x_chip *chip =
		container_of(work, struct bq2415x_chip, reset_timer_work.work);
	int val;
	
	printk(CHG_DEBUG "%s \n",__func__);
	
	val = bq2415x_read_reg(chip->client, BQ2415X_STAT_CTRL_REG) |(0x1<<7);   //write 1 to reset safety timer
	bq2415x_write_reg(chip->client, BQ2415X_STAT_CTRL_REG, val);

	schedule_delayed_work(&chip->reset_timer_work, BQ2415X_RESET_TIMER_INTERVAL * HZ);
}


static void bq2415x_set_charger(struct bq2415x_chip *chip, int on)
	
{
	struct i2c_client	*client = chip->client;
	int val;
	
	printk(CHG_DEBUG "%s : %s, vbus :%d\n",__func__,on?"on":"off", chip->vbus_online);

	if(on &&  chip->vbus_online){
		if(chip->vbus_online && chip->ac_online){
			printk(CHG_DEBUG "%s : AC, 800ma\n", __func__);
			/* FCC : 550+400 =950mA, Battery Termination current : 100mA, */
			/* TODO: we should modify the Battery Termination current later */
			val = (bq2415x_read_reg(client, BQ2415X_BT_FCC_REG) & ~(0x7<<4 | 0x7<<0) |(0x4<<4|0x1<<0));
			bq2415x_write_reg(client, BQ2415X_BT_FCC_REG, val);	
			/*enable Nomarl change mode*/
			val = (bq2415x_read_reg(client, BQ2415X_SCV_EPS_REG) & ~(0x1<<5) );
			bq2415x_write_reg(client, BQ2415X_SCV_EPS_REG, val);
			
			/* Current limit 800mA, enable charge, enable charge current termination*/
			val = (bq2415x_read_reg(client, BQ2415X_CTRL_REG) & ~(0x3<<6 |0x1<<2) |(0x2<<6 /* | 0x1<<3*/));
			bq2415x_write_reg(client, BQ2415X_CTRL_REG, val);
		}
		else if(chip->vbus_online){
			printk(CHG_DEBUG "%s : USB, 500ma\n", __func__);
			/* FCC : 550mA, Battery Termination current : 100mA, */
			/* TODO: we should modify the Battery Termination current later */
			val = (bq2415x_read_reg(client, BQ2415X_BT_FCC_REG) & ~(0x7<<4 |0x7<<0) |(0x0<<4 |0x1<<0));
			bq2415x_write_reg(client, BQ2415X_BT_FCC_REG, val);	
			/*enable Nomarl change mode*/
			val = (bq2415x_read_reg(client, BQ2415X_SCV_EPS_REG) & ~(0x1<<5) );
			bq2415x_write_reg(client, BQ2415X_SCV_EPS_REG, val);	

			/* Current limit 500mA, enable charge, enable charge current termination*/
			val = (bq2415x_read_reg(client, BQ2415X_CTRL_REG) & ~(0x3<<6 |0x1<<2) |(0x1<<6 /* | 0x1<<3 */));
			bq2415x_write_reg(client, BQ2415X_CTRL_REG, val);
		}
		schedule_delayed_work(&chip->reset_timer_work, 0);

		wake_lock(&vbus_wake_lock);
	}
	else{
		printk(CHG_DEBUG "%s : off\n", __func__);
		val = (bq2415x_read_reg(client, BQ2415X_CTRL_REG) & ~(0x3<<6  | 0x1<<3)  | (0x1<<2));
		bq2415x_write_reg(client, BQ2415X_CTRL_REG, val);	
		cancel_delayed_work_sync(&chip->reset_timer_work);	

		wake_lock_timeout(&vbus_wake_lock, HZ / 2);
		
	}

}


static void bq2415x_reset(struct bq2415x_chip *chip)
{
	struct i2c_client	*client = chip->client;
	int val;
	
	printk(CHG_DEBUG "%s \n",__func__);
	val = bq2415x_read_reg(client, BQ2415X_BT_FCC_REG) & ~(0x1<<7); 
	bq2415x_write_reg(client, BQ2415X_BT_FCC_REG, val);
}

static int bq2415x_ac_get_prop(struct power_supply *psy,
			       enum power_supply_property psp,
			       union power_supply_propval *val)
{
	struct bq2415x_chip *chip_info = dev_get_drvdata(psy->dev->parent);
	int ret = 0;

	//printk(CHG_DEBUG "%s\n", __func__);
	
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = chip_info->ac_online;
		break;

	default:
		ret = -ENODEV;
		break;
	}

	return ret;
}

static enum power_supply_property bq2415x_ac_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static int bq2415x_usb_get_prop(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	struct bq2415x_chip *chip_info = dev_get_drvdata(psy->dev->parent);
	int ret = 0;

	//printk(CHG_DEBUG "%s\n", __func__);

	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = chip_info->usb_online;
		break;

	default:
		ret = -ENODEV;
		break;
	}

	return ret;
}

static enum power_supply_property bq2415x_usb_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

void samsung_cable_check_status(int flag)
{
#if 1
	if (flag == 0)
		g_usb_udc_status = 0;
	else
		g_usb_udc_status = 1;
#endif	
	printk(CHG_DEBUG "[[[[[ %s : USB UDC: %s ]]]]]\n", __func__,  flag?"on":"off");
		
}
EXPORT_SYMBOL(samsung_cable_check_status);


//extern bool s3c_udc_is_adc_cable();
static void cable_status_update_work_func(struct work_struct *work)
{
	struct bq2415x_chip *chip =
		container_of(work, struct bq2415x_chip, status_work.work);
	int gpio = chip->pdata->chg_det_irq_gpio;
	int val;
	int reg;

	val = gpio_get_value(gpio);
	chip->vbus_online = !val;
	
	if (g_vbus_online_cache == chip->vbus_online)
		goto out;
	else
		g_vbus_online_cache = chip->vbus_online;

	printk(CHG_DEBUG "%s, vbus_online: %d\n", __func__, chip->vbus_online);

	if(chip->vbus_online){
		if(g_usb_udc_status){
			chip->usb_online = 1;
			chip->ac_online = 0;
		}else{
			chip->usb_online = 0;
			chip->ac_online = 1;		
		}
		bq2415x_set_charger(chip, 1);
	}
	else{
		chip->usb_online = 0;
		chip->ac_online = 0;
		bq2415x_set_charger(chip, 0);
	}

	power_supply_changed(&chip->usb);
	power_supply_changed(&chip->ac);

out:	
	
	/* dump reg for debug*/
/*	
	for(reg = BQ2415X_STAT_CTRL_REG; reg <= BQ2415X_SAFE_LIMI_REG; reg++){
		val = bq2415x_read_reg(chip->client, reg);
		printk(CHG_DEBUG "BQ2415X reg 0x%02x : 0x%02x\n", reg, val);
	}
*/	
	if(poll_interval){
		schedule_delayed_work(&chip->status_work, poll_interval * HZ);
	}
}


static irqreturn_t vbus_det_irq_thread(int irq, void *data)
{
	struct bq2415x_chip *chip = (struct bq2415x_chip *)data;

	printk("%s ++ \n",__func__);
		
	cancel_delayed_work_sync(&chip->status_work);

	/* wait OTG udc driver update status*/
	schedule_delayed_work(&chip->status_work, HZ);
	
	printk("%s -- \n",__func__);
	
	return IRQ_HANDLED;
}



static __devinit int bq2415x_power_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct bq2415x_chip *chip;
	int val;
	int ret;

	printk(CHG_DEBUG "%s\n", __func__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		return -EIO;


	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;
	chip->pdata = client->dev.platform_data;

	i2c_set_clientdata(client, chip);


	if(!chip->pdata->chg_det_irq_gpio) {
		dev_warn(&client->dev, "No interrupt gpio specified.\n");
		ret = 0;
		goto out;
	}
	chip->irq_chg_det = gpio_to_irq(chip->pdata->chg_det_irq_gpio);
	ret = gpio_request(chip->pdata->chg_det_irq_gpio, "chg_det_irq");
	if (ret < 0) {
		dev_err(&client->dev,
			"Failed to request gpio %d with ret: %d\n",
			chip->irq_chg_det, ret);
		ret = IRQ_NONE;
		goto out;
	}
	
	//jeff, pull up
	s3c_gpio_setpull(chip->pdata->chg_det_irq_gpio,  S3C_GPIO_PULL_UP);
	gpio_direction_input(chip->pdata->chg_det_irq_gpio);
	
	val = gpio_get_value(chip->pdata->chg_det_irq_gpio);
	gpio_free(chip->pdata->chg_det_irq_gpio);

	printk("%s: chg detect pin status : %s\n",__func__, val?("off"):("on"));
	
	ret = request_threaded_irq(chip->irq_chg_det, NULL, vbus_det_irq_thread,
				   IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
				   "chager-det-irq", chip);

	if (ret) {
		dev_err(&client->dev, "Failed to request IRQ %d: %d\n",
			chip->irq_chg_det, ret);
		goto out;
	}
	enable_irq_wake(chip->irq_chg_det);
	
	if(!chip->pdata->stat_irq_gpio) {
		dev_warn(&client->dev, "No stat  gpio specified.\n");
	}

	chip->ac.name = "bq2415x-ac";
	chip->ac.type = POWER_SUPPLY_TYPE_MAINS;
	chip->ac.properties = bq2415x_ac_props;
	chip->ac.num_properties = ARRAY_SIZE(bq2415x_ac_props);
	chip->ac.get_property = bq2415x_ac_get_prop;
	ret = power_supply_register(&client->dev,  &chip->ac);
	if (ret)
		goto out;
	chip->ac.dev->parent = &client->dev;

	chip->usb.name = "bq2415x-usb";
	chip->usb.type = POWER_SUPPLY_TYPE_USB;
	chip->usb.properties = bq2415x_usb_props;
	chip->usb.num_properties = ARRAY_SIZE(bq2415x_usb_props);
	chip->usb.get_property = bq2415x_usb_get_prop;
	ret = power_supply_register(&client->dev,  &chip->usb);
	if (ret)
		goto out_usb;
	chip->usb.dev->parent = &client->dev;

	//chip->batt_detect = pdata->batt_detect;
	//chip->topoff_threshold = pdata->topoff_threshold;
	//chip->fast_charge = pdata->fast_charge;
	//chip->set_charger = pdata->set_charger;

	bq2415x_init_charger(chip);
	
	wake_lock_init(&vbus_wake_lock, WAKE_LOCK_SUSPEND, "vbus_present");
	
	INIT_DELAYED_WORK(&chip->status_work, cable_status_update_work_func);
	schedule_delayed_work(&chip->status_work, poll_interval*HZ);

	INIT_DELAYED_WORK(&chip->reset_timer_work, bq2145x_charge_reset_timer);

	printk("read chip id 0x%x\n",bq2415x_read_reg(client, 0x03));
	
	g_bq2415x_initial = 1;
	return 0;

out_usb:
	power_supply_unregister(&chip->ac);
out:
	printk(CHG_DEBUG "%s, error, return\n", __func__);
	kfree(chip);
	return ret;
}


static __devexit int bq2415x_power_remove(struct i2c_client *client)
{
	struct bq2415x_chip *chip = i2c_get_clientdata(client);;

	if (chip) {
		power_supply_unregister(&chip->ac);
		power_supply_unregister(&chip->usb);
		cancel_delayed_work_sync(&chip->status_work);
		cancel_delayed_work_sync(&chip->reset_timer_work);
		//bq2415x_deinit_charger(info);
		wake_lock_destroy(&vbus_wake_lock);
		kfree(chip);
		
	}
	return 0;
}




#ifdef CONFIG_PM
static int bq2415x_power_suspend(struct device *dev)
{
	return 0;
}

static void bq2415x_power_resume(struct device *dev)
{
}
#else
#define bq2415x_power_suspend NULL
#define bq2415x_power_resume NULL
#endif /* CONFIG_PM */

static const struct dev_pm_ops bq2415x_power_pm_ops = {
	.prepare	= bq2415x_power_suspend,
	.complete	= bq2415x_power_resume,
};

static const struct i2c_device_id bq2415x_id[] = {
	{ "bq24153A", BQ24153A },
	{ "bq24156A", BQ24156A },
	{ "bq24158", BQ24158 },	
	{ }
};
MODULE_DEVICE_TABLE(i2c, bq2415x_id);

static struct i2c_driver bq2415x_i2c_driver = {
	.driver	= {
		.name	= "bq2415x-power",
		.owner = THIS_MODULE,
		.pm	= &bq2415x_power_pm_ops,
	},
	.probe		= bq2415x_power_i2c_probe,
	.remove		= __devexit_p(bq2415x_power_remove),
	
	.id_table	= bq2415x_id,
};

static int __init bq2415x_power_init(void)
{
	return i2c_add_driver(&bq2415x_i2c_driver);
}
module_init(bq2415x_power_init);

static void __exit bq2415x_power_exit(void)
{
	i2c_del_driver(&bq2415x_i2c_driver);
}
module_exit(bq2415x_power_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Power supply driver for bq2415x");
MODULE_ALIAS("platform:bq2415x-power");
