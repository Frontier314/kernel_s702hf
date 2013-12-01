#ifndef	__LEDS_LM3532_H__
#define __LEDS_LM3532_H__

#ifdef	CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

/* LM3532 Registers */
#define HWEN	EXYNOS4_GPD0(1)
#define LM3532_SLAVE_ADDR	(0x38)	//7-bit address
#define OUTPUT_CONTL_BASE	(0x10)

#define OUTPUT_CONFIGURATION	(0x10)
#define STAR_SHUT_RAMP_RATE		(OUTPUT_CONTL_BASE + 1)
#define RUN_TIME_RAMP_RATE		(OUTPUT_CONTL_BASE + 2)
#define CONTL_A_PWM				(OUTPUT_CONTL_BASE + 3)
#define CONTL_B_PWM				(OUTPUT_CONTL_BASE + 4)
#define CONTL_C_PWM				(OUTPUT_CONTL_BASE + 5)
#define CONTL_A_BRIGHT			(OUTPUT_CONTL_BASE + 6)
#define CONTL_A_FULL_SCALE_CURRENT	(OUTPUT_CONTL_BASE + 7)
#define CONTL_B_BRIGHT			(OUTPUT_CONTL_BASE + 8) 
#define CONTL_B_FULL_SCALE_CURRENT	(OUTPUT_CONTL_BASE + 9)
#define CONTL_C_BRIGHT			(OUTPUT_CONTL_BASE + 0x0A)
#define CONTL_C_FULL_SCALE_CURRENT	(OUTPUT_CONTL_BASE + 0x0B)
#define FEEDBACK_EN				(OUTPUT_CONTL_BASE + 0x0C)
#define CONTL_EN				(OUTPUT_CONTL_BASE + 0x0D)

#define CONTL_ZONE_BASE			(0x70)
#define CONTL_A_ZONE_TARGET0	(CONTL_ZONE_BASE)
#define CONTL_A_ZONE_TARGET1	(CONTL_ZONE_BASE+1)
#define CONTL_A_ZONE_TARGET2	(CONTL_ZONE_BASE+2)
#define CONTL_A_ZONE_TARGET3	(CONTL_ZONE_BASE+3)
#define CONTL_A_ZONE_TARGET4	(CONTL_ZONE_BASE+4)

#define CONTL_B_ZONE_TARGET0	(CONTL_ZONE_BASE+5)
#define CONTL_B_ZONE_TARGET1	(CONTL_ZONE_BASE+6)
#define CONTL_B_ZONE_TARGET2	(CONTL_ZONE_BASE+7)
#define CONTL_B_ZONE_TARGET3	(CONTL_ZONE_BASE+8)
#define CONTL_B_ZONE_TARGET4	(CONTL_ZONE_BASE+9)

#define CONTL_C_ZONE_TARGET0	(CONTL_ZONE_BASE+0x0A)
#define CONTL_C_ZONE_TARGET1	(CONTL_ZONE_BASE+0x0B)
#define CONTL_C_ZONE_TARGET2	(CONTL_ZONE_BASE+0x0C)
#define CONTL_C_ZONE_TARGET3	(CONTL_ZONE_BASE+0x0D)
#define CONTL_C_ZONE_TARGET4	(CONTL_ZONE_BASE+0x0E)

/* Output Configuration register settings */
#define OUTPUT_CONFIG_ILED1_CONTL_A_PWM	(0x00)
#define OUTPUT_CONFIG_ILED1_CONTL_B_PWM	(0x01)
#define OUTPUT_CONFIG_ILED1_CONTL_C_PWM	(0x10)

#define OUTPUT_CONFIG_ILED2_CONTL_A_PWM	(0x00)
#define OUTPUT_CONFIG_ILED2_CONTL_B_PWM	(1 << 2)
#define OUTPUT_CONFIG_ILED2_CONTL_C_PWM	(1 << 3)

#define OUTPUT_CONFIG_ILED3_CONTL_A_PWM	(0x00)
#define OUTPUT_CONFIG_ILED3_CONTL_B_PWM	(1 << 4)
#define OUTPUT_CONFIG_ILED3_CONTL_C_PWM	(1 << 5)

