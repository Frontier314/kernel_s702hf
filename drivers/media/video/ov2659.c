/*
 * Support for ov2659 Camera Sensor.
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

#include <linux/videodev2.h>
#include <linux/videodev2_samsung.h>
#include "ov2659.h"

static struct regulator *cam1_regulator, *cam2_regulator/*, *cam3_regulator*/;
#define to_ov2659(sd)          container_of(sd, struct ov2659_device, sd)


static int mt9v114_i2c_write(struct v4l2_subdev *sd, unsigned char i2c_data[], unsigned char length);
static int mt9v114_i2c_read(struct v4l2_subdev *sd, u8* cmddata, int cdlen, u8 *res, int reslen);
/*
 * TODO: use debug parameter to actually define when debug messages should
 * be printed.
 */
static int debug;
module_param(debug, int, 0644);
MODULE_PARM_DESC(debug, "Debug level (0-1)");

static int ov2659_t_vflip(struct v4l2_subdev *sd, int value);
static int ov2659_t_hflip(struct v4l2_subdev *sd, int value);
static int
ov2659_read_reg(struct i2c_client *client, u16 data_length, u32 reg, u32 *val)
{
	int err;
	struct i2c_msg msg[2];
	unsigned char data[4];
	//u16 *wreg;

	if (!client->adapter) {
		v4l2_err(client, "%s error, no client->adapter\n", __func__);
		return -ENODEV;
	}

	if (data_length != MISENSOR_8BIT && data_length != MISENSOR_16BIT
					 && data_length != MISENSOR_32BIT) {
		v4l2_err(client, "%s error, invalid data length\n", __func__);
		return -EINVAL;
	}

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = MSG_LEN_OFFSET;
	msg[0].buf = data;

	/* high byte goes out first */
	data[0] = (u16) (reg >> 8);
	data[1] = (u16) (reg & 0xff);

	msg[1].addr = client->addr;
	msg[1].len = data_length;
	msg[1].flags = I2C_M_RD;
	msg[1].buf = data;

	err = i2c_transfer(client->adapter, msg, 2);

	if (err >= 0) {
		*val = 0;
		/* high byte comes first */
		if (data_length == MISENSOR_8BIT)
			*val = data[0];
		else if (data_length == MISENSOR_16BIT)
			*val = data[1] + (data[0] << 8);
		else
			*val = data[3] + (data[2] << 8) +
			    (data[1] << 16) + (data[0] << 24);

		return 0;
	}

	dev_err(&client->dev, "read from offset 0x%x error %d", reg, err);
	return err;
}


static int
ov2659_write_reg(struct i2c_client *client, u16 data_length, u16 reg, u32 val)
{
	int num_msg;
	struct i2c_msg msg;
	unsigned char data[6] = {0};
	u16 *wreg;
	int retry = 0;

	if (!client->adapter) {
		v4l2_err(client, "%s error, no client->adapter\n", __func__);
		return -ENODEV;
	}

	if (data_length != MISENSOR_8BIT && data_length != MISENSOR_16BIT
					 && data_length != MISENSOR_32BIT) {
		v4l2_err(client, "%s error, invalid data_length\n", __func__);
		return -EINVAL;
	}

	memset(&msg, 0, sizeof(msg));

again:
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 2 + data_length;
	msg.buf = data;

	/* high byte goes out first */
	wreg = (u16 *)data;
	*wreg = cpu_to_be16(reg);

	if (data_length == MISENSOR_8BIT) {
		data[2] = (u8)(val);
	} else if (data_length == MISENSOR_16BIT) {
		u16 *wdata = (u16 *)&data[2];
		*wdata = be16_to_cpu((u16)val);
	} else {
		/* MISENSOR_32BIT */
		u32 *wdata = (u32 *)&data[2];
		*wdata = be32_to_cpu(val);
	}

	num_msg = i2c_transfer(client->adapter, &msg, 1);

	/*
	 * HACK: Need some delay here for Rev 2 sensors otherwise some
	 * registers do not seem to load correctly.
	 */
	mdelay(1);

	if (num_msg >= 0)
		return 0;

	dev_err(&client->dev, "write error: wrote 0x%x to offset 0x%x error %d",
		val, reg, num_msg);
	if (retry <= I2C_RETRY_COUNT) {
		dev_err(&client->dev, "retrying... %d", retry);
		retry++;
		msleep(20);
		goto again;
	}

	return num_msg;
}

/**
 * misensor_rmw_reg - Read/Modify/Write a value to a register in the sensor
 * device
 * @client: i2c driver client structure
 * @data_length: 8/16/32-bits length
 * @reg: register address
 * @mask: masked out bits
 * @set: bits set
 *
 * Read/modify/write a value to a register in the  sensor device.
 * Returns zero if successful, or non-zero otherwise.
 */
int misensor_rmw_reg(struct i2c_client *client, u16 data_length, u16 reg,
		     u32 mask, u32 set)
{
	int err;
	u32 val;

	/* Exit when no mask */
	if (mask == 0)
		return 0;

	/* @mask must not exceed data length */
	switch (data_length) {
	case MISENSOR_8BIT:
		if (mask & ~0xff)
			return -EINVAL;
		break;
	case MISENSOR_16BIT:
		if (mask & ~0xffff)
			return -EINVAL;
		break;
	case MISENSOR_32BIT:
		break;
	default:
		/* Wrong @data_length */
		return -EINVAL;
	}

	err = ov2659_read_reg(client, data_length, reg, &val);
	if (err) {
		v4l2_err(client, "misensor_rmw_reg error exit, read failed\n");
		return -EINVAL;
	}

	val &= ~mask;

	/*
	 * Perform the OR function if the @set exists.
	 * Shift @set value to target bit location. @set should set only
	 * bits included in @mask.
	 *
	 * REVISIT: This function expects @set to be non-shifted. Its shift
	 * value is then defined to be equal to mask's LSB position.
	 * How about to inform values in their right offset position and avoid
	 * this unneeded shift operation?
	 */
	set <<= ffs(mask) - 1;
	val |= set & mask;

	err = ov2659_write_reg(client, data_length, reg, val);
	if (err) {
		v4l2_err(client, "misensor_rmw_reg error exit, write failed\n");
		return -EINVAL;
	}

	return 0;
}


/*
 * ov2659_write_reg_array - Initializes a list of OV2659 registers
 * @client: i2c driver client structure
 * @reglist: list of registers to be written
 *
 * This function initializes a list of registers. When consecutive addresses
 * are found in a row on the list, this function creates a buffer and sends
 * consecutive data in a single i2c_transfer().
 *
 * __ov2659_flush_reg_array, __ov2659_buf_reg_array() and
 * __ov2659_write_reg_is_consecutive() are internal functions to
 * ov2659_write_reg_array() and should be not used anywhere else.
 *
 */

static int __ov2659_flush_reg_array(struct i2c_client *client,
				     struct ov2659_write_ctrl *ctrl)
{
	struct i2c_msg msg;
	const int num_msg = 1;
	int ret;
	int retry = 0;

	if (ctrl->index == 0)
		return 0;

again:
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 2 + ctrl->index;
	ctrl->buffer.addr = cpu_to_be16(ctrl->buffer.addr);
	msg.buf = (u8 *)&ctrl->buffer;

	ret = i2c_transfer(client->adapter, &msg, num_msg);
	if (ret != num_msg) {
		dev_err(&client->dev, "%s: i2c transfer error\n", __func__);
		if (++retry <= I2C_RETRY_COUNT) {
			dev_err(&client->dev, "retrying... %d\n", retry);
			msleep(20);
			goto again;
		}
		return -EIO;
	}

	ctrl->index = 0;

	/*
	 * REVISIT: Previously we had a delay after writing data to sensor.
	 * But it was removed as our tests have shown it is not necessary
	 * anymore.
	 */

	return 0;
}

static int __ov2659_buf_reg_array(struct i2c_client *client,
				   struct ov2659_write_ctrl *ctrl,
				   const struct misensor_reg *next)
{
	u16 *data16;
	u32 *data32;

	/* Insufficient buffer? Let's flush and get more free space. */
	if (ctrl->index + next->length >= OV2659_MAX_WRITE_BUF_SIZE)
		__ov2659_flush_reg_array(client, ctrl);

	switch (next->length) {
	case MISENSOR_8BIT:
		ctrl->buffer.data[ctrl->index] = (u8)next->val;
		break;
	case MISENSOR_16BIT:
		data16 = (u16 *)&ctrl->buffer.data[ctrl->index];
		*data16 = cpu_to_be16((u16)next->val);
		break;
	case MISENSOR_32BIT:
		data32 = (u32 *)&ctrl->buffer.data[ctrl->index];
		*data32 = cpu_to_be32(next->val);
		break;
	default:
		return -EINVAL;
	}

	/* When first item is added, we need to store its starting address */
	if (ctrl->index == 0)
		ctrl->buffer.addr = next->reg;

	ctrl->index += next->length;

	return 0;
}

static int
__ov2659_write_reg_is_consecutive(struct i2c_client *client,
				   struct ov2659_write_ctrl *ctrl,
				   const struct misensor_reg *next)
{
	if (ctrl->index == 0)
		return 1;

	return ctrl->buffer.addr + ctrl->index == next->reg;
}

static int ov2659_write_reg_array(struct i2c_client *client,
				   const struct misensor_reg *reglist)
{
	const struct misensor_reg *next = reglist;
	struct ov2659_write_ctrl ctrl;
	int err;

	ctrl.index = 0;
	for (; next->length != MISENSOR_TOK_TERM; next++) {
		switch (next->length & MISENSOR_TOK_MASK) {
		case MISENSOR_TOK_DELAY:
			err = __ov2659_flush_reg_array(client, &ctrl);
			if (err)
				return err;
			msleep(next->val);
			break;
		case MISENSOR_TOK_RMW:
			err = __ov2659_flush_reg_array(client, &ctrl);
			err |= misensor_rmw_reg(client,
						next->length &
							~MISENSOR_TOK_RMW,
						next->reg, next->val,
						next->val2);
			if (err) {
				dev_err(&client->dev, "%s read err. aborted\n",
					__func__);
				return -EINVAL;
			}
			break;
		default:
			/*
			 * If next address is not consecutive, data needs to be
			 * flushed before proceed.
			 */
			if (!__ov2659_write_reg_is_consecutive(client, &ctrl,
								next)) {
				err = __ov2659_flush_reg_array(client, &ctrl);
				if (err)
					return err;
			}
			err = __ov2659_buf_reg_array(client, &ctrl, next);
			if (err) {
				v4l2_err(client, "%s: write error, aborted\n",
					 __func__);
				return err;
			}
			break;
		}
	}

	return __ov2659_flush_reg_array(client, &ctrl);
}

