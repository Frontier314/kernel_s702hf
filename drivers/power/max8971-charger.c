/* 
 *  MAXIM MAX8971 Charger Driver
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/sysfs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/power/max8971-charger.h>

#ifdef CONFIG_MACH_STUTTGART
#include <linux/gpio.h>
#include <plat/gpio-core.h>
#include <plat/gpio-cfg.h>
#include <plat/gpio-cfg-helpers.h>
#endif
#include <plat/usbgadget.h>

#define CHG_DEBUG  KERN_DEBUG //

// define register map
#define MAX8971_REG_CHGINT      0x0F

#define MAX8971_REG_CHGINT_MASK 0x01

#define MAX8971_REG_CHG_STAT    0x02

#define MAX8971_DCV_MASK        0x80
#define MAX8971_DCV_SHIFT       7
#define MAX8971_TOPOFF_MASK     0x40
#define MAX8971_TOPOFF_SHIFT    6
#define MAX8971_DCI_OK          0x40    // for status register
#define MAX8971_DCI_OK_SHIFT    6
#define MAX8971_DCOVP_MASK      0x20
#define MAX8971_DCOVP_SHIFT     5
#define MAX8971_DCUVP_MASK      0x10
#define MAX8971_DCUVP_SHIFT     4
#define MAX8971_CHG_MASK        0x08
#define MAX8971_CHG_SHIFT       3
#define MAX8971_BAT_MASK        0x04
#define MAX8971_BAT_SHIFT       2
#define MAX8971_THM_MASK        0x02
#define MAX8971_THM_SHIFT       1
#define MAX8971_PWRUP_OK_MASK   0x01
#define MAX8971_PWRUP_OK_SHIFT  0
#define MAX8971_I2CIN_MASK      0x01
#define MAX8971_I2CIN_SHIFT     0

#define MAX8971_REG_DETAILS1    0x03
#define MAX8971_DC_V_MASK       0x80
#define MAX8971_DC_V_SHIFT      7
#define MAX8971_DC_I_MASK       0x40
#define MAX8971_DC_I_SHIFT      6
#define MAX8971_DC_OVP_MASK     0x20
#define MAX8971_DC_OVP_SHIFT    5
#define MAX8971_DC_UVP_MASK     0x10
#define MAX8971_DC_UVP_SHIFT    4
#define MAX8971_THM_DTLS_MASK   0x07
#define MAX8971_THM_DTLS_SHIFT  0

#define MAX8971_THM_DTLS_COLD   1       // charging suspended(temperature<T1)
#define MAX8971_THM_DTLS_COOL   2       // (T1<temperature<T2)
#define MAX8971_THM_DTLS_NORMAL 3       // (T2<temperature<T3)
#define MAX8971_THM_DTLS_WARM   4       // (T3<temperature<T4)
#define MAX8971_THM_DTLS_HOT    5       // charging suspended(temperature>T4)

#define MAX8971_REG_DETAILS2    0x04
#define MAX8971_BAT_DTLS_MASK   0x30
#define MAX8971_BAT_DTLS_SHIFT  4
#define MAX8971_CHG_DTLS_MASK   0x0F
#define MAX8971_CHG_DTLS_SHIFT  0

#define MAX8971_BAT_DTLS_BATDEAD        0   // VBAT<2.1V
#define MAX8971_BAT_DTLS_TIMER_FAULT    1   // The battery is taking longer than expected to charge
#define MAX8971_BAT_DTLS_BATOK          2   // VBAT is okay.
#define MAX8971_BAT_DTLS_GTBATOV        3   // VBAT > BATOV

#define MAX8971_CHG_DTLS_DEAD_BAT           0   // VBAT<2.1V, TJSHDN<TJ<TJREG
#define MAX8971_CHG_DTLS_PREQUAL            1   // VBAT<3.0V, TJSHDN<TJ<TJREG
#define MAX8971_CHG_DTLS_FAST_CHARGE_CC     2   // VBAT>3.0V, TJSHDN<TJ<TJREG
#define MAX8971_CHG_DTLS_FAST_CHARGE_CV     3   // VBAT=VBATREG, TJSHDN<TJ<TJREG
#define MAX8971_CHG_DTLS_TOP_OFF            4   // VBAT>=VBATREG, TJSHDN<TJ<TJREG
#define MAX8971_CHG_DTLS_DONE               5   // VBAT>VBATREG, T>Ttopoff+16s, TJSHDN<TJ<TJREG
#define MAX8971_CHG_DTLS_TIMER_FAULT        6   // VBAT<VBATOV, TJ<TJSHDN
#define MAX8971_CHG_DTLS_TEMP_SUSPEND       7   // TEMP<T1 or TEMP>T4
#define MAX8971_CHG_DTLS_USB_SUSPEND        8   // charger is off, DC is invalid or chaarger is disabled(USBSUSPEND)
#define MAX8971_CHG_DTLS_THERMAL_LOOP_ACTIVE    9   // TJ > REGTEMP
#define MAX8971_CHG_DTLS_CHG_OFF            10  // charger is off and TJ >TSHDN

#define MAX8971_REG_CHGCNTL1    0x05
#define MAX8971_DCMON_DIS_MASK  0x02
#define MAX8971_DCMON_DIS_SHIFT 1
#define MAX8971_USB_SUS_MASK    0x01
#define MAX8971_USB_SUS_SHIFT   0

#define MAX8971_REG_FCHGCRNT    0x06
#define MAX8971_CHGCC_MASK      0x1F
#define MAX8971_CHGCC_SHIFT     0
#define MAX8971_FCHGTIME_MASK   0xE0
#define MAX8971_FCHGTIME_SHIFT  5


#define MAX8971_REG_DCCRNT      0x07
#define MAX8971_CHGRSTRT_MASK   0x40
#define MAX8971_CHGRSTRT_SHIFT  6
#define MAX8971_DCILMT_MASK     0x3F
#define MAX8971_DCILMT_SHIFT    0

#define MAX8971_REG_TOPOFF          0x08
#define MAX8971_TOPOFFTIME_MASK     0xE0
#define MAX8971_TOPOFFTIME_SHIFT    5
#define MAX8971_IFST2P8_MASK        0x10
#define MAX8971_IFST2P8_SHIFT       4
#define MAX8971_TOPOFFTSHLD_MASK    0x0C
#define MAX8971_TOPOFFTSHLD_SHIFT   2
#define MAX8971_CHGCV_MASK          0x03
#define MAX8971_CHGCV_SHIFT         0

#define MAX8971_REG_TEMPREG     0x09
#define MAX8971_REGTEMP_MASK    0xC0
#define MAX8971_REGTEMP_SHIFT   6
#define MAX8971_THM_CNFG_MASK   0x08
#define MAX8971_THM_CNFG_SHIFT  3
#define MAX8971_SAFETYREG_MASK  0x01
#define MAX8971_SAFETYREG_SHIFT 0

#define MAX8971_REG_PROTCMD     0x0A
#define MAX8971_CHGPROT_MASK    0x0C
#define MAX8971_CHGPROT_SHIFT   2

extern void update_battery_icon(void);
#ifdef CONFIG_USB_GADGET
extern bool s3c_udc_is_adc_cable(void);
#else
bool s3c_udc_is_adc_cable(void);
{
        return 0;
}
#endif
struct max8971_chip {
	struct i2c_client *client;
	struct power_supply charger;//for AC
	struct power_supply usb_charger;//for usb
	struct max8971_platform_data *pdata;
	int irq;
  	int chg_online;
	int usb_chg; //wz, 1:usb charger; 0: ac charger
	struct delayed_work status_work;  //jeff
	struct timer_list irq_timer;
	struct work_struct  irq_work;         //jeff
	int suspended;
	int chip_exist; //wz, for factory tool
	
};

static struct max8971_chip *max8971_chg;
static struct wake_lock vbus_on_wake_lock;  //jeff, wake_lock for vbus
static struct wake_lock vbus_off_wake_lock;
static int wake_lock_inited = 0;
static bool need_run_once = false;
static int max8971_write_reg(struct i2c_client *client, u8 reg, u8 value)
{

	//printk(CHG_DEBUG "%s > reg:0x%02x, val:0x%02x\n",__func__, reg,value); //jeff
	
	int ret = i2c_smbus_write_byte_data(client, reg, value);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int max8971_read_reg(struct i2c_client *client, u8 reg)
{
	int ret = i2c_smbus_read_byte_data(client, reg);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
	
	//printk(CHG_DEBUG "%s > reg:0x%02x, val:0x%02x\n",__func__, reg, ret);//jeff
	
	return ret;
}

static int max8971_set_bits(struct i2c_client *client, u8 reg, u8 mask, u8 data)
{
	u8 value = 0;
	int ret;

	ret = max8971_read_reg(client, reg);
	if (ret < 0)
		goto out;
    value=ret;  //correct error 
	value &= ~mask;
	value |= data;
	ret = max8971_write_reg(client, reg, value);
out:
	return ret;
}

//jeff,
extern void s3c_usb_device_power_control(int is_active);
static void status_update_work_func(struct work_struct *work)
{
	struct max8971_chip *chip =
		container_of(work, struct max8971_chip, status_work.work);
	int ret;
	//dev_info(&chip->client->dev, "%s\n",__func__);
        //mdelay(100);

	s3c_usb_device_power_control(max8971_chg->chg_online); //haozz

   if(max8971_chg->chg_online) {      
      if(s3c_udc_is_adc_cable()){
	max8971_chg->usb_chg = 0;
        // AC charger, fast charging cureent 1000ma
        max8971_write_reg(chip->client, MAX8971_REG_FCHGCRNT,0x14);//0x14=1000mA
        ret=max8971_read_reg(chip->client, MAX8971_REG_FCHGCRNT);
        dev_info(&chip->client->dev, " AC charger current 1000mA, disable fast charge timer(0x14),details:%x\n",ret);
        max8971_write_reg(chip->client,MAX8971_REG_DCCRNT,0x6c);//DClimit 1100mA, 	restart -100mA
        ret=max8971_read_reg(chip->client,MAX8971_REG_DCCRNT);
        dev_info(&chip->client->dev, " AC charger limit current 1100mA, restart threshold -100mV (0x6c),details:%x\n",ret);
        wake_unlock(&vbus_on_wake_lock);
        }else{
        max8971_chg->usb_chg = 1;
        // USB charger, fast charging cureent 500ma
        max8971_write_reg(chip->client, MAX8971_REG_FCHGCRNT, 0x0A);//0x0A=500mA
        ret=max8971_read_reg(chip->client, MAX8971_REG_FCHGCRNT);
        dev_info(&chip->client->dev, " USB charger current 500mA, disable fast charge timer(0x0A),details:%x\n",ret);
        max8971_write_reg(chip->client,MAX8971_REG_DCCRNT,0x54);//DClimit 500mA, 	restart -100mA
        ret=max8971_read_reg(chip->client,MAX8971_REG_DCCRNT);
        dev_info(&chip->client->dev, " USB charger limit current 500mA, restart threshold -100mV (0x54),details:%x\n",ret);
     }
   }
	power_supply_changed(&chip->charger);
	power_supply_changed(&chip->usb_charger);
	update_battery_icon();
}
//end

/*diog.zhao,0614
**add a function which can get chg status in kernel*/
int usb_check_curchage_status(void)
{
	return max8971_chg->chg_online;
}
EXPORT_SYMBOL_GPL(usb_check_curchage_status);

