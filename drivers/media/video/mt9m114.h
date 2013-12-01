/*
 * Support for mt9m114 Camera Sensor.
 *
 * Copyright (c) 2010 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef __A1040_H__
#define __A1040_H__

#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <media/v4l2-chip-ident.h>
#include <media/soc_camera.h>
#include <linux/gpio.h>
#include <plat/gpio-cfg.h>
#include <mach/gpio-exynos4.h>
#include <linux/regulator/consumer.h>

#define V4L2_IDENT_MT9M114 8245

#define MT9P111_REV3
#define FULLINISUPPORT

/* #defines for register writes and register array processing */
#define MISENSOR_8BIT		1
#define MISENSOR_16BIT		2
#define MISENSOR_32BIT		4

#define MISENSOR_FWBURST0	0x80
#define MISENSOR_FWBURST1	0x81
#define MISENSOR_FWBURST4	0x84
#define MISENSOR_FWBURST	0x88

#define MISENSOR_TOK_TERM	0xf000	/* terminating token for reg list */
#define MISENSOR_TOK_DELAY	0xfe00	/* delay token for reg list */
#define MISENSOR_TOK_FWLOAD	0xfd00	/* token indicating load FW */
#define MISENSOR_TOK_POLL	0xfc00	/* token indicating poll instruction */
#define MISENSOR_TOK_RMW	0x0010  /* RMW operation */
#define MISENSOR_TOK_MASK	0xfff0
#define MISENSOR_FLIP_EN	(1<<1)	/* enable vert_flip */
#define MISENSOR_MIRROR_EN	(1<<0)	/* enable horz_mirror */

/* mask to set sensor read_mode via misensor_rmw_reg */
#define MISENSOR_R_MODE_MASK	0x0330
/* mask to set sensor vert_flip and horz_mirror */
#define MISENSOR_F_M_MASK	0x0003

/* bits set to set sensor read_mode via misensor_rmw_reg */
#define MISENSOR_SKIPPING_SET	0x0011
#define MISENSOR_SUMMING_SET	0x0033
#define MISENSOR_NORMAL_SET	0x0000

/* bits set to set sensor vert_flip and horz_mirror */
#define MISENSOR_F_M_EN	(MISENSOR_FLIP_EN | MISENSOR_MIRROR_EN)
#define MISENSOR_F_EN		MISENSOR_FLIP_EN
#define MISENSOR_F_M_DIS	(MISENSOR_FLIP_EN & MISENSOR_MIRROR_EN)

/* sensor register that control sensor read-mode and mirror */
#define MISENSOR_READ_MODE	0xC834

#define SENSOR_DETECTED		1
#define SENSOR_NOT_DETECTED	0

#define I2C_RETRY_COUNT		5
#define MSG_LEN_OFFSET		2
#define MAX_FMTS		1

#ifndef MIPI_CONTROL
#define MIPI_CONTROL		0x3400	/* MIPI_Control */
#endif

/* GPIO pin on Moorestown */
#define GPIO_SCLK_25		44
#define GPIO_STB_PIN		47

#define GPIO_STDBY_PIN		49   /* ab:new */
#define GPIO_RESET_PIN		50

/* System control register for Aptina A-1040SOC*/
#define MT9M114_PID		0x0

/* MT9P111_DEVICE_ID */
#define MT9M114_MOD_ID		0x2481

/* ulBPat; */

#define MT9M114_BPAT_RGRGGBGB	(1 << 0)
#define MT9M114_BPAT_GRGRBGBG	(1 << 1)
#define MT9M114_BPAT_GBGBRGRG	(1 << 2)
#define MT9M114_BPAT_BGBGGRGR	(1 << 3)

#define MT9M114_FOCAL_LENGTH_NUM	208	/*2.08mm*/
#define MT9M114_FOCAL_LENGTH_DEM	100
#define MT9M114_F_NUMBER_DEFAULT_NUM	24
#define MT9M114_F_NUMBER_DEM	10
#define MT9M114_WAIT_STAT_TIMEOUT	100
#define MT9M114_FLICKER_MODE_50HZ	1
#define MT9M114_FLICKER_MODE_60HZ	2
/*
 * focal length bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define MT9M114_FOCAL_LENGTH_DEFAULT 0xD00064

/*
 * current f-number bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */
#define MT9M114_F_NUMBER_DEFAULT 0x18000a

/*
 * f-number range bits definition:
 * bits 31-24: max f-number numerator
 * bits 23-16: max f-number denominator
 * bits 15-8: min f-number numerator
 * bits 7-0: min f-number denominator
 */
#define MT9M114_F_NUMBER_RANGE 0x180a180a

/* Supported resolutions */
enum {
	MT9M114_RES_QVGA,
	MT9M114_RES_VGA,
	MT9M114_RES_720P,
	MT9M114_RES_960P,
};
#define MT9M114_RES_960P_SIZE_H		1280
#define MT9M114_RES_960P_SIZE_V		960
#define MT9M114_RES_720P_SIZE_H		1280
#define MT9M114_RES_720P_SIZE_V		720
#define MT9M114_RES_VGA_SIZE_H		640
#define MT9M114_RES_VGA_SIZE_V		480
#define MT9M114_RES_QVGA_SIZE_H		320
#define MT9M114_RES_QVGA_SIZE_V		240

/*
 * struct misensor_reg - MI sensor  register format
 * @length: length of the register
 * @reg: 16-bit offset to register
 * @val: 8/16/32-bit register value
 * Define a structure for sensor register initialization values
 */
struct misensor_reg {
	u32 length;
	u32 reg;
	u32 val;	/* value or for read/mod/write, AND mask */
	u32 val2;	/* optional; for rmw, OR mask */
};

/*
 * struct misensor_fwreg - Firmware burst command
 * @type: FW burst or 8/16 bit register
 * @addr: 16-bit offset to register or other values depending on type
 * @valx: data value for burst (or other commands)
 *
 * Define a structure for sensor register initialization values
 */
struct misensor_fwreg {
	u32	type;	/* type of value, register or FW burst string */
	u32	addr;	/* target address */
	u32	val0;
	u32	val1;
	u32	val2;
	u32	val3;
	u32	val4;
	u32	val5;
	u32	val6;
	u32	val7;
};

struct regval_list {
	u16 reg_num;
	u8 value;
};

struct mt9m114_device {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_mbus_framefmt format;

	struct camera_sensor_platform_data *platform_data;
	int fmt_idx;
	int real_model_id;
	int nctx;
	int power;

	unsigned int bus_width;
	unsigned int mode;
	unsigned int field_inv;
	unsigned int field_sel;
	unsigned int ycseq;
	unsigned int conv422;
	unsigned int bpat;
	unsigned int hpol;
	unsigned int vpol;
	unsigned int edge;
	unsigned int bls;
	unsigned int gamma;
	unsigned int cconv;
	unsigned int res;
	unsigned int dwn_sz;
	unsigned int blc;
	unsigned int agc;
	unsigned int awb;
	unsigned int aec;
	/* extention SENSOR version 2 */
	unsigned int cie_profile;

	/* extention SENSOR version 3 */
	unsigned int flicker_freq;

	/* extension SENSOR version 4 */
	unsigned int smia_mode;
	unsigned int mipi_mode;

	/* Add name here to load shared library */
	unsigned int type;

	/*Number of MIPI lanes*/
	unsigned int mipi_lanes;
	char name[32];

	u8 lightfreq;
};

struct mt9m114_format_struct {
	u8 *desc;
	u32 pixelformat;
	struct regval_list *regs;
};

struct mt9m114_res_struct {
	u8 *desc;
	int res;
	int width;
	int height;
	int fps;
	bool used;
	struct regval_list *regs;
};

struct mt9m114_control {
	struct v4l2_queryctrl qc;
	int (*query)(struct v4l2_subdev *sd, s32 *value);
	int (*tweak)(struct v4l2_subdev *sd, int value);
};

#define MT9M114_MAX_WRITE_BUF_SIZE 	128	
struct mt9m114_write_buffer {
	u16 addr;
	u8 data[MT9M114_MAX_WRITE_BUF_SIZE];
};

struct mt9m114_write_ctrl {
	int index;
	struct mt9m114_write_buffer buffer;
};

/*
static struct mt9m114_format_struct  mt9m114_formats[] = {
	{
		.desc = "YUYV 4:2:2",
	},
};
*/

#define N_MT9M114_FMTS ARRAY_SIZE(mt9m114_formats)

/*
 * Modes supported by the mt9m114 driver.
 * Please, keep them in ascending order.
 */