static int ov2659_write_i2c(struct v4l2_subdev *sd, unsigned short address,
                                unsigned char value)
{
	char buff[3];
	buff[0]=(address>>8)&0xff;
	buff[1]=address&0xff;
	buff[2]=value;

	mt9v114_i2c_write(sd, buff, 3);
}

static int ov2659_read_i2c(struct v4l2_subdev *sd, unsigned short address)
{
	char buff[2];
	char result;
	buff[0]=(address>>8)&0xff;
	buff[1]=address&0xff;

	mt9v114_i2c_read(sd, buff, 2,&result,1);

	return result;
}

static int ov2659_wait_3a(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	//struct ov2659_device *dev = to_ov2659(sd);
	int timeout = 100;
	int status;

	while (timeout--) {
		ov2659_read_reg(client, MISENSOR_16BIT, 0xA800, &status);
		if (status & 0x8) {
			v4l2_info(client, "3a stablize time:%dms.\n",
				  (100-timeout)*20);
			return 0;
		}
		msleep(20);
	}

	return -EINVAL;
}

static int ov2659_wait_state(struct v4l2_subdev *sd, int timeout)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	//struct ov2659_device *dev = to_ov2659(sd);
	int val, ret;

	while (timeout-- > 0) {
		ret = ov2659_read_reg(client, MISENSOR_16BIT, 0x0080, &val);
		if (ret)
			return ret;
		if ((val & 0x2) == 0)
			return 0;
		msleep(20);
	}

	return -EINVAL;

}

static int ov2659_set_suspend(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;

	return 0;
}

static int ov2659_set_streaming(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return 0;
}

static int ov2659_init_common(struct v4l2_subdev *sd)
{
	return 0;
}

static int power_up( struct v4l2_subdev *sd)
{

	printk("%s\n", __func__);
	//return regulator_enable(cam3_regulator);
	return 0;

}

static int power_down(struct v4l2_subdev *sd)
{
	printk("%s\n", __func__);
	return 0;
}
int ov2659_power(int power)
{
	s3c_gpio_cfgpin(EXYNOS4_GPC0(1), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(EXYNOS4_GPC0(1), S3C_GPIO_PULL_NONE);

	printk("%s :%d\n", __func__, power);
	if(power)
	{
		printk("power on-----\n");
		gpio_set_value(EXYNOS4_GPC0(1), 0);
	}
	else
	{
		printk("power down-----\n");
		gpio_set_value(EXYNOS4_GPC0(1), 1);
	}
	return 0;
}

static int ov2659_s_power(struct v4l2_subdev *sd, int power)
{
	//struct ov2659_device *dev = to_ov2659(sd);
	//struct i2c_client *client = v4l2_get_subdevdata(sd);
	return 0;

	printk("%s \n", __func__);
	if (power == 0)
		return power_down(sd);
	else {
		if (power_up(sd))
			return -EINVAL;

		//return ov2659_init_common(sd);
	}
	return 0;
}

static int ov2659_try_res(u32 *w, u32 *h)
{
	int i;

	/*
	 * The mode list is in ascending order. We're done as soon as
	 * we have found the first equal or bigger size.
	 */
	for (i = 0; i < N_RES; i++) {
		if ((ov2659_res[i].width >= *w) &&
		    (ov2659_res[i].height >= *h))
			break;
	}

	/*
	 * If no mode was found, it means we can provide only a smaller size.
	 * Returning the biggest one available in this case.
	 */
	if (i == N_RES)
		i--;

	*w = ov2659_res[i].width;
	*h = ov2659_res[i].height;

	return 0;
}

static struct ov2659_res_struct *ov2659_to_res(u32 w, u32 h)
{
	int  index;

	for (index = 0; index < N_RES; index++) {
		if ((ov2659_res[index].width == w) &&
		    (ov2659_res[index].height == h))
			break;
	}

	/* No mode found */
	if (index >= N_RES)
		return NULL;

	return &ov2659_res[index];
}

static int ov2659_try_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	//return ov2659_try_res(&fmt->width, &fmt->height);
	return 0;
}

static int ov2659_res2size(unsigned int res, int *h_size, int *v_size)
{
	unsigned short hsize;
	unsigned short vsize;

	switch (res) {
	case OV2659_RES_QVGA:
		hsize = OV2659_RES_QVGA_SIZE_H;
		vsize = OV2659_RES_QVGA_SIZE_V;
		break;
	case OV2659_RES_VGA:
		hsize = OV2659_RES_VGA_SIZE_H;
		vsize = OV2659_RES_VGA_SIZE_V;
		break;
	case OV2659_RES_720P:
		hsize = OV2659_RES_720P_SIZE_H;
		vsize = OV2659_RES_720P_SIZE_V;
		break;
	case OV2659_RES_960P:
		hsize = OV2659_RES_960P_SIZE_H;
		vsize = OV2659_RES_960P_SIZE_V;
		break;
	default:
		WARN(1, "%s: Resolution 0x%08x unknown\n", __func__, res);
		return -EINVAL;
	}

	if (h_size != NULL)
		*h_size = hsize;
	if (v_size != NULL)
		*v_size = vsize;

	return 0;
}

static int ov2659_get_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt)
{
	struct ov2659_device *dev = to_ov2659(sd);
	int width, height;
	int ret;

	ret = ov2659_res2size(dev->res, &width, &height);
	if (ret)
		return ret;
	fmt->width = width;
	fmt->height = height;

	return 0;
}

int m_iCombo_Night_mode = 0; // 0: off, 1: on
int XVCLK = 2400; // real clock/10000
int preview_sysclk, preview_HTS;

int OV2659_get_shutter(struct v4l2_subdev *sd)
{
    // read shutter, in number of line period
    int shutter;
    shutter = (ov2659_read_i2c(sd,0x03500) & 0x0f);
    shutter = (shutter<<8) + ov2659_read_i2c(sd,0x3501);
    shutter = (shutter<<4) + (ov2659_read_i2c(sd,0x3502)>>4);

    return shutter;
}

int OV2659_set_shutter(struct v4l2_subdev *sd,int shutter)
{
    // write shutter, in number of line period
    int temp;
    
    shutter = shutter & 0xffff;
    temp = shutter & 0x0f;
    temp = temp<<4;
	printk("weichenli test %s 0x3502 = 0x%x\n",__func__,temp);

    ov2659_write_i2c(sd,0x3502, temp);
    temp = shutter & 0xfff;
    temp = temp>>4;
		printk("weichenli test %s 0x3501 = 0x%x\n",__func__,temp);

    ov2659_write_i2c(sd,0x3501, temp);
    temp = shutter>>12;
    ov2659_write_i2c(sd,0x3500, temp);
    
    return 0;
}
int OV2659_get_gain16(struct v4l2_subdev *sd)
{
    // read gain, 16 = 1x
    int gain16;
    gain16 = ov2659_read_i2c(sd,0x350a) & 0x03;
    gain16 = (gain16<<8) + ov2659_read_i2c(sd,0x350b);
    
    return gain16;
}
int OV2659_set_gain16(struct v4l2_subdev *sd,int gain16)
{
    // write gain, 16 = 1x
    int temp;

    gain16 = gain16 & 0x3ff;
    temp = gain16 & 0xff;
printk("weichenli test %s 0x350b = 0x%x\n",__func__,temp);
    ov2659_write_i2c(sd,0x350b, temp);
    temp = gain16>>8;
    ov2659_write_i2c(sd,0x350a, temp);

    return 0;
}

int OV2659_get_sysclk(struct v4l2_subdev *sd)
{
    // calculate sysclk
    int sysclk, temp1, temp2;
    int Pre_div2x, PLLDiv, FreqDiv2x, Bit8Div, SysDiv, ScaleDiv, VCO;
    int Pre_div2x_map[] = {
    2, 3, 4, 6, 4, 6, 8, 12};
    int FreqDiv2x_map[] = {
    2, 3, 4, 6};
    int Bit8Div_map[] = {
    1, 1, 4, 5};
    int SysDiv_map[] = {
    1, 2, 8, 16};
    int ScaleDiv_map[] = {
    1, 2, 4, 6, 8, 10, 12, 14};
    
    temp1 = ov2659_read_i2c(sd,0x3003);
    temp2 = temp1>>6;
    FreqDiv2x = FreqDiv2x_map[temp2];
    temp2 = temp1 & 0x03;
    Bit8Div = Bit8Div_map[temp2];
    temp1 = ov2659_read_i2c(sd,0x3004);
    temp2 = temp1 >>4;
    ScaleDiv = ScaleDiv_map[temp2];
    temp1 = ov2659_read_i2c(sd,0x3005);
    PLLDiv = temp1 & 0x3f;
    temp1 = ov2659_read_i2c(sd,0x3006);
    temp2 = temp1 & 0x07;
    Pre_div2x = Pre_div2x_map[temp2];
    temp2 = temp1 & 0x18;
    temp2 = temp2>>3;
    SysDiv = SysDiv_map[temp2];
    VCO = XVCLK * PLLDiv * FreqDiv2x * Bit8Div / Pre_div2x;
    sysclk = VCO / SysDiv / ScaleDiv / 4;
    
    return sysclk;
}

int OV2659_get_HTS(struct v4l2_subdev *sd)
{
    // read HTS from register settings
    int HTS;
    
    HTS = ov2659_read_i2c(sd,0x380c);
    HTS = (HTS<<8) + ov2659_read_i2c(sd,0x380d);
    
    return HTS;
}

int OV2659_get_VTS(struct v4l2_subdev *sd)
{
    // read VTS from register settings
    int VTS;
    
    VTS = ov2659_read_i2c(sd,0x380e);
    VTS = (VTS<<8) + ov2659_read_i2c(sd,0x380f);
    
    return VTS;
}