static int __set_charger(struct max8971_chip *chip, int enable)
{
    u8  reg_val= 0;
    // unlock charger protection
    reg_val = MAX8971_CHGPROT_UNLOCKED<<MAX8971_CHGPROT_SHIFT;
    max8971_write_reg(chip->client, MAX8971_REG_PROTCMD, reg_val);   

	if (enable) {
		/* enable charger */
             
         //Set fast charge current and timer
         reg_val = ((chip->pdata->chgcc<<MAX8971_CHGCC_SHIFT) |
                    (chip->pdata->fchgtime<<MAX8971_FCHGTIME_SHIFT));
         max8971_write_reg(chip->client, MAX8971_REG_FCHGCRNT,reg_val);
        // Set input current limit and charger restart threshold
        reg_val = ((chip->pdata->chgrstrt<<MAX8971_CHGRSTRT_SHIFT) |
                   (chip->pdata->dcilmt<<MAX8971_DCILMT_SHIFT));
        max8971_write_reg(chip->client, MAX8971_REG_DCCRNT, reg_val);

        // Set topoff condition
        reg_val = ((chip->pdata->topofftime<<MAX8971_TOPOFFTIME_SHIFT) |
                   (chip->pdata->topofftshld<<MAX8971_TOPOFFTSHLD_SHIFT) |
                   (chip->pdata->chgcv<<MAX8971_CHGCV_SHIFT));
        max8971_write_reg(chip->client, MAX8971_REG_TOPOFF, reg_val);

        // Set temperature condition
        reg_val = ((chip->pdata->regtemp<<MAX8971_REGTEMP_SHIFT) |
                   (chip->pdata->thm_config<<MAX8971_THM_CNFG_SHIFT) |
                   (chip->pdata->safetyreg<<MAX8971_SAFETYREG_SHIFT));
        max8971_write_reg(chip->client, MAX8971_REG_TEMPREG, reg_val);       

        // USB Suspend and DC Voltage Monitoring
        // Set DC Voltage Monitoring to Enable and USB Suspend to Disable
        reg_val = (0<<MAX8971_DCMON_DIS_SHIFT) | (0<<MAX8971_USB_SUS_SHIFT);
        max8971_write_reg(chip->client, MAX8971_REG_CHGCNTL1, reg_val);  
		
        //jeff, TODO: we should not lock if vbus is charger, it will fixed later
        wake_lock(&vbus_on_wake_lock);
	} else {
		/* disable charge */
		max8971_set_bits(chip->client, MAX8971_REG_CHGCNTL1, MAX8971_USB_SUS_MASK, 1);
		wake_lock_timeout(&vbus_off_wake_lock, 2*HZ);
		if(wake_lock_active(&vbus_on_wake_lock))
			wake_unlock(&vbus_on_wake_lock);
	}
	dev_info(&chip->client->dev, "%s\n", (enable) ? "Enable charger" : "Disable charger");
	return 0;
}

