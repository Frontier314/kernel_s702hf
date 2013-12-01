/* include/linux/regulator/act8847.h
 *
 * Copyright (c) 2013, net.314 Development
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __LINUX_REGULATOR_act8847_H
#define __LINUX_REGULATOR_act8847_H

#include <linux/regulator/machine.h>

//#define ACT8847_START 30

#define ACT8847_DCDC1  0                     //(0+ACT8847_START) 

#define ACT8847_LDO1 4                //(4+ACT8847_START)


#define act8847_NUM_REGULATORS 13
struct act8847;

int act8847_device_shutdown(void);

struct act8847_regulator_subdev {
	int id;
	struct regulator_init_data *initdata;
};

struct act8847_platform_data {
	int num_regulators;
	int (*set_init)(struct act8847 *act8847);
	struct act8847_regulator_subdev *regulators;
};

#endif