int OV2659_set_VTS(struct v4l2_subdev *sd,int VTS)
{
    // write VTS to registers
    int temp;

    temp = VTS & 0xff;
    ov2659_write_i2c(sd,0x380f, temp);
    temp = VTS>>8;
    ov2659_write_i2c(sd,0x380e, temp);

    return 0;
}

int OV2659_get_binning_factor(struct v4l2_subdev *sd)
{
    // read VTS from register settings
    int temp, binning;
    
    temp = ov2659_read_i2c(sd,0x3821);
    if (temp & 0x01) {
    binning = 2;
    }
    else {
    binning = 1;
    }
    
    return binning;
}

int OV2659_get_light_frequency(struct v4l2_subdev *sd)
{
    // get light frequency
    int temp, light_frequency;

    temp = ov2659_read_i2c(sd,0x3a05);
    if (temp & 0x80) {
    // 60Hz
    light_frequency = 60;
    }
    else {
    // 50Hz
    light_frequency = 50;
    }

    return light_frequency;
}

void OV2659_set_bandingfilter(struct v4l2_subdev *sd)
{
    int preview_VTS;
    int band_step60, max_band60, band_step50, max_band50;

    // read preview PCLK
    preview_sysclk = OV2659_get_sysclk(sd);
    // read preview HTS
    preview_HTS = OV2659_get_HTS(sd);
    // read preview VTS
    preview_VTS = OV2659_get_VTS(sd);
    // calculate banding filter
    // 60Hz
    band_step60 = preview_sysclk * 100/preview_HTS * 100/120;
    ov2659_write_i2c(sd,0x3a0a, (band_step60 >> 8));
    ov2659_write_i2c(sd,0x3a0b, (band_step60 & 0xff));
    max_band60 = (preview_VTS-4)/band_step60;
    ov2659_write_i2c(sd,0x3a0d, max_band60);
    // 50Hz
    band_step50 = preview_sysclk * 100/preview_HTS;
    ov2659_write_i2c(sd,0x3a08, (band_step50 >> 8));
    ov2659_write_i2c(sd,0x3a09, (band_step50 & 0xff));
    max_band50 = (preview_VTS-4)/band_step50;
    ov2659_write_i2c(sd,0x3a0e, max_band50);
}

int OV2659_capture(struct v4l2_subdev *sd)
{
    int preview_shutter, preview_gain16, preview_binning;
    int capture_shutter, capture_gain16, capture_sysclk, capture_HTS, capture_VTS;
    int light_frequency, capture_bandingfilter, capture_max_band;
    long capture_gain16_shutter;
    int i = 0;
    // read preview shutter
    preview_shutter = OV2659_get_shutter(sd);
    // read preview gain
    preview_gain16 = OV2659_get_gain16(sd);
    // get preview binning factor
    preview_binning = OV2659_get_binning_factor(sd);
    // turn off night mode for capture
    // to do OV2659_set_night_mode(0);
    
    // todo Write capture table
    for(i = 0;i < (sizeof(init_uxga)/sizeof(init_uxga[0]));i++)
    {
        ov2659_write_i2c(sd, init_uxga[i][0],init_uxga[i][1]);
    }
    
    // read capture sysclk
    capture_sysclk = OV2659_get_sysclk(sd);
    // read capture HTS
    capture_HTS = OV2659_get_HTS(sd);
    // read capture VTS
    capture_VTS = OV2659_get_VTS(sd);
    // calculate capture banding filter
    light_frequency = OV2659_get_light_frequency(sd);
    if (light_frequency == 60) {
    // 60Hz
    printk("weichenli capture_sysclk %d capture_HTS %d\n",capture_sysclk,capture_HTS);
    capture_bandingfilter = capture_sysclk * 100/capture_HTS * 100/120;
    }
    else {
    // 50Hz
    printk("weichenli capture_sysclk %d capture_HTS %d\n",capture_sysclk,capture_HTS);
    capture_bandingfilter = capture_sysclk * 100/capture_HTS;
    }
	    printk("weichenli capture_bandingfilter %d\n",capture_bandingfilter);

    capture_max_band = (capture_VTS - 4)/capture_bandingfilter;
    // gain to shutter
    printk(" weichenli  preview_gain16 0x%x preview_shutter 0x%x preview_binning 0x%x capture_sysclk 0x%x preview_sysclk 0x%x preview_HTS 0x%x capture_HTS 0x%x\n"
		, preview_gain16, preview_shutter, preview_binning, capture_sysclk, preview_sysclk, preview_HTS, capture_HTS);

    capture_gain16_shutter = preview_gain16 * preview_shutter * preview_binning * capture_sysclk/preview_sysclk *
    preview_HTS/capture_HTS;
    if(capture_gain16_shutter < (capture_bandingfilter * 16)) {
    // shutter < 1/100
    capture_shutter = capture_gain16_shutter/16;
    if(capture_shutter<1) capture_shutter = 1;
    	    printk("weichenli 111 capture_shutter %d\n",capture_shutter);
    capture_gain16 = ( 2*capture_gain16_shutter + capture_shutter)/ (2*capture_shutter);
    if(capture_gain16 < 16) capture_gain16 = 16;
    }
    else {
    if(capture_gain16_shutter > (capture_bandingfilter*capture_max_band*16)) {
    // exposure reach max
    capture_shutter = capture_bandingfilter*capture_max_band;
	    	    printk("weichenli 222 capture_shutter %d\n",capture_shutter);

    capture_gain16 = ( 2*capture_gain16_shutter + capture_shutter)/ (2*capture_shutter);
    }
    else {
    // 1/100 < capture_shutter < max, capture_shutter = n/100
    capture_shutter = (capture_gain16_shutter/16/capture_bandingfilter) * capture_bandingfilter;
	    	    printk("weichenli 333 capture_shutter %d\n",capture_shutter);
    capture_gain16 = ( 2*capture_gain16_shutter + capture_shutter)/ (2*capture_shutter);
    }
    }
    // write capture gain
    printk("weichenli capture_gain16_shutter 0x%x capture_gain16 0x%x\n",capture_gain16_shutter,capture_gain16);
    if(capture_gain16>=0x28)
    {
        ov2659_write_i2c(sd,0x5000,0xf3);
    }

    OV2659_set_gain16(sd,capture_gain16);
    // write capture shutter
    if (capture_shutter > (capture_VTS - 4)) {
    capture_VTS = capture_shutter + 4;
    OV2659_set_VTS(sd,capture_VTS);
    }
    OV2659_set_shutter(sd,capture_shutter);
    // skip 2 vysnc
    msleep(500);
    // start capture at 3rd vsync
    return 0;
}

static int ov2659_resize(struct v4l2_subdev *sd, unsigned int res)
{

    int i = 0;


    printk("%s res = %d\n",__func__,res);
    switch (res)
    {
        case OV2659_RES_WVGA:
                for(i = 0;i < (sizeof(init_vga)/sizeof(init_vga[0]));i++)
                {
                    ov2659_write_i2c(sd, init_vga[i][0],init_vga[i][1]);
                }
                OV2659_set_bandingfilter(sd);
               break;
        
        case OV2659_RES_UXGA:
               printk("weichenli test 2M capture!!!!\n");

               OV2659_capture(sd);
               break;		   

        case OV2659_RES_720P:              
                for(i = 0;i < (sizeof(init_uxga)/sizeof(init_uxga[0]));i++)
                {
                    ov2659_write_i2c(sd, init_uxga[i][0],init_uxga[i][1]);
                }
                for(i = 0;i < (sizeof(init_720p)/sizeof(init_720p[0]));i++)
                {
                    ov2659_write_i2c(sd, init_720p[i][0],init_720p[i][1]);
                }
               break;
       default:
		WARN(1, "%s: Resolution 0x%08x unknown\n", __func__, res);
		return -EINVAL;
	}

	return 0;
}

static int ov2659_set_mbus_fmt(struct v4l2_subdev *sd,
			      struct v4l2_mbus_framefmt *fmt)
{
    struct ov2659_device *dev = to_ov2659(sd);
    int width, height;
    int ret = 0;

    printk("weichenli %s %d  %d \n",__func__,fmt->width ,fmt->height);
	if((fmt->width == 800) && (fmt->height == 600))
    {
        dev->res = OV2659_RES_WVGA;
	dev->pix.width = 800;
	dev->pix.height = 600;	
	dev->framesize_index = 2;
    }
    else if((fmt->width == 1280) && (fmt->height == 720))
    {
        dev->res = OV2659_RES_720P;
	dev->pix.width = 1280;
	dev->pix.height =720;	
	dev->framesize_index = 4;
    }
    else if((fmt->width == 1600) && (fmt->height == 1200))
    {
        dev->res = OV2659_RES_UXGA;
	dev->pix.width = 1600;
	dev->pix.height =1200;	
	dev->framesize_index = 3;
    }
    else
    {
        ret = -1;
		printk("this fmt is not support width(%d) height(%d)\n",fmt->width ,fmt->height);
		return -1;
    }
    
    //ret = ov2659_resize(sd,dev->res);
    
    return ret;
}

static struct ov2659_control ov2659_controls[] = {
	{
		.qc = {
			.id = V4L2_CID_VFLIP,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "Image v-Flip",
			.minimum = 0,
			.maximum = 1,
			.step = 1,
			.default_value = 0,
		},
		.tweak = ov2659_t_vflip,
	},
	{
		.qc = {
			.id = V4L2_CID_HFLIP,
			.type = V4L2_CTRL_TYPE_INTEGER,
			.name = "Image h-Flip",
			.minimum = 0,
			.maximum = 1,
			.step = 1,
			.default_value = 0,
		},
		.tweak = ov2659_t_hflip,
	},
};
#define N_CONTROLS (ARRAY_SIZE(ov2659_controls))