static int max8971_charger_detail_irq(int irq, void *data, u8 *val)
{
    struct max8971_chip *chip = (struct max8971_chip *)data;
    u8  ret;
    switch (irq) 
    {
    case MAX8971_IRQ_PWRUP_OK:
        dev_info(&chip->client->dev, "Power Up OK Interrupt\n");
        if ((val[0] & MAX8971_DCUVP_MASK) == 0) {
            // check DCUVP_OK bit in CGH_STAT
            // Vbus is valid //
            // Mask interrupt regsiter //
            max8971_write_reg(chip->client, MAX8971_REG_CHGINT_MASK, chip->pdata->int_mask);            
            // DC_V valid and start charging
	    if(gpio_get_value(EXYNOS4_GPX2(4))){
            	chip->chg_online = 1;
            	__set_charger(chip, 1);
		}
        }
	else{
		chip->chg_online = 0;
		__set_charger(chip,0);
		}
        break;

    case MAX8971_IRQ_THM:
		//mc
        ret=max8971_read_reg(chip->client, MAX8971_REG_DETAILS1);
		ret &=MAX8971_THM_DTLS_MASK;
		if((ret==MAX8971_THM_DTLS_COOL)||(ret==MAX8971_THM_DTLS_NORMAL)||(ret==MAX8971_THM_DTLS_WARM)){
            chip->chg_online = 1;
            __set_charger(chip, 1);//inside the range of temperature,normal charger
	    max8971_write_reg(chip->client, MAX8971_REG_CHGINT_MASK, chip->pdata->int_mask);//disable temperature interrupt when temperature is normal
		}
		else if((ret==MAX8971_THM_DTLS_COLD)||(ret==MAX8971_THM_DTLS_HOT)){
		    chip->chg_online = 0;
            __set_charger(chip, 0);//outside the range of temperature,charger suspend
		   }
		dev_info(&chip->client->dev, "Thermistor Interrupt: details-0x%x\n", (val[1] & MAX8971_THM_DTLS_MASK));
		break;
       //end
    case MAX8971_IRQ_BAT:
        dev_info(&chip->client->dev, "Battery Interrupt: details-0x%x\n", (val[2] & MAX8971_BAT_DTLS_MASK));
        switch ((val[2] & MAX8971_BAT_MASK)>>MAX8971_BAT_SHIFT) 
        {
        case MAX8971_BAT_DTLS_BATDEAD:
            break;
        case MAX8971_BAT_DTLS_TIMER_FAULT:
            break;
        case MAX8971_BAT_DTLS_BATOK:
            break;
        case MAX8971_BAT_DTLS_GTBATOV:
            break;
        default:
            break;
        }
        break;

    case MAX8971_IRQ_CHG:
        dev_info(&chip->client->dev, "Fast Charge Interrupt: details-0x%x\n", (val[2] & MAX8971_CHG_DTLS_MASK));
        switch (val[2] & MAX8971_CHG_DTLS_MASK) 
        {
        case MAX8971_CHG_DTLS_DEAD_BAT:                                                       
            // insert event if a customer need to do something //
            break;
        case MAX8971_CHG_DTLS_PREQUAL:
            // insert event if a customer need to do something //
            break;
        case MAX8971_CHG_DTLS_FAST_CHARGE_CC:
            // insert event if a customer need to do something //
            break;
        case MAX8971_CHG_DTLS_FAST_CHARGE_CV:
            // insert event if a customer need to do something //
            break;
        case MAX8971_CHG_DTLS_TOP_OFF:
            // insert event if a customer need to do something //
            break;
        case MAX8971_CHG_DTLS_DONE:
	    max8971_set_bits(chip->client, MAX8971_REG_CHGCNTL1, MAX8971_USB_SUS_MASK, 1);//disable charger
	    max8971_set_bits(chip->client, MAX8971_REG_CHGCNTL1, MAX8971_USB_SUS_MASK, 0);
            // insert event if a customer need to do something //
            // Charging done and charge off automatically
            break;
        case MAX8971_CHG_DTLS_TIMER_FAULT:
            // insert event if a customer need to do something //
            break;
        case MAX8971_CHG_DTLS_TEMP_SUSPEND:
            // insert event if a customer need to do something //
            break;
        case MAX8971_CHG_DTLS_USB_SUSPEND:
            // insert event if a customer need to do something //
            break;
        case MAX8971_CHG_DTLS_THERMAL_LOOP_ACTIVE:
            // insert event if a customer need to do something //
            break;
        case MAX8971_CHG_DTLS_CHG_OFF:
            // insert event if a customer need to do something //
            break;
        default:
            break;
        }
        break;

    case MAX8971_IRQ_DCUVP:
        if ((val[1] & MAX8971_DC_UVP_MASK) == 0) {
            // VBUS is invalid. VDC < VDC_UVLO
            __set_charger(chip, 0);
            chip->chg_online = 0;	//jeff		
        }
        dev_info(&chip->client->dev, "DC Under voltage Interrupt: details-0x%x\n", (val[1] & MAX8971_DC_UVP_MASK));
        break;

    case MAX8971_IRQ_DCOVP:
        if (val[1] & MAX8971_DC_OVP_MASK) {
            // VBUS is invalid. VDC > VDC_OVLO
            __set_charger(chip, 0);
		chip->chg_online = 0;  //jeff
        }
        dev_info(&chip->client->dev, "DC Over voltage Interrupt: details-0x%x\n", (val[1] & MAX8971_DC_OVP_MASK));
        break;

    case MAX8971_IRQ_TOPOFF:
        dev_info(&chip->client->dev, "TOPOFF Interrupt\n");
        break;

    case MAX8971_IRQ_DCV:
        dev_info(&chip->client->dev, "DC Input Voltage Limit Interrupt: details-0x%x\n", (val[1] & MAX8971_DC_V_MASK));
        break;

    }
	if(chip->chg_online)
    schedule_delayed_work(&chip->status_work, 0); //jeff
    else
    schedule_delayed_work(&chip->status_work, HZ); //jeff
		
    
    return 0;
}