static struct mt9m114_res_struct mt9m114_res[] = {
	{
	.desc	= "QVGA",
	.res	= MT9M114_RES_QVGA,
	.width	= 320,
	.height	= 240,
	.fps	= 30,
	.used	= 0,
	.regs	= NULL,
	},
	{
	.desc	= "VGA",
	.res	= MT9M114_RES_VGA,
	.width	= 640,
	.height	= 480,
	.fps	= 30,
	.used	= 0,
	.regs	= NULL,
	},
	{
	.desc	= "720p",
	.res	= MT9M114_RES_720P,
	.width	= 1280,
	.height	= 720,
	.fps	= 30,
	.used	= 0,
	.regs	= NULL,
	},
	{
	.desc	= "960P",
	.res	= MT9M114_RES_960P,
	.width	= 1280,
	.height	= 960,
	.fps	= 15,
	.used	= 0,
	.regs	= NULL,
	},
};
#define N_RES (ARRAY_SIZE(mt9m114_res))

static const struct i2c_device_id mt9m114_id[] = {
	{"MT9M114", 0},//mt9m114
	{}
};

static struct misensor_reg const mt9m114_suspend[] = {
	 {MISENSOR_16BIT,  0x098E, 0xDC00},
	 {MISENSOR_8BIT,  0xDC00, 0x40},
	 {MISENSOR_16BIT,  0x0080, 0x8002},
	 {MISENSOR_TOK_TERM, 0, 0}
};

static struct misensor_reg const mt9m114_streaming[] = {
	 {MISENSOR_16BIT,  0x098E, 0xDC00},
	 {MISENSOR_8BIT,  0xDC00, 0x34},
	 {MISENSOR_16BIT,  0x0080, 0x8002},
	 {MISENSOR_TOK_TERM, 0, 0}
};

static struct misensor_reg const mt9m114_standby_reg[] = {
	 {MISENSOR_16BIT,  0x098E, 0xDC00},
	 {MISENSOR_8BIT,  0xDC00, 0x50},
	 {MISENSOR_16BIT,  0x0080, 0x8002},
	 {MISENSOR_TOK_TERM, 0, 0}
};

static struct misensor_reg const mt9m114_wakeup_reg[] = {
	 {MISENSOR_16BIT,  0x098E, 0xDC00},
	 {MISENSOR_8BIT,  0xDC00, 0x54},
	 {MISENSOR_16BIT,  0x0080, 0x8002},
	 {MISENSOR_TOK_TERM, 0, 0}
};

static struct misensor_reg const mt9m114_qvga_init[] = {

	{MISENSOR_16BIT,  0x301A, 0x0234},
	{MISENSOR_16BIT,  0x98E, 0x1000},
	{MISENSOR_8BIT,  0xC97E, 0x01},
	{MISENSOR_16BIT,  0xC980, 0x0128},
	{MISENSOR_16BIT,  0xC982, 0x0700},
	{MISENSOR_16BIT,  0xC984, 0x8041},
	{MISENSOR_16BIT,  0xC800, 0x0000},
	{MISENSOR_16BIT,  0xC802, 0x0000},
	{MISENSOR_16BIT,  0xC804, 0x03CD},
	{MISENSOR_16BIT,  0xC806, 0x050D},
	{MISENSOR_32BIT,  0xC808, 0x2349340},
	{MISENSOR_16BIT,  0xC80C, 0x0001},
	{MISENSOR_16BIT,  0xC80E, 0x00DB},
	{MISENSOR_16BIT,  0xC810, 0x05B3},
	{MISENSOR_16BIT,  0xC812, 0x03EE},
	{MISENSOR_16BIT,  0xC814, 0x0656},
	{MISENSOR_16BIT,  0xC816, 0x0060},
	{MISENSOR_16BIT,  0xC818, 0x01E3},
	{MISENSOR_16BIT,  0xC826, 0x0020},
	{MISENSOR_16BIT,  0xC854, 0x0000},
	{MISENSOR_16BIT,  0xC856, 0x0000},
	{MISENSOR_16BIT,  0xC858, 0x0280},
	{MISENSOR_16BIT,  0xC85A, 0x01E0},
	{MISENSOR_8BIT,  0xC85C, 0x03},
	{MISENSOR_16BIT,  0xC868, 0x0140},
	{MISENSOR_16BIT,  0xC86A, 0x00F0},
	{MISENSOR_8BIT,  0xC878, 0x0},
	{MISENSOR_16BIT,  0xC88C, 0x1E03},
	{MISENSOR_16BIT,  0xC88E, 0x1E03},
	{MISENSOR_16BIT,  0xC914, 0x0000},
	{MISENSOR_16BIT,  0xC916, 0x0000},
	{MISENSOR_16BIT,  0xC918, 0x013F},
	{MISENSOR_16BIT,  0xC91A, 0x00EF},
	{MISENSOR_16BIT,  0xC91C, 0x0000},
	{MISENSOR_16BIT,  0xC91E, 0x0000},
	{MISENSOR_16BIT,  0xC920, 0x003F},
	{MISENSOR_16BIT,  0xC922, 0x002F},
	{MISENSOR_8BIT,  0xE801, 0x00},
	{MISENSOR_TOK_TERM, 0, 0}
};