static int ov2659_reset(struct v4l2_subdev *sd, u32 val)
{
	printk(KERN_ERR "######## %s ######## (%d)\n",__func__,val);
	s3c_gpio_cfgpin(EXYNOS4_GPF3(2), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(EXYNOS4_GPF3(2), S3C_GPIO_PULL_NONE);

	gpio_set_value(EXYNOS4_GPF3(2), 1);
	udelay(100);
	gpio_set_value(EXYNOS4_GPF3(2), 0);
	udelay(100);
	gpio_set_value(EXYNOS4_GPF3(2), 1);
	mdelay(50);

	return 0;
}

static int mt9v114_i2c_write(struct v4l2_subdev *sd, unsigned char i2c_data[], unsigned char length)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char buf[length], i;
	struct i2c_msg msg = {client->addr, 0, length, buf};

//	printk("%s weichenli test ",__func__);

	for (i = 0; i < length; i++)
	{
		buf[i] = i2c_data[i];
//		printk("0x%x ",i2c_data[i]);
	}

//	printk("\n");
		
	return i2c_transfer(client->adapter, &msg, 1) == 1 ? 0 : -EIO;
}

static int mt9v114_i2c_read(struct v4l2_subdev *sd, u8* cmddata, int cdlen, u8 *res, int reslen)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	void *data=client->dev.platform_data;
	struct i2c_adapter *adapter =to_i2c_adapter(client->dev.parent);
	
	struct i2c_msg msg[] = {
		{
			.addr	= client->addr,
			.flags	= 0,
			.buf	= cmddata,
			.len	= cdlen
		}, {
			.addr	= client->addr,
			.flags	= I2C_M_RD,
			.buf	= res,
			.len	= reslen
		}
	};

	ret = i2c_transfer(client->adapter, msg, 2);

	if (ret != 2) {
		dev_err(&client->dev, "%s: 0x%x error ret %d   client->adapter->nr = %d!\n", __func__, client->addr,ret,adapter->nr);
		return -1;
	} else {
		//printk("------------------------------ i2c read in success \n");
	}

	return 0;
}

static int
ov2659_init(struct v4l2_subdev *sd, u32 val)
{
	struct ov2659_device *dev = to_ov2659(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int ret;
	int ini_q;
	int i;

       unsigned char FaceAFCheck[] = {0x30,0x0a};
       unsigned char res_buff[10];
       unsigned char reset[] = {0x01,0x03,0x01};

	
	printk(KERN_ERR "%s \n",__func__);
	ov2659_reset(sd, val);

	msleep(10);
	ret = ov2659_s_power(sd, 1);

	if (ret) {
		v4l2_err(client, "ov2659 power-up err");
		return ret;
	}


        ret = mt9v114_i2c_read(sd, FaceAFCheck, sizeof(FaceAFCheck), res_buff, 1);
        if (ret < 0)
        {
            v4l_info(client, "weichenli  %s: firmversion write failed\n", __func__);
        }
        else
        {
            printk("%s weichenli %x\n",__func__,res_buff[0]);
        }

      mt9v114_i2c_write(sd,reset,sizeof(reset));
      
      mdelay(10);

      for(i = 0;i < (sizeof(init_data)/sizeof(init_data[0]));i++)
      {
	      ov2659_write_i2c(sd, init_data[i][0],init_data[i][1]);
      }
		
	return;
}

static int ov2659_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	return 0;
}

/* Horizontal flip the image. */
static int ov2659_t_hflip(struct v4l2_subdev *sd, int value)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	struct ov2659_device *dev = to_ov2659(sd);
	int err;

	/* set for direct mode */
	err = ov2659_write_reg(c, MISENSOR_16BIT, 0x098E, 0xC850);
	if (value) {
		/* enable H flip ctx A */
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0xC850, 0x01, 0x01);
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0xC851, 0x01, 0x01);
		/* ctx B */
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0xC888, 0x01, 0x01);
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0xC889, 0x01, 0x01);

		/* enable vert_flip and horz_mirror */
		err += misensor_rmw_reg(c, MISENSOR_16BIT, MISENSOR_READ_MODE,
					MISENSOR_F_M_MASK, MISENSOR_F_M_EN);

		dev->bpat = OV2659_BPAT_GRGRBGBG;
	} else {
		/* disable H flip ctx A */
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0xC850, 0x01, 0x00);
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0xC851, 0x01, 0x00);
		/* ctx B */
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0xC888, 0x01, 0x00);
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0xC889, 0x01, 0x00);

		/* enable vert_flip and disable horz_mirror */
		err += misensor_rmw_reg(c, MISENSOR_16BIT, MISENSOR_READ_MODE,
					MISENSOR_F_M_MASK, MISENSOR_F_EN);

		dev->bpat = OV2659_BPAT_BGBGGRGR;
	}

	err += ov2659_write_reg(c, MISENSOR_8BIT, 0x8404, 0x06);
	udelay(10);

	return !!err;
}


/* Vertically flip the image */
static int ov2659_t_vflip(struct v4l2_subdev *sd, int value)
{
	struct i2c_client *c = v4l2_get_subdevdata(sd);
	//struct ov2659_device *dev = to_ov2659(sd);
	int err;

	/* set for direct mode */
	err = ov2659_write_reg(c, MISENSOR_16BIT, 0x098E, 0xC850);
	if (value >= 1) {
		/* enable H flip - ctx A */
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0xC850, 0x02, 0x01);
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0xC851, 0x02, 0x01);
		/* ctx B */
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0xC888, 0x02, 0x01);
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0xC889, 0x02, 0x01);

		/* disable vert_flip and horz_mirror */
		err += misensor_rmw_reg(c, MISENSOR_16BIT, MISENSOR_READ_MODE,
					MISENSOR_F_M_MASK, MISENSOR_F_M_DIS);
	} else {
		/* disable H flip - ctx A */
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0xC850, 0x02, 0x00);
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0xC851, 0x02, 0x00);
		/* ctx B */
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0xC888, 0x02, 0x00);
		err += misensor_rmw_reg(c, MISENSOR_8BIT, 0xC889, 0x02, 0x00);

		/* enable vert_flip and disable horz_mirror */
		err += misensor_rmw_reg(c, MISENSOR_16BIT, MISENSOR_READ_MODE,
					MISENSOR_F_M_MASK, MISENSOR_F_EN);
	}

	err += ov2659_write_reg(c, MISENSOR_8BIT, 0x8404, 0x06);
	udelay(10);

	return !!err;
}

static int ov2659_s_effects(struct v4l2_subdev *sd, int value)
{
	printk("%s:%d\n",__func__,value);
	switch(value)
	{
		case IMAGE_EFFECT_NONE:
		        printk("IMAGE_EFFECT_NONE--\n");
			ov2659_write_i2c(sd, 0x3208, 0x00); // enable group 0
			ov2659_write_i2c(sd, 0x507b, 0x00);
			ov2659_write_i2c(sd, 0x507e, 0x40);
			ov2659_write_i2c(sd, 0x507f, 0x20);
			ov2659_write_i2c(sd, 0x3208, 0x10); // end group 0
			ov2659_write_i2c(sd, 0x3208, 0xa0); // launch group 0                
		break;        
		case IMAGE_EFFECT_BNW:
			printk("IMAGE_EFFECT_BNW--\n");
			ov2659_write_i2c(sd, 0x3208, 0x00); // enable group 0
			ov2659_write_i2c(sd, 0x507b, 0x20);
			ov2659_write_i2c(sd, 0x507e, 0x40);
			ov2659_write_i2c(sd, 0x507f, 0x20);
			ov2659_write_i2c(sd, 0x3208, 0x10); // end group 0
			ov2659_write_i2c(sd, 0x3208, 0xa0); // launch group 0                        
		break;
		case IMAGE_EFFECT_SEPIA:
			printk("IMAGE_EFFECT_SEPIA--\n");
			ov2659_write_i2c(sd, 0x3208, 0x00); // enable group 0
			ov2659_write_i2c(sd, 0x507b, 0x18);
			ov2659_write_i2c(sd, 0x507e, 0x40);
			ov2659_write_i2c(sd, 0x507f, 0xa0);
			ov2659_write_i2c(sd, 0x3208, 0x10); // end group 0
			ov2659_write_i2c(sd, 0x3208, 0xa0); // launch group 0                
		break;
		case IMAGE_EFFECT_BLUE:
			printk("IMAGE_EFFECT_BLUE--\n");
			ov2659_write_i2c(sd, 0x3208, 0x00); // enable group 0
			ov2659_write_i2c(sd, 0x507b, 0x18);
			ov2659_write_i2c(sd, 0x507e, 0xa0);
			ov2659_write_i2c(sd, 0x507f, 0x40);
			ov2659_write_i2c(sd, 0x3208, 0x10); // end group 0
			ov2659_write_i2c(sd, 0x3208, 0xa0); // launch group 0                
		break;
		case IMAGE_EFFECT_GREEN:
			printk("IMAGE_EFFECT_GREEN--\n");
			ov2659_write_i2c(sd, 0x3208, 0x00); // enable group 0
			ov2659_write_i2c(sd, 0x507b, 0x18);
			ov2659_write_i2c(sd, 0x507e, 0x60);
			ov2659_write_i2c(sd, 0x507f, 0x60);
			ov2659_write_i2c(sd, 0x3208, 0x10); // end group 0
			ov2659_write_i2c(sd, 0x3208, 0xa0); // launch group 0                
		break;                
		case IMAGE_EFFECT_RED:
			printk("IMAGE_EFFECT_RED--\n");
			ov2659_write_i2c(sd, 0x3208, 0x00); // enable group 0
			ov2659_write_i2c(sd, 0x507b, 0x18);
			ov2659_write_i2c(sd, 0x507e, 0x80);
			ov2659_write_i2c(sd, 0x507f, 0xc0);
			ov2659_write_i2c(sd, 0x3208, 0x10); // end group 0
			ov2659_write_i2c(sd, 0x3208, 0xa0); // launch group 0                
		break;           
		case IMAGE_EFFECT_NEGATIVE:
			ov2659_write_i2c(sd, 0x507b, 0x40); 
	}
	return 0;
}