static int max8971_charger_irq_work(struct work_struct *work)
{
	struct max8971_chip *chip =
	    container_of(work, struct max8971_chip, irq_work);
	
	int irq_val, irq_mask, irq_name;
      u8 val[3];
	 
	irq_val = max8971_read_reg(chip->client, MAX8971_REG_CHGINT);
	irq_mask = max8971_read_reg(chip->client, MAX8971_REG_CHGINT_MASK);
	val[0] = max8971_read_reg(chip->client, MAX8971_REG_CHG_STAT);
	val[1] = max8971_read_reg(chip->client, MAX8971_REG_DETAILS1);
	val[2] = max8971_read_reg(chip->client, MAX8971_REG_DETAILS2);
   dev_info(&chip->client->dev, "chgint(%x),chgint_mask(%x),chg_stat(%x),details1(%x),details2(%x)\n",irq_val,irq_mask,val[0],val[1],val[2] );

	for (irq_name = MAX8971_IRQ_PWRUP_OK; irq_name<MAX8971_NR_IRQS; irq_name++) {
		if ((irq_val & (0x01<<irq_name)) && !(irq_mask & (0x01<<irq_name))) {
    			max8971_charger_detail_irq(irq_name, chip, val);
		}
	}
	return 0;
}

static void max8971_irq_timer(unsigned long _data)
{
	struct max8971_chip *chip = (struct max8971_chip *)_data;

	schedule_work(&chip->irq_work);
}