static struct misensor_reg const mt9m114_720p_init[] = {

	{MISENSOR_16BIT,  0x301A, 0x0234},
	{MISENSOR_16BIT,  0x98E0, 0x0000},
	{MISENSOR_8BIT,   0xC97E, 0x01},
	{MISENSOR_16BIT,  0xC980, 0x0128},
	{MISENSOR_16BIT,  0xC982, 0x0700},
	{MISENSOR_16BIT,  0x98E0, 0x0000},
	{MISENSOR_16BIT,  0xC800, 0x007C},
	{MISENSOR_16BIT,  0xC802, 0x0004},
	{MISENSOR_16BIT,  0xC804, 0x0353},
	{MISENSOR_16BIT,  0xC806, 0x050B},
	{MISENSOR_32BIT,  0xC808, 0x2349340},
	{MISENSOR_16BIT,  0xC80C, 0x0001},
	{MISENSOR_16BIT,  0xC80E, 0x00DB},
	{MISENSOR_16BIT,  0xC810, 0x05BD},
	{MISENSOR_16BIT,  0xC812, 0x03E8},
	{MISENSOR_16BIT,  0xC814, 0x0640},
	{MISENSOR_16BIT,  0xC816, 0x0060},
	{MISENSOR_16BIT,  0xC818, 0x02D3},
	{MISENSOR_16BIT,  0xC826, 0x0020},


	{MISENSOR_16BIT,  0xC854, 0x0000},
	{MISENSOR_16BIT,  0xC856, 0x0000},
	{MISENSOR_16BIT,  0xC858, 0x0500},
	{MISENSOR_16BIT,  0xC85A, 0x02D0},
	{MISENSOR_8BIT,   0xC85C, 0x03},
	{MISENSOR_16BIT,  0xC868, 0x0500},
	{MISENSOR_16BIT,  0xC86A, 0x02D0},
	{MISENSOR_16BIT,  0xC88C, 0x1E00},
	{MISENSOR_16BIT,  0xC88E, 0x1E00},
	{MISENSOR_16BIT,  0xC914, 0x0000},
	{MISENSOR_16BIT,  0xC916, 0x0000},
	{MISENSOR_16BIT,  0xC918, 0x04FF},
	{MISENSOR_16BIT,  0xC91A, 0x02CF},
	{MISENSOR_16BIT,  0xC91C, 0x0000},
	{MISENSOR_16BIT,  0xC91E, 0x0000},
	{MISENSOR_16BIT,  0xC920, 0x00FF},
	{MISENSOR_16BIT,  0xC922, 0x008F},
	{MISENSOR_8BIT,   0xE801, 0x00},
	{MISENSOR_TOK_TERM, 0, 0}
};

static struct misensor_reg const mt9m114_vga_init[] = {

