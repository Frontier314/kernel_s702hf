/*
 * Regulator driver for Active-semi act8847 PMIC chip for Exynos4
 *
 * Copyleft (c) 2013, net.314 Development. Adapted by Manos S. Pappas (mpappas@net314.eu)

 * Based on act8846.c that is work by zhangqing<zhangqing@rock-chips.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/bug.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/act8847.h>
#include <mach/gpio.h>
#include <linux/delay.h>
//#include <mach/iomux.h>
#include <linux/slab.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif


#if 0
#define DBG(x...)	printk(KERN_INFO x)
#else
#define DBG(x...)
#endif
#if 1
#define DBG_INFO(x...)	printk(KERN_INFO x)
#else
#define DBG_INFO(x...)
#endif
#define PM_CONTROL

struct act8847 {
	struct device *dev;
	struct mutex io_lock;
	struct i2c_client *i2c;
	int num_regulators;
	struct regulator_dev **rdev;
	struct early_suspend act8847_suspend;
};

struct act8847 *g_act8847;

static u8 act8847_reg_read(struct act8847 *act8847, u8 reg);
static int act8847_set_bits(struct act8847 *act8847, u8 reg, u16 mask, u16 val);


#define act8847_BUCK1_SET_VOL_BASE 0x10	// REG1
#define act8847_BUCK2_SET_VOL_BASE 0x20	// REG2 
#define act8847_BUCK3_SET_VOL_BASE 0x30	// REG3
#define act8847_BUCK4_SET_VOL_BASE 0x40	// REG4

#define act8847_BUCK2_SLP_VOL_BASE 0x21	// REG2
#define act8847_BUCK3_SLP_VOL_BASE 0x31	// REG3
#define act8847_BUCK4_SLP_VOL_BASE 0x41	// REG4

#define act8847_LDO1_SET_VOL_BASE 0x50	// REG5
#define act8847_LDO2_SET_VOL_BASE 0x58	// REG6
#define act8847_LDO3_SET_VOL_BASE 0x60	// REG7
#define act8847_LDO4_SET_VOL_BASE 0x68	// REG8
#define act8847_LDO5_SET_VOL_BASE 0x70	// REG9
#define act8847_LDO6_SET_VOL_BASE 0x80	// REG10
#define act8847_LDO7_SET_VOL_BASE 0x90	// REG11
#define act8847_LDO8_SET_VOL_BASE 0xa0	// REG12
//#define act8847_LDO9_SET_VOL_BASE 0xb1

#define act8847_BUCK1_CONTR_BASE 0x12	// REG1
#define act8847_BUCK2_CONTR_BASE 0x22	// REG2
#define act8847_BUCK3_CONTR_BASE 0x32	// REG3
#define act8847_BUCK4_CONTR_BASE 0x42	// REG4

#define act8847_LDO1_CONTR_BASE 0x51	// REG5
#define act8847_LDO2_CONTR_BASE 0x59	// REG6
#define act8847_LDO3_CONTR_BASE 0x61	// REG7
#define act8847_LDO4_CONTR_BASE 0x69	// REG8
#define act8847_LDO5_CONTR_BASE 0x71	// REG9
#define act8847_LDO6_CONTR_BASE 0x81	// REG10
#define act8847_LDO7_CONTR_BASE 0x91	// REG11
#define act8847_LDO8_CONTR_BASE 0xa1	// REG12
//#define act8847_LDO9_CONTR_BASE 0xb1

/* do not know what these do */
#define BUCK_VOL_MASK 0x3f
#define LDO_VOL_MASK 0x3f

#define VOL_MIN_IDX 0x00
#define VOL_MAX_IDX 0x3f
/* do not know what these do */