static irqreturn_t max8971_charger_handler(int irq, void *data)
{
	struct max8971_chip *chip = (struct max8971_chip *)data;
   
#if 0  //jeff, move to async work, because the max8971 wakeup IRQ will read i2c before i2c wakeup.
	struct max8971_chip *chip = (struct max8971_chip *)data;
	int irq_val, irq_mask, irq_name;
    u8 val[3];

	irq_val = max8971_read_reg(chip->client, MAX8971_REG_CHGINT);
    irq_mask = max8971_read_reg(chip->client, MAX8971_REG_CHGINT_MASK);

    val[0] = max8971_read_reg(chip->client, MAX8971_REG_CHG_STAT);
    val[1] = max8971_read_reg(chip->client, MAX8971_REG_DETAILS1);
    val[2] = max8971_read_reg(chip->client, MAX8971_REG_DETAILS2);

    for (irq_name = MAX8971_IRQ_PWRUP_OK; irq_name<MAX8971_NR_IRQS; irq_name++) {
        if ((irq_val & (0x01<<irq_name)) && !(irq_mask & (0x01<<irq_name))) {
            max8971_charger_detail_irq(irq_name, data, val);
        }
    }
#endif
	if(chip->suspended){
		mod_timer(&chip->irq_timer, jiffies + msecs_to_jiffies(HZ));
	}	
	else
		schedule_work(&chip->irq_work);

	return IRQ_HANDLED;
}
//wz:only a temp workaround for bug 165794 & 165795,something need to do.
static irqreturn_t max8971_charger_det_handler(int irq, void *data)
{
	struct max8971_chip *chip = (struct max8971_chip *)data;
	static int index=0;
	if(index==0){
		if(need_run_once){ 
			dev_info(&chip->client->dev, "will disable charger\n");
			__set_charger(chip,0);
			chip->chg_online = 0;
			cancel_delayed_work(&chip->status_work);
			schedule_delayed_work(&chip->status_work, 0);
		}
		index++;
		}
	return IRQ_HANDLED;
}
int max8971_charger_online(void){
	static int counter = 0;
	if(!max8971_chg)
		return 0; //no charger
	if(max8971_chg->chg_online){
		if(max8971_chg->usb_chg){
			if(counter == 0){
				cancel_delayed_work(&max8971_chg->status_work);
				schedule_delayed_work(&max8971_chg->status_work, 5*HZ);
			}
			counter = 1;
			return 1; //usb charger
		}
		else{	
			counter = 0;
			return 2; //ac charger
		    }
	}
	else{
		counter = 0;
		return 0;
	    }
}