	{MISENSOR_16BIT,  0x301A, 0x0234},
	{MISENSOR_16BIT,   0x98E, 0x1000},
	{MISENSOR_8BIT,   0xC97E, 0x01},
	{MISENSOR_16BIT,   0xC980, 0x0128},
	{MISENSOR_16BIT,   0xC982, 0x0700},
	{MISENSOR_16BIT,   0xC984, 0x8001},
	{MISENSOR_16BIT,   0xC988, 0x0F00},
	{MISENSOR_16BIT,   0xC98A, 0x0B07},
	{MISENSOR_16BIT,   0xC98C, 0x0D01},
	{MISENSOR_16BIT,   0xC98E, 0x071D},
	{MISENSOR_16BIT,   0xC990, 0x0006},
	{MISENSOR_16BIT,   0xC992, 0x0A0C},
	{MISENSOR_16BIT,   0xC800, 0x0000},
	{MISENSOR_16BIT,   0xC802, 0x0000},
	{MISENSOR_16BIT,   0xC804, 0x03CD},
	{MISENSOR_16BIT,   0xC806, 0x050D},
	{MISENSOR_32BIT,   0xC808, 0x2349340},
	{MISENSOR_16BIT,   0xC80C, 0x0001},
	{MISENSOR_16BIT,   0xC80E, 0x01C3},
	{MISENSOR_16BIT,   0xC810, 0x03B3},
	{MISENSOR_16BIT,   0xC812, 0x0549},
	{MISENSOR_16BIT,   0xC814, 0x049E},
	{MISENSOR_16BIT,   0xC816, 0x00E0},
	{MISENSOR_16BIT,   0xC818, 0x01E3},
	{MISENSOR_16BIT,   0xC826, 0x0020},
	{MISENSOR_16BIT,   0xC854, 0x0000},
	{MISENSOR_16BIT,   0xC856, 0x0000},
	{MISENSOR_16BIT,   0xC858, 0x0280},
	{MISENSOR_16BIT,   0xC85A, 0x01E0},
	{MISENSOR_8BIT,   0xC85C, 0x03},
	{MISENSOR_16BIT,   0xC868, 0x0280},
	{MISENSOR_16BIT,   0xC86A, 0x01E0},
	{MISENSOR_8BIT,   0xC878, 0x00},
	{MISENSOR_16BIT,   0xC88C, 0x1E04},
	{MISENSOR_16BIT,   0xC88E, 0x1E04},
	{MISENSOR_16BIT,   0xC914, 0x0000},
	{MISENSOR_16BIT,   0xC916, 0x0000},
	{MISENSOR_16BIT,   0xC918, 0x027F},
	{MISENSOR_16BIT,   0xC91A, 0x01DF},
	{MISENSOR_16BIT,   0xC91C, 0x0000},
	{MISENSOR_16BIT,   0xC91E, 0x0000},
	{MISENSOR_16BIT,   0xC920, 0x007F},
	{MISENSOR_16BIT,   0xC922, 0x005F},
	{MISENSOR_8BIT,  0xE801, 0x00},
	{MISENSOR_TOK_TERM, 0, 0}
};

static struct misensor_reg const mt9m114_960P_init[] = {
	{MISENSOR_16BIT, 0x98E, 0x1000},
	{MISENSOR_8BIT, 0xC97E, 0x01},
	{MISENSOR_16BIT, 0xC980, 0x0128},
	{MISENSOR_16BIT, 0xC982, 0x0700},
	{MISENSOR_16BIT, 0xC800, 0x0004},
	{MISENSOR_16BIT, 0xC802, 0x0004},
	{MISENSOR_16BIT, 0xC804, 0x03CB},
	{MISENSOR_16BIT, 0xC806, 0x050B},
	{MISENSOR_32BIT, 0xC808, 0x2349340},
	{MISENSOR_16BIT, 0xC80C, 0x0001},
	{MISENSOR_16BIT, 0xC80E, 0x00DB},
	{MISENSOR_16BIT, 0xC810, 0x062E},
	{MISENSOR_16BIT, 0xC812, 0x074C},
	{MISENSOR_16BIT, 0xC814, 0x06B1},
	{MISENSOR_16BIT, 0xC816, 0x0060},
	{MISENSOR_16BIT, 0xC818, 0x03C3},
	{MISENSOR_16BIT, 0xC826, 0x0020},
	{MISENSOR_16BIT, 0xC854, 0x0000},
	{MISENSOR_16BIT, 0xC856, 0x0000},
	{MISENSOR_16BIT, 0xC858, 0x0500},
	{MISENSOR_16BIT, 0xC85A, 0x03C0},
	{MISENSOR_8BIT, 0xC85C, 0x03},
	{MISENSOR_16BIT, 0xC868, 0x0500},
	{MISENSOR_16BIT, 0xC86A, 0x03C0},
	{MISENSOR_8BIT, 0xC878, 0x00},
	{MISENSOR_16BIT, 0xC88C, 0x0F00},
	{MISENSOR_16BIT, 0xC88E, 0x0F00},
	{MISENSOR_16BIT, 0xC914, 0x0000},
	{MISENSOR_16BIT, 0xC916, 0x0000},
	{MISENSOR_16BIT, 0xC918, 0x04FF},
	{MISENSOR_16BIT, 0xC91A, 0x03BF},
	{MISENSOR_16BIT, 0xC91C, 0x0000},
	{MISENSOR_16BIT, 0xC91E, 0x0000},
	{MISENSOR_16BIT, 0xC920, 0x00FF},
	{MISENSOR_16BIT, 0xC922, 0x00BF},
	{MISENSOR_TOK_TERM, 0, 0}
};