const static int buck_set_vol_base_addr[] = {
	act8847_BUCK1_SET_VOL_BASE,
	act8847_BUCK2_SET_VOL_BASE,
	act8847_BUCK3_SET_VOL_BASE,
	act8847_BUCK4_SET_VOL_BASE,
};
const static int buck_contr_base_addr[] = {
	act8847_BUCK1_CONTR_BASE,
 	act8847_BUCK2_CONTR_BASE,
 	act8847_BUCK3_CONTR_BASE,
 	act8847_BUCK4_CONTR_BASE,
};
#define act8847_BUCK_SET_VOL_REG(x) (buck_set_vol_base_addr[x])
#define act8847_BUCK_CONTR_REG(x) (buck_contr_base_addr[x])

const static int ldo_set_vol_base_addr[] = {
	act8847_LDO1_SET_VOL_BASE,
	act8847_LDO2_SET_VOL_BASE,
	act8847_LDO3_SET_VOL_BASE,
	act8847_LDO4_SET_VOL_BASE, 
	act8847_LDO5_SET_VOL_BASE, 
	act8847_LDO6_SET_VOL_BASE, 
	act8847_LDO7_SET_VOL_BASE, 
	act8847_LDO8_SET_VOL_BASE, 
//	act8847_LDO9_SET_VOL_BASE, 
};
const static int ldo_contr_base_addr[] = {
	act8847_LDO1_CONTR_BASE,
	act8847_LDO2_CONTR_BASE,
	act8847_LDO3_CONTR_BASE,
	act8847_LDO4_CONTR_BASE,
	act8847_LDO5_CONTR_BASE,
	act8847_LDO6_CONTR_BASE,
	act8847_LDO7_CONTR_BASE,
	act8847_LDO8_CONTR_BASE,
//	act8847_LDO9_CONTR_BASE,
};
#define act8847_LDO_SET_VOL_REG(x) (ldo_set_vol_base_addr[x])
#define act8847_LDO_CONTR_REG(x) (ldo_contr_base_addr[x])

/* REGx/VSET Output Voltage Settings */
const static int buck_voltage_map[] = {
	 600, 625, 650, 675, 700, 725, 750, 775,
	 800, 825, 850, 875, 900, 925, 950, 975,
	 1000, 1025, 1050, 1075, 1100, 1125, 1150,
	 1175, 1200, 1250, 1300, 1350, 1400, 1450,
	 1500, 1550, 1600, 1650, 1700, 1750, 1800, 
	 1850, 1900, 1950, 2000, 2050, 2100, 2150, 
	 2200, 2250, 2300, 2350, 2400, 2500, 2600, 
	 2700, 2800, 2850, 2900, 3000, 3100, 3200,
	 3300, 3400, 3500, 3600, 3700, 3800, 3900,
};

const static int ldo_voltage_map[] = {
	 600, 625, 650, 675, 700, 725, 750, 775,
	 800, 825, 850, 875, 900, 925, 950, 975,
	 1000, 1025, 1050, 1075, 1100, 1125, 1150,
	 1175, 1200, 1250, 1300, 1350, 1400, 1450,
	 1500, 1550, 1600, 1650, 1700, 1750, 1800, 
	 1850, 1900, 1950, 2000, 2050, 2100, 2150, 
	 2200, 2250, 2300, 2350, 2400, 2500, 2600, 
	 2700, 2800, 2850, 2900, 3000, 3100, 3200,
	 3300, 3400, 3500, 3600, 3700, 3800, 3900,
};