static int ov2659_s_saturation(struct v4l2_subdev *sd, int value)
{
	switch(value)
	{
		case -4:
			ov2659_write_i2c(sd, 0x3208, 0x00); // enable group 0
			ov2659_write_i2c(sd, 0x5070, 0x1d);
			ov2659_write_i2c(sd, 0x5071, 0x5e);
			ov2659_write_i2c(sd, 0x5072, 0x05);
			ov2659_write_i2c(sd, 0x5073, 0x13);
			ov2659_write_i2c(sd, 0x5074, 0x59);
			ov2659_write_i2c(sd, 0x5075, 0x6C);
			ov2659_write_i2c(sd, 0x5076, 0x6C);
			ov2659_write_i2c(sd, 0x5077, 0x69);
			ov2659_write_i2c(sd, 0x5078, 0x03);
			ov2659_write_i2c(sd, 0x5079, 0x98);
			ov2659_write_i2c(sd, 0x507a, 0x21);
			ov2659_write_i2c(sd, 0x3208, 0x10); // end group 0
			ov2659_write_i2c(sd, 0x3208, 0xa0); // launch group 0		
		break;
		case -3:
			ov2659_write_i2c(sd, 0x3208, 0x00); // enable group 0
			ov2659_write_i2c(sd, 0x5070, 0x1d);
			ov2659_write_i2c(sd, 0x5071, 0x5e);
			ov2659_write_i2c(sd, 0x5072, 0x05);
			ov2659_write_i2c(sd, 0x5073, 0x16);
			ov2659_write_i2c(sd, 0x5074, 0x68);
			ov2659_write_i2c(sd, 0x5075, 0x7E);
			ov2659_write_i2c(sd, 0x5076, 0x7F);
			ov2659_write_i2c(sd, 0x5077, 0x7B);
			ov2659_write_i2c(sd, 0x5078, 0x04);
			ov2659_write_i2c(sd, 0x5079, 0x98);
			ov2659_write_i2c(sd, 0x507a, 0x21);
			ov2659_write_i2c(sd, 0x3208, 0x10); // end group 0
			ov2659_write_i2c(sd, 0x3208, 0xa0); // launch group 0
		break;
		case -2:
			ov2659_write_i2c(sd, 0x3208, 0x00); // enable group 0
			ov2659_write_i2c(sd, 0x5070, 0x1d);
			ov2659_write_i2c(sd, 0x5071, 0x5e);
			ov2659_write_i2c(sd, 0x5072, 0x05);
			ov2659_write_i2c(sd, 0x5073, 0x1A);
			ov2659_write_i2c(sd, 0x5074, 0x76);
			ov2659_write_i2c(sd, 0x5075, 0x90);
			ov2659_write_i2c(sd, 0x5076, 0x90);
			ov2659_write_i2c(sd, 0x5077, 0x8C);
			ov2659_write_i2c(sd, 0x5078, 0x04);
			ov2659_write_i2c(sd, 0x5079, 0x98);
			ov2659_write_i2c(sd, 0x507a, 0x21);
			ov2659_write_i2c(sd, 0x3208, 0x10); // end group 0
			ov2659_write_i2c(sd, 0x3208, 0xa0); // launch group 0		break;
		case -1:
			ov2659_write_i2c(sd, 0x3208, 0x00); // enable group 0
			ov2659_write_i2c(sd, 0x5070, 0x1d);
			ov2659_write_i2c(sd, 0x5071, 0x5e);
			ov2659_write_i2c(sd, 0x5072, 0x05);
			ov2659_write_i2c(sd, 0x5073, 0x1D);
			ov2659_write_i2c(sd, 0x5074, 0x85);
			ov2659_write_i2c(sd, 0x5075, 0xA2);
			ov2659_write_i2c(sd, 0x5076, 0xA3);
			ov2659_write_i2c(sd, 0x5077, 0x9E);
			ov2659_write_i2c(sd, 0x5078, 0x05);
			ov2659_write_i2c(sd, 0x5079, 0x98);
			ov2659_write_i2c(sd, 0x507a, 0x21);
			ov2659_write_i2c(sd, 0x3208, 0x10); // end group 0
			ov2659_write_i2c(sd, 0x3208, 0xa0); // launch group 0		break;
		case 0:
			ov2659_write_i2c(sd, 0x3208, 0x00); // enable group 0
			ov2659_write_i2c(sd, 0x5070, 0x1d);
			ov2659_write_i2c(sd, 0x5071, 0x5e);
			ov2659_write_i2c(sd, 0x5072, 0x05);
			ov2659_write_i2c(sd, 0x5073, 0x20);
			ov2659_write_i2c(sd, 0x5074, 0x94);		
			ov2659_write_i2c(sd, 0x5075, 0xB4);
			ov2659_write_i2c(sd, 0x5076, 0xB4);
			ov2659_write_i2c(sd, 0x5077, 0xAF);
			ov2659_write_i2c(sd, 0x5078, 0x05);
			ov2659_write_i2c(sd, 0x5079, 0x98);
			ov2659_write_i2c(sd, 0x507a, 0x21);
			ov2659_write_i2c(sd, 0x3208, 0x10); // end group 0
			ov2659_write_i2c(sd, 0x3208, 0xa0); // launch group 0
		break;
		case 1:
			ov2659_write_i2c(sd, 0x3208, 0x00); // enable group 0
			ov2659_write_i2c(sd, 0x5070, 0x1d);
			ov2659_write_i2c(sd, 0x5071, 0x5e);
			ov2659_write_i2c(sd, 0x5072, 0x05);
			ov2659_write_i2c(sd, 0x5073, 0x23);
			ov2659_write_i2c(sd, 0x5074, 0xA3);
			ov2659_write_i2c(sd, 0x5075, 0xC6);
			ov2659_write_i2c(sd, 0x5076, 0xC7);
			ov2659_write_i2c(sd, 0x5077, 0xC1);
			ov2659_write_i2c(sd, 0x5078, 0x06);
			ov2659_write_i2c(sd, 0x5079, 0x98);
			ov2659_write_i2c(sd, 0x507a, 0x21);
			ov2659_write_i2c(sd, 0x3208, 0x10); // end group 0
			ov2659_write_i2c(sd, 0x3208, 0xa0); // launch group 0		
		break;
		case 2:
			ov2659_write_i2c(sd, 0x3208, 0x00); // enable group 0
			ov2659_write_i2c(sd, 0x5070, 0x1d);
			ov2659_write_i2c(sd, 0x5071, 0x5e);
			ov2659_write_i2c(sd, 0x5072, 0x05);
			ov2659_write_i2c(sd, 0x5073, 0x26);
			ov2659_write_i2c(sd, 0x5074, 0xB2);
			ov2659_write_i2c(sd, 0x5075, 0xD8);
			ov2659_write_i2c(sd, 0x5076, 0xD8);
			ov2659_write_i2c(sd, 0x5077, 0xD2);
			ov2659_write_i2c(sd, 0x5078, 0x06);
			ov2659_write_i2c(sd, 0x5079, 0x98);
			ov2659_write_i2c(sd, 0x507a, 0x21);
			ov2659_write_i2c(sd, 0x3208, 0x10); // end group 0
			ov2659_write_i2c(sd, 0x3208, 0xa0); // launch group 0
		break;
		case 3:
			ov2659_write_i2c(sd, 0x3208, 0x00); // enable group 0
			ov2659_write_i2c(sd, 0x5070, 0x1d);
			ov2659_write_i2c(sd, 0x5071, 0x5e);
			ov2659_write_i2c(sd, 0x5072, 0x05);
			ov2659_write_i2c(sd, 0x5073, 0x2A);
			ov2659_write_i2c(sd, 0x5074, 0xC0);
			ov2659_write_i2c(sd, 0x5075, 0xEA);
			ov2659_write_i2c(sd, 0x5076, 0xEB);
			ov2659_write_i2c(sd, 0x5077, 0xE4);
			ov2659_write_i2c(sd, 0x5078, 0x07);
			ov2659_write_i2c(sd, 0x5079, 0x98);
			ov2659_write_i2c(sd, 0x507a, 0x21);
			ov2659_write_i2c(sd, 0x3208, 0x10); // end group 0
			ov2659_write_i2c(sd, 0x3208, 0xa0); // launch group 0
		break;
		case 4:
			ov2659_write_i2c(sd, 0x3208, 0x00); // enable group 0
			ov2659_write_i2c(sd, 0x5070, 0x1d);
			ov2659_write_i2c(sd, 0x5071, 0x5e);
			ov2659_write_i2c(sd, 0x5072, 0x05);
			ov2659_write_i2c(sd, 0x5073, 0x2D);
			ov2659_write_i2c(sd, 0x5074, 0xCF);
			ov2659_write_i2c(sd, 0x5075, 0xfC);
			ov2659_write_i2c(sd, 0x5076, 0xFC);
			ov2659_write_i2c(sd, 0x5077, 0xF5);
			ov2659_write_i2c(sd, 0x5078, 0x07);
			ov2659_write_i2c(sd, 0x5079, 0x98);
			ov2659_write_i2c(sd, 0x507a, 0x21);
			ov2659_write_i2c(sd, 0x3208, 0x10); // end group 0
			ov2659_write_i2c(sd, 0x3208, 0xa0); // launch group 0		
		break;
		default:
		break;
	}
        return 0;
}
static int ov2659_s_white_balance(struct v4l2_subdev *sd, int value)
{
	switch(value)
	{
		case WHITE_BALANCE_AUTO:
			printk("WHITE_BALANCE_AUTO----\n");
			ov2659_write_i2c(sd, 0x3208, 0x00); // enable group 0
			ov2659_write_i2c(sd, 0x3406, 0x00); // AWB auto
			ov2659_write_i2c(sd, 0x3208, 0x10); // end group 0
			ov2659_write_i2c(sd, 0x3208, 0xa0); // launch group 0		break;
		//bulk
		break;
		case WHITE_BALANCE_TUNGSTEN:
			printk("WHITE_BALANCE_TUNGSTEN----\n");
			ov2659_write_i2c(sd, 0x3208, 0x00); // enable group 0
			ov2659_write_i2c(sd, 0x3406, 0x01);
			ov2659_write_i2c(sd, 0x3400, 0x04);
			ov2659_write_i2c(sd, 0x3401, 0x48);
			ov2659_write_i2c(sd, 0x3402, 0x04);
			ov2659_write_i2c(sd, 0x3403, 0x00);
			ov2659_write_i2c(sd, 0x3404, 0x06);
			ov2659_write_i2c(sd, 0x3405, 0x20);
			ov2659_write_i2c(sd, 0x3208, 0x10); // end group 0
			ov2659_write_i2c(sd, 0x3208, 0xa0); // launch group 0              
		break;
		case WHITE_BALANCE_FLUORESCENT:
			printk("WHITE_BALANCE_FLUORESCENT----\n");
			ov2659_write_i2c(sd, 0x3208, 0x00); // enable group 0
			ov2659_write_i2c(sd, 0x3406, 0x01);
			ov2659_write_i2c(sd, 0x3400, 0x05);
			ov2659_write_i2c(sd, 0x3401, 0x10);
			ov2659_write_i2c(sd, 0x3402, 0x04);
			ov2659_write_i2c(sd, 0x3403, 0x00);
			ov2659_write_i2c(sd, 0x3404, 0x05);
			ov2659_write_i2c(sd, 0x3405, 0x94);
			ov2659_write_i2c(sd, 0x3208, 0x10); // end group 0
			ov2659_write_i2c(sd, 0x3208, 0xa0); // launch group 0
		break;
		case WHITE_BALANCE_SUNNY:
			printk("WHITE_BALANCE_SUNNY----\n");
			ov2659_write_i2c(sd, 0x3208, 0x00); // enable group 0
			ov2659_write_i2c(sd, 0x3406, 0x01); // awb manual
			ov2659_write_i2c(sd, 0x3400, 0x06); // R gain
			ov2659_write_i2c(sd, 0x3401, 0x02); // R gain
			ov2659_write_i2c(sd, 0x3402, 0x04); // G gain
			ov2659_write_i2c(sd, 0x3403, 0x00); // G gain
			ov2659_write_i2c(sd, 0x3404, 0x04); // B gain
			ov2659_write_i2c(sd, 0x3405, 0x65); // B gain
			ov2659_write_i2c(sd, 0x3208, 0x10); // end group 0
			ov2659_write_i2c(sd, 0x3208, 0xa0); // launch group 0              
		break;
		case WHITE_BALANCE_CLOUDY:
			printk("WHITE_BALANCE_CLOUDY----\n");
			ov2659_write_i2c(sd, 0x3208, 0x00); // enable group 0
			ov2659_write_i2c(sd, 0x3406, 0x01);
			ov2659_write_i2c(sd, 0x3400, 0x06);
			ov2659_write_i2c(sd, 0x3401, 0x28);
			ov2659_write_i2c(sd, 0x3402, 0x04);
			ov2659_write_i2c(sd, 0x3403, 0x00);
			ov2659_write_i2c(sd, 0x3404, 0x04);
			ov2659_write_i2c(sd, 0x3405, 0x60);
			ov2659_write_i2c(sd, 0x3208, 0x10); // end group 0
			ov2659_write_i2c(sd, 0x3208, 0xa0); // launch group 0
		break;
	}

	return 0;
}
static int ov2659_s_exposure(struct v4l2_subdev *sd, int value)
{
    switch(value)
    {
        case -4:
        //EV -4
        ov2659_write_i2c(sd, 0x3a0f,0x18); 
        ov2659_write_i2c(sd, 0x3a10,0x10); 
        ov2659_write_i2c(sd, 0x3a1b,0x18); 
        ov2659_write_i2c(sd, 0x3a1e,0x10); 
        ov2659_write_i2c(sd, 0x3a11,0x30); 
        ov2659_write_i2c(sd, 0x3a1f,0x10); 
        break;
		
        case -3:
        //EV -3
        ov2659_write_i2c(sd, 0x3a0f,0x20); 
        ov2659_write_i2c(sd, 0x3a10,0x18); 
        ov2659_write_i2c(sd, 0x3a11,0x41); 
        ov2659_write_i2c(sd, 0x3a1b,0x20); 
        ov2659_write_i2c(sd, 0x3a1e,0x18); 
        ov2659_write_i2c(sd, 0x3a1f,0x10); 		
        break;
		
        case -2:
        //EV -2
        ov2659_write_i2c(sd, 0x3a0f,0x28); 
        ov2659_write_i2c(sd, 0x3a10,0x20); 
        ov2659_write_i2c(sd, 0x3a11,0x51); 
        ov2659_write_i2c(sd, 0x3a1b,0x28); 
        ov2659_write_i2c(sd, 0x3a1e,0x20); 
        ov2659_write_i2c(sd, 0x3a1f,0x10); 		
        break;
		
        case -1:
        //EV -1
        ov2659_write_i2c(sd, 0x3a0f,0x30); 
        ov2659_write_i2c(sd, 0x3a10,0x28); 
        ov2659_write_i2c(sd, 0x3a11,0x61); 
        ov2659_write_i2c(sd, 0x3a1b,0x30); 
        ov2659_write_i2c(sd, 0x3a1e,0x28); 
        ov2659_write_i2c(sd, 0x3a1f,0x10); 	
        break;
		
        case 0:
        //EV -2
        ov2659_write_i2c(sd, 0x3a0f,0x38); 
        ov2659_write_i2c(sd, 0x3a10,0x30); 
        ov2659_write_i2c(sd, 0x3a11,0x61); 
        ov2659_write_i2c(sd, 0x3a1b,0x38); 
        ov2659_write_i2c(sd, 0x3a1e,0x30); 
        ov2659_write_i2c(sd, 0x3a1f,0x10); 		
        break;
		
        case 1:
        //EV +1
        ov2659_write_i2c(sd, 0x3a0f,0x40); 
        ov2659_write_i2c(sd, 0x3a10,0x38); 
        ov2659_write_i2c(sd, 0x3a11,0x71); 
        ov2659_write_i2c(sd, 0x3a1b,0x40); 
        ov2659_write_i2c(sd, 0x3a1e,0x38); 
        ov2659_write_i2c(sd, 0x3a1f,0x10); 	
        break;
		
        case 2:
        //EV +2
        ov2659_write_i2c(sd, 0x3a0f,0x48); 
        ov2659_write_i2c(sd, 0x3a10,0x40); 
        ov2659_write_i2c(sd, 0x3a11,0x80); 
        ov2659_write_i2c(sd, 0x3a1b,0x48); 
        ov2659_write_i2c(sd, 0x3a1e,0x40); 
        ov2659_write_i2c(sd, 0x3a1f,0x20); 	
        break;
		
        case 3:
        //EV +3
        ov2659_write_i2c(sd, 0x3a0f,0x50); 
        ov2659_write_i2c(sd, 0x3a10,0x48); 
        ov2659_write_i2c(sd, 0x3a11,0x90); 
        ov2659_write_i2c(sd, 0x3a1b,0x50); 
        ov2659_write_i2c(sd, 0x3a1e,0x48); 
        ov2659_write_i2c(sd, 0x3a1f,0x20); 	
        break;	
		
        case 4:
        //EV +4
        ov2659_write_i2c(sd, 0x3a0f,0x58); 
        ov2659_write_i2c(sd, 0x3a10,0x50); 
        ov2659_write_i2c(sd, 0x3a11,0x91); 
        ov2659_write_i2c(sd, 0x3a1b,0x58); 
        ov2659_write_i2c(sd, 0x3a1e,0x50); 
        ov2659_write_i2c(sd, 0x3a1f,0x20); 	
        break;		

    }

    return 0;
}
static int ov2659_s_brightness(struct v4l2_subdev *sd, int value)
{
	unsigned char temp=0;
	switch(value)
	{

		case -3:
			//brightness -3

			ov2659_write_i2c(sd,0x3208, 0x00);
			ov2659_write_i2c(sd,0x5001, 0x1f);
			ov2659_write_i2c(sd,0x5082, 0x30);
			ov2659_write_i2c(sd,0x507b, 0x06);
			temp = ov2659_read_i2c(sd,0x5083);
			temp = temp | 0x08;
			ov2659_write_i2c(sd,0x5083, temp);
			ov2659_write_i2c(sd,0x3208, 0x10);
			ov2659_write_i2c(sd,0x3208, 0xa0);
	
		break;
		case -2:
			//brightness-2
			ov2659_write_i2c(sd,0x3208, 0x00);
			ov2659_write_i2c(sd,0x5001, 0x1f);
			ov2659_write_i2c(sd,0x5082, 0x20);
			ov2659_write_i2c(sd,0x507b, 0x06);
			temp =  ov2659_read_i2c(sd,0x5083);
			temp = temp | 0x08;
			ov2659_write_i2c(sd,0x5083, temp);
			ov2659_write_i2c(sd,0x3208, 0x10);
			ov2659_write_i2c(sd,0x3208, 0xa0);

		break;
		case -1:
			//brightness -1

			ov2659_write_i2c(sd,0x3208, 0x00);
			ov2659_write_i2c(sd,0x5001, 0x1f);
			ov2659_write_i2c(sd,0x5082, 0x10);
			ov2659_write_i2c(sd,0x507b, 0x06);
			temp =  ov2659_read_i2c(sd,0x5083);
			temp = temp | 0x08;
			ov2659_write_i2c(sd,0x5083, temp);
			ov2659_write_i2c(sd,0x3208, 0x10);
			ov2659_write_i2c(sd,0x3208, 0xa0);
	
		break;
		case 0:
			//brightness -2

			ov2659_write_i2c(sd,0x3208, 0x00);
			ov2659_write_i2c(sd,0x5001, 0x1f);
			ov2659_write_i2c(sd,0x5082, 0x00);
			ov2659_write_i2c(sd,0x507b, 0x06);
			temp =  ov2659_read_i2c(sd,0x5083);
			temp = temp & 0xf7;
			ov2659_write_i2c(sd,0x5083, temp);
			ov2659_write_i2c(sd,0x3208, 0x10);
			ov2659_write_i2c(sd,0x3208, 0xa0);
	
		break;
		case 1:
			//brightness +1

			ov2659_write_i2c(sd,0x3208, 0x00);
			ov2659_write_i2c(sd,0x5001, 0x1f);
			ov2659_write_i2c(sd,0x5082, 0x10);
			ov2659_write_i2c(sd,0x507b, 0x06);
			temp =  ov2659_read_i2c(sd,0x5083);
			temp = temp & 0xf7;
			ov2659_write_i2c(sd,0x5083, temp);
			ov2659_write_i2c(sd,0x3208, 0x10);
			ov2659_write_i2c(sd,0x3208, 0xa0);
	
		break;
		case 2:
			//brightness +2
			ov2659_write_i2c(sd,0x3208, 0x00);
			ov2659_write_i2c(sd,0x5001, 0x1f);
			ov2659_write_i2c(sd,0x5082, 0x20);
			ov2659_write_i2c(sd,0x507b, 0x06);
			temp =  ov2659_read_i2c(sd,0x5083);
			temp = temp & 0xf7;
			ov2659_write_i2c(sd,0x5083, temp);
			ov2659_write_i2c(sd,0x3208, 0x10);
			ov2659_write_i2c(sd,0x3208, 0xa0);

		break;		
		case 3:
			//brightness +3

			ov2659_write_i2c(sd,0x3208, 0x00);
			ov2659_write_i2c(sd,0x5001, 0x1f);
			ov2659_write_i2c(sd,0x5082, 0x30);
			ov2659_write_i2c(sd,0x507b, 0x06);
			temp =  ov2659_read_i2c(sd,0x5083);
			temp = temp & 0xf7;
			ov2659_write_i2c(sd,0x5083, temp);
			ov2659_write_i2c(sd,0x3208, 0x10);
			ov2659_write_i2c(sd,0x3208, 0xa0);
	
		break;		

	
	}
        return 0;
}
static int ov2659_s_contrast(struct v4l2_subdev *sd, int value)
{
	switch(value)
	{

		case -3:
			//contrast -3

			ov2659_write_i2c(sd,0x3208, 0x00);
			ov2659_write_i2c(sd,0x5001, 0x1f);
			ov2659_write_i2c(sd,0x507b, 0x04);
			ov2659_write_i2c(sd,0x5080, 0x14);
			ov2659_write_i2c(sd,0x5081, 0x14);
			ov2659_write_i2c(sd,0x5083, 0x01);
			ov2659_write_i2c(sd,0x3208, 0x10);
			ov2659_write_i2c(sd,0x3208, 0xa0);

	
		break;
		case -2:
			//contrast -2
			ov2659_write_i2c(sd,0x3208, 0x00);
			ov2659_write_i2c(sd,0x5001, 0x1f);
			ov2659_write_i2c(sd,0x507b, 0x04);
			ov2659_write_i2c(sd,0x5080, 0x18);
			ov2659_write_i2c(sd,0x5081, 0x18);
			ov2659_write_i2c(sd,0x5083, 0x01);
			ov2659_write_i2c(sd,0x3208, 0x10);
			ov2659_write_i2c(sd,0x3208, 0xa0);



		break;
		case -1:
			//contrast -1

			ov2659_write_i2c(sd,0x3208, 0x00);
			ov2659_write_i2c(sd,0x5001, 0x1f);
			ov2659_write_i2c(sd,0x507b, 0x04);
			ov2659_write_i2c(sd,0x5080, 0x1c);
			ov2659_write_i2c(sd,0x5081, 0x1c);
			ov2659_write_i2c(sd,0x5083, 0x01);
			ov2659_write_i2c(sd,0x3208, 0x10);
			ov2659_write_i2c(sd,0x3208, 0xa0);

	
		break;
		case 0:
			//contrast -2

			ov2659_write_i2c(sd,0x3208, 0x00);
			ov2659_write_i2c(sd,0x5001, 0x1f);
			ov2659_write_i2c(sd,0x507b, 0x04);
			ov2659_write_i2c(sd,0x5080, 0x20);
			ov2659_write_i2c(sd,0x5081, 0x20);
			ov2659_write_i2c(sd,0x5083, 0x01);
			ov2659_write_i2c(sd,0x3208, 0x10);
			ov2659_write_i2c(sd,0x3208, 0xa0);

	
		break;
		case 1:
			//contrast +1
			ov2659_write_i2c(sd,0x3208, 0x00);
			ov2659_write_i2c(sd,0x5001, 0x1f);
			ov2659_write_i2c(sd,0x507b, 0x04);
			ov2659_write_i2c(sd,0x5080, 0x24);
			ov2659_write_i2c(sd,0x5081, 0x24);
			ov2659_write_i2c(sd,0x5083, 0x01);
			ov2659_write_i2c(sd,0x3208, 0x10);
			ov2659_write_i2c(sd,0x3208, 0xa0);

	
		break;
		case 2:
			//contrast +2
			ov2659_write_i2c(sd,0x3208, 0x00);
			ov2659_write_i2c(sd,0x5001, 0x1f);
			ov2659_write_i2c(sd,0x507b, 0x04);
			ov2659_write_i2c(sd,0x5080, 0x28);
			ov2659_write_i2c(sd,0x5081, 0x28);
			ov2659_write_i2c(sd,0x5083, 0x01);
			ov2659_write_i2c(sd,0x3208, 0x10);
			ov2659_write_i2c(sd,0x3208, 0xa0);


		break;		
		case 3:
			//contrast +3

			ov2659_write_i2c(sd,0x3208, 0x00);
			ov2659_write_i2c(sd,0x5001, 0x1f);
			ov2659_write_i2c(sd,0x507b, 0x04);
			ov2659_write_i2c(sd,0x5080, 0x2c);
			ov2659_write_i2c(sd,0x5081, 0x2c);
			ov2659_write_i2c(sd,0x5083, 0x01);
			ov2659_write_i2c(sd,0x3208, 0x10);
			ov2659_write_i2c(sd,0x3208, 0xa0);

	
		break;		

	
	}
        return 0;
}
static int ov2659_s_sharpness(struct v4l2_subdev *sd, int value)
{

    
    printk("%s %d\n",__func__,value);
    
    value = value -2;
    
    switch(value)
    {
        case -2:
        
        ov2659_write_i2c(sd,0x506e, 0x44);
        ov2659_write_i2c(sd,0x5064, 0x08);
        ov2659_write_i2c(sd,0x5065, 0x10);
        ov2659_write_i2c(sd,0x5066, 0x00);
        ov2659_write_i2c(sd,0x5067, 0x00);
        ov2659_write_i2c(sd,0x506c, 0x08);
        ov2659_write_i2c(sd,0x506d, 0x10);
        ov2659_write_i2c(sd,0x506e, 0x04);
        ov2659_write_i2c(sd,0x506f, 0x06);
        break;
        case -1:
        
        
        ov2659_write_i2c(sd,0x506e, 0x44);
        ov2659_write_i2c(sd,0x5064, 0x08);
        ov2659_write_i2c(sd,0x5065, 0x10);
        ov2659_write_i2c(sd,0x5066, 0x08);
        ov2659_write_i2c(sd,0x5067, 0x01);
        ov2659_write_i2c(sd,0x506c, 0x08);
        ov2659_write_i2c(sd,0x506d, 0x10);
        ov2659_write_i2c(sd,0x506e, 0x04);
        ov2659_write_i2c(sd,0x506f, 0x06);
        break;
        case 0:
        
        
        ov2659_write_i2c(sd,0x506e, 0x44);
        ov2659_write_i2c(sd,0x5064, 0x08);
        ov2659_write_i2c(sd,0x5065, 0x10);
        ov2659_write_i2c(sd,0x5066, 0x12);
        ov2659_write_i2c(sd,0x5067, 0x02);
        ov2659_write_i2c(sd,0x506c, 0x08);
        ov2659_write_i2c(sd,0x506d, 0x10);
        ov2659_write_i2c(sd,0x506e, 0x04);
        ov2659_write_i2c(sd,0x506f, 0x06);
        break;
        case 1:
        
        ov2659_write_i2c(sd,0x506e, 0x44);
        ov2659_write_i2c(sd,0x5064, 0x08);
        ov2659_write_i2c(sd,0x5065, 0x10);
        ov2659_write_i2c(sd,0x5066, 0x22);
        ov2659_write_i2c(sd,0x5067, 0x0c);
        ov2659_write_i2c(sd,0x506c, 0x08);
        ov2659_write_i2c(sd,0x506d, 0x10);
        ov2659_write_i2c(sd,0x506e, 0x04);
        ov2659_write_i2c(sd,0x506f, 0x06);
        break;
        case 2:
        
        ov2659_write_i2c(sd,0x506e, 0x44);
        ov2659_write_i2c(sd,0x5064, 0x08);
        ov2659_write_i2c(sd,0x5065, 0x10);
        ov2659_write_i2c(sd,0x5066, 0x44);
        ov2659_write_i2c(sd,0x5067, 0x18);
        ov2659_write_i2c(sd,0x506c, 0x08);
        ov2659_write_i2c(sd,0x506d, 0x10);
        ov2659_write_i2c(sd,0x506e, 0x03);
        ov2659_write_i2c(sd,0x506f, 0x05);
        break;
        
    
    }
    return 0;
}