static struct misensor_reg const mt9m114_common[] = {
	 /*Step8-Features*/
	 {MISENSOR_16BIT,  0x98E0, 0x0000},
	 {MISENSOR_16BIT,  0xC984, 0x8040},
	 {MISENSOR_16BIT,  0x001E, 0x0777},
	 /*MIPI settings for MT9M114*/
	 {MISENSOR_16BIT,  0xC984, 0x8041},
	 {MISENSOR_16BIT,  0xC988, 0x0F00},
	 {MISENSOR_16BIT,  0xC98A, 0x0B07},
	 {MISENSOR_16BIT,  0xC98C, 0x0D01},
	 {MISENSOR_16BIT,  0xC98E, 0x071D},
	 {MISENSOR_16BIT,  0xC990, 0x0006},
	 {MISENSOR_16BIT,  0xC992, 0x0A0C},
	 {MISENSOR_16BIT,  0x098E, 0xDC00},
	 {MISENSOR_8BIT,  0xDC00, 0x28},
	 {MISENSOR_16BIT,  0x0080, 0x8002},
	 {MISENSOR_TOK_TERM, 0, 0}
};

static struct misensor_reg const mt9m114_antiflicker_50hz[] = {
	 {MISENSOR_16BIT,  0x098E, 0xC88B},
	 {MISENSOR_8BIT,  0xC88B, 0x32},
	 {MISENSOR_8BIT,  0xDC00, 0x28},
	 {MISENSOR_16BIT,  0x0080, 0x8002},
	 {MISENSOR_TOK_TERM, 0, 0}
};

static struct misensor_reg const mt9m114_antiflicker_60hz[] = {
	 {MISENSOR_16BIT,  0x098E, 0xC88B},
	 {MISENSOR_8BIT,  0xC88B, 0x3C},
	 {MISENSOR_8BIT,  0xDC00, 0x28},
	 {MISENSOR_16BIT,  0x0080, 0x8002},
	 {MISENSOR_TOK_TERM, 0, 0}
};