/*Startup/Shutdown ramp rate */
#define SHUTDN_RAMP_8us_STEP		(0x00)
#define SHUTDN_RAMP_1p024ms_STEP		(1 << 3)
#define SHUTDN_RAMP_2p048ms_STEP		(1 << 4)
#define SHUTDN_RAMP_4p096ms_STEP		((1 << 3) | (1 << 4))
#define SHUTDN_RAMP_8p192ms_STEP		(1 << 5)
#define SHUTDN_RAMP_16p384ms_STEP	((1 << 5) | (1 << 3))
#define SHUTDN_RAMP_32p768ms_STEP	((1 << 5) | (1 << 4))
#define SHUTDN_RAMP_65p536ms_STEP	((1 << 5) | (1 << 4) | (1 << 3))

#define START_RAMP_8us_STEP		(0x00)
#define START_RAMP_1p024ms_STEP		(1 << 0)
#define START_RAMP_2p048ms_STEP		(1 << 1)
#define START_RAMP_4p096ms_STEP		((1 << 1) | (1 << 0))
#define START_RAMP_8p192ms_STEP		(1 << 2)
#define START_RAMP_16p384ms_STEP	((1 << 2) | (1 << 0))
#define START_RAMP_32p768ms_STEP	((1 << 2) | (1 << 1))
#define START_RAMP_65p536ms_STEP	((1 << 2) | (1 << 1) | (1 << 0))

/*Runtime Ramp Rate*/
#define RT_RAMPDN_8us_STEP		(0x00)
#define RT_RAMPDN_1p024ms_STEP		(1 << 3)
#define RT_RAMPDN_2p048ms_STEP		(1 << 4)
#define RT_RAMPDN_4p096ms_STEP		((1 << 3) | (1 << 4))
#define RT_RAMPDN_8p192ms_STEP		(1 << 5)
#define RT_RAMPDN_16p384ms_STEP	((1 << 5) | (1 << 3))
#define RT_RAMPDN_32p768ms_STEP	((1 << 5) | (1 << 4))
#define RT_RAMPDN_65p536ms_STEP	((1 << 5) | (1 << 4) | (1 << 3))

#define RT_RAMPUP_8us_STEP		(0x00)
#define RT_RAMPUP_1p024ms_STEP		(1 << 0)
#define RT_RAMPUP_2p048ms_STEP		(1 << 1)
#define RT_RAMPUP_4p096ms_STEP		((1 << 1) | (1 << 0))
#define RT_RAMPUP_8p192ms_STEP		(1 << 2)
#define RT_RAMPUP_16p384ms_STEP	((1 << 2) | (1 << 0))
#define RT_RAMPUP_32p768ms_STEP	((1 << 2) | (1 << 1))
#define RT_RAMPUP_65p536ms_STEP	((1 << 2) | (1 << 1) | (1 << 0))

/*CONTROL A&B&C PWM*/
#define PWM_CHANNEL_SEL_PWM1	(0x00)
#define PWM_CHANNEL_SEL_PWM2	(0x01)
#define PWM_INPUT_POLARITY_HIGH	(1 << 1)
#define PWM_ZONE_0_EN	(1 << 2)
#define PWM_ZONE_1_EN	(1 << 3)
#define PWM_ZONE_2_EN	(1 << 4)
#define PWM_ZONE_3_EN	(1 << 5)
#define PWM_ZONE_4_EN	(1 << 6)

/* Control A & B & C Brightness Configuration */
#define I2C_CURRENT_CONTROL	(1 << 0)
#define LINEAR_MAPPING		(1 << 1)
#define CONTROL_ZONE_TARGET_0	(0x00)
#define CONTROL_ZONE_TARGET_1	(1 << 2)
#define CONTROL_ZONE_TARGET_2	(1 << 3)
#define CONTROL_ZONE_TARGET_3	((1 << 3) | (1 << 2))
#define CONTROL_ZONE_TARGET_4	(1 << 4)