static int ov2659_s_anti_banding(struct v4l2_subdev *sd, int value)
{
	switch(value)
	{
		case ANTI_BANDING_50HZ:
			printk("ANTI_BANDING_50HZ--\n");
			//bit[7]select 50/60hz banding, 0:50hz
			ov2659_write_i2c(sd, 0X3a05, 0x30);
		break;
		case ANTI_BANDING_60HZ:
			printk("ANTI_BANDING_60HZ--\n");
			ov2659_write_i2c(sd, 0X3a05, 0x70);
		break;
	}
	return 0;
}

static int ov2659_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
        struct ov2659_device *pdev;
        pdev = container_of(sd, struct ov2659_device, sd);

        printk("pdev=%x\n",(unsigned int)pdev);
        switch (ctrl->id) {
                case V4L2_CID_AUTOGAIN:
                	break;
                case V4L2_CID_GAIN:
                	break;
                case V4L2_CID_AUTO_WHITE_BALANCE:
                	break;
                case V4L2_CID_BLUE_BALANCE:
                	break;
                case V4L2_CID_RED_BALANCE:
                	break;
                case V4L2_CID_SATURATION:
                	break;
                case V4L2_CID_HUE:
                	break;
                case V4L2_CID_BRIGHTNESS:
                	break;
                case V4L2_CID_EXPOSURE_AUTO:
                	break;
                case V4L2_CID_CAMERA_ANTI_BANDING:
                	ctrl->value = pdev->anti_banding;
                	break;
                case V4L2_CID_CAMERA_BRIGHTNESS:
                	ctrl->value = pdev->exposure;
                	break;
                case V4L2_CID_CAMERA_EFFECT:
                	ctrl->value = pdev->color_effect;
                	break;
                case V4L2_CID_CAMERA_SATURATION:
                	ctrl->value = pdev->saturation;
                        break;
                case V4L2_CID_CAMERA_WHITE_BALANCE:
                	ctrl->value = pdev->white_balance;
                        break;
                case V4L2_CID_CAMERA_ZOOM:
                	//ov2659_s_zoom(sd,ctrl->value);
                        break;
                case V4L2_CID_VFLIP:
                	break;
                case V4L2_CID_HFLIP:
                	break;
        }

	return 0;
}
static int myValue=0;
static int ov2659_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
        int ret = 0;
        struct ov2659_device *pdev;
        pdev = container_of(sd, struct ov2659_device, sd);

        printk("pdev=%x\n",(unsigned int)pdev);
        switch (ctrl->id) {
                case V4L2_CID_AUTOGAIN:
                	break;
                case V4L2_CID_GAIN:
                	break;
                case V4L2_CID_AUTO_WHITE_BALANCE:
                	break;
                case V4L2_CID_BLUE_BALANCE:
                	break;
                case V4L2_CID_RED_BALANCE:
                	break;
                case V4L2_CID_SATURATION:
                	break;
                case V4L2_CID_HUE:
                	break;
                case V4L2_CID_BRIGHTNESS:
                	break;
                case V4L2_CID_EXPOSURE_AUTO:
                	break;
                case V4L2_CID_CAMERA_ANTI_BANDING:
                	ret = ov2659_s_anti_banding(sd, ctrl->value);
                	if(!ret)
                	{
                		pdev->anti_banding= ctrl->value;
                	}
               
                	break;
                case V4L2_CID_CAMERA_EXPOSURE:
                	ret = ov2659_s_exposure(sd, ctrl->value);
                	if(!ret)
                	{
                		pdev->exposure = ctrl->value;
                	}
                	break;
                case V4L2_CID_CAMERA_EFFECT:
                        ret=ov2659_s_effects(sd, ctrl->value);
                	if(!ret)
                	{
                		pdev->color_effect= ctrl->value;
                	}                        
                	break;
                case V4L2_CID_CAMERA_SATURATION:
                        ret=ov2659_s_saturation(sd, ctrl->value);
                	if(!ret)
                	{
                		pdev->saturation= ctrl->value;
                	}                          
                        break;
                case V4L2_CID_CAMERA_WHITE_BALANCE:
                        ret=ov2659_s_white_balance(sd, ctrl->value);
                	if(!ret)
                	{
                		pdev->white_balance= ctrl->value;
                	}                         
                        break;
                case V4L2_CID_CAMERA_ZOOM:
                	//ov2659_s_zoom(sd,ctrl->value);
                        break;
                case V4L2_CID_VFLIP:
                	break;
                case V4L2_CID_HFLIP:
                	break;
				case V4L2_CID_CAMERA_CONTRAST:
                        ret=ov2659_s_contrast(sd, ctrl->value);
                	if(!ret)
                	{
                		pdev->contrast= ctrl->value;
                	}  

					break;
				case V4L2_CID_CAMERA_SHARPNESS:
                        ret=ov2659_s_sharpness(sd, ctrl->value);
                	if(!ret)
                	{
                		pdev->sharpness= ctrl->value;
                	}  

					break;
				case V4L2_CID_CAMERA_BRIGHTNESS:
                        ret=ov2659_s_brightness(sd, ctrl->value);
                	if(!ret)
                	{
                		pdev->brightness= ctrl->value;
                	}  

					break;
        }

        return ret;
        
}
static int ov2659_init_param(struct v4l2_subdev *sd)
{
        int ret = 0;
        struct ov2659_device *pdev;
        pdev = container_of(sd, struct ov2659_device, sd);

	pdev->exposure = 0;
	pdev->color_effect= IMAGE_EFFECT_NONE;
	pdev->saturation= 0;
	pdev->white_balance= WHITE_BALANCE_AUTO;
	pdev->anti_banding= ANTI_BANDING_50HZ;
        
}