static int act8847_ldo_list_voltage(struct regulator_dev *dev, unsigned index)
{
	if (index >= ARRAY_SIZE(ldo_voltage_map))
		return -EINVAL;
	return 1000 * ldo_voltage_map[index];
}
static int act8847_ldo_is_enabled(struct regulator_dev *dev)
{
	struct act8847 *act8847 = rdev_get_drvdata(dev);
	int ldo = rdev_get_id(dev) - ACT8847_LDO1;
	u16 val;
	u16 mask=0x80;
	val = act8847_reg_read(act8847, act8847_LDO_CONTR_REG(ldo));	 
	if (val < 0)
		return val;
	val=val&~0x7f;
	if (val & mask)
		return 1;
	else
		return 0; 	
}
static int act8847_ldo_enable(struct regulator_dev *dev)
{
	struct act8847 *act8847 = rdev_get_drvdata(dev);
	int ldo= rdev_get_id(dev) - ACT8847_LDO1;
	u16 mask=0x80;	
	
	return act8847_set_bits(act8847, act8847_LDO_CONTR_REG(ldo), mask, 0x80);
	
}
static int act8847_ldo_disable(struct regulator_dev *dev)
{
	struct act8847 *act8847 = rdev_get_drvdata(dev);
	int ldo= rdev_get_id(dev) - ACT8847_LDO1;
	u16 mask=0x80;
	
	return act8847_set_bits(act8847, act8847_LDO_CONTR_REG(ldo), mask, 0);

}
static int act8847_ldo_get_voltage(struct regulator_dev *dev)
{
	struct act8847 *act8847 = rdev_get_drvdata(dev);
	int ldo= rdev_get_id(dev) - ACT8847_LDO1;
	u16 reg = 0;
	int val;
	reg = act8847_reg_read(act8847,act8847_LDO_SET_VOL_REG(ldo));
	reg &= LDO_VOL_MASK;
	val = 1000 * ldo_voltage_map[reg];	
	return val;
}
static int act8847_ldo_set_voltage(struct regulator_dev *dev,
				  int min_uV, int max_uV,unsigned *selector)
{
	struct act8847 *act8847 = rdev_get_drvdata(dev);
	int ldo= rdev_get_id(dev) - ACT8847_LDO1;
	int min_vol = min_uV / 1000, max_vol = max_uV / 1000;
	const int *vol_map =ldo_voltage_map;
	u16 val;
	int ret = 0;
	if (min_vol < vol_map[VOL_MIN_IDX] ||
	    min_vol > vol_map[VOL_MAX_IDX])
		return -EINVAL;

	for (val = VOL_MIN_IDX; val <= VOL_MAX_IDX; val++){
		if (vol_map[val] >= min_vol)
			break;	
        }
		
	if (vol_map[val] > max_vol)
		return -EINVAL;

	ret = act8847_set_bits(act8847, act8847_LDO_SET_VOL_REG(ldo),
	       	LDO_VOL_MASK, val);
	return ret;

}
static unsigned int act8847_ldo_get_mode(struct regulator_dev *dev)
{
	struct act8847 *act8847 = rdev_get_drvdata(dev);
	int ldo = rdev_get_id(dev) - ACT8847_LDO1;
	u16 mask = 0x80;
	u16 val;
	val = act8847_reg_read(act8847, act8847_LDO_CONTR_REG(ldo));
        if (val < 0) {
                return val;
        }
	val=val & mask;
	if (val== mask)
		return REGULATOR_MODE_NORMAL;
	else
		return REGULATOR_MODE_STANDBY;

}
static int act8847_ldo_set_mode(struct regulator_dev *dev, unsigned int mode)
{
	struct act8847 *act8847 = rdev_get_drvdata(dev);
	int ldo = rdev_get_id(dev) - ACT8847_LDO1;
	u16 mask = 0x80;
	switch(mode)
	{
	case REGULATOR_MODE_NORMAL:
		return act8847_set_bits(act8847, act8847_LDO_CONTR_REG(ldo), mask, mask);		
	case REGULATOR_MODE_STANDBY:
		return act8847_set_bits(act8847, act8847_LDO_CONTR_REG(ldo), mask, 0);
	default:
		printk("error: pmu_act8847 only lowpower and normal mode\n");
		return -EINVAL;
	}


}
static struct regulator_ops act8847_ldo_ops = {
	.set_voltage = act8847_ldo_set_voltage,
	.get_voltage = act8847_ldo_get_voltage,
	.list_voltage = act8847_ldo_list_voltage,
	.is_enabled = act8847_ldo_is_enabled,
	.enable = act8847_ldo_enable,
	.disable = act8847_ldo_disable,
	.get_mode = act8847_ldo_get_mode,
	.set_mode = act8847_ldo_set_mode,
	
};

