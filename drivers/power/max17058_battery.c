/*
 *  max17058_battery.c
 *  fuel-gauge systems for lithium-ion (Li+) batteries
 *
 *  Copyright (C) 2009 Samsung Electronics
 *  Minkyu Kang <mk7.kang@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/power_supply.h>
#include <linux/power/max17058_battery.h>
#ifdef CONFIG_MACH_STUTTGART
#include <linux/gpio.h>
#include <plat/gpio-core.h>
#include <plat/gpio-cfg.h>
#include <plat/gpio-cfg-helpers.h>
#endif

#define MAX17058_VCELL_MSB	0x02
#define MAX17058_VCELL_LSB	0x03
#define MAX17058_SOC_MSB	0x04
#define MAX17058_SOC_LSB	0x05
#define MAX17058_MODE_MSB	0x06
#define MAX17058_MODE_LSB	0x07
#define MAX17058_VER_MSB	0x08
#define MAX17058_VER_LSB	0x09
#define MAX17058_CONFIG_MSB	0x0C
#define MAX17058_CONFIG_LSB	0x0D
#define MAX17058_VRESET_MSB	0x18
#define MAX17058_VRESET_LSB	0x19
#define MAX17058_CMD_MSB	0xFE
#define MAX17058_CMD_LSB	0xFF

#define MAX17058_DELAY		(50*1000)   //15sec polling
#define MAX17058_BATTERY_FULL	100
extern int max8971_charger_online(void);
extern int max8971_wakelock_status(void);
extern int max8971_check_temperature(void);
extern int max8971_check_battery_details(void);
extern int max8971_check_charger_details(void);

struct max17058_chip {
	struct i2c_client		*client;
	struct delayed_work		work;
	struct power_supply		battery;
	struct max17058_platform_data	*pdata;

	/* State Of Connect */
	int online;
	/* battery voltage */
	int vcell;
	/* battery ovc */
	int ovc;
	/* battery capacity */
	int soc;
	/* State Of Charge */
	int status;
	/* state of health */
	int health;
};
static struct max17058_chip *max17058 = NULL;
static struct wake_lock low_bat_wake_lock;
static int max17058_get_property(struct power_supply *psy,
			    enum power_supply_property psp,
			    union power_supply_propval *val)
{
	struct max17058_chip *chip = container_of(psy,
				struct max17058_chip, battery);
	static int prev_soc = 0;
	static int counter = 0;
	if(prev_soc==0)
		prev_soc = chip->soc;//init prev_soc

	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		val->intval = chip->status;
		break;
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = chip->online;
		break;
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = chip->vcell*1000;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		if(max8971_charger_online()){ //charger is online
			prev_soc = chip->soc;		
		}
		else{
			if(chip->soc < prev_soc) 
				prev_soc = chip->soc;
		}
		val->intval = prev_soc;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval = chip->health;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int max17058_write_reg(struct i2c_client *client, int reg, u8 value)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int max17058_read_reg(struct i2c_client *client, int reg)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int max17058_write_word_reg(struct i2c_client *client, int reg, u16 value)
{
    int ret = i2c_smbus_write_word_data(client, reg,swab16(value));

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int max17058_read_word_reg(struct i2c_client *client, int reg)
{
    int ret = i2c_smbus_read_word_data(client, reg);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return swab16(ret);
}

static void max17058_reset(struct i2c_client *client)
{
	 max17058_write_word_reg(client, MAX17058_CMD_MSB, 0x5400); //compeletly reset
	 
}

static void max17058_get_vcell(struct i2c_client *client)
{
	struct max17058_chip *chip = i2c_get_clientdata(client);
	int  read_data;
 	 	 
 	read_data = max17058_read_word_reg(client, MAX17058_VCELL_MSB);//16bit  read
 	 
 	 
 	chip->vcell = ((read_data>>8)&0xff)* 20+(read_data&0xff)*10/128; //mc,78.125uV/cell  78.125uV*2^8=20mV
 	//dev_info(&client->dev, "vcell : %d mv\n", chip->vcell);  //jeff
}

static void max17058_get_ocv(struct i2c_client *client){
	struct max17058_chip *chip = i2c_get_clientdata(client);
	int  read_data;
 	max17058_write_word_reg(client, 0x3e, 0x4a57);//unlock
 	read_data = max17058_read_word_reg(client, 0x0e);//16bit  read
	max17058_write_word_reg(client, 0x3e, 0x00);//lock
	//dev_info(&client->dev, "ocv=0x%x\n", read_data);
 	chip->ovc = ((read_data>>8)&0xff)* 20+(read_data&0xff)*10/128;
}

static void max17058_get_soc(struct i2c_client *client)
{
	struct max17058_chip *chip = i2c_get_clientdata(client);
	int read_data;
 	int soc_init;
 	int alrt;
 	read_data = max17058_read_word_reg(client, MAX17058_SOC_MSB);//16bit  read
	if(((read_data>>8)&0xff) == 1){
		alrt = max17058_read_word_reg(client, 0x0C);
		if(alrt&0x20)
			max17058_write_word_reg(client,0x0C,(alrt & 0xFFDF));//clear alrt bit
	}
 	// if((read_data&0xff)>0x7F)  
 	//        soc_init = min(((read_data>>8)&0xff)/2+1, 100);
 	// else 
             soc_init = min(((read_data>>8)&0xff)/2, 100); //mc,uboot-load custom model,1%/512cell,
#if 0
 	 if(soc_init>60)
 	 	   soc_init =(soc_init*11-56)/10;//mc,96% is set for full(100%)
#endif 	 	   
 	       chip->soc=min(soc_init,100);
 	 	   
 	//dev_info(&client->dev, "soc : %d\n", chip->soc);  //jeff
}

static void max17058_get_version(struct i2c_client *client)
{
	int version;
 	 
 	version = max17058_read_word_reg(client, MAX17058_VER_MSB);
 	  	 
 	dev_info(&client->dev, "MAX17058 Fuel-Gauge Ver %.4x\n", version);

}

static void max17058_get_online(struct i2c_client *client)
{
	struct max17058_chip *chip = i2c_get_clientdata(client);
#if 0
	//jeff, TODO
	if (chip->pdata->battery_online)
		chip->online = chip->pdata->battery_online();
	else
		chip->online = 1;
#endif

	chip->online = 1;
}

static void max17058_get_status(struct i2c_client *client)
{
	struct max17058_chip *chip = i2c_get_clientdata(client);

/* 
	//jeff, TODO
	if (!chip->pdata->charger_online || !chip->pdata->charger_enable) {
		chip->status = POWER_SUPPLY_STATUS_UNKNOWN;
		return;
	}

	if (chip->pdata->charger_online()) {
		if (chip->pdata->charger_enable())
			chip->status = POWER_SUPPLY_STATUS_CHARGING;
		else
			chip->status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	} else {
		chip->status = POWER_SUPPLY_STATUS_DISCHARGING;
	}
*/
	int charger_type = max8971_charger_online();
	int charger_details = max8971_check_charger_details();
	if(!charger_type){ //without charger
		chip->status = POWER_SUPPLY_STATUS_DISCHARGING;
	}
	else{//with charger
		if(charger_details<5){
			chip->status = POWER_SUPPLY_STATUS_CHARGING;
			if (chip->soc >= MAX17058_BATTERY_FULL)
				chip->status = POWER_SUPPLY_STATUS_FULL;
		}
		else
			chip->status = POWER_SUPPLY_STATUS_NOT_CHARGING;
	}
}

static void max17058_work(struct work_struct *work)
{
	struct max17058_chip *chip;

	int charger_type = max8971_charger_online();
	int temperature = max8971_check_temperature();
	int battery_details = max8971_check_battery_details();
	int charger_details = max8971_check_charger_details();
	
	chip = container_of(work, struct max17058_chip, work.work);
	
	max17058_get_vcell(chip->client);
	max17058_get_ocv(chip->client);
	max17058_get_soc(chip->client);
	max17058_get_online(chip->client);
	max17058_get_status(chip->client);
#if 1
	chip->health = POWER_SUPPLY_HEALTH_GOOD;
	if(temperature == 1){
		chip->health = POWER_SUPPLY_HEALTH_COLD;
	}
	else if(temperature == 5){
		chip->health = POWER_SUPPLY_HEALTH_OVERHEAT;
	}
	if(battery_details == 0){
		chip->health = POWER_SUPPLY_HEALTH_DEAD;
	}
	else if(battery_details == 3){
		chip->health = POWER_SUPPLY_HEALTH_OVERVOLTAGE;
	}
#endif
	dev_info(&chip->client->dev, "battery:SOC(%d), VCELL(%dmV), OCV(%dmV)\n",
        chip->soc, chip->vcell, chip->ovc);
	dev_info(&chip->client->dev, "charger: type(%d), status(%d), T(%d)\n",
        charger_type, charger_details, temperature);
	//jeff
	power_supply_changed(&chip->battery);

	schedule_delayed_work(&chip->work, msecs_to_jiffies(MAX17058_DELAY));
}

static irqreturn_t max17058_lowbat_handler(int irq, void *data)
{
	struct max17058_chip *chip = (struct max17058_chip *)data;
	wake_lock_timeout(&low_bat_wake_lock, 5*HZ);
	//max17058_get_soc(chip->client);
	dev_info(&chip->client->dev, "%s,soc=%d\n", __func__,chip->soc);
	cancel_delayed_work(&chip->work);
	schedule_delayed_work(&chip->work,2*HZ);
	return IRQ_HANDLED;
}
void update_battery_icon(void){
	cancel_delayed_work(&max17058->work);
	schedule_delayed_work(&max17058->work, 0);
}
static enum power_supply_property max17058_battery_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_ONLINE,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_HEALTH,
};

static int __devinit max17058_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct max17058_chip *chip;
	int ret;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		return -EIO;

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;
	max17058 = chip;
	chip->client = client;
	chip->pdata = client->dev.platform_data;