static int ov2659_s_stream(struct v4l2_subdev *sd, int enable)
{
	int ret = -1;
	struct ov2659_device *dev = to_ov2659(sd);
	if (enable)
		ret = ov2659_resize(sd,dev->res);
	//	else

	return ret;
}

static int
ov2659_enum_framesizes(struct v4l2_subdev *sd, struct v4l2_frmsizeenum *fsize)
{
	struct ov2659_device *dev = to_ov2659(sd);
	int index = 0;
	int i = 0;

	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;
	fsize->discrete.width = dev->pix.width;
	fsize->discrete.height = dev->pix.height;
	index = dev->framesize_index;
	return 0;
}

static int ov2659_enum_frameintervals(struct v4l2_subdev *sd,
				       struct v4l2_frmivalenum *fival)
{
	unsigned int index = fival->index;
	int i;

	return 0;

}

static int
ov2659_g_chip_ident(struct v4l2_subdev *sd, struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return v4l2_chip_ident_i2c_client(client, chip, V4L2_IDENT_OV2659, 0);
}


static const struct v4l2_subdev_video_ops ov2659_video_ops = {
	.try_mbus_fmt = ov2659_try_mbus_fmt,
	.s_mbus_fmt = ov2659_set_mbus_fmt,
	.g_mbus_fmt = ov2659_get_mbus_fmt,
	.s_stream = ov2659_s_stream,
	/*FIMC will reinitialize crop window after enum framesizes,
	 * which will cause zoom failed, so disabled this*/
	.enum_framesizes = ov2659_enum_framesizes,
	.enum_frameintervals = ov2659_enum_frameintervals,
};