static int act8847_dcdc_list_voltage(struct regulator_dev *dev, unsigned index)
{
	if (index >= ARRAY_SIZE(buck_voltage_map))
		return -EINVAL;
	return 1000 * buck_voltage_map[index];
}
static int act8847_dcdc_is_enabled(struct regulator_dev *dev)
{
	struct act8847 *act8847 = rdev_get_drvdata(dev);
	int buck = rdev_get_id(dev) - ACT8847_DCDC1;
	u16 val;
	u16 mask=0x80;	
	val = act8847_reg_read(act8847, act8847_BUCK_CONTR_REG(buck));
	if (val < 0)
		return val;
	 val=val&~0x7f;
	if (val & mask)
		return 1;
	else
		return 0; 	
}
static int act8847_dcdc_enable(struct regulator_dev *dev)
{
	struct act8847 *act8847 = rdev_get_drvdata(dev);
	int buck = rdev_get_id(dev) - ACT8847_DCDC1;
	u16 mask=0x80;	
	return act8847_set_bits(act8847, act8847_BUCK_CONTR_REG(buck), mask, 0x80);

}
static int act8847_dcdc_disable(struct regulator_dev *dev)
{
	struct act8847 *act8847 = rdev_get_drvdata(dev);
	int buck = rdev_get_id(dev) - ACT8847_DCDC1;
	u16 mask=0x80;
	 return act8847_set_bits(act8847, act8847_BUCK_CONTR_REG(buck), mask, 0);
}
static int act8847_dcdc_get_voltage(struct regulator_dev *dev)
{
	struct act8847 *act8847 = rdev_get_drvdata(dev);
	int buck = rdev_get_id(dev) - ACT8847_DCDC1;
	u16 reg = 0;
	int val;
	#ifdef CONFIG_ACT8847_SUPPORT_RESET
	reg = act8847_reg_read(act8847,(act8847_BUCK_SET_VOL_REG(buck)+0x1));
	#else
	reg = act8847_reg_read(act8847,act8847_BUCK_SET_VOL_REG(buck));
	#endif
	reg &= BUCK_VOL_MASK;
        DBG("%d\n", reg);
	val = 1000 * buck_voltage_map[reg];	
        DBG("%d\n", val);
	return val;
}
static int act8847_dcdc_set_voltage(struct regulator_dev *dev,
				  int min_uV, int max_uV,unsigned *selector)
{
	struct act8847 *act8847 = rdev_get_drvdata(dev);
	int buck = rdev_get_id(dev) - ACT8847_DCDC1;
	int min_vol = min_uV / 1000, max_vol = max_uV / 1000;
	const int *vol_map = buck_voltage_map;
	u16 val;
	int ret = 0;

        DBG("%s, min_uV = %d, max_uV = %d!\n", __func__, min_uV, max_uV);
	if (min_vol < vol_map[VOL_MIN_IDX] ||
	    min_vol > vol_map[VOL_MAX_IDX])
		return -EINVAL;

	for (val = VOL_MIN_IDX; val <= VOL_MAX_IDX; val++){
		if (vol_map[val] >= min_vol)
			break;
        }

	if (vol_map[val] > max_vol)
		printk("WARNING:this voltage is not supported!voltage set to %d mv\n",vol_map[val]);

	#ifdef CONFIG_ACT8847_SUPPORT_RESET
	ret = act8847_set_bits(act8847, (act8847_BUCK_SET_VOL_REG(buck) +0x1),BUCK_VOL_MASK, val);
	#else
	ret = act8847_set_bits(act8847, act8847_BUCK_SET_VOL_REG(buck) ,BUCK_VOL_MASK, val);
	#endif
	
	return ret;
}
static int act8847_dcdc_set_sleep_voltage(struct regulator_dev *dev,
					    int uV)
{
	struct act8847 *act8847 = rdev_get_drvdata(dev);
	int buck = rdev_get_id(dev) - ACT8847_DCDC1;
	int min_vol = uV / 1000,max_vol = uV / 1000;
	const int *vol_map = buck_voltage_map;
	u16 val;
	int ret = 0;

        DBG("%s, min_uV = %d, max_uV = %d!\n", __func__, min_uV, max_uV);
	if (min_vol < vol_map[VOL_MIN_IDX] ||
	    min_vol > vol_map[VOL_MAX_IDX])
		return -EINVAL;

	for (val = VOL_MIN_IDX; val <= VOL_MAX_IDX; val++){
		if (vol_map[val] >= min_vol)
			break;
        }

	if (vol_map[val] > max_vol)
		printk("WARNING: this voltage is not supported!voltage set to %d mv\n",vol_map[val]);
	#ifdef CONFIG_ACT8847_SUPPORT_RESET
	 ret = act8847_set_bits(act8847, (act8847_BUCK_SET_VOL_REG(buck) ),BUCK_VOL_MASK, val);
	#else
	ret = act8847_set_bits(act8847, (act8847_BUCK_SET_VOL_REG(buck) +0x01),BUCK_VOL_MASK, val);
	#endif
	
	return ret;
}
static unsigned int act8847_dcdc_get_mode(struct regulator_dev *dev)
{
	struct act8847 *act8847 = rdev_get_drvdata(dev);
	int buck = rdev_get_id(dev) - ACT8847_DCDC1;
	u16 mask = 0x08;
	u16 val;
	val = act8847_reg_read(act8847, act8847_BUCK_CONTR_REG(buck));
        if (val < 0) {
                return val;
        }
	val=val & mask;
	if (val== mask)
		return REGULATOR_MODE_NORMAL;
	else
		return REGULATOR_MODE_STANDBY;

}
static int act8847_dcdc_set_mode(struct regulator_dev *dev, unsigned int mode)
{
	struct act8847 *act8847 = rdev_get_drvdata(dev);
	int buck = rdev_get_id(dev) - ACT8847_DCDC1;
	u16 mask = 0x80;
	switch(mode)
	{
	case REGULATOR_MODE_STANDBY:
		return act8847_set_bits(act8847, act8847_BUCK_CONTR_REG(buck), mask, 0);
	case REGULATOR_MODE_NORMAL:
		return act8847_set_bits(act8847, act8847_BUCK_CONTR_REG(buck), mask, mask);
	default:
		printk("error: pmu_act8847 supports only powersave and pwm mode\n");
		return -EINVAL;
	}


}
static int act8847_dcdc_set_voltage_time_sel(struct regulator_dev *dev,   unsigned int old_selector,
				     unsigned int new_selector)
{
	struct act8847 *act8847 = rdev_get_drvdata(dev);
	int ret =0,old_volt, new_volt;
	
	old_volt = act8847_dcdc_list_voltage(dev, old_selector);
	if (old_volt < 0)
		return old_volt;
	
	new_volt = act8847_dcdc_list_voltage(dev, new_selector);
	if (new_volt < 0)
		return new_volt;

	return DIV_ROUND_UP(abs(old_volt - new_volt)*2, 25000);
}