	i2c_set_clientdata(client, chip);

	chip->battery.name		= "battery";
	chip->battery.type		= POWER_SUPPLY_TYPE_BATTERY;
	chip->battery.get_property	= max17058_get_property;
	chip->battery.properties	= max17058_battery_props;
	chip->battery.num_properties	= ARRAY_SIZE(max17058_battery_props);

	ret = power_supply_register(&client->dev, &chip->battery);
	if (ret) {
		dev_err(&client->dev, "failed: power supply register\n");
		kfree(chip);
		return ret;
	}
	wake_lock_init(&low_bat_wake_lock, WAKE_LOCK_SUSPEND, "low_bat");
	//max17058_reset(client);  //reset in u-boot
	max17058_get_version(client);
	max17058_write_word_reg(client,0x0C,(0x1E |(max17058_read_word_reg(client,0x0C) & 0xFF00)));
	INIT_DELAYED_WORK_DEFERRABLE(&chip->work, max17058_work);
	schedule_delayed_work(&chip->work, msecs_to_jiffies(0));

#ifdef CONFIG_MACH_STUTTGART
	/* low SOC irq for AP*/
	int irq_gpio = EXYNOS4_GPX3(5);  /* jeff, platform not support irq_to_gpio() API, so direct set GPIO*/

	dev_info(&chip->client->dev, "low_bat_irq GPIO :%d\n",irq_gpio );
	ret = gpio_request(irq_gpio, " max17058");
	if (ret < 0) {
		dev_err(&chip->client->dev,
			"Failed to request gpio %d with ret: %d\n",
			irq_gpio, ret);
		ret = IRQ_NONE;
		goto out;
	}
	