int max8971_wakelock_status(void){
	if(wake_lock_inited){
		return wake_lock_active(&vbus_on_wake_lock);
	}
	else
		return 0; //not init
}

int max8971_check_temperature(void){
	u8 value = 0,val_mask;
	if(!max8971_chg)
		return 0;
	value = max8971_read_reg(max8971_chg->client, MAX8971_REG_DETAILS1);
	value &= MAX8971_THM_DTLS_MASK;
	if(max8971_chg->chg_online){
		if((value==MAX8971_THM_DTLS_COLD)||(value==MAX8971_THM_DTLS_HOT)){
			val_mask = max8971_read_reg(max8971_chg->client, MAX8971_REG_CHGINT_MASK);
			val_mask &= ~MAX8971_THM_MASK;
			max8971_write_reg(max8971_chg->client,MAX8971_REG_CHGINT_MASK,val_mask); //enable temperature interrupt,to en/disable charger in ISR
		}
		else
			max8971_write_reg(max8971_chg->client, MAX8971_REG_CHGINT_MASK, max8971_chg->pdata->int_mask);//disable temperature interrupt when temperature is normal
	}
	return value;
}

int max8971_check_battery_details(void){
	u8 value;

	if(!max8971_chg)
		return 0xff;
	value = max8971_read_reg(max8971_chg->client, MAX8971_REG_DETAILS2);
	value &= MAX8971_BAT_DTLS_MASK;
	value = value >> MAX8971_BAT_DTLS_SHIFT;

	if(value == MAX8971_BAT_DTLS_TIMER_FAULT ){
		max8971_set_bits(max8971_chg->client, MAX8971_REG_CHGCNTL1, MAX8971_USB_SUS_MASK, 1);//disable charger
		msleep(1000);
		max8971_set_bits(max8971_chg->client, MAX8971_REG_CHGCNTL1, MAX8971_USB_SUS_MASK, 0);//enable charger
	}
	return value;
}

int max8971_check_charger_details(void){
	u8 value;
	if(!max8971_chg)
		return 0xff;
	value = max8971_read_reg(max8971_chg->client, MAX8971_REG_DETAILS2);
	value &= MAX8971_CHG_DTLS_MASK;
	return value;
}

int max8971_stop_charging(void)
{
    // Charger removed
    max8971_chg->chg_online = 0;
    __set_charger(max8971_chg, 0);
    // Disable GSM TEST MODE
    max8971_set_bits(max8971_chg->client, MAX8971_REG_TOPOFF, MAX8971_IFST2P8_MASK, 0<<MAX8971_IFST2P8_SHIFT);

    return 0;
}
EXPORT_SYMBOL(max8971_stop_charging);

int max8971_start_charging(unsigned mA)
{
    if (mA == 2800) 
    {
        // GSM TEST MODE
        max8971_set_bits(max8971_chg->client, MAX8971_REG_TOPOFF, MAX8971_IFST2P8_MASK, 1<<MAX8971_IFST2P8_SHIFT);
    }
    else
    {
        // charger inserted
        max8971_chg->chg_online = 1;
        max8971_chg->pdata->chgcc = FCHG_CURRENT(mA);
        __set_charger(max8971_chg, 1);
    }
    return 0;
}
EXPORT_SYMBOL(max8971_start_charging);

static int max8971_charger_get_property(struct power_supply *psy,
                                        enum power_supply_property psp,
                                        union power_supply_propval *val)
{
	struct max8971_chip *chip = dev_get_drvdata(psy->dev->parent);	 //jeff
	
	int ret = 0;
    int chg_dtls_val;
	char status[20];
	int check_usb_chg = 0;
	