static struct regulator_ops act8847_dcdc_ops = { 
	.set_voltage = act8847_dcdc_set_voltage,
	.get_voltage = act8847_dcdc_get_voltage,
	.list_voltage= act8847_dcdc_list_voltage,
	.is_enabled = act8847_dcdc_is_enabled,
	.enable = act8847_dcdc_enable,
	.disable = act8847_dcdc_disable,
	.get_mode = act8847_dcdc_get_mode,
	.set_mode = act8847_dcdc_set_mode,
	.set_suspend_voltage = act8847_dcdc_set_sleep_voltage,
	.set_voltage_time_sel = act8847_dcdc_set_voltage_time_sel,
};
static struct regulator_desc regulators[] = {

        {
		.name = "DCDC1",
		.id = 0,
		.ops = &act8847_dcdc_ops,
		.n_voltages = ARRAY_SIZE(buck_voltage_map),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "DCDC2",
		.id = 1,
		.ops = &act8847_dcdc_ops,
		.n_voltages = ARRAY_SIZE(buck_voltage_map),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "DCDC3",
		.id = 2,
		.ops = &act8847_dcdc_ops,
		.n_voltages = ARRAY_SIZE(buck_voltage_map),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "DCDC4",
		.id = 3,
		.ops = &act8847_dcdc_ops,
		.n_voltages = ARRAY_SIZE(buck_voltage_map),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},