static const struct v4l2_subdev_core_ops ov2659_core_ops = {
	.init = ov2659_init,
	.reset = ov2659_reset,
	.g_chip_ident = ov2659_g_chip_ident,
	.queryctrl = ov2659_queryctrl,
	.g_ctrl = ov2659_g_ctrl,
	.s_ctrl = ov2659_s_ctrl,
	.s_power = ov2659_s_power,
};

/* REVISIT: Do we need pad operations? */
/*
static const struct v4l2_subdev_pad_ops ov2659_pad_ops = {
	.enum_mbus_code = ov2659_enum_mbus_code,
	.enum_frame_size = ov2659_enum_frame_size,
	.get_fmt = ov2659_get_pad_format,
	.set_fmt = ov2659_set_pad_format,
};
*/

static const struct v4l2_subdev_ops ov2659_ops = {
	.core = &ov2659_core_ops,
	.video = &ov2659_video_ops,
};


static int ov2659_remove(struct i2c_client *client)
{
	struct ov2659_device *dev;
	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	printk("++++++ %s++++++++",__func__);
	dev = container_of(sd, struct ov2659_device, sd);
	//dev->platform_data->csi_cfg(sd, 0);

	v4l2_device_unregister_subdev(sd);
	kfree(dev);
	regulator_disable(cam1_regulator);
	regulator_disable(cam2_regulator);
//	regulator_disable(cam3_regulator);


	return 0;
}

static void ov2659_cfg_gpio(void)
{
	s3c_gpio_cfgpin(EXYNOS4212_GPJ0(0), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(EXYNOS4212_GPJ0(1), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(EXYNOS4212_GPJ0(2), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(EXYNOS4212_GPJ0(3), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(EXYNOS4212_GPJ0(4), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(EXYNOS4212_GPJ0(5), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(EXYNOS4212_GPJ0(6), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(EXYNOS4212_GPJ0(7), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(EXYNOS4212_GPJ1(0), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(EXYNOS4212_GPJ1(1), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(EXYNOS4212_GPJ1(2), S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(EXYNOS4212_GPJ1(3), S3C_GPIO_SFN(2));

}
static int ov2659_probe(struct i2c_client *client,
		       const struct i2c_device_id *id)
{
	struct ov2659_device *dev;
	struct v4l2_subdev *sd;
	int ret;

	cam1_regulator = regulator_get(NULL, "vdd_ldo23");
	cam2_regulator = regulator_get(NULL, "vdd_ldo19");
//	cam3_regulator = regulator_get(NULL, "vdd_ldo5");
	regulator_enable(cam1_regulator);
	regulator_enable(cam2_regulator);
//	regulator_enable(cam3_regulator);
	msleep(500);
	ov2659_cfg_gpio();
	printk("################ %s #################\n", __func__);

	printk("%s i2c addr = 0x%x, name = %s \n", __func__, client->addr, client->name);

	/* Setup sensor configuration structure */
	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev) {
		dev_err(&client->dev, "out of memory\n");
		ret = -ENOMEM;
		goto err;
	}

	sd = &dev->sd;
	v4l2_i2c_subdev_init(&dev->sd, client, &ov2659_ops);

	/*TODO add format code here*/
	dev->sd.flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;
	//dev->pad.flags = MEDIA_PAD_FLAG_OUTPUT;

	/* set res index to be invalid */
	dev->res = -1;
	printk("################ %s finished :dev :%x #################\n", __func__,(unsigned int)dev);

	return 0;
err:
	kfree(dev);
	regulator_disable(cam1_regulator);
	regulator_disable(cam2_regulator);
//        regulator_disable(cam3_regulator);
	return ret;

}

static struct i2c_driver ov2659_i2c_driver = {
	.driver = {
		.name = "OV2659", //ov2659
	},   
	.probe    = ov2659_probe,
	.remove   = ov2659_remove,
	.id_table = ov2659_id,
	//.suspend                = ov2659_suspend,
	//.resume                 = ov2659_resume,
};

static int __devinit ov2659_module_init(void)
{
	printk("%s\n", __func__);
	return i2c_add_driver(&ov2659_i2c_driver);
}

static void __exit ov2659_module_exit(void)
{
	i2c_del_driver(&ov2659_i2c_driver);
}

module_init(ov2659_module_init);
module_exit(ov2659_module_exit);


MODULE_AUTHOR("Shuguang Gong <Shuguang.gong@intel.com>");
MODULE_LICENSE("GPL");