	if(!strcmp(psy->name,"usb-charger"))
		check_usb_chg = 1;
	switch (psp) {
	case POWER_SUPPLY_PROP_ONLINE:
		val->intval = check_usb_chg?(chip->chg_online?(chip->usb_chg?1:0):0):(chip->chg_online?(chip->usb_chg?0:1):0);
		//dev_info(&chip->client->dev, "POWER_SUPPLY_PROP_ONLINE, online %d\n", chip->chg_online);//jeff
		break;
	case POWER_SUPPLY_PROP_CHARGER_EXIST:
		val->intval = chip->chip_exist;
		break;
    case POWER_SUPPLY_PROP_STATUS:
		ret = max8971_read_reg(chip->client, MAX8971_REG_DETAILS2);
        chg_dtls_val = (ret & MAX8971_CHG_DTLS_MASK);
        if (chip->chg_online) {
            if (chg_dtls_val == MAX8971_CHG_DTLS_DONE) {
                val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
                strcpy(status, "DISCHARGING");
            }
            else if ((chg_dtls_val == MAX8971_CHG_DTLS_TIMER_FAULT) ||
                     (chg_dtls_val == MAX8971_CHG_DTLS_TEMP_SUSPEND) ||
                     (chg_dtls_val == MAX8971_CHG_DTLS_USB_SUSPEND) ||
                     (chg_dtls_val == MAX8971_CHG_DTLS_CHG_OFF)) {
                val->intval = POWER_SUPPLY_STATUS_NOT_CHARGING;
                strcpy(status, "NOT_CHARGING");
            }
            else {
                val->intval = POWER_SUPPLY_STATUS_CHARGING;
                strcpy(status, "CHARGING");
            }
        }
        else {
            val->intval = POWER_SUPPLY_STATUS_DISCHARGING;
            strcpy(status, "DISCHARGING");
        }

	 //dev_info(&chip->client->dev, "POWER_SUPPLY_PROP_STATUS : %s \n",status);//jeff, debug
        ret = 0;
		break;	
    default:
		ret = -ENODEV;
		break;
	}
	return ret;
}

static enum power_supply_property max8971_charger_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
      	POWER_SUPPLY_PROP_STATUS,
      	POWER_SUPPLY_PROP_CHARGER_EXIST,
};