	s3c_gpio_setpull(irq_gpio,  S3C_GPIO_PULL_UP);
	gpio_direction_input(irq_gpio);
	gpio_free(irq_gpio);

      ret = request_threaded_irq(client->irq, NULL, max17058_lowbat_handler,
            		/*IRQF_TRIGGER_RISING | */IRQF_TRIGGER_FALLING , client->name, chip);  //jeff
      enable_irq_wake(client->irq);
out:	
#endif

	return 0;
}

static int __devexit max17058_remove(struct i2c_client *client)
{
	struct max17058_chip *chip = i2c_get_clientdata(client);

	power_supply_unregister(&chip->battery);
	cancel_delayed_work(&chip->work);
	kfree(chip);
	return 0;
}

#ifdef CONFIG_PM

static int max17058_suspend(struct i2c_client *client,
		pm_message_t state)
{
	struct max17058_chip *chip = i2c_get_clientdata(client);
	dev_info(&chip->client->dev,"%s++\n",__func__);
	cancel_delayed_work(&chip->work);
	return 0;
}

static int max17058_resume(struct i2c_client *client)
{
	struct max17058_chip *chip = i2c_get_clientdata(client);
	//dev_info(&chip->client->dev,"%s++\n",__func__);
	schedule_delayed_work(&chip->work, 0/*msecs_to_jiffies(MAX17058_DELAY)*/); //report the soc at once when the phone is resumed
	return 0;
}

#else

#define max17058_suspend NULL
#define max17058_resume NULL

#endif /* CONFIG_PM */

static const struct i2c_device_id max17058_id[] = {
	{ "max17058", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max17058_id);

static struct i2c_driver max17058_i2c_driver = {
	.driver	= {
		.name	= "max17058",
	},
	.probe		= max17058_probe,
	.remove		= __devexit_p(max17058_remove),
	.suspend	= max17058_suspend,
	.resume		= max17058_resume,
	.id_table	= max17058_id,
};

static int __init max17058_init(void)
{
	return i2c_add_driver(&max17058_i2c_driver);
}
module_init(max17058_init);

static void __exit max17058_exit(void)
{
	i2c_del_driver(&max17058_i2c_driver);
}
module_exit(max17058_exit);

MODULE_AUTHOR("Minkyu Kang <mk7.kang@samsung.com>");
MODULE_DESCRIPTION("MAX17058 Fuel Gauge");
MODULE_LICENSE("GPL");