	{
		.name = "LDO1",
		.id =4,
		.ops = &act8847_ldo_ops,
		.n_voltages = ARRAY_SIZE(ldo_voltage_map),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "LDO2",
		.id = 5,
		.ops = &act8847_ldo_ops,
		.n_voltages = ARRAY_SIZE(ldo_voltage_map),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "LDO3",
		.id = 6,
		.ops = &act8847_ldo_ops,
		.n_voltages = ARRAY_SIZE(ldo_voltage_map),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "LDO4",
		.id = 7,
		.ops = &act8847_ldo_ops,
		.n_voltages = ARRAY_SIZE(ldo_voltage_map),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},

	{
		.name = "LDO5",
		.id =8,
		.ops = &act8847_ldo_ops,
		.n_voltages = ARRAY_SIZE(ldo_voltage_map),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "LDO6",
		.id = 9,
		.ops = &act8847_ldo_ops,
		.n_voltages = ARRAY_SIZE(ldo_voltage_map),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "LDO7",
		.id = 10,
		.ops = &act8847_ldo_ops,
		.n_voltages = ARRAY_SIZE(ldo_voltage_map),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "LDO8",
		.id = 11,
		.ops = &act8847_ldo_ops,
		.n_voltages = ARRAY_SIZE(ldo_voltage_map),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
	{
		.name = "LDO9",
		.id = 12,
		.ops = &act8847_ldo_ops,
		.n_voltages = ARRAY_SIZE(ldo_voltage_map),
		.type = REGULATOR_VOLTAGE,
		.owner = THIS_MODULE,
	},
};

/*
 *
 */
static int act8847_i2c_read(struct i2c_client *i2c, char reg, int count, u16 *dest)
{
    int ret;
    struct i2c_adapter *adap;
    struct i2c_msg msgs[2];

    if(!i2c)
	return ret;

    if (count != 1)
	return -EIO;  
  
    adap = i2c->adapter;		
    
    msgs[0].addr = i2c->addr;
    msgs[0].buf = &reg;
    msgs[0].flags = i2c->flags;
    msgs[0].len = 1;
/* Enable this on kernel >= .36 */
    //msgs[0].scl_rate = 200*1000;
    
    msgs[1].buf = (u8 *)dest;
    msgs[1].addr = i2c->addr;
    msgs[1].flags = i2c->flags | I2C_M_RD;
    msgs[1].len = 1;
/* Enable this on kernel >= .36 */
    //msgs[1].scl_rate = 200*1000;
    ret = i2c_transfer(adap, msgs, 2);

	DBG("act8847.c: ***run in %s %d msgs[1].buf = %d\n",__FUNCTION__,__LINE__,*(msgs[1].buf));

	return 0;   
}

static int act8847_i2c_write(struct i2c_client *i2c, char reg, int count, const u16 src)
{
	int ret=-1;
	
	struct i2c_adapter *adap;
	struct i2c_msg msg;
	char tx_buf[2];

	if(!i2c)
	  return ret;
	if (count != 1)
	  return -EIO;
    
	adap = i2c->adapter;		
	tx_buf[0] = reg;
	tx_buf[1] = src;
	
	msg.addr = i2c->addr;
	msg.buf = &tx_buf[0];
	msg.len = 1 +1;
	msg.flags = i2c->flags;   
/* Enable this on kernel >= .36 */
	//msg.scl_rate = 200*1000;	

	ret = i2c_transfer(adap, &msg, 1);
	return ret;	
}

static u8 act8847_reg_read(struct act8847 *act8847, u8 reg)
{
	u16 val = 0;

	mutex_lock(&act8847->io_lock);

	act8847_i2c_read(act8847->i2c, reg, 1, &val);

	DBG("act8847.c: reg read 0x%02x -> 0x%02x\n", (int)reg, (unsigned)val&0xff);

	mutex_unlock(&act8847->io_lock);

	return val & 0xff;	
}

static int act8847_set_bits(struct act8847 *act8847, u8 reg, u16 mask, u16 val)
{
	u16 tmp;
	int ret;

	mutex_lock(&act8847->io_lock);

	ret = act8847_i2c_read(act8847->i2c, reg, 1, &tmp);
	DBG("act8847.c: 1 reg read 0x%02x -> 0x%02x\n", (int)reg, (unsigned)tmp&0xff);
	tmp = (tmp & ~mask) | val;
	if (ret == 0) {
	  ret = act8847_i2c_write(act8847->i2c, reg, 1, tmp);
	  DBG("act8847.c: reg write 0x%02x -> 0x%02x\n", (int)reg, (unsigned)val&0xff);
	}
	act8847_i2c_read(act8847->i2c, reg, 1, &tmp);
	DBG("act8847.c: 2 reg read 0x%02x -> 0x%02x\n", (int)reg, (unsigned)tmp&0xff);
	mutex_unlock(&act8847->io_lock);

	return 0;//ret;	
}
static int __devinit setup_regulators(struct act8847 *act8847, struct act8847_platform_data *pdata)
{	
	int i, err;

	act8847->num_regulators = pdata->num_regulators;
	act8847->rdev = kcalloc(pdata->num_regulators,
			       sizeof(struct regulator_dev *), GFP_KERNEL);
	if (!act8847->rdev) {
		return -ENOMEM;
	}
	/* Instantiate the regulators */
	for (i = 0; i < pdata->num_regulators; i++) {
		int id = pdata->regulators[i].id;
		act8847->rdev[i] = regulator_register(&regulators[id],
			act8847->dev, pdata->regulators[i].initdata, act8847);
/*
		if (IS_ERR(act8847->rdev[i])) {
			err = PTR_ERR(act8847->rdev[i]);
			dev_err(act8847->dev, "regulator init failed: %d\n",
				err);
			goto error;
		}*/
	}

	return 0;
error:
	while (--i >= 0)
		regulator_unregister(act8847->rdev[i]);
	kfree(act8847->rdev);
	act8847->rdev = NULL;
	return err;
}


int act8847_device_shutdown(void)
{
	int ret;
	int err = -1;
	struct act8847 *act8847 = g_act8847;
	
	printk("%s\n",__func__);

	ret = act8847_reg_read(act8847,0xc3);
	ret = act8847_set_bits(act8847, 0xc3,(0x1<<3),(0x1<<3));
	ret = act8847_set_bits(act8847, 0xc3,(0x1<<4),(0x1<<4));
	if (ret < 0) {
		printk("act8847 initialization: set 0xc3 error!\n");
		return err;
	}
	return 0;	
}
EXPORT_SYMBOL_GPL(act8847_device_shutdown);

__weak void  act8847_device_suspend(void) {}
__weak void  act8847_device_resume(void) {}
#ifdef CONFIG_PM
static int act8847_suspend(struct i2c_client *i2c, pm_message_t mesg)
{		
	act8847_device_suspend();
	return 0;
}

static int act8847_resume(struct i2c_client *i2c)
{
	act8847_device_resume();
	return 0;
}
#else
static int act8847_suspend(struct i2c_client *i2c, pm_message_t mesg)
{		
	return 0;
}

static int act8847_resume(struct i2c_client *i2c)
{
	return 0;
}
#endif


#ifdef CONFIG_HAS_EARLYSUSPEND
__weak void act8847_early_suspend(struct early_suspend *h) {}
__weak void act8847_late_resume(struct early_suspend *h) {}
#endif

static int __devinit act8847_i2c_probe(struct i2c_client *i2c, const struct i2c_device_id *id)
{
	struct act8847 *act8847;	
	struct act8847_platform_data *pdata = i2c->dev.platform_data;
	int ret;
	act8847 = kzalloc(sizeof(struct act8847), GFP_KERNEL);
	if (act8847 == NULL) {
		ret = -ENOMEM;		
		goto err;
	}
	act8847->i2c = i2c;
	act8847->dev = &i2c->dev;
	i2c_set_clientdata(i2c, act8847);
	mutex_init(&act8847->io_lock);	

	ret = act8847_reg_read(act8847,0x22);
	if ((ret < 0) || (ret == 0xff)){
		printk("act8847.c: The device is not act8847 \n");
		return 0;
	}

	ret = act8847_set_bits(act8847, 0xf4,(0x1<<7),(0x0<<7));
	if (ret < 0) {
		printk("act8847.c: set 0xf4 error!\n");
		goto err;
	}
	
	if (pdata) {
		ret = setup_regulators(act8847, pdata);
		if (ret < 0)		
			goto err;
	} else
		dev_warn(act8847->dev, "act8847.c: No platform init data supplied\n");

	g_act8847 = act8847;
	pdata->set_init(act8847);

	#ifdef CONFIG_HAS_EARLYSUSPEND
	act8847->act8847_suspend.suspend = act8847_early_suspend,
	act8847->act8847_suspend.resume = act8847_late_resume,
	act8847->act8847_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 1,
	register_early_suspend(&act8847->act8847_suspend);
	#endif
	
	return 0;

err:
	return ret;	

}

static int __devexit act8847_i2c_remove(struct i2c_client *i2c)
{
	struct act8847 *act8847 = i2c_get_clientdata(i2c);
	int i;

	for (i = 0; i < act8847->num_regulators; i++)
		if (act8847->rdev[i])
			regulator_unregister(act8847->rdev[i]);
	kfree(act8847->rdev);
	i2c_set_clientdata(i2c, NULL);
	kfree(act8847);

	return 0;
}

static const struct i2c_device_id act8847_i2c_id[] = {
       { "act8847", 0 },
       { }
};

MODULE_DEVICE_TABLE(i2c, act8847_i2c_id);

static struct i2c_driver act8847_i2c_driver = {
	.driver = {
		.name = "act8847",
		.owner = THIS_MODULE,
	},
	.probe    = act8847_i2c_probe,
	.remove   = __devexit_p(act8847_i2c_remove),
	.id_table = act8847_i2c_id,
	#ifdef CONFIG_PM
	.suspend	= act8847_suspend,
	.resume		= act8847_resume,
	#endif
};

static int __init act8847_module_init(void)
{
	int ret;
	ret = i2c_add_driver(&act8847_i2c_driver);
	if (ret != 0)
		pr_err("Failed to register I2C driver: %d\n", ret);
	return ret;
}
//module_init(act8847_module_init);
//subsys_initcall(act8847_module_init);
//rootfs_initcall(act8847_module_init);
subsys_initcall_sync(act8847_module_init);

static void __exit act8847_module_exit(void)
{
	i2c_del_driver(&act8847_i2c_driver);
}
module_exit(act8847_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("mpappas <mpappas@net314.eu>");
MODULE_DESCRIPTION("act8847 PMIC driver");