static struct misensor_reg const mt9m114_iq[] = {
	{MISENSOR_TOK_DELAY, 0,  100},
	{MISENSOR_16BIT, 0x301A, 0x0234 },
	{MISENSOR_16BIT, 0x098E, 0x0000 },
	{MISENSOR_16BIT, 0xC97E, 0x0001 },
	{MISENSOR_16BIT, 0xC980, 0x0225 },
	{MISENSOR_16BIT, 0xC982, 0x0700 },
	{MISENSOR_16BIT, 0x098E, 0x0000 },
	{MISENSOR_16BIT, 0xC800, 0x007C },
	{MISENSOR_16BIT, 0xC802, 0x0004 },
	{MISENSOR_16BIT, 0xC804, 0x0353 },
	{MISENSOR_16BIT, 0xC806, 0x050B },
	{MISENSOR_32BIT, 0xC808, 0x02349340 },
	{MISENSOR_16BIT, 0xC80C, 0x0001 },
	{MISENSOR_16BIT, 0xC80E, 0x00DB },
	{MISENSOR_16BIT, 0xC810, 0x05AD },
	{MISENSOR_16BIT, 0xC812, 0x02FE },
	{MISENSOR_16BIT, 0xC814, 0x0650 },
	{MISENSOR_16BIT, 0xC816, 0x0060 },
	{MISENSOR_16BIT, 0xC818, 0x02D3 },
	{MISENSOR_16BIT, 0xC834, 0x0000 },
	{MISENSOR_16BIT, 0xC854, 0x0000 },
	{MISENSOR_16BIT, 0xC856, 0x0000 },
	{MISENSOR_16BIT, 0xC858, 0x0500 },
	{MISENSOR_16BIT, 0xC85A, 0x02D0 },
	{MISENSOR_8BIT,  0xC85C, 0x03 },
	{MISENSOR_16BIT, 0xC868, 0x0500 },
	{MISENSOR_16BIT, 0xC86A, 0x02D0 },
	{MISENSOR_16BIT, 0xC88C, 0x1E00 },
	{MISENSOR_16BIT, 0xC88E, 0x0F00 },
	{MISENSOR_16BIT, 0xC914, 0x0000 },
	{MISENSOR_16BIT, 0xC916, 0x0000 },
	{MISENSOR_16BIT, 0xC918, 0x04FF },
	{MISENSOR_16BIT, 0xC91A, 0x02CF },
	{MISENSOR_16BIT, 0xC91C, 0x0000 },
	{MISENSOR_16BIT, 0xC91E, 0x0000 },
	{MISENSOR_16BIT, 0xC920, 0x00FF },
	{MISENSOR_16BIT, 0xC922, 0x008F },
	{MISENSOR_8BIT,  0xE801, 0x00 },
	{MISENSOR_16BIT, 0x098E, 0x0000 },
	{MISENSOR_16BIT, 0xC984, 0x8040 },
	{MISENSOR_16BIT, 0x001E, 0x0777 },
	{MISENSOR_8BIT,  0xDC00, 0x28 },
	{MISENSOR_16BIT, 0x0080, 0x8002 },
	{MISENSOR_TOK_DELAY, 0,  100},
	{MISENSOR_16BIT, 0x098E, 0x4868 },
	{MISENSOR_16BIT, 0xC868, 0x0280 },
	{MISENSOR_16BIT, 0xC86A, 0x01E0 },
	{MISENSOR_8BIT,  0xC85C, 0x03 },
	{MISENSOR_16BIT, 0xC854, 0x00A0 },
	{MISENSOR_16BIT, 0xC856, 0x0000 },
	{MISENSOR_16BIT, 0xC858, 0x03C0 },
	{MISENSOR_16BIT, 0xC85A, 0x02D0 },
	{MISENSOR_16BIT, 0xC914, 0x0000 },
	{MISENSOR_16BIT, 0xC916, 0x0000 },
	{MISENSOR_16BIT, 0xC918, 0x027F },
	{MISENSOR_16BIT, 0xC91A, 0x01DF },
	{MISENSOR_16BIT, 0xC91C, 0x0000 },
	{MISENSOR_16BIT, 0xC91E, 0x0000 },
	{MISENSOR_16BIT, 0xC920, 0x007F },
	{MISENSOR_16BIT, 0xC922, 0x005F },
	{MISENSOR_8BIT,  0xDC00, 0x28 },
	{MISENSOR_16BIT, 0x0080, 0x8002 },
	{MISENSOR_TOK_DELAY, 0,  100},	 
	{MISENSOR_16BIT, 0x098E, 0x4868 },
	{MISENSOR_16BIT, 0xC868, 0x0280 },
	{MISENSOR_16BIT, 0xC86A, 0x01E0 },
	{MISENSOR_8BIT,  0xC85C, 0x03 },
	{MISENSOR_16BIT, 0xC854, 0x00A0 },
	{MISENSOR_16BIT, 0xC856, 0x0000 },
	{MISENSOR_16BIT, 0xC858, 0x03C0 },
	{MISENSOR_16BIT, 0xC85A, 0x02D0 },
	{MISENSOR_16BIT, 0xC914, 0x0000 },
	{MISENSOR_16BIT, 0xC916, 0x0000 },
	{MISENSOR_16BIT, 0xC918, 0x027F },
	{MISENSOR_16BIT, 0xC91A, 0x01DF },
	{MISENSOR_16BIT, 0xC91C, 0x0000 },
	{MISENSOR_16BIT, 0xC91E, 0x0000 },
	{MISENSOR_16BIT, 0xC920, 0x007F },
	{MISENSOR_16BIT, 0xC922, 0x005F },
	{MISENSOR_8BIT,  0xDC00, 0x28 },
	{MISENSOR_16BIT, 0x0080, 0x8002 },
	{MISENSOR_TOK_TERM, 0, 0}
};
#endif