static __devinit int max8971_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{

	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct max8971_chip *chip;
    int ret;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		return -EIO;

	chip = kzalloc(sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->client = client;
	chip->pdata = client->dev.platform_data;
	
       max8971_chg = chip;

	i2c_set_clientdata(client, chip);
	//wz, check the i2c interface
	ret = max8971_read_reg(chip->client, MAX8971_REG_FCHGCRNT);
	if(ret < 0){
		dev_err(&client->dev,"i2c check failed :(\n");
		ret = -EIO;
		goto out;
		}
	else
		chip->chip_exist = 1;

	chip->charger.name = "max8971-charger";
	chip->charger.type = POWER_SUPPLY_TYPE_MAINS;
	chip->charger.properties = max8971_charger_props;
	chip->charger.num_properties = ARRAY_SIZE(max8971_charger_props);
	chip->charger.get_property = max8971_charger_get_property;
	ret = power_supply_register(&client->dev,  &chip->charger);
	if (ret){
		dev_err(&client->dev, "failed:ac power supply register\n");
		i2c_set_clientdata(client, NULL);
		goto out;
	}
	chip->charger.dev->parent = &client->dev;

	//add by wz
	chip->usb_charger.name = "usb-charger";
	chip->usb_charger.type = POWER_SUPPLY_TYPE_USB;
	chip->usb_charger.properties = max8971_charger_props;
	chip->usb_charger.num_properties = ARRAY_SIZE(max8971_charger_props);
	chip->usb_charger.get_property = max8971_charger_get_property;
	ret = power_supply_register(&client->dev,  &chip->usb_charger);
	if (ret){
		dev_err(&client->dev, "failed: usb power supply register\n");
		i2c_set_clientdata(client, NULL);
		goto out;
	}
	chip->usb_charger.dev->parent = &client->dev;

	//jeff
	wake_lock_init(&vbus_on_wake_lock, WAKE_LOCK_SUSPEND, "vbus_on");
	wake_lock_init(&vbus_off_wake_lock, WAKE_LOCK_SUSPEND, "vbus_off");
	wake_lock_inited = 1;
	setup_timer(&chip->irq_timer, max8971_irq_timer, (unsigned long)chip);
	INIT_WORK(&chip->irq_work, max8971_charger_irq_work);

	INIT_DELAYED_WORK(&chip->status_work, status_update_work_func);
	schedule_delayed_work(&chip->status_work, 3*HZ);

	chip->suspended = 0;
	//end

#ifdef CONFIG_MACH_STUTTGART
  /* jeff, platform not support irq_to_gpio() API, so direct set GPIO*/

        int irq_gpio= EXYNOS4_GPX3(6);  
	pr_info("max8971, GPIO :%d\n",irq_gpio);
	ret = gpio_request(irq_gpio, " max8971_irq");
	if (ret < 0) {
		dev_err(&client->dev,
			"Failed to request gpio %d with ret: %d\n",
			irq_gpio, ret);
		ret = IRQ_NONE;
		goto out;
	}
	
	s3c_gpio_setpull(irq_gpio,  S3C_GPIO_PULL_UP);
	gpio_direction_input(irq_gpio);
	gpio_free(irq_gpio);

	
	int chg_det_gpio = EXYNOS4_GPX2(4); // WAKE_EINT2_4_CHG_DET
	int chg_det_irq = gpio_to_irq(chg_det_gpio);
	pr_info("max8971_chg_det, GPIO :%d\n",chg_det_gpio );
	ret = gpio_request(chg_det_gpio, " max8971_irq_chg_det");
	if (ret < 0) {
		dev_err(&client->dev,
				"Failed to request gpio %d with ret: %d\n",
				chg_det_gpio, ret);
		ret = IRQ_NONE;
		goto out;
		}
	
	//s3c_gpio_setpull(irq_gpio,	S3C_GPIO_PULL_UP);
	s3c_gpio_setpull(chg_det_gpio,  S3C_GPIO_PULL_DOWN);
	gpio_direction_input(chg_det_gpio);
	gpio_free(chg_det_gpio);
	
	ret = request_threaded_irq(chg_det_irq, NULL, max8971_charger_det_handler,
            		  IRQF_TRIGGER_FALLING/* | IRQF_TRIGGER_RISING*/ , client->name, chip);
#endif

    ret = request_threaded_irq(client->irq, NULL, max8971_charger_handler,
            		  IRQF_TRIGGER_FALLING /*|IRQF_TRIGGER_RISING */, client->name, chip);  //jeff,  IRQF_ONESHOT | IRQF_TRIGGER_LOW ?
	
    if (unlikely(ret < 0))
    {
        pr_debug("max8971: failed to request IRQ	%X\n", ret);
        goto out;
    }
    enable_irq_wake(client->irq); /* jeff, Enable wakeup*/
    enable_irq_wake(chg_det_irq);
	chip->chg_online = 0;
	ret = max8971_read_reg(chip->client, MAX8971_REG_CHG_STAT);
	printk("MAX8971_REG_CHG_STAT,reg: %x,\n", ret);
	if (ret >= 0) 
    {
		chip->chg_online = (ret & MAX8971_DCUVP_MASK) ? 0 : 1;
        if (chip->chg_online) 
        {
            // Set IRQ MASK register
            max8971_write_reg(chip->client, MAX8971_REG_CHGINT_MASK, chip->pdata->int_mask);
            __set_charger(chip, 1);
	    need_run_once = true; //the phone may be powered on with charger.
        }
	}

	return 0;
out:
	kfree(chip);
	return ret;
}

static __devexit int max8971_remove(struct i2c_client *client)
{
    struct max8971_chip *chip = i2c_get_clientdata(client);

	free_irq(client->irq, chip);
	power_supply_unregister(&chip->charger);
	power_supply_unregister(&chip->usb_charger);
	wake_lock_destroy(&vbus_on_wake_lock);
	wake_lock_destroy(&vbus_off_wake_lock);
	wake_lock_inited = 0;
	kfree(chip);

	return 0;
}


#ifdef CONFIG_PM
static int max8971_suspend(struct i2c_client *client,
		pm_message_t state)
{
	struct max8971_chip *chip =  i2c_get_clientdata(client);

	//dev_info(&client->dev,"%s\n", __func__);
	chip->suspended = 1;
	return 0;
}

static int max8971_resume(struct i2c_client *client)
{
	struct max8971_chip *chip = i2c_get_clientdata(client);
	
	//dev_info(&client->dev,"%s\n", __func__);
	chip->suspended = 0;
	return 0;
}
#else
#define max8971_suspend NULL
#define max8971_resume NULL
#endif /* CONFIG_PM */


static const struct i2c_device_id max8971_id[] = {
	{ "max8971", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, max8971_id);

static struct i2c_driver max8971_i2c_driver = {
	.driver = {
		.name = "max8971",
	},
	.suspend		= max8971_suspend,
	.resume		= max8971_resume,
	.probe		= max8971_probe,
	.remove		= __devexit_p(max8971_remove),
	.id_table	= max8971_id,
};

static int __init max8971_init(void)
{
	return i2c_add_driver(&max8971_i2c_driver);
}
module_init(max8971_init);

static void __exit max8971_exit(void)
{
	i2c_del_driver(&max8971_i2c_driver);
}
module_exit(max8971_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Clark Kim <clark.kim@maxim-ic.com>");
MODULE_DESCRIPTION("Power supply driver for MAX8971");
MODULE_VERSION("4.0");
MODULE_ALIAS("platform:max8971-charger");