/* Control A/B/C Full-scale current */
#define CNTL_FS_CURRENT_5mA		(0x00)
#define CNTL_FS_CURRENT_5p8mA	(1 << 0)
#define CNTL_FS_CURRENT_6p6mA	(1 << 1)
#define CNTL_FS_CURRENT_7p4mA	((1 << 1) | (1 << 0))
#define CNTL_FS_CURRENT_8p2mA	(1 << 2)
#define CNTL_FS_CURRENT_9mA		((1 << 2) | (1 << 0))
#define CNTL_FS_CURRENT_9p8mA	((1 << 2) | (1 << 1))
#define CNTL_FS_CURRENT_10p6mA	((1 << 2) | (1 << 1) | (1 << 0))
#define CNTL_FS_CURRENT_11p4mA	((1 << 2) | (1 << 1) | (1 << 0))
#define CNTL_FS_CURRENT_12p2mA	((1 << 3) | (1 << 0))
#define CNTL_FS_CURRENT_13mA	((1 << 3) | (1 << 1))
#define CNTL_FS_CURRENT_13p8mA	((1 << 3) | (1 << 1) |  (1 << 0))
#define CNTL_FS_CURRENT_14p6mA	((1 << 3) | (1 << 2))
#define CNTL_FS_CURRENT_15p4mA	((1 << 3) | (1 << 2) | (1 << 0))
#define CNTL_FS_CURRENT_16p2mA	((1 << 3) | (1 << 2) | (1 << 1))
#define CNTL_FS_CURRENT_17mA	((1 << 3) | (1 << 2) | (1 << 1) | (1 << 0))
#define CNTL_FS_CURRENT_17p8mA	(1 << 4)
#define CNTL_FS_CURRENT_18p6hmA	((1 << 4) | (1 << 0))
#define CNTL_FS_CURRENT_19p4mA	((1 << 4) | (1 << 1))
#define CNTL_FS_CURRENT_20P2mA	((1 << 4) | (1 << 1) | (1 << 0))
#define CNTL_FS_CURRENT_21mA	((1 << 4) | (1 << 2))
#define CNTL_FS_CURRENT_21p8mA	((1 << 4) | (1 << 2) | (1 << 0))
#define CNTL_FS_CURRENT_22p6mA	((1 << 4) | (1 << 2) | (1 << 1))
#define CNTL_FS_CURRENT_23p4mA	((1 << 4) | (1 << 2) | (1 << 1) | (1 << 0))
#define CNTL_FS_CURRENT_24p2mA	((1 << 4) | (1 << 3))
#define CNTL_FS_CURRENT_25mA	((1 << 4) | (1 << 3) | (1 << 0))
#define CNTL_FS_CURRENT_25p8mA	((1 << 4) | (1 << 3) | (1 << 1))
#define CNTL_FS_CURRENT_26p6mA	((1 << 4) | (1 << 3) | (1 << 1) | (1 << 0))
#define CNTL_FS_CURRENT_27p4mA	((1 << 4) | (1 << 3) | (1 << 2))
#define CNTL_FS_CURRENT_28p2mA	((1 << 4) | (1 << 3) | (1 << 2) | (1 << 0))
#define CNTL_FS_CURRENT_29mA	((1 << 4) | (1 << 3) | (1 << 2) | (1 << 1))
#define CNTL_FS_CURRENT_29p8mA	((1 << 4) | (1 << 3) | (1 << 2) | (1 << 1) | (1 << 0))

/* Feedback Enable Register */
#define FEEDBACK_ENABLE_ILED1	(1 << 0)
#define FEEDBACK_ENABLE_ILED2	(1 << 1)

/* Control Enable */
#define ENABLE_CNTL_A	(1 << 0)
#define ENABLE_CNTL_B	(1 << 1)
#define ENABLE_CNTL_C	(1 << 2)

#define LM3532_LED_NAME	"lcd-backlight"

struct lm3532_data {
	struct i2c_client *client;	//i2c client device 
	struct led_classdev led_dev;
	unsigned initialized;		//Mark it whether the device is initialized
	unsigned do_ramp;			//Default the Ramp up/down is disable
	unsigned current_value;		//save current brightness value
	struct early_suspend early_suspend;
};

#endif /*__LEDS_LM3532_H__*/

