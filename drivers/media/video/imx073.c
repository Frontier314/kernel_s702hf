/* linux/drivers/media/video/imx073.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *	         http://www.samsung.com/
 *
 * Driver for IMX073 (SXGA camera) from Samsung Electronics
 * 1/6" 1.3Mp CMOS Image Sensor SoC with an Embedded Image Processor
 * supporting MIPI CSI-2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>
//#include <media/v4l2-i2c-drv.h>
#include <media/imx073_platform.h>

#ifdef CONFIG_VIDEO_SAMSUNG_V4L2
#include <linux/videodev2_samsung.h>
#endif

#include "imx073.h"
#include <plat/gpio-cfg.h>
#include <linux/gpio.h>
#include <linux/clk.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/uaccess.h>
#include <plat/media.h>
#include <plat/clock.h>
#include <plat/fimc.h>
#include <mach/regs-clock.h>
#include <mach/regs-gpio.h>

#include <linux/regulator/consumer.h>
#include "samsung/fimc/fimc.h"

static struct regulator *cam_buck9, *cam_regulator5, *cam_regulator9, *cam_regulator17,\
					*cam_regulator24 , *cam_regulator26;

static void imx073_reset(struct v4l2_subdev *sd, u32 val);
void imx073_clock(void);
static int imx073_s_JPGQuality(struct v4l2_subdev *sd, struct v4l2_control *ctrl);

#define IMX073_DRIVER_NAME	"IMX073"

/* Default resolution & pixelformat. plz ref imx073_platform.h */
//#define DEFAULT_RES		WVGA	/* Index of resoultion */
//#define DEFAUT_FPS_INDEX	IMX073_15FPS
#define DEFAULT_FMT		V4L2_PIX_FMT_UYVY	/* YUV422 */
#define MAX_QUEUE_SIZE		256
#define MAX_MSG_LEN				256
#define MAX_MSG_BUFFER_SIZE		512

/*
 * Specification
 * Parallel : ITU-R. 656/601 YUV422, RGB565, RGB888 (Up to VGA), RAW10
 * Serial : MIPI CSI2 (single lane) YUV422, RGB565, RGB888 (Up to VGA), RAW10
 * Resolution : 1280 (H) x 1024 (V)
 * Image control : Brightness, Contrast, Saturation, Sharpness, Glamour
 * Effect : Mono, Negative, Sepia, Aqua, Sketch
 * FPS : 15fps @full resolution, 30fps @VGA, 24fps @720p
 * Max. pixel clock frequency : 48MHz(upto)
 * Internal PLL (6MHz to 27MHz input frequency)
 */

static int imx073_init(struct v4l2_subdev *sd, u32 val);
static int imx073_init_vga(struct v4l2_subdev *sd);
static int imx073_init_720p(struct v4l2_subdev *sd);
static int imx073_convert_fmt(struct v4l2_subdev *sd, int zoom_val, struct v4l2_format *fmt);

/* Camera functional setting values configured by user concept */
struct imx073_userset {
	signed int exposure_bias;	/* V4L2_CID_EXPOSURE */
	unsigned int ae_lock;
	unsigned int awb_lock;
	unsigned int auto_wb;	/* V4L2_CID_CAMERA_WHITE_BALANCE */
	unsigned int manual_wb;	/* V4L2_CID_WHITE_BALANCE_PRESET */
	unsigned int wb_temp;	/* V4L2_CID_WHITE_BALANCE_TEMPERATURE */
	unsigned int effect;	/* Color FX (AKA Color tone) */
	unsigned int contrast;	/* V4L2_CID_CAMERA_CONTRAST */
	unsigned int saturation;/* V4L2_CID_CAMERA_SATURATION */
	unsigned int brightness;
	unsigned int sharpness;	/* V4L2_CID_CAMERA_SHARPNESS */
	unsigned int glamour;
	unsigned int scene;
	unsigned int focus_position;
	unsigned int zoom;
	unsigned int fast_shutter;
};
enum state_type{
	STATE_CAPTURE_OFF = 0,
	STATE_CAPTURE_ON,
};
enum shutter_type{
	STATE_SHUTTER_OFF = 0,
	STATE_SHUTTER_ON,
};
enum init_type{
	STATE_UNINITIALIZED = 0,
	STATE_INIT_PRIVEW,
	STATE_INIT_COMMAND,
};

struct imx073_state {
	struct imx073_platform_data *pdata;
	struct v4l2_subdev sd;
	struct v4l2_pix_format pix;
	struct v4l2_fract timeperframe;
	struct imx073_userset userset;
	enum v4l2_pix_format_mode format_mode;
	int framesize_index;
	int freq;	/* MCLK in KHz */
	int is_mipi;
	int isize;
	int ver;
	int fps;
	int check_previewdata;

	int state;
	int mode;
	int shutter;
	struct i2c_msg msg[MAX_MSG_LEN];

	int initialized;
	int first_on;
	
	unsigned int okjpegthredhod;
	int ready_irq;
	struct completion   ready_completion;

	struct work_struct      imx073_work;
	struct workqueue_struct *imx073_wq;
	spinlock_t		msg_lock;
	struct mutex i2c_lock;
	unsigned int focus_mode;
	unsigned int jpeg_size;
};

#if 0
enum {
      IMX073_PREVIEW_VGA,
};
#else
enum imx073_preview_frame_size {
	IMX073_PREVIEW_720P = 0,      /*1280*720*/
	IMX073_PREVIEW_QCIF,	        /* 176x144 */
	IMX073_PREVIEW_CIF,		/* 352x288 */
	IMX073_PREVIEW_VGA,		/* 640x480 */
	IMX073_PREVIEW_D1,		/* 720x480 */
	IMX073_PREVIEW_WVGA,		/* 800x480 */
	IMX073_PREVIEW_SVGA,		/* 800x600 */
	IMX073_PREVIEW_WSVGA,		/* 1024x600*/
	IMX073_PREVIEW_MAX,
};

enum imx073_capture_frame_size {
	IMX073_CAPTURE_VGA = 0,	/* 640x480 */
	IMX073_CAPTURE_WVGA,	/* 800x480 */
	IMX073_CAPTURE_SVGA,	/* 800x600 */
	IMX073_CAPTURE_WSVGA,	/* 1024x600 */
	IMX073_CAPTURE_1MP,		/* 1280x960 */
	IMX073_CAPTURE_W1MP,	/* 1600x960 */
	IMX073_CAPTURE_2MP,		/* UXGA  - 1600x1200 */
	IMX073_CAPTURE_W2MP,	/* 35mm Academy Offset Standard 1.66 */
	IMX073_CAPTURE_3MP,		/* QXGA  - 2048x1536 */
	IMX073_CAPTURE_W4MP,	/* WQXGA - 2560x1536 */
	IMX073_CAPTURE_5MP,		/* 2560x1920 */
	IMX073_CAPTURE_8MP,      /*4608x1728*/
};
#endif

struct imx073_enum_framesize {
	unsigned int index;
	unsigned int width;
	unsigned int height;
};

struct imx073_enum_framesize imx073_framesize_list[] = {
          {IMX073_PREVIEW_720P,1280, 720},
          {IMX073_PREVIEW_QCIF, 176, 144 },
          {IMX073_PREVIEW_CIF, 352, 288 },
          {IMX073_PREVIEW_VGA, 640, 480},
          {IMX073_PREVIEW_D1,720, 480 },
          
};

static inline struct imx073_state *to_state(struct v4l2_subdev *sd)
{
	return container_of(sd, struct imx073_state, sd);
}

//    err = imx073_i2c_read(sd, firmversion, sizeof(firmversion), res_buff, 4);
#if 0
static int ak8975_read_data(struct i2c_client *client,
			    u8 reg, u8 length, u8 *buffer)
{
	struct i2c_msg msg[2];
	u8 w_data[2];
	int ret;

	w_data[0] = reg;

	msg[0].addr = client->addr;
	msg[0].flags = I2C_M_NOSTART;	/* set repeated start and write */
	msg[0].len = 1;
	msg[0].buf = w_data;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = length;
	msg[1].buf = buffer;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (ret < 0) {
		dev_err(&client->dev, "Read from device fails\n");
		return ret;
	}

	return 0;
}
#endif

int imx073_i2c_read(struct v4l2_subdev *sd, u8* cmddata, int cdlen, u8 *res, int reslen)
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
		printk("------------------------------ i2c read in success \n");
	}

	return 0;
}

static int imx073_i2c_write(struct v4l2_subdev *sd, unsigned char i2c_data[], unsigned char length)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	unsigned char buf[length], i;
	struct i2c_msg msg = {client->addr, 0, length, buf};


	for (i = 0; i < length; i++)
		buf[i] = i2c_data[i];

	return i2c_transfer(client->adapter, &msg, 1) == 1 ? 0 : -EIO;
}

#if 0
static int imx073_write_regs(struct v4l2_subdev *sd, unsigned char regs[],
				int size)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int i, err;

	for (i = 0; i < size; i++) {
		err = imx073_i2c_write(sd, &regs[i], sizeof(regs[i]));
		if (err < 0)
			v4l_info(client, "%s: register set failed\n", __func__);
	}

	return 0;
}
#endif

static const char *imx073_querymenu_wb_preset[] = {
	"WB sunny",
	"WB cloudy",
	"WB Tungsten",
	"WB Fluorescent",
	NULL
};

static const char *imx073_querymenu_effect_mode[] = {
	"Effect Normal",
	"Effect Monochrome",
	"Effect Sepia",
	"Effect Negative",
	"Effect Aqua",
	"Effect Sketch",
	NULL
};

static const char *imx073_querymenu_ev_bias_mode[] = {
	"-3EV",	"-2,1/2EV", "-2EV", "-1,1/2EV",
	"-1EV", "-1/2EV", "0", "1/2EV",
	"1EV", "1,1/2EV", "2EV", "2,1/2EV",
	"3EV", NULL
};

static struct v4l2_queryctrl imx073_controls[] = {
	{
		/*
		 * For now, we just support in preset type
		 * to be close to generic WB system,
		 * we define color temp range for each preset
		 */
		.id = V4L2_CID_WHITE_BALANCE_TEMPERATURE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "White balance in kelvin",
		.minimum = 0,
		.maximum = 10000,
		.step = 1,
		.default_value = 0,	/* FIXME */
	},
	{
		.id = V4L2_CID_WHITE_BALANCE_PRESET,
		.type = V4L2_CTRL_TYPE_MENU,
		.name = "White balance preset",
		.minimum = 0,
		.maximum = ARRAY_SIZE(imx073_querymenu_wb_preset) - 2,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_CAMERA_WHITE_BALANCE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Auto white balance",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_EXPOSURE,
		.type = V4L2_CTRL_TYPE_MENU,
		.name = "Exposure bias",
		.minimum = 0,
		.maximum = ARRAY_SIZE(imx073_querymenu_ev_bias_mode) - 2,
		.step = 1,
		.default_value = (ARRAY_SIZE(imx073_querymenu_ev_bias_mode)
				- 2) / 2,	/* 0 EV */
	},
	{
		.id = V4L2_CID_CAMERA_EFFECT,
		.type = V4L2_CTRL_TYPE_MENU,
		.name = "Image Effect",
		.minimum = 0,
		.maximum = ARRAY_SIZE(imx073_querymenu_effect_mode) - 2,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_CAMERA_CONTRAST,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Contrast",
		.minimum = 0,
		.maximum = 4,
		.step = 1,
		.default_value = 2,
	},
	{
		.id = V4L2_CID_CAMERA_SATURATION,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Saturation",
		.minimum = 0,
		.maximum = 4,
		.step = 1,
		.default_value = 2,
	},
	{
		.id = V4L2_CID_CAMERA_SHARPNESS,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Sharpness",
		.minimum = 0,
		.maximum = 4,
		.step = 1,
		.default_value = 2,
	},
};


const char **imx073_ctrl_get_menu(u32 id)
{
	switch (id) {
	case V4L2_CID_WHITE_BALANCE_PRESET:
		return imx073_querymenu_wb_preset;

	case V4L2_CID_CAMERA_EFFECT:
		return imx073_querymenu_effect_mode;

	case V4L2_CID_EXPOSURE:
		return imx073_querymenu_ev_bias_mode;

	default:
		return v4l2_ctrl_get_menu(id);
	}
}

static inline struct v4l2_queryctrl const *imx073_find_qctrl(int id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(imx073_controls); i++)
		if (imx073_controls[i].id == id)
			return &imx073_controls[i];

	return NULL;
}

static int imx073_queryctrl(struct v4l2_subdev *sd, struct v4l2_queryctrl *qc)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(imx073_controls); i++) {
		if (imx073_controls[i].id == qc->id) {
			memcpy(qc, &imx073_controls[i],
				sizeof(struct v4l2_queryctrl));
			return 0;
		}
	}

	return -EINVAL;
}

static int imx073_querymenu(struct v4l2_subdev *sd, struct v4l2_querymenu *qm)
{
	struct v4l2_queryctrl qctrl;

	qctrl.id = qm->id;
	imx073_queryctrl(sd, &qctrl);

	return v4l2_ctrl_query_menu(qm, &qctrl, imx073_ctrl_get_menu(qm->id));
}

/*
 * Clock configuration
 * Configure expected MCLK from host and return EINVAL if not supported clock
 * frequency is expected
 * freq : in Hz
 * flag : not supported for now
 */
static int imx073_s_crystal_freq(struct v4l2_subdev *sd, u32  freq, u32 flags)
{
	int err = -EINVAL;

	return err;
}

static inline int imx073_is_Initialized(struct v4l2_subdev *sd)
{
	struct imx073_state *state = to_state(sd);

	return state->initialized;
}

static inline int imx073_is_accept_command(struct v4l2_subdev *sd)
{
	struct imx073_state *state = to_state(sd);

	printk("\n state->initialized:%d \n", state->initialized);

	if (state->initialized == STATE_INIT_COMMAND)
		return 1;

	return 1;
}
static int imx073powered = 0;
unsigned char data_buff[10];
unsigned char res_buff[10];
unsigned char fwboot[] = {0xf0};
unsigned char firmversion[] = {0x0, 0x0};
unsigned char color_bar[] = {0xec, 0x01, 0x01};
unsigned char mipi_out[] = {0x05, 0x00, 0x04};
unsigned char para_set_framerate_15[] = {0x5a, 15,0x00};
unsigned char para_enable_framerate[] = {0x01};
unsigned char para_set_vga_size[] = {0x54, 0x0b, 0x01};
unsigned char para_set_qvga_size[] = {0x54, 0x05, 0x01};
unsigned char para_set_720p_size[] = {0x54, 0x1a, 0x01};
unsigned char para_set_qcif_size[] = {0x54, 0x02, 0x01};
unsigned char para_set_cif_size[] = {0x54, 0x09, 0x01};
unsigned char para_set_480p_size[] = {0x54, 0x0d, 0x01};
unsigned char para_set_800x480_size[] = {0x54, 0x0f, 0x01};
unsigned char para_set_slow_motion[] = {0x54, 0x05, 0x04};
unsigned char ae_unlock[] = {0x11, 0x00};
unsigned char ae_lock[] = {0x11, 0x11};
unsigned char buffer_capture[] = {0x74};
unsigned char jpg_compress[] = {0x90, 0x00, 0xdc, 0x05, 0x78, 0x05, 0x0a, 0x00};
unsigned char jpg_start[] = {0x92, 0x00};
unsigned char jpg_status[] = {0x93, 0x01, 0x00};
unsigned char data_output[] = {0x67, 0x01, 0x01, 0x00, 0x00};
unsigned char data_request[] = {0x68};
unsigned char para_set_capture_size[] = {0x73, 0x21, 0x00, 0x00, 0x0};
unsigned char preview_start[] = {0x6b, 0x01};
unsigned char preview_max[] = {0x6a, 0x01};  /*3264*2448 preview*/
unsigned char capture_status[] = {0x6c};
unsigned char preview_stop[] = {0x6b, 0x00};
unsigned char FaceDetectionOpen0[] = {
                                         0x41,0x00,0x0A,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00
                                     };
unsigned char FaceDetectionOpen1[] = {0x42,0x01};
unsigned char FaceDetectionOpen2[] = {0x4A,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
unsigned char FaceDetectionOpen3[] = {
                                         0x4B,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
                                     };

unsigned char FaceAF[] = {0x23};
unsigned char FaceAFCheck[] = {0x24};
unsigned char FaceAFMask = 0x01;
unsigned char FaceDetectionOff[] = {0x42,0x00};
#define MAX_FACEAF_CKHEC 100

unsigned char Preview_AFSet[] = {0x20,0x00};
unsigned char Preview_AFStart[] = {0x23};
unsigned char Preview_AFStatus[] = {0x24};
unsigned char Preview_AFStop[] = {0x35};

unsigned char Postview_setting[] = {0x55,0x0B,0x03};//{0x55,0x0B,0x00,0x01};
unsigned char Postview_start[] = {0x67,0x02,0x01,0x00,0x00};
unsigned char Postview_exityuv[] = {0x68};


void imx073_start_FaceDetection(struct v4l2_subdev *sd)
{
	//struct imx073_state *state = to_state(sd);	
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = -EINVAL;

	//err = imx073_i2c_write(sd, FaceDetectionOpen0, sizeof(FaceDetectionOpen0));
	//if (err < 0)
		//v4l_info(client, "%s: FaceDetectionOpen write failed\n", __func__);

	err = imx073_i2c_write(sd, FaceDetectionOpen1, sizeof(FaceDetectionOpen1));
	if (err < 0)
		v4l_info(client, "%s: FaceDetectionOpen write failed\n", __func__);

	err = imx073_i2c_write(sd, FaceDetectionOpen2, sizeof(FaceDetectionOpen2));
	if (err < 0)
		v4l_info(client, "%s: FaceDetectionOpen write failed\n", __func__);

	//err = imx073_i2c_write(sd, FaceDetectionOpen3, sizeof(FaceDetectionOpen3));
	//if (err < 0)
		//v4l_info(client, "%s: FaceDetectionOpen write failed\n", __func__);

	v4l_info(client, "%s: FaceDetectionOpen write\n", __func__);
}

void imx073_stop_FaceDetection(struct v4l2_subdev *sd)
{
	//struct imx073_state *state = to_state(sd);	
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = -EINVAL;

	err = imx073_i2c_write(sd, FaceDetectionOff, sizeof(FaceDetectionOff));
	if (err < 0)
		v4l_info(client, "%s: FaceDetectionOff write failed\n", __func__);
	v4l_info(client, "%s: FaceDetectionOff write \n", __func__);
}

void imx073_FaceDetectionAF(struct v4l2_subdev *sd)
{
	//struct imx073_state *state = to_state(sd);	
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = -EINVAL;
    int count = MAX_FACEAF_CKHEC; 

	err = imx073_i2c_write(sd, FaceAF, sizeof(FaceAF));
	if (err < 0)
		v4l_info(client, "%s: FaceDetectionOpen write failed\n", __func__);

    while(count--)
    {
        err = imx073_i2c_read(sd, FaceAFCheck, sizeof(FaceAFCheck), res_buff, 1);
        if (err < 0)
        {
            v4l_info(client, "%s: firmversion write failed\n", __func__);
        }
        else
        {
            if(res_buff[0]&FaceAFMask)
            {
                break;
            }
        }

		mdelay(10);
    }
}


int  imx073_start_preview(struct v4l2_subdev *sd)
{
	//struct imx073_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = -EINVAL;
	int retrycount = 0;
	v4l_info(client, "#######%s\n", __func__);

	if(imx073powered ==0)
		return err;

	//err = imx073_i2c_write(sd, ae_unlock, sizeof(ae_unlock));
	//if (err < 0)
	//	v4l_info(client, "%s: ae_unlock write failed\n", __func__);
	
	err = imx073_i2c_write(sd, para_set_vga_size, sizeof(para_set_vga_size));
	if (err < 0)
		v4l_info(client, "%s: para_set_vga_size write failed\n", __func__);

/*
	err = imx073_i2c_write(sd, para_set_capture_size, sizeof(para_set_capture_size));
	if (err < 0)
		v4l_info(client, "%s: para_set_capture_size write failed\n", __func__);
*/		
	
	err = imx073_i2c_write(sd, ae_unlock, sizeof(ae_unlock));
	if (err < 0)
		v4l_info(client, "%s: ae_unlock write failed\n", __func__);
	
	err = imx073_i2c_write(sd, preview_start, sizeof(preview_start));
	if (err < 0)
		v4l_info(client, "%s: preview_start write failed\n", __func__);

rereadstartpreviewstate:
	err = imx073_i2c_read(sd, capture_status, sizeof(capture_status), res_buff, 1);
	if(res_buff[0] != 8 && retrycount++<CMD_DELAY_LONG) {
		msleep(10);
		goto rereadstartpreviewstate;
	}
	v4l_info(client, "%s: read capture_status %x, read %d times\n", __func__, res_buff[0], retrycount);
	return err;
}

void imx073_stop_preview(struct v4l2_subdev *sd)
{
	//struct imx073_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = -EINVAL;
	int retrycount = 0;

	v4l_info(client, "#######%s\n", __func__);

	if(imx073powered ==0)
		return;

	err = imx073_i2c_write(sd, preview_stop, sizeof(preview_stop));
	if (err < 0)
		v4l_info(client, "%s: preview_stop write failed\n", __func__);
	
rereadstopstate:
	err = imx073_i2c_read(sd, capture_status, sizeof(capture_status), res_buff, 1);
	if(res_buff[0] != 0 && retrycount++<CMD_DELAY_LONG) {
		msleep(10);
		goto rereadstopstate;
	}
	v4l_info(client, "##%s: read capture_status %x, read %d times\n", __func__, res_buff[0],retrycount);
}


void imx073_capture_init(struct v4l2_subdev *sd)
{
	//struct imx073_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = -EINVAL;
	int retrycount = 0;

	v4l_info(client, "#######%s\n", __func__);
	if(imx073powered ==0)
		return;

recapinit:
	err = imx073_i2c_write(sd, para_set_capture_size, sizeof(para_set_capture_size));
	if (err < 0)
		v4l_info(client, "%s: para_set_capture_size write failed\n", __func__);

       err = imx073_i2c_write(sd, Postview_setting, sizeof(Postview_setting));
	if (err < 0)
		v4l_info(client, "%s: Postview_setting write failed\n", __func__);

	err = imx073_i2c_write(sd, ae_lock, sizeof(ae_lock));
	if (err < 0)
		v4l_info(client, "%s: ae_lock write failed\n", __func__);

	err = imx073_i2c_write(sd, buffer_capture, sizeof(buffer_capture));
	if (err < 0)
		v4l_info(client, "%s: buffer_capture write failed\n", __func__);
buffercapturestate:
	err = imx073_i2c_read(sd, capture_status, sizeof(capture_status), res_buff, 1);
	if(res_buff[0] != 0 && retrycount++<CMD_DELAY_LONG){
		msleep(10);
		//goto buffercapturestate;
		goto recapinit;
	}
	v4l_info(client, "%s: read buffercapturestate %x, read %d times\n", __func__, res_buff[0], retrycount);
	
}


void imx073_start_capture(struct v4l2_subdev *sd)
{
	//struct imx073_state *state = to_state(sd);	
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct imx073_state *state = to_state(sd);
	int err = -EINVAL;
      int retrycount = 0;

	v4l_info(client, "#######%s\n", __func__);
	if(imx073powered ==0)
		return;

     err = imx073_i2c_write(sd, jpg_compress, sizeof(jpg_compress));
	if (err < 0)
		v4l_info(client, "%s: jpg_compress write failed\n", __func__);
	
	err = imx073_i2c_write(sd, jpg_start, sizeof(jpg_start));
	if (err < 0)
		v4l_info(client, "%s: jpg_start write failed\n", __func__);
	
jpgcompressresult:
	err = imx073_i2c_read(sd, capture_status, sizeof(capture_status), res_buff, 1);
	if(res_buff[0] != 0 && retrycount++<CMD_DELAY_LONG){
		msleep(10);
		goto jpgcompressresult;
	}
	v4l_info(client, "%s: read jpgcompressresult %x, read %d times\n", __func__, res_buff[0], retrycount);
	
	err = imx073_i2c_read(sd, jpg_status, sizeof(jpg_status), res_buff, 4);
	state->jpeg_size = res_buff[3]<<16 | res_buff[2] <<8 | res_buff[1];	
	v4l_info(client,"data size:res_buff[0]=0x%x,res_buff[1]=0x%x,res_buff[2]=0x%x,res_buff[3]=0x%x,\n",res_buff[0],res_buff[1],res_buff[2],res_buff[3]);

	err = imx073_i2c_read(sd, data_output, sizeof(data_output), res_buff, 4);
	//v4l_info(client,"Image size :res_buff[0]=0x%x,res_buff[1]=0x%x,res_buff[2]=0x%x,res_buff[4]=0x%x,\n",res_buff[0],res_buff[1],res_buff[2],res_buff[3]);
	
	err = imx073_i2c_write(sd, data_request, sizeof(data_request));
	if (err < 0)
		v4l_info(client, "%s: data_request write failed\n", __func__);
	retrycount = 0;
jpgoutputstate:
	err = imx073_i2c_read(sd, capture_status, sizeof(capture_status), res_buff, 1);
	if(res_buff[0] != 0 && retrycount++<CMD_DELAY_LONG){
		msleep(10);
		goto jpgoutputstate;
	}
	v4l_info(client, "%s: read jpgoutputstate %x, read %d times\n", __func__, res_buff[0], retrycount);


}


#ifdef USE_POSTVIEW
void imx073_start_postview(struct v4l2_subdev *sd)
{
	//struct imx073_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = -EINVAL;
	int retrycount = 0;

	v4l_info(client, "#######%s\n", __func__);
	if(imx073powered ==0)
		return;

    	err = imx073_i2c_write(sd, Postview_start, sizeof(Postview_start));
	if (err < 0)
		v4l_info(client, "%s: Postview_start write failed\n", __func__);

	err = imx073_i2c_write(sd, Postview_exityuv, sizeof(Postview_exityuv));
	if (err < 0)
		v4l_info(client, "%s: Postview_exityuv write failed\n", __func__);

rereadcapturestate3:
	err = imx073_i2c_read(sd, capture_status, sizeof(capture_status), res_buff, 1);
	if(res_buff[0] != 0 && retrycount++<CMD_DELAY_LONG){
		msleep(10);
		goto rereadcapturestate3;
	}
	v4l_info(client, "%s: read capture_status %x, read %d times\n", __func__, res_buff[0], retrycount);

}
#endif


static int imx073_set_framerate(struct v4l2_subdev *sd, int framerate)
{
	int err  = 0;
	//struct imx073_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	printk(KERN_ERR "###%s ### %d \n",__func__,framerate);	
	
	if(imx073powered ==0)
		return err;
	if(framerate!=15)
		return err;
	err = imx073_i2c_write(sd, para_set_framerate_15, sizeof(para_set_framerate_15));
	if (err < 0)
		v4l_info(client, "%s: para_set_framerate_15 write failed\n", __func__);

	//err = imx073_i2c_write(sd, para_enable_framerate, sizeof(para_enable_framerate));
	//if (err < 0)
	//	v4l_info(client, "%s: para_set_framerate_15 write failed\n", __func__);
	return err;
}

static int imx073_init_vga(struct v4l2_subdev *sd)
{
	int err  = 0;
	//struct imx073_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	
	if(imx073powered ==0)
		return err;

	err = imx073_i2c_write(sd, para_set_vga_size, sizeof(para_set_vga_size));
	if (err < 0)
		v4l_info(client, "%s: para_set_vga_size write failed\n", __func__);

	return err;
}

static int imx073_init_qvga(struct v4l2_subdev *sd)
{
	int err  = 0;
	//struct imx073_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	
	if(imx073powered ==0)
		return err;

	err = imx073_i2c_write(sd, para_set_qvga_size, sizeof(para_set_qvga_size));
	if (err < 0)
		v4l_info(client, "%s: para_set_qvga_size write failed\n", __func__);

	return err;
}

static int imx073_init_qcif(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = -EINVAL;

	err = imx073_i2c_write(sd, para_set_qcif_size, sizeof(para_set_qcif_size));
	if (err < 0)
		v4l_info(client, "%s: para_set_qcif_size write failed\n", __func__);

	return err;
}

static int imx073_init_720p(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = -EINVAL;

	err = imx073_i2c_write(sd, para_set_720p_size, sizeof(para_set_720p_size));
	if (err < 0)
		v4l_info(client, "%s: para_set_720p_size write failed\n", __func__);

	return err;
}

static int imx073_init_480p(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = -EINVAL;

	err = imx073_i2c_write(sd, para_set_480p_size, sizeof(para_set_480p_size));
	if (err < 0)
		v4l_info(client, "%s: para_set_480p_size write failed\n", __func__);

	return err;
}

static int imx073_init_800x480(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = -EINVAL;

	err = imx073_i2c_write(sd, para_set_800x480_size, sizeof(para_set_800x480_size));
	if (err < 0)
		v4l_info(client, "%s: para_set_800x480_size write failed\n", __func__);

	return err;
}


static int imx073_g_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
{
	struct imx073_state *state = to_state(sd);

	fmt->fmt.pix.width = state->pix.width;
	fmt->fmt.pix.height = state->pix.height;
	fmt->fmt.pix.pixelformat = state->pix.pixelformat;

	return 0;
}

static int imx073_check_focus(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int retrycount = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = 0;
	int value = ctrl->value;

	v4l_info(client, "%s:  value = %x\n", __func__, value);
	
	err = imx073_i2c_write(sd, Preview_AFStart, sizeof(Preview_AFStart));
	if (err < 0)
		v4l_info(client, "%s: Preview_AFStart write failed\n", __func__);

ReadAFstatus:
	err = imx073_i2c_read(sd, Preview_AFStatus, sizeof(Preview_AFStatus), res_buff, 1);
	if((((res_buff[0] &0x06)!= 0x00)|((res_buff[0] &0x06)!= 0x02)) &&(retrycount < 40)){
		mdelay(10);
		retrycount++;
		goto ReadAFstatus;
	}
	v4l_info(client, "%s: read II Preview_AFStatus %x, read %d times\n", __func__, (res_buff[0] &0x06) , retrycount);

	if((res_buff[0] &0x06)!= 0x02)
		return 0x01;
	else 
		return 0;
}

static int imx073_s_shutter(struct v4l2_subdev *sd, int on_off)
{
	int err = 0;
#if 0	
	int try_count = 10;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct imx073_state *state = to_state(sd);

	if(on_off){
		if(state->shutter == STATE_SHUTTER_OFF){
			dev_info(&client->dev, "Shutter_ON\n\n");
			err = imx073_write_regs_sync(sd,  imx073_Shutter_ON, RJ64SC110_SHUTTER_ON_REGS);
			//check command result
			while(imx073_i2c_read_8bit(client, ACK_REG) == COMMAND_NACK)
			{
				err = imx073_write_regs_sync(sd, imx073_Shutter_ON, RJ64SC110_SHUTTER_ON_REGS);
				if(try_count<0)
					break;
				try_count--;
				msleep(100);
			}
			state->shutter = STATE_SHUTTER_ON;
		}
	}else{
		if(state->shutter == STATE_SHUTTER_ON)
		{
			dev_info(&client->dev, "Shutter_OFF\n\n");
			err = imx073_write_regs_sync(sd, imx073_Shutter_OFF, RJ64SC110_SHUTTER_OFF_REGS);
			//check command result
			while(imx073_i2c_read_8bit(client, ACK_REG) == COMMAND_NACK)
			{
				err = imx073_write_regs_sync(sd, imx073_Shutter_OFF, RJ64SC110_SHUTTER_OFF_REGS);
				if(try_count<0)
					break;
				try_count--;
				msleep(100);
			}
			state->shutter = STATE_SHUTTER_OFF;
		}
	}
#endif	
	return err;
}


#if 0
static int bytenum = 0;
unsigned char fw_download_status[] = {0xf5};
unsigned char fw_status_resp[10];
static int temval = 0;
static int CE150X_dw_firmware(struct v4l2_subdev *sd, struct v4l2_format *fmt)
{
	int err = 0, repeattimes=0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct imx073_state *state = to_state(sd);
	unsigned char * pdata;

	if(imx073powered ==0)
		return err;
	pdata = &fmt->fmt.raw_data[1];
	if(fmt->fmt.raw_data[1] == 0)		//file id ==0
	{
		fmt->fmt.raw_data[1] = 0xf2;
		err = imx073_i2c_write(sd, pdata, fmt->fmt.raw_data[0]+1);
		if (err < 0)
			v4l_info(client, "%s: fw download write failed\n", __func__);
	}else if(fmt->fmt.raw_data[1] == 1)	//file id ==1
	{
		v4l_info(client, "%s: last 4 bytes %x %x %x %x, num %d\n", __func__,fmt->fmt.raw_data[127],fmt->fmt.raw_data[128],fmt->fmt.raw_data[129],fmt->fmt.raw_data[130],temval++);
		fmt->fmt.raw_data[1] = 0xf4;
		err = imx073_i2c_write(sd, pdata, fmt->fmt.raw_data[0]+1);
		if (err < 0)
			v4l_info(client, "%s: fw download write failed\n", __func__);
	}else if(fmt->fmt.raw_data[1] == 2)	//file id ==2
	{
		fmt->fmt.raw_data[1] = 0xf2;
		err = imx073_i2c_write(sd, pdata, fmt->fmt.raw_data[0]+1);
		if (err < 0)
			v4l_info(client, "%s: fw download write failed\n", __func__);
	}else if(fmt->fmt.raw_data[1] == 3)	//file id ==3
	{
		fmt->fmt.raw_data[1] = 0xf4;
		err = imx073_i2c_write(sd, pdata, fmt->fmt.raw_data[0]+1);
		if (err < 0)
			v4l_info(client, "%s: fw download write failed\n", __func__);
	}else if(fmt->fmt.raw_data[1] == 5)	//wait status
	{
		mdelay(10);
refwstatus:
		err = imx073_i2c_read(sd, fw_download_status, sizeof(fw_download_status), fw_status_resp, 1);
		if (err < 0)
			v4l_info(client, "%s: wait step %d failed\n", __func__,fmt->fmt.raw_data[0]);
		if(fmt->fmt.raw_data[0] == 2)
		{
			if(fw_status_resp[0] != 5  && repeattimes++<CMD_DELAY_LONG) 
			{
				mdelay(10);
				if(repeattimes%100==0)
				v4l_info(client, "%s: wait step %d , status %d\n", __func__,fmt->fmt.raw_data[0],fw_status_resp[0] );
				goto refwstatus;
			}
		}else if(fmt->fmt.raw_data[0] == 4)
		{
			if(fw_status_resp[0] != 6  && repeattimes++<CMD_DELAY_LONG) 
			{
				mdelay(10);
				if(repeattimes%100==0)
				v4l_info(client, "%s: wait step %d , status %d\n", __func__,fmt->fmt.raw_data[0],fw_status_resp[0] );
				goto refwstatus;
			}
		}else if(fmt->fmt.raw_data[0] == 8)//reset and fwboot
		{
			imx073_reset(sd,0xff);
		}else if(fmt->fmt.raw_data[0] == 9)//reset and fwboot
		{
			imx073_reset(sd,0xf7);
		}
		v4l_info(client, "#%s: wait step %d , status %c\n", __func__,fmt->fmt.raw_data[0],fw_status_resp[0] );
	}
	return err;
}
#endif

static int imx073_load_fw(struct v4l2_subdev *sd)
{
      //struct imx073_state *state = to_state(sd);
      struct i2c_client *client = v4l2_get_subdevdata(sd);
      int err = -EINVAL;

	//queue_work(state->imx073_wq, &state->imx073_work);
	v4l_info(client, "#######%s\n", __func__);

    	err = imx073_i2c_write(sd, fwboot, sizeof(fwboot));
	if (err < 0)
		v4l_info(client, "%s: fwboot write failed\n", __func__);
    
	v4l_info(client, "%s: fwboot write size %d\n", __func__, sizeof(fwboot));
    
	msleep(500);

    err = imx073_i2c_read(sd, firmversion, sizeof(firmversion), res_buff, 4);
	if (err < 0)
		v4l_info(client, "%s: firmversion write failed\n", __func__);
	v4l_info(client, "%s: write size:%d, read version %d.%d %d.%d \n", __func__, sizeof(firmversion), res_buff[1], res_buff[0], res_buff[3], res_buff[2]);

    	//err = imx073_i2c_write(sd, mipi_out, sizeof(mipi_out));
	//if (err < 0)
	//	v4l_info(client, "%s: mipi_out write failed\n", __func__);
    
	return 0;
}

static int imx073_repreview(struct v4l2_subdev *sd)
{
	int err = 0;
	int repeatetime = 0;
      struct imx073_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	
    	//err = imx073_i2c_write(sd, mipi_out, sizeof(mipi_out));
	//if (err < 0)
	//	v4l_info(client, "%s: mipi_out write failed\n", __func__);
	
	if(state->fps ==15)	//only for vt mode,must before preview start, after set preview size.
		imx073_set_framerate(sd,15);
	err = imx073_i2c_write(sd, ae_unlock, sizeof(ae_unlock));
	if (err < 0)
		v4l_info(client, "%s: ae_unlock write failed\n", __func__);
	
	err = imx073_i2c_write(sd, preview_start, sizeof(preview_start));
	if (err < 0)
		v4l_info(client, "%s: preview_start write failed\n", __func__);

repreviewstatus:
	err = imx073_i2c_read(sd, capture_status, sizeof(capture_status), res_buff, 1);
	if(res_buff[0] != 8 && repeatetime++<CMD_DELAY_LONG) {
		mdelay(10);
		//v4l_info(client, "%s: capture_status %d\n", __func__,res_buff[0]);
		goto repreviewstatus;
	}
	v4l_info(client, "###%s: repreviewstatus %d repeatetime:%d\n", __func__,res_buff[0],repeatetime);
	return err;
}


static int imx073_s_res(struct v4l2_subdev *sd, int width)
{
	if(width==640)
		imx073_init_vga(sd);
	else if(width==1280)
		imx073_init_720p(sd);
	else if(width==176)
		imx073_init_qcif(sd);
	else if(width==800)
		imx073_init_800x480(sd);
	else if(width==720)
		imx073_init_480p(sd);
	else if(width==320)
		imx073_init_qvga(sd);
	else printk("\n%s :unsuported resolutin %d\n",__func__,width);
	return 0;
}

static int imx073_s_captureres(struct v4l2_subdev *sd, int width,int height)
{
	int err = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	printk("##%s, %d*%d******************",__func__,width,height);

	if(width==160)//160*120
		para_set_capture_size[1] = 0x00;
	else if(width==176)//176*144
		para_set_capture_size[1] = 0x02;
	else if(width==640)//640*480
		para_set_capture_size[1] = 0x0B;
	else if(width==800)//800*480
		para_set_capture_size[1] = 0x0F;
	else if(width==1920)//1920*1080
		para_set_capture_size[1] = 0x1E;
	else if(width==2000)//2000*1600
		para_set_capture_size[1] = 0x2A;
	else if(width==2048)//2048*1536
		para_set_capture_size[1] = 0x1F;	
	else if(width==1280)
		{
			if(height == 720)//1280*720
				para_set_capture_size[1] = 0x1A;	
			else//1280*960
				para_set_capture_size[1] = 0x1C;	
		}
	else if(width==2560)
		{
			if(height == 1920)
				para_set_capture_size[1] = 0x20;	
			else//2560*1536
				para_set_capture_size[1] = 0x2B;	
		}
	else if(width==3264)
		{
			if(height == 2448)
				para_set_capture_size[1] = 0x21;	
			else//3264*1960
				para_set_capture_size[1] = 0x2C;	
		}
	else printk("\n%s :unsuported resolutin %dx%d\n",__func__,width,height);
/*
	err = imx073_i2c_write(sd, para_set_capture_size, sizeof(para_set_capture_size));
	if (err < 0)
		v4l_info(client, "%s: para_set_capture_size write failed\n", __func__);
*/		
	return err;
}

static int imx073_s_fmt(struct v4l2_subdev *sd, struct v4l2_mbus_framefmt *fmt)
{
	int err = 0;

	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct imx073_state *state = to_state(sd);

#if 0
	if(fmt->type==V4L2_BUF_TYPE_PRIVATE)//frimware update mode
	{
		err = CE150X_dw_firmware(sd,fmt);
		return err;
	}
#endif

//	dev_info(&client->dev, "%s: fmt width==%d,fmt height==%d\n", __func__,fmt->fmt.pix.width,fmt->fmt.pix.height);
	state->pix.width = fmt->width;
	state->pix.height = fmt->height;
//	state->pix.pixelformat = fmt->reserved[0];
    if (fmt->colorspace == V4L2_COLORSPACE_JPEG) {
			state->format_mode = V4L2_PIX_FMT_MODE_CAPTURE;	
  			state->okjpegthredhod = fmt->width*fmt->height*3/2;
			imx073_s_captureres(sd,fmt->width,fmt->height);			
			imx073_capture_init(sd);		
						
			if(state->okjpegthredhod <= 38016)	//176*144*3/2  //jpeg data of low resolution with low compress ratio will output error data
				imx073_s_JPGQuality(sd, 1000);
	} else {
	
		state->format_mode = V4L2_PIX_FMT_MODE_PREVIEW;
	}
	
	if(imx073powered ==0)
		return err;

	mutex_lock(&state->i2c_lock);
#if 0
	switch( fmt->reserved[1])
	{
		case CAMERA_PREVIEW:
#if 0
            /*shutter off*/
			switch(state->mode)
			{
				case CAMERA_PREVIEW:	
				case CAMERA_RECORD:
					imx073_stop_preview(sd);
					imx073_s_res(sd,fmt->fmt.pix.width);
					imx073_repreview(sd);
					break;
                    
				case CAMERA_CAPTURE_INIT:
				case CAMERA_CAPTURE:
                                imx073_stop_preview(sd);
                                imx073_s_res(sd,fmt->fmt.pix.width);
   					//imx073_init_vga(sd);
					imx073_repreview(sd);
					break;
			}
#endif            
            		imx073_stop_preview(sd);
			imx073_s_res(sd,fmt->width);
			imx073_repreview(sd);
			state->mode = CAMERA_PREVIEW;
			break;

		case CAMERA_STOPPREVIEW:
            		imx073_stop_preview(sd);
			break;
			
		case CAMERA_RECORD:
			/*
			imx073_stop_preview(sd);
			imx073_s_res(sd,fmt->fmt.pix.width);
			imx073_repreview(sd);
			*/
			state->mode = CAMERA_RECORD;
			state->state = STATE_CAPTURE_OFF;
			break;

		case CAMERA_CAPTURE_INIT:
            		imx073_s_captureres(sd,fmt->width,fmt->height);
#ifndef USE_POSTVIEW	
			imx073_capture_init(sd);
#endif
			state->mode = CAMERA_CAPTURE_INIT;
			state->state = STATE_CAPTURE_ON;
			break;
			
		case CAMERA_CAPTURE:
			imx073_start_capture(sd);
			state->mode = CAMERA_CAPTURE;
			state->state = STATE_CAPTURE_ON;
			break;

#ifdef USE_POSTVIEW
            case CAMERA_POSTVIEW:
                   imx073_capture_init(sd); 
                   imx073_start_postview(sd);
                	state->mode = CAMERA_POSTVIEW;
			state->state = STATE_CAPTURE_ON;
                   break;
#endif

		default:
			break;

	}
#endif
	mutex_unlock(&state->i2c_lock);

	return err;
}

static int imx073_enum_framesizes(struct v4l2_subdev *sd,
					struct v4l2_frmsizeenum *fsize)
{
	struct imx073_state *state = to_state(sd);
	int num_entries = sizeof(imx073_framesize_list) /
				sizeof(struct imx073_enum_framesize);
	struct imx073_enum_framesize *elem;
	int index = 0;
	int i = 0;

	/* The camera interface should read this value, this is the resolution
	 * at which the sensor would provide framedata to the camera i/f
	 *
	 * In case of image capture,
	 * this returns the default camera resolution (WVGA)
	 */
	fsize->type = V4L2_FRMSIZE_TYPE_DISCRETE;

	fsize->discrete.width = state->pix.width;//176;//
	fsize->discrete.height = state->pix.height;//144;//

	index = state->framesize_index;
	//printk("\n%s : %d %d\n",__func__,index,fsize->discrete.width);
	return 0;

	for (i = 0; i < num_entries; i++) {
		elem = &imx073_framesize_list[i];
		if (elem->index == index) {
			fsize->discrete.width =
			    imx073_framesize_list[index].width;
			fsize->discrete.height =
			    imx073_framesize_list[index].height;
			return 0;
		}
	}

	return -EINVAL;
}

static int imx073_enum_frameintervals(struct v4l2_subdev *sd,
					struct v4l2_frmivalenum *fival)
{
	int err = 0;

	return err;
}

static int imx073_enum_fmt(struct v4l2_subdev *sd,
				struct v4l2_fmtdesc *fmtdesc)
{
	int err = 0;

	return err;
}

static int imx073_try_fmt(struct v4l2_subdev *sd, struct v4l2_format *fmt)
{
	int err = 0;

	return err;
}

static int imx073_g_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	int err = 0;

	return err;
}

static int imx073_s_parm(struct v4l2_subdev *sd, struct v4l2_streamparm *param)
{
	int err = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	
	dev_info(&client->dev, "%s: numerator %d, denominator: %d\n",
		__func__, param->parm.capture.timeperframe.numerator,
		param->parm.capture.timeperframe.denominator);

	struct imx073_state *state = to_state(sd);

	u32 fps = param->parm.capture.timeperframe.denominator /
					param->parm.capture.timeperframe.numerator;


	if (fps != state->fps) {
		if (fps <= 0 || fps > 30) {
			printk("invalid frame rate %d\n", fps);
			fps = 30;
		}
		state->fps = fps;
	}
	return err;

	return err;
}

static int imx073_g_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct imx073_state *state = to_state(sd);
	struct imx073_userset userset = state->userset;
	int err = 0;
    
#if 1//def IMX073_COMPLETE
	switch (ctrl->id) {
	case V4L2_CID_CAMERA_WHITE_BALANCE:
		ctrl->value = userset.auto_wb;
		break;
	case V4L2_CID_WHITE_BALANCE_PRESET:
		ctrl->value = userset.manual_wb;
		break;
	case V4L2_CID_CAMERA_EFFECT:
		ctrl->value = userset.effect;
		break;
	case V4L2_CID_CAMERA_CONTRAST:
		ctrl->value = userset.contrast;
		break;
	case V4L2_CID_CAMERA_SATURATION:
		ctrl->value = userset.saturation;
		break;
	case V4L2_CID_CAMERA_SHARPNESS:
		ctrl->value = userset.saturation;
		break;
	case V4L2_CID_CAM_JPEG_MAIN_SIZE:
//		ctrl->value =  (4608 * 1728 * 8) / 32;
		ctrl->value = state->jpeg_size;
    	break;
	case V4L2_CID_CAM_JPEG_MEMSIZE:
		ctrl->value = 4608*1728/2;
		break;
	case V4L2_CID_CAMERA_AUTO_FOCUS_RESULT:
		ctrl->value = imx073_check_focus(sd,ctrl);
		break;
	case V4L2_CID_CAMERA_TOUCH_AF_START_STOP:
		break;

	case V4L2_CID_CAM_JPEG_MAIN_OFFSET:
	case V4L2_CID_CAM_JPEG_THUMB_SIZE:
	case V4L2_CID_CAM_JPEG_THUMB_OFFSET:
		ctrl->value = 0;
		break;
	case V4L2_CID_CAM_JPEG_POSTVIEW_OFFSET:
		ctrl->value = 0;
		break;
	case V4L2_CID_CAM_JPEG_QUALITY:
	case V4L2_CID_CAM_DATE_INFO_YEAR:
	case V4L2_CID_CAM_DATE_INFO_MONTH:
	case V4L2_CID_CAM_DATE_INFO_DATE:
	case V4L2_CID_CAM_SENSOR_VER:
	case V4L2_CID_CAM_FW_MINOR_VER:
	case V4L2_CID_CAM_FW_MAJOR_VER:
	case V4L2_CID_CAM_PRM_MINOR_VER:
	case V4L2_CID_CAM_PRM_MAJOR_VER:
//	case V4L2_CID_ESD_INT:
//	case V4L2_CID_CAMERA_GET_ISO:
//	case V4L2_CID_CAMERA_GET_SHT_TIME:
	case V4L2_CID_CAMERA_OBJ_TRACKING_STATUS:
	case V4L2_CID_CAMERA_SMART_AUTO_STATUS:
		ctrl->value = 0;
		break;
	case V4L2_CID_EXPOSURE:
		ctrl->value = userset.exposure_bias;
		break;
	default:
		dev_err(&client->dev, "%s: no such ctrl\n", __func__);
		/* err = -EINVAL; */
		break;
	}

	return err;
#else
	return 0;
#endif    
}

static int imx073_s_Focus(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int retrycount = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct imx073_state *state = to_state(sd);
	int err = 0;
	int value = ctrl->value;

	v4l_info(client, "%s:  value = %x\n", __func__, value);
	switch(state->focus_mode)
	{
		case FOCUS_MODE_AUTO:
		Preview_AFSet[1] = 0x00;
		break;
		
		case FOCUS_MODE_MACRO:
		Preview_AFSet[1] = 0x01;
		break;

		case FOCUS_MODE_INFINITY://continuous
		Preview_AFSet[1] = 0x02;
		break;
	}

rereadfocusstate:
	err = imx073_i2c_read(sd, capture_status, sizeof(capture_status), res_buff, 1);
	if(res_buff[0] != 8  && retrycount++<CMD_DELAY_SHORT) {
		mdelay(10);
		goto rereadfocusstate;
	}
	v4l_info(client, "%s: read capture_status %x, read %d times\n", __func__, res_buff[0], retrycount);

	err = imx073_i2c_write(sd, Preview_AFSet, sizeof(Preview_AFSet));
	if (err < 0)
		v4l_info(client, "%s: Preview_AFSet write failed\n", __func__);

	retrycount = 0;
	
ReadAFstatus1:
	err = imx073_i2c_read(sd, Preview_AFStatus, sizeof(Preview_AFStatus), res_buff, 1);
	if(((res_buff[0] &0x06) != 0x04) &&(retrycount < CMD_DELAY_SHORT)){
		mdelay(10);
		retrycount++;
		goto ReadAFstatus1;
	}
	v4l_info(client, "%s: read I Preview_AFStatus %x, read %d times\n", __func__, (res_buff[0] &0x06), retrycount);

	err = imx073_i2c_write(sd, Preview_AFStart, sizeof(Preview_AFStart));
	if (err < 0)
		v4l_info(client, "%s: Preview_AFStart write failed\n", __func__);

#if 0
    retrycount = 0;
ReadAFstatus2:
	err = imx073_i2c_read(sd, Preview_AFStatus, sizeof(Preview_AFStatus), res_buff, 1);
	if((((res_buff[0] &0x06)!= 0x00)|((res_buff[0] &0x06)!= 0x02)) &&(retrycount < CMD_DELAY_SHORT)){
		mdelay(10);
		retrycount++;
		goto ReadAFstatus2;
	}
	v4l_info(client, "%s: read II Preview_AFStatus %x, read %d times\n", __func__, res_buff[0] , retrycount);
#endif

	return err;
}

static int imx073_slowmotion(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = 0;

	//TODO: the array most be saved before change the default vaule
	para_set_vga_size[0] = para_set_slow_motion[0];
	para_set_vga_size[1] = para_set_slow_motion[1];
	para_set_vga_size[2] = para_set_slow_motion[2];

	imx073_stop_preview(sd);
	imx073_start_preview(sd);
	
	err = imx073_i2c_write(sd, Preview_AFStart, sizeof(Preview_AFStart));
	if (err < 0)
		v4l_info(client, "%s: Preview_AFStart write failed\n", __func__);

	return err;
}

static int imx073_s_Brightness(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int retrycount = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = 0;
	int value = ctrl->value;
	unsigned char Exposure[]={0x04,0x02,0x06};//MM=06 is the default vaule
	unsigned char Exposure_Start[]={0x01};
	unsigned char Exposure_State[]={0x02};
	unsigned char res[10] = {0};


	v4l_info(client, "%s:  value = %x\n", __func__, value);
	switch(value)
	{
		case EV_PLUS_6:
		Exposure[2] = 0x0c;
		break;
		
		case EV_PLUS_5:
		Exposure[2] = 0x0b;
		break;
		
		case EV_PLUS_4:
		Exposure[2] = 0x0a;
		break;
		
		case EV_PLUS_3:
		Exposure[2] = 0x09;
		break;

		case EV_PLUS_2:
		Exposure[2] = 0x08;
		break;
		
		case EV_PLUS_1:
		Exposure[2] = 0x07;
		break;
		
		case EV_DEFAULT:
		Exposure[2] = 0x06;
		break;
		
		case EV_MINUS_1:
		Exposure[2] = 0x05;
		break;
		
		case EV_MINUS_2:
		Exposure[2] = 0x04;
		break;
		
		case EV_MINUS_3:
		Exposure[2] = 0x03;
		break;
		
		case EV_MINUS_4:
		Exposure[2] = 0x02;
		break;

		case EV_MINUS_5:
		Exposure[2] = 0x01;
		break;

		case EV_MINUS_6:
		Exposure[2] = 0x00;
		break;

		default:
		Exposure[2] = 0x06;
		break;

	}


	err = imx073_i2c_write(sd, Exposure, sizeof(Exposure));
	if (err < 0)
		v4l_info(client, "%s: Exposure write failed\n", __func__);

	err = imx073_i2c_write(sd, Exposure_Start, sizeof(Exposure_Start));
	if (err < 0)
		v4l_info(client, "%s: Exposure_Start write failed\n", __func__);

ReadStatusbrightness:
	err = imx073_i2c_read(sd, Exposure_State, sizeof(Exposure_State), res, 1);
	if((res[0] != 0x00)&&(retrycount < CMD_DELAY_SHORT)){
		mdelay(10);
		retrycount++;
		goto ReadStatusbrightness;
	}
	else
		v4l_info(client, "%s: Exposure_Start setting success res = 0x%x\n", __func__,res[0]);

	return err;
}

static int imx073_s_Scene(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int retrycount = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = 0;
	int value = ctrl->value;
	unsigned char res[10] = {0};
	unsigned char Scene_Start[]={0x01};
	unsigned char Scene_State[]={0x02};

	//Normal
	unsigned char Normal0[] ={0x04,0x01,0x00}; 
	unsigned char Normal1[] ={0x04,0x00,0x00}; 
	unsigned char Normal2[] ={0x04,0x02,0x06}; 
	//unsigned char Normal3[] ={0x3D,0x01,0x00}; 
	unsigned char Normal4[] ={0x3D,0x03,0x00}; 
	unsigned char Normal5[] ={0x3D,0x02,0x02}; 
	unsigned char Normal6[] ={0x3D,0x06,0x08};
	unsigned char Normal7[] ={0x42,0x00};

	 //Landscape 
	unsigned char Landscape0[] ={0x04,0x01,0x00}; 
	unsigned char Landscape1[] ={0x04,0x00,0x05}; 
	unsigned char Landscape2[] ={0x04,0x02,0x06}; 
	//unsigned char Landscape3[] ={0x3D,0x01,0x01}; 
	unsigned char Landscape4[] ={0x3D,0x03,0x01}; 
	unsigned char Landscape5[] ={0x3D,0x02,0x03}; 
	unsigned char Landscape6[] ={0x3D,0x06,0x08}; 
	unsigned char Landscape7[] ={0x42,0x00}; 

	//Portrait
	unsigned char Portrait0[] ={0x04,0x01,0x00};
	unsigned char Portrait1[] ={0x04,0x00,0x00};
	unsigned char Portrait2[] ={0x04,0x02,0x06};
	//unsigned char Portrait3[] ={0x3D,0x01,0x02};
	unsigned char Portrait4[] ={0x3D,0x03,0x02};
	unsigned char Portrait5[] ={0x3D,0x02,0x03};
	unsigned char Portrait6[] ={0x3D,0x06,0x02};
	unsigned char Portrait7[] ={0x41,0x00,0x0A,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
	unsigned char Portrait8[] ={0x42,0x01};

	//Night 
	unsigned char Night0[] ={0x04,0x01,0x0C}; 
	unsigned char Night1[] ={0x04,0x00,0x00}; 
	unsigned char Night2[] ={0x04,0x02,0x06}; 
	//unsigned char Night3[] ={0x3D,0x01,0x04}; 
	unsigned char Night4[] ={0x3D,0x03,0x04}; 
	unsigned char Night5[] ={0x3D,0x02,0x02}; 
	unsigned char Night6[] ={0x3D,0x06,0x08}; 
	unsigned char Night7[] ={0x42,0x00};

	// Sports 
	//unsigned char Sports0[] = {0x04,0x01,0x0A}; 
	//unsigned char Sports1[] = {0x04,0x00,0x02}; 
	//unsigned char Sports2[] = {0x04,0x02,0x06}; 
	//unsigned char Sports3[] = {0x3D,0x01,0x01}; 
	//unsigned char Sports4[] = {0x3D,0x03,0x01}; 
	//unsigned char Sports5[] = {0x3D,0x02,0x03}; 
	//unsigned char Sports6[] = {0x3D,0x06,0x08}; 
	//unsigned char Sports7[] = {0x42,0x00};

	// Candle Light
	unsigned char Light0[] = {0x04,0x01,0x0B};
	unsigned char Light1[] = {0x04,0x00,0x02};
	unsigned char Light2[] = {0x04,0x02,0x05};
	//unsigned char Light3[] = {0x3D,0x01,0x04};
	unsigned char Light4[] = {0x3D,0x03,0x04};
	unsigned char Light5[] = {0x3D,0x02,0x02};
	unsigned char Light6[] = {0x3D,0x06,0x08};
	unsigned char Light7[] = {0x42,0x00};

	// Text
	//unsigned char Text0[] = {0x04,0x01,0x00};
	//unsigned char Text1[] = {0x04,0x00,0x00};
	//unsigned char Text2[] = {0x04,0x02,0x06};
	//unsigned char Text3[] = {0x3D,0x01,0x00};
	//unsigned char Text4[] = {0x3D,0x03,0x00};
	//unsigned char Text5[] = {0x3D,0x02,0x03};
	//unsigned char Text6[] = {0x3D,0x06,0x08};
	//unsigned char Text7[] = {0x42,0x00};

	// Beach_Snow
	unsigned char Beach_Snow0[] = {0x04,0x01,0x00};
	unsigned char Beach_Snow1[] = {0x04,0x00,0x00};
	unsigned char Beach_Snow2[] = {0x04,0x02,0x07};
	//unsigned char Beach_Snow3[] = {0x3D,0x01,0x01};
	unsigned char Beach_Snow4[] = {0x3D,0x03,0x01};
	unsigned char Beach_Snow5[] = {0x3D,0x02,0x03};
	unsigned char Beach_Snow6[] = {0x3D,0x06,0x08};
	unsigned char Beach_Snow7[] = {0x42,0x00};

	// Flower
	//unsigned char Flower0[] = {0x04,0x01,0x00};
	//unsigned char Flower1[] = {0x04,0x00,0x00};
	//unsigned char Flower2[] = {0x04,0x02,0x06};
	//unsigned char Flower3[] = {0x3D,0x01,0x00};
	//unsigned char Flower4[] = {0x3D,0x03,0x01};
	//unsigned char Flower5[] = {0x3D,0x02,0x02};
	//unsigned char Flower6[] = {0x3D,0x06,0x08};
	//unsigned char Flower7[] = {0x42,0x00};

	// Sunset
	unsigned char Sunset0[] = {0x04,0x01,0x00};
	unsigned char Sunset1[] = {0x04,0x00,0x00};
	unsigned char Sunset2[] = {0x04,0x02,0x06};
	//unsigned char Sunset3[] = {0x04,0x11,0x01};
	unsigned char Sunset4[] = {0x3D,0x01,0x00};
	unsigned char Sunset5[] = {0x3D,0x03,0x01};
	unsigned char Sunset6[] = {0x3D,0x02,0x02};
	unsigned char Sunset7[] = {0x3D,0x06,0x08};
	unsigned char Sunset8[] = {0x42,0x00};

	v4l_info(client, "%s: value = %x\n", __func__, value);

	switch(value){
    case SCENE_MODE_NONE:
	err = imx073_i2c_write(sd, Normal0, sizeof(Normal0));
	if (err < 0)
		v4l_info(client, "%s: Normal0 write failed\n", __func__);

	err = imx073_i2c_write(sd, Normal1, sizeof(Normal1));
	if (err < 0)
		v4l_info(client, "%s: Normal1 write failed\n", __func__);

	err = imx073_i2c_write(sd, Normal2, sizeof(Normal2));
	if (err < 0)
		v4l_info(client, "%s: Normal2 write failed\n", __func__);

	err = imx073_i2c_write(sd, Normal4, sizeof(Normal4));
	if (err < 0)
		v4l_info(client, "%s: Normal4 write failed\n", __func__);

	err = imx073_i2c_write(sd, Normal5, sizeof(Normal5));
	if (err < 0)
		v4l_info(client, "%s: Normal5 write failed\n", __func__);

	err = imx073_i2c_write(sd, Normal6, sizeof(Normal6));
	if (err < 0)
		v4l_info(client, "%s: Normal6 write failed\n", __func__);

	err = imx073_i2c_write(sd, Normal7, sizeof(Normal7));
	if (err < 0)
		v4l_info(client, "%s: Normal7 write failed\n", __func__);

	break;
	
	case SCENE_MODE_PORTRAIT:
	err = imx073_i2c_write(sd, Portrait0, sizeof(Portrait0));
	if (err < 0)
		v4l_info(client, "%s: Portrait0 write failed\n", __func__);

	err = imx073_i2c_write(sd, Portrait1, sizeof(Portrait1));
	if (err < 0)
		v4l_info(client, "%s: Portrait1 write failed\n", __func__);

	err = imx073_i2c_write(sd, Portrait2, sizeof(Portrait2));
	if (err < 0)
		v4l_info(client, "%s: Portrait2 write failed\n", __func__);

	err = imx073_i2c_write(sd, Portrait4, sizeof(Portrait4));
	if (err < 0)
		v4l_info(client, "%s: Portrait4 write failed\n", __func__);

	err = imx073_i2c_write(sd, Portrait5, sizeof(Portrait5));
	if (err < 0)
		v4l_info(client, "%s: Portrait5 write failed\n", __func__);

	err = imx073_i2c_write(sd, Portrait6, sizeof(Portrait6));
	if (err < 0)
		v4l_info(client, "%s: Portrait6 write failed\n", __func__);

	err = imx073_i2c_write(sd, Portrait7, sizeof(Portrait7));
	if (err < 0)
		v4l_info(client, "%s: Portrait7 write failed\n", __func__);

	err = imx073_i2c_write(sd, Portrait8, sizeof(Portrait8));
	if (err < 0)
		v4l_info(client, "%s: Portrait8 write failed\n", __func__);

	break;

	case SCENE_MODE_LANDSCAPE:
	err = imx073_i2c_write(sd, Landscape0, sizeof(Landscape0));
	if (err < 0)
		v4l_info(client, "%s: Landscape0 write failed\n", __func__);

	err = imx073_i2c_write(sd, Landscape1, sizeof(Landscape1));
	if (err < 0)
		v4l_info(client, "%s: Landscape1 write failed\n", __func__);

	err = imx073_i2c_write(sd, Landscape2, sizeof(Landscape2));
	if (err < 0)
		v4l_info(client, "%s: Landscape2 write failed\n", __func__);

	err = imx073_i2c_write(sd, Landscape4, sizeof(Landscape4));
	if (err < 0)
		v4l_info(client, "%s: Landscape4 write failed\n", __func__);

	err = imx073_i2c_write(sd, Landscape5, sizeof(Landscape5));
	if (err < 0)
		v4l_info(client, "%s: Landscape5 write failed\n", __func__);

	err = imx073_i2c_write(sd, Landscape6, sizeof(Landscape6));
	if (err < 0)
		v4l_info(client, "%s: Landscape6 write failed\n", __func__);

	err = imx073_i2c_write(sd, Landscape7, sizeof(Landscape7));
	if (err < 0)
		v4l_info(client, "%s: Landscape7 write failed\n", __func__);

	break;

	case SCENE_MODE_NIGHTSHOT:
	err = imx073_i2c_write(sd, Night0, sizeof(Night0));
	if (err < 0)
		v4l_info(client, "%s: Night0 write failed\n", __func__);

	err = imx073_i2c_write(sd, Night1, sizeof(Night1));
	if (err < 0)
		v4l_info(client, "%s: Night1 write failed\n", __func__);

	err = imx073_i2c_write(sd, Night2, sizeof(Night2));
	if (err < 0)
		v4l_info(client, "%s: Night2 write failed\n", __func__);

	err = imx073_i2c_write(sd, Night4, sizeof(Night4));
	if (err < 0)
		v4l_info(client, "%s: Night4 write failed\n", __func__);

	err = imx073_i2c_write(sd, Night5, sizeof(Night5));
	if (err < 0)
		v4l_info(client, "%s: Night5 write failed\n", __func__);

	err = imx073_i2c_write(sd, Night6, sizeof(Night6));
	if (err < 0)
		v4l_info(client, "%s: Night6 write failed\n", __func__);

	err = imx073_i2c_write(sd, Night7, sizeof(Night7));
	if (err < 0)
		v4l_info(client, "%s: Night7 write failed\n", __func__);

	break;

	case SCENE_MODE_BEACH_SNOW:
	err = imx073_i2c_write(sd, Beach_Snow0, sizeof(Beach_Snow0));
	if (err < 0)
		v4l_info(client, "%s: Beach_Snow0 write failed\n", __func__);

	err = imx073_i2c_write(sd, Beach_Snow1, sizeof(Beach_Snow1));
	if (err < 0)
		v4l_info(client, "%s: Beach_Snow1 write failed\n", __func__);

	err = imx073_i2c_write(sd, Beach_Snow2, sizeof(Beach_Snow2));
	if (err < 0)
		v4l_info(client, "%s: Beach_Snow2 write failed\n", __func__);

	err = imx073_i2c_write(sd, Beach_Snow4, sizeof(Beach_Snow4));
	if (err < 0)
		v4l_info(client, "%s: Beach_Snow4 write failed\n", __func__);

	err = imx073_i2c_write(sd, Beach_Snow5, sizeof(Beach_Snow5));
	if (err < 0)
		v4l_info(client, "%s: Beach_Snow5 write failed\n", __func__);

	err = imx073_i2c_write(sd, Beach_Snow6, sizeof(Beach_Snow6));
	if (err < 0)
		v4l_info(client, "%s: Beach_Snow6 write failed\n", __func__);

	err = imx073_i2c_write(sd, Beach_Snow7, sizeof(Beach_Snow7));
	if (err < 0)
		v4l_info(client, "%s: Beach_Snow7 write failed\n", __func__);

	break;

	case SCENE_MODE_SUNSET:
	err = imx073_i2c_write(sd, Sunset0, sizeof(Sunset0));
	if (err < 0)
		v4l_info(client, "%s: Sunset0 write failed\n", __func__);

	err = imx073_i2c_write(sd, Sunset1, sizeof(Sunset1));
	if (err < 0)
		v4l_info(client, "%s: Sunset1 write failed\n", __func__);

	err = imx073_i2c_write(sd, Sunset2, sizeof(Sunset2));
	if (err < 0)
		v4l_info(client, "%s: Sunset2 write failed\n", __func__);

	err = imx073_i2c_write(sd, Sunset4, sizeof(Sunset4));
	if (err < 0)
		v4l_info(client, "%s: Sunset4 write failed\n", __func__);

	err = imx073_i2c_write(sd, Sunset5, sizeof(Sunset5));
	if (err < 0)
		v4l_info(client, "%s: Sunset5 write failed\n", __func__);

	err = imx073_i2c_write(sd, Sunset6, sizeof(Sunset6));
	if (err < 0)
		v4l_info(client, "%s: Sunset6 write failed\n", __func__);

	err = imx073_i2c_write(sd, Sunset7, sizeof(Sunset7));
	if (err < 0)
		v4l_info(client, "%s: Sunset7 write failed\n", __func__);

	err = imx073_i2c_write(sd, Sunset8, sizeof(Sunset8));
	if (err < 0)
		v4l_info(client, "%s: Sunset8 write failed\n", __func__);

	break;

	case SCENE_MODE_BACK_LIGHT:
	case SCENE_MODE_SPORTS:
	case SCENE_MODE_PARTY_INDOOR:
	case SCENE_MODE_DUST_DAWN:
	case SCENE_MODE_FALL_COLOR:
	case SCENE_MODE_FIREWORKS:
	case SCENE_MODE_TEXT:
	break;

	case SCENE_MODE_CANDLE_LIGHT:
	err = imx073_i2c_write(sd, Light0, sizeof(Light0));
	if (err < 0)
		v4l_info(client, "%s: Light0 write failed\n", __func__);

	err = imx073_i2c_write(sd, Light1, sizeof(Light1));
	if (err < 0)
		v4l_info(client, "%s: Light1 write failed\n", __func__);

	err = imx073_i2c_write(sd, Light2, sizeof(Light2));
	if (err < 0)
		v4l_info(client, "%s: Light2 write failed\n", __func__);

	err = imx073_i2c_write(sd, Light4, sizeof(Light4));
	if (err < 0)
		v4l_info(client, "%s: Light4 write failed\n", __func__);

	err = imx073_i2c_write(sd, Light5, sizeof(Light5));
	if (err < 0)
		v4l_info(client, "%s: Light5 write failed\n", __func__);

	err = imx073_i2c_write(sd, Light6, sizeof(Light6));
	if (err < 0)
		v4l_info(client, "%s: Light6 write failed\n", __func__);

	err = imx073_i2c_write(sd, Light7, sizeof(Light7));
	if (err < 0)
		v4l_info(client, "%s: Light7 write failed\n", __func__);

	break;

	case SCENE_MODE_MAX:
	break;

	}


	err = imx073_i2c_write(sd, Scene_Start, sizeof(Scene_Start));
	if (err < 0)
		v4l_info(client, "%s: Scene_Start write failed\n", __func__);

ReadStatusScene:
	err = imx073_i2c_read(sd, Scene_State, sizeof(Scene_State), res, 1);
	if((res[0] != 0x00)&&(retrycount < CMD_DELAY_SHORT)){
		mdelay(10);
		retrycount++;
		goto ReadStatusScene;
	}
	else
		v4l_info(client, "%s: Scene_Start setting success res = 0x%x\n", __func__,res[0]);

	return err;
}

static int imx073_s_WB(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int retrycount = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = 0;
	int value = ctrl->value;
	unsigned char WB_Start[]={0x1a,0x00};
	unsigned char WBMode[]={0x04,0x11,0x00};//MM=06 is the default vaule
	unsigned char WB_run[]={0x01};
	unsigned char WB_State[]={0x02};
	unsigned char res[10] = {0};

	v4l_info(client, "%s:  value = %x\n", __func__, value);

	switch(value)
	{
		case WHITE_BALANCE_AUTO:
		WBMode[2] = 0x00;
		break;

		case WHITE_BALANCE_SUNNY:
		WBMode[2] = 0x01;
		break;
		
		case WHITE_BALANCE_CLOUDY:
		WBMode[2] = 0x02;
		break;
		
		case WHITE_BALANCE_TUNGSTEN:
		WBMode[2] = 0x03;
		break;
		
		case WHITE_BALANCE_FLUORESCENT:
		WBMode[2] = 0x04;
		break;
	}


	err = imx073_i2c_write(sd, WB_Start, sizeof(WB_Start));
	if (err < 0)
		v4l_info(client, "%s: WB_Start write failed\n", __func__);

	err = imx073_i2c_write(sd, WBMode, sizeof(WBMode));
	if (err < 0)
		v4l_info(client, "%s: WBMode write failed\n", __func__);

	err = imx073_i2c_write(sd, WB_run, sizeof(WB_run));
	if (err < 0)
		v4l_info(client, "%s: WB_run write failed\n", __func__);

ReadWB:
	err = imx073_i2c_read(sd, WB_State, sizeof(WB_State), res, 1);
	if((res[0] != 0x00)&&(retrycount < CMD_DELAY_SHORT)){
		mdelay(10);
		retrycount++;
		goto ReadWB;
	}
	else
		v4l_info(client, "%s: WB setting success res = 0x%x\n", __func__,res[0]);

	return err;
}

static int imx073_s_Effect(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int retrycount = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = 0;
	int value = ctrl->value;
	unsigned char Effect_Setting[]={0x3D,0x05,0x00};
	unsigned char Effect_run[]={0x01};
	unsigned char Effect_Status[]={0x02};
	unsigned char res[10] = {0};

	printk("%s:  value = %x\n", __func__, value);

	switch(value)
	{
		case IMAGE_EFFECT_NONE:
		Effect_Setting[2] = 0x00;
		break;

		case IMAGE_EFFECT_BNW:
		Effect_Setting[2] = 0x01;
		break;
		
		case IMAGE_EFFECT_SEPIA:
		Effect_Setting[2] = 0x02;
		break;
		
		case IMAGE_EFFECT_AQUA:
		Effect_Setting[2] = 0x03;
		break;

		case IMAGE_EFFECT_ANTIQUE:
		Effect_Setting[2] = 0x06;

		case IMAGE_EFFECT_NEGATIVE:
		Effect_Setting[2] = 0x05;

		case IMAGE_EFFECT_SHARPEN:
		Effect_Setting[2] = 0x04;
		break;

		case IMAGE_EFFECT_RED:
		Effect_Setting[2] = 0x07;
		break;

		case IMAGE_EFFECT_PINK:
		Effect_Setting[2] = 0x08;
		break;

		case IMAGE_EFFECT_YELLOW:
		Effect_Setting[2] = 0x09;
		break;

		case IMAGE_EFFECT_GREEN:
		Effect_Setting[2] = 0x0a;
		break;

		case IMAGE_EFFECT_BLUE:
		Effect_Setting[2] = 0x0b;
		break;

		case IMAGE_EFFECT_PURPLE:
		Effect_Setting[2] = 0x0c;
		break;

		case IMAGE_EFFECT_WATERCOLOR:
		Effect_Setting[2] = 0x0d;
		break;

		default:
		Effect_Setting[2] = 0x00;
		break;
	}


	err = imx073_i2c_write(sd, Effect_Setting, sizeof(Effect_Setting));
	if (err < 0)
		v4l_info(client, "%s: Effect_Setting write failed\n", __func__);

	err = imx073_i2c_write(sd, Effect_run, sizeof(Effect_run));
	if (err < 0)
		v4l_info(client, "%s: Effect_run write failed\n", __func__);

ReadEffect:
	err = imx073_i2c_read(sd, Effect_Status, sizeof(Effect_Status), res, 1);
	if((res[0] != 0x00)&&(retrycount < CMD_DELAY_SHORT)){
		mdelay(10);
		retrycount++;
		goto ReadEffect;
	}
	else
		v4l_info(client, "%s: Effect setting success res = 0x%x\n", __func__,res[0]);

	return err;
}

static int imx073_s_Contrast(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int retrycount = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = 0;
	int value = ctrl->value;
	//unsigned char Contrast_Start[]={0x1a,0x00};
	unsigned char Contrast_Mode[]={0x3d,0x07,0x08};//MM=08 is the default vaule
	unsigned char Contrast_run[]={0x01};
	unsigned char Contrast_State[]={0x02};
	unsigned char res[10] = {0};

	v4l_info(client, "%s:  value = %x\n", __func__, value);

	switch(value)
	{
		case CONTRAST_MINUS_6:
		Contrast_Mode[2] = 0x00;
		break;

		case CONTRAST_MINUS_5:
		Contrast_Mode[2] = 0x03;
		break;

		case CONTRAST_MINUS_4:
		Contrast_Mode[2] = 0x04;
		break;

		case CONTRAST_MINUS_3:
		Contrast_Mode[2] = 0x05;
		break;

		case CONTRAST_MINUS_2:
		Contrast_Mode[2] = 0x06;
		break;
		
		case CONTRAST_MINUS_1:
		Contrast_Mode[2] = 0x07;
		break;
		
		case CONTRAST_DEFAULT:
		Contrast_Mode[2] = 0x08;
		break;
		
		case CONTRAST_PLUS_1:
		Contrast_Mode[2] = 0x09;
		break;

		case CONTRAST_PLUS_2:
		Contrast_Mode[2] = 0x0a;
		break;

		case CONTRAST_PLUS_3:
		Contrast_Mode[2] = 0x0b;
		break;

		case CONTRAST_PLUS_4:
		Contrast_Mode[2] = 0x0c;
		break;

		case CONTRAST_PLUS_5:
		Contrast_Mode[2] = 0x0d;
		break;

		case CONTRAST_PLUS_6:
		Contrast_Mode[2] = 0x10;
		break;
	}


	err = imx073_i2c_write(sd, Contrast_Mode, sizeof(Contrast_Mode));
	if (err < 0)
		v4l_info(client, "%s: Contrast_Mode write failed\n", __func__);

	err = imx073_i2c_write(sd, Contrast_run, sizeof(Contrast_run));
	if (err < 0)
		v4l_info(client, "%s: Contrast_run write failed\n", __func__);

ReadWB:
	err = imx073_i2c_read(sd, Contrast_State, sizeof(Contrast_State), res, 1);
	if((res[0] != 0x00)&&(retrycount < CMD_DELAY_SHORT)){
		mdelay(10);
		retrycount++;
		goto ReadWB;
	}
	else
		v4l_info(client, "%s: Contrast setting success res = 0x%x\n", __func__,res[0]);

	return err;
}


static int imx073_s_TouchAF(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	//int retrycount = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = 0;
	int value = ctrl->value;

	//Area_AF
	//Drawing
	unsigned char  AF_Drawing[] = {0x4A,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
	// Touch AF setting
	unsigned char AF_Setting[] = {0x41,0x05,0x03,0x70,0x00,0x70,0x00,0xFF,0x00,0xFF,0x00};
	//start
	unsigned char AF_Start[] = {0x42,0x05};

	v4l_info(client, "%s:  value = %x\n", __func__, value);

	err = imx073_i2c_write(sd, AF_Drawing, sizeof(AF_Drawing));
	if (err < 0)
		v4l_info(client, "%s: AF_Drawing write failed\n", __func__);

	err = imx073_i2c_write(sd, AF_Setting, sizeof(AF_Setting));
	if (err < 0)
		v4l_info(client, "%s: AF_Setting write failed\n", __func__);

	err = imx073_i2c_write(sd, AF_Start, sizeof(AF_Start));
	if (err < 0)
		v4l_info(client, "%s: Preview_AFSet write failed\n", __func__);

	return err;
}

static int imx073_s_ObjectTracking(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	//int retrycount = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	//struct imx073_state *state = to_state(sd);
	int err = 0;

	//Object_AF
	// Frame Drawing
	unsigned char Drawing[] = {0x4A,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
	// Object Tracking Setting 
	unsigned char Setting[] = {0x41,0x04,0x03,0x80,0x00,0x80,0x00,0x00,0x00,0x00,0x00}; 
	// Start Object Tracking
	unsigned char Start[] = {0x42,0x04};

	err = imx073_i2c_write(sd, Drawing, sizeof(Drawing));
	if (err < 0)
		v4l_info(client, "%s:Object Tracking Drawing write failed\n", __func__);

	err = imx073_i2c_write(sd, Setting, sizeof(Setting));
	if (err < 0)
		v4l_info(client, "%s:Object Tracking Setting write failed\n", __func__);

	err = imx073_i2c_write(sd, Start, sizeof(Start));
	if (err < 0)
		v4l_info(client, "%s: Object Tracking Start write failed\n", __func__);

	return err;
}

static int imx073_s_Panoroma(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int retrycount = 0;
	int err = -EINVAL;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	//struct imx073_state *state = to_state(sd);

#if 1
	//01PanoramaPreview(down2up)
	// Change EV chart(over 1/30)
	//unsigned char Change_EV1[] = {0x04,0x01,0x0D};
	//unsigned char Change_EV2[] = {0x01};
	//unsigned char Change_EV_Status[] = {0x02};// (WAIT)10 (RETRY)100 (EXP)00
#endif

	// Panorama Setting 
	// 1st 77H = Command ID
	// 2nd 0BH = Preview Image Size (now setting is VGA)
	// 3rd 19H = Capture Image Size (Now setting is 1024x768) It can change.
	// 4th 01H = Sensor Drive mode
	// 5th 00H = Reserved 
	// 6th 00H = direction 
	// (00:left to right,01:right to left,02:up to down,03:down to up)

	unsigned char Command_ID[] = {0x77,0x0B,0x19,0x01,0x00,0x03};

	// Panorama Preview Start
	unsigned char Preview_Start[] = {0x78,0x01};

	// Check response 
	unsigned char Response_Status[] = {0x6C};//(WAIT)10 (RETRY)100 (EXP)08

	//(TITLE)02PanoramaCapture
	// Start Panorama Capture
	unsigned char PanoramaCapture[] = {0x78,0x02};
	// Check status(Panorama status)
	unsigned char PanoramaCapture_Status[] = {0x79};// (WAIT)150 (RETRY)100 (EXP)64 (STARTEXP)5

	//(TITLE)03PanoramaCombination
	unsigned char Combination[] = {0x78,0x03};
	// Combination status check
	unsigned char Combination_Status[] = {0x7A};// (WAIT)10 (RETRY)100 (EXP)64
#if 1
	//(TITLE)04PanoramaImageOut
	// Jpeg Convert
	unsigned char Convert1[] = {0x90,0x00,0x0B,0x07,0x78,0x05,0x05,0x01};
	unsigned char Convert2[] = {0x92,0x00};
	// Jpeg compress status check
	unsigned char Convert_Status[] ={0x6C};//(WAIT)10 (RETRY)100 (EXP)00
	//unsigned char Convert3[] = {0x93};

	// output image
	unsigned char ImageOut[] = {0x67,0x01,0x01,0x00,0x00};
	unsigned char ImageOutEnd[] = {0x68};// (EXT)jpg
	v4l_info(client, "%s:imx073_s_Panoroma\n", __func__);
#endif
#if 0
	err = imx073_i2c_write(sd, Change_EV1, sizeof(Change_EV1));
	if (err < 0)
		v4l_info(client, "%s:Change_EV1 write failed\n", __func__);

	err = imx073_i2c_write(sd, Change_EV2, sizeof(Change_EV2));
	if (err < 0)
		v4l_info(client,  "%s:Change_EV2 write failed\n", __func__);

	//Change_EV_Status
	retrycount = 0;
Check_EV_Status:
	err = imx073_i2c_read(sd, Change_EV_Status, sizeof(Change_EV_Status), res_buff, 1);
	if((res_buff[0] != 0x00)&&(retrycount < CMD_DELAY_LONG)){
		mdelay(10);
		retrycount++;
		goto Check_EV_Status;
	};
#endif
	v4l_info(client,  "%s:Command_ID write \n", __func__);

	err = imx073_i2c_write(sd, Command_ID, sizeof(Command_ID));
	if (err < 0)
		v4l_info(client, "%s: Command_ID write failed\n", __func__);

	err = imx073_i2c_write(sd, Preview_Start, sizeof(Preview_Start));
	if (err < 0)
		v4l_info(client, "%s:Preview_Start write failed\n", __func__);

//Response_Status
	retrycount = 0;
Check_Response_Status:
	err = imx073_i2c_read(sd, Response_Status, sizeof(Response_Status), res_buff, 1);
	if (err < 0)
		v4l_info(client, "%s:Response_Status read failed\n", __func__);

	if((res_buff[0] != 0x08)&&(retrycount < CMD_DELAY_LONG)){
		mdelay(10);
		retrycount++;
		v4l_info(client, "%s:Response_Status  read %d times\n", __func__,retrycount);

		goto Check_Response_Status;
	};

// This sequence start need push botton. (Panorama Capture start)
	err = imx073_i2c_write(sd, PanoramaCapture, sizeof(PanoramaCapture));
	if (err < 0)
		v4l_info(client, "%s:PanoramaCapture write failed\n", __func__);

//	mdelay(66);


//PanoramaCapture_Status
	do{
		err = imx073_i2c_read(sd, PanoramaCapture_Status, sizeof(PanoramaCapture_Status), res_buff, 1);
		if(res_buff[0]!=0x00) break;

		if((res_buff[5] != 0x64)&&(retrycount < 500)){
			mdelay(20);
			retrycount++;
		}
		else
			break;
	}while(1);

	err = imx073_i2c_write(sd, Combination, sizeof(Combination));
	if (err < 0)
		v4l_info(client, "%s: Combination write failed\n", __func__);
#if 1
//Combination_Status
	retrycount = 0;
Check_Combination_Status:
	err = imx073_i2c_read(sd, Combination_Status, sizeof(Combination_Status), res_buff, 1);
	if((res_buff[0] != 0x64)&&(retrycount < CMD_DELAY_LONG)){
		mdelay(10);
		retrycount++;
		goto Check_Combination_Status;
	};

	err = imx073_i2c_write(sd, Convert1, sizeof(Convert1));
	if (err < 0)
		v4l_info(client, "%s:Convert1 write failed\n", __func__);
	err = imx073_i2c_write(sd, Convert2, sizeof(Convert2));
	if (err < 0)
		v4l_info(client, "%s:Convert2 write failed\n", __func__);
// H.Matsuzaki 
//	err = imx073_i2c_write(sd, Convert1, sizeof(Convert1));
//	if (err < 0)
//		v4l_info(client, "%s:Convert2 write failed\n", __func__);

//Convert_Status
	retrycount = 0;
Check_Convert_Status:
	err = imx073_i2c_read(sd, Convert_Status, sizeof(Convert_Status), res_buff, 1);
	if((res_buff[0] != 0x00)&&(retrycount < CMD_DELAY_LONG)){
		mdelay(10);
		retrycount++;
		goto Check_Convert_Status;
	};

//	err = imx073_i2c_write(sd, Convert3, sizeof(Convert3));
//	if (err < 0)
//		v4l_info(client, "%s:Convert3 write failed\n", __func__);

// Start output Image
	err = imx073_i2c_write(sd, ImageOut, sizeof(ImageOut));
	if (err < 0)
		v4l_info(client, "%s:ImageOut write failed\n", __func__);

	err = imx073_i2c_write(sd, ImageOutEnd, sizeof(ImageOutEnd));
	if (err < 0)
		v4l_info(client, "%s: ImageOutEnd write failed\n", __func__);

#endif
	return err;
}

static int imx073_s_SmileDetection(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int retrycount = 0;
	int err = -EINVAL;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	//struct imx073_state *state = to_state(sd);
	int value = ctrl->value;

	//SmileDetection
	// Frame Drawing Setting
	// 1st 4AH Command ID
	// 2nd 00H select mode (00:Drawing of Preview)
	// 3rd 01H drawing mode setting (00 preview asist function)
	// 4 to 11 Reserved
	unsigned char Smile_Drawing[] = {0x4A,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

	// Face Detection Setting
	// 1st 41H Command ID
	// 2nd 00H Select Face Detection setting
	// 3rd 0AH People Number
	// 4th 03H AF and AE ON
	// 5th 00H Mode Select (Normal)
	// 6th 00H Search direction (00H:Up->left->right->down)
	// 7th 00H minimum size (00H:20pixel)
	// 8 to 11 Reserved
	unsigned char Smile_Setting1[] = {0x41,0x00,0x0A,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

	// Smile Detection Setting
	// 1st 41H Command ID
	// 2nd 03H Select Smile detection setting
	// 3rd 0AH People Number 
	// 4 to 11 Reserved
	unsigned char Smile_Setting2[] = {0x41,0x03,0x0A,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

	// Smile Start 
	unsigned char Smile_Start[] = {0x42,0x06};
	unsigned char Smile_Off[] = {0x42,0x00};

	switch(value)
	{
		case SMILE_ON:
			v4l_info(client,  "%s:Command_ID write \n", __func__);

			err = imx073_i2c_write(sd, Smile_Drawing, sizeof(Smile_Drawing));
			if (err < 0)
				v4l_info(client, "%s: Smile_Drawing write failed\n", __func__);

			err = imx073_i2c_write(sd, Smile_Setting1, sizeof(Smile_Setting1));
			if (err < 0)
				v4l_info(client, "%s:Smile_Setting1 write failed\n", __func__);

			retrycount = 0;
			err = imx073_i2c_write(sd, Smile_Setting2, sizeof(Smile_Setting2));
			if (err < 0)
				v4l_info(client, "%s:Smile_Setting2 write failed\n", __func__);

			err = imx073_i2c_write(sd, Smile_Start, sizeof(Smile_Start));
			if (err < 0)
				v4l_info(client, "%s: Smile_Start write failed\n", __func__);
				break;
		case SMILE_OFF:
		err = imx073_i2c_write(sd, Smile_Off, sizeof(Smile_Off));
		if (err < 0)
			v4l_info(client, "%s: Smile_Off write failed\n", __func__);
		break;

	}

	return err;
}

static int imx073_s_Blink(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	//int retrycount = 0;
	int err = -EINVAL;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	//struct imx073_state *state = to_state(sd);
	unsigned char Blink_Setting[] = {0x47,0x00,0x01};
	unsigned char Blink_Off[] = {0x47,0x00};
	int value =ctrl->value;

	switch(value)
	{
		case BLINK_ON:
			
		err = imx073_i2c_write(sd, Blink_Setting, sizeof(Blink_Setting));
		if (err < 0)
			v4l_info(client, "%s: Blink_Setting write failed\n", __func__);

		break;

		case BLINK_OFF:
		default:
		err = imx073_i2c_write(sd, Blink_Off, sizeof(Blink_Off));
		if (err < 0)
			v4l_info(client, "%s: Blink_Off write failed\n", __func__);
		break;
	}
	
	return err;
}

static int imx073_g_Blink(struct v4l2_subdev *sd/*, struct v4l2_control *ctrl*/)
{
	//int retrycount = 0;
	int err = -EINVAL;
	int i = 0;
	unsigned char Rate[24] = {0};
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	//struct imx073_state *state = to_state(sd);

	// Smile Start 
	unsigned char Blink_Rate[] = {0x46};

	v4l_info(client,  "%s:Blink_Rate write \n", __func__);

	err = imx073_i2c_read(sd, Blink_Rate, sizeof(Blink_Rate), Rate, 21);

	if (err < 0)
		v4l_info(client, "%s: Blink_Rate write failed\n", __func__);
	else
	{
		//memcpy(ctrl->value,Rate,21);
		for( i = 0;i<21;i++)
		{
			v4l_info(client, "%s: Rate[%d] = %x\n", __func__,i,Rate[i]);
		}
	}
	return err;
}


static int imx073_s_LedLight(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	//struct i2c_client *client = v4l2_get_subdevdata(sd);
	//struct imx073_state *state = to_state(sd);

	imx073_clock();
	imx073_reset(sd,0);
	imx073_load_fw(sd);
#if 0	
	s3c_gpio_cfgpin(S5PV210_MP05(3), S3C_GPIO_INPUT);


	if(FLASH_LED_ON == ctrl->value)
	{
		s3c_gpio_setpull(S5PV210_MP05(3), S3C_GPIO_PULL_UP);	
	}
	else
	{
		s3c_gpio_setpull(S5PV210_MP05(3), S3C_GPIO_PULL_DOWN);	
	}
#endif	
	return 0;
}


static int imx073_s_ISO(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int retrycount = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	//struct imx073_state *state = to_state(sd);
	int err = 0;
	int value = ctrl->value;
	unsigned char ISO_Setting[]={0x04,0x01,0x00};//MM=00 is the default vaule
	unsigned char ISO_Run[]={0x01};
	unsigned char ISO_Status[]={0x02};
	unsigned char res[10] = {0};


	printk("%s:  value = %x\n", __func__, value);
	switch(value)
	{
		case ISO_50:
		ISO_Setting[2] = 0x01;
		break;
		case ISO_100:
		ISO_Setting[2] = 0x02;
		break;

		case ISO_200:
		ISO_Setting[2] = 0x03;
		break;
		
		case ISO_400:
		ISO_Setting[2] = 0x04;
		break;
		
		case ISO_800:
		ISO_Setting[2] = 0x05;
		break;
		
		case ISO_1600:
		ISO_Setting[2] = 0x06;
		break;
		
		default:
		ISO_Setting[2] = 0x00;//AUTO and others
		break;
	}


	err = imx073_i2c_write(sd, ISO_Setting, sizeof(ISO_Setting));
	if (err < 0)
		v4l_info(client, "%s: ISO_Setting write failed\n", __func__);

	err = imx073_i2c_write(sd, ISO_Run, sizeof(ISO_Run));
	if (err < 0)
		v4l_info(client, "%s: ISO_Run write failed\n", __func__);

ReadStatusbrightness:
	err = imx073_i2c_read(sd, ISO_Status, sizeof(ISO_Status), res, 1);
	if((res[0] != 0x00)&&(retrycount < CMD_DELAY_SHORT)){
		mdelay(10);
		retrycount++;
		goto ReadStatusbrightness;
	}
	else
		v4l_info(client, "%s: ISO Setting success res = 0x%x\n", __func__,res[0]);

	return err;
}

static int imx073_s_Saturation(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int retrycount = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	//struct imx073_state *state = to_state(sd);
	int err = 0;
	int value = ctrl->value;
	unsigned char STMode[]={0x3D,0x06,0x08};//MM=08 is the default vaule
	unsigned char ST_run[]={0x01};
	unsigned char ST_State[]={0x02};
	unsigned char res[10] = {0};

	v4l_info(client, "%s:  value = %x\n", __func__, value);

	STMode[2] = value;

	err = imx073_i2c_write(sd, STMode, sizeof(STMode));
	if (err < 0)
		v4l_info(client, "%s: STMode write failed\n", __func__);

	err = imx073_i2c_write(sd, ST_run, sizeof(ST_run));
	if (err < 0)
		v4l_info(client, "%s: ST_run write failed\n", __func__);

ReadST:
	err = imx073_i2c_read(sd, ST_State, sizeof(ST_State), res, 1);
	if((res[0] != 0x00)&&(retrycount < CMD_DELAY_SHORT)){
		mdelay(10);
		retrycount++;
		goto ReadST;
	}
	else
		v4l_info(client, "%s: ST setting success res = 0x%x\n", __func__,res[0]);

	return err;
}

static int imx073_s_JPGQuality(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	int retrycount = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	//struct imx073_state *state = to_state(sd);
	int err = 0;
	int value = ctrl->value;
	unsigned char Quality_Setting[]={0x90,0x00,0x00,0x00,0x00,0x00,0x01};//MM=00 is the default vaule
	unsigned char Run[]={0x92,0x00};
	unsigned char Status[]={0x6c};
	unsigned char res[10] = {0};


	printk("%s:  value = %x\n", __func__, value);
	switch(value/10*10)
	{
		case 100:
		Quality_Setting[2] = 0x60;
		Quality_Setting[3] = 0x09;
		Quality_Setting[4] = 0xD0;
		Quality_Setting[5] = 0x07;
		break;
		case 90:
		Quality_Setting[2] = 0xD7;
		Quality_Setting[3] = 0x07;
		Quality_Setting[4] = 0x40;
		Quality_Setting[5] = 0x06;
		break;

		case 80:
		Quality_Setting[2] = 0x40;
		Quality_Setting[3] = 0x09;
		Quality_Setting[4] = 0xB0;
		Quality_Setting[5] = 0x04;
		break;
		
		case 70:
		Quality_Setting[2] = 0xB0;
		Quality_Setting[3] = 0x04;
		Quality_Setting[4] = 0x20;
		Quality_Setting[5] = 0x03;
		break;
			
		default:
		Quality_Setting[2] = 0x60;
		Quality_Setting[3] = 0x09;
		Quality_Setting[4] = 0xD0;
		Quality_Setting[5] = 0x07;
		break;
	}


	err = imx073_i2c_write(sd, Quality_Setting, sizeof(Quality_Setting));
	if (err < 0)
		v4l_info(client, "%s: Quality_Setting write failed\n", __func__);

	err = imx073_i2c_write(sd, Run, sizeof(Run));
	if (err < 0)
		v4l_info(client, "%s: Run write failed\n", __func__);

ReadStatusJpgquality:
	err = imx073_i2c_read(sd, Status, sizeof(Status), res, 1);
	if((res[0] != 0x00)&&(retrycount < CMD_DELAY_SHORT)){
		mdelay(10);
		retrycount++;
		goto ReadStatusJpgquality;
	}
	else
		v4l_info(client, "%s:  Setting success res = 0x%x\n", __func__,res[0]);

	return err;
}

static int imx073_s_Antibanding(struct v4l2_subdev *sd, int value)
{
	//int retrycount = 0;
	int err = -EINVAL;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	//struct imx073_state *state = to_state(sd);
	unsigned char antibanding_Setting[] = {0x14,0x00};

	switch(value)
	{
		case ANTI_BANDING_AUTO:
			antibanding_Setting[1] =0x01;
			break;
		case ANTI_BANDING_50HZ:
			antibanding_Setting[1] =0x02;
			break;
		case ANTI_BANDING_60HZ:
			antibanding_Setting[1] =0x03;
			break;
		case ANTI_BANDING_OFF:
			antibanding_Setting[1] =0x00;
			break;
		default:
			break;
	}
	err = imx073_i2c_write(sd, antibanding_Setting, sizeof(antibanding_Setting));
	if (err < 0)
		v4l_info(client, "%s: antibanding_Setting write failed\n", __func__);

	return err;
}

static int imx073_s_Zoom(struct v4l2_subdev *sd, int value)
{
	int retrycount = 0;
	int err = -EINVAL;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	//struct imx073_state *state = to_state(sd);
	unsigned char Zoom_Setting[] = {0xb9,0x00};
	unsigned char Zoom_State[]={0xba};
	unsigned char res[10] = {0};

	if(value == 0)
		return 0;
	Zoom_Setting[1] = 256/value - 1;

	err = imx073_i2c_write(sd, Zoom_Setting, sizeof(Zoom_Setting));
	if (err < 0)
		v4l_info(client, "%s: Zoom_Setting write failed\n", __func__);

ReadZOOM:
	err = imx073_i2c_read(sd, Zoom_State, sizeof(Zoom_State), res, 1);
	if((res[0] != 0x00)&&(retrycount < CMD_DELAY_SHORT)){
		mdelay(10);
		retrycount++;
		goto ReadZOOM;
	}
	else
		v4l_info(client, "%s: ST setting success res = 0x%x\n", __func__,res[0]);

	return err;
}
static int FaceDetection_Setting(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	//int retrycount = 0;
	//struct i2c_client *client = v4l2_get_subdevdata(sd);
	//struct imx073_state *state = to_state(sd);
	int err = 0;
	int value = ctrl->value;

	printk("%s:  value = %x\n", __func__, value);
	switch(value)
	{
		case FACE_DETECTION_OFF:
		imx073_stop_FaceDetection(sd);
		break;

		case FACE_DETECTION_ON:
		imx073_start_FaceDetection(sd);
		break;

		
		default:
		imx073_stop_FaceDetection(sd);
		break;
	}

	return err;
}


static int AbtiShaking_Setting(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	//int retrycount = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	//struct imx073_state *state = to_state(sd);
	int err = 0;
	int value = ctrl->value;
	unsigned char AbtiShaking_Setting[]={0x5B,0x00};//MM=00 is the default vaule


	printk("%s:  value = %x\n", __func__, value);
	switch(value)
	{
		case ANTI_SHAKE_OFF:
		AbtiShaking_Setting[1] = 0x00;
		break;

		case ANTI_SHAKE_STILL_ON:
		AbtiShaking_Setting[1]  = 0x11;
		break;

		case ANTI_SHAKE_MOVIE_ON:
		AbtiShaking_Setting[1]  = 0x10;
		break;
		
		default:
		AbtiShaking_Setting[1] = 0x00;
		break;
	}


	err = imx073_i2c_write(sd, AbtiShaking_Setting, sizeof(AbtiShaking_Setting));
	if (err < 0)
		v4l_info(client, "%s: AbtiShaking_Setting write failed\n", __func__);

	return err;
}

static int WDB_setting(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	//int retrycount = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	//struct imx073_state *state = to_state(sd);
	int err = 0;
	int value = ctrl->value;
	unsigned char WDR_Setting[]={0x88,0x00};//MM=00 is the default vaule

	printk("%s:  value = %x\n", __func__, value);
	switch(value)
	{
		case WDR_OFF:
		WDR_Setting[1] = 0x00;
		break;

		case WDR_ON:
		WDR_Setting[1]  = 0x11;
		break;
		
		default:
		WDR_Setting[1] = 0x00;
		break;
	}


	err = imx073_i2c_write(sd, WDR_Setting, sizeof(WDR_Setting));
	if (err < 0)
		v4l_info(client, "%s:  write failed\n", __func__);

	return err;
}

static int TimeStamp_setting(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	//int retrycount = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	//struct imx073_state *state = to_state(sd);
	int err = 0;
	int value = ctrl->value;
	unsigned char TimeStamp_setting[]={0XC2,0x00,0x00,0x00,0x00,0x00,0x00};//MM=00 is the default vaule
/*
	printk("%s:  value = %x\n", __func__, value);
	switch(value)
	{
		case TIME_STAMP_OFF:
		TimeStamp_setting[1]  = 0x00;
		break;

		case TIME_STAMP_ON:
		TimeStamp_setting[1]  = 0x01;
		break;

		default:
		TimeStamp_setting[1] = 0x00;
		break;
	}
*/
	err = imx073_i2c_write(sd, TimeStamp_setting, sizeof(TimeStamp_setting));
	if (err < 0)
		v4l_info(client, "%s:  write failed\n", __func__);

	return err;
}

static int Flash_setting(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	//int retrycount = 0;
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	//struct imx073_state *state = to_state(sd);
	int err = 0;
	int value = ctrl->value;
	unsigned char init[]={0xB3,0x03,0x01,0x10,0x00};
	unsigned char setting[]={0xB2,0x03,0x01};

	printk("%s:  value = %x\n", __func__, value);
	switch(value)
	{
		case FLASH_MODE_OFF:
		setting[2]  = 0x00;
		break;

		case FLASH_MODE_ON:
		setting[2]  = 0x01;
		break;

		case FLASH_MODE_AUTO:
		setting[2]  = 0x02;
		break;

		case FLASH_MODE_TORCH:
//		setting[2]  = 0x01;
		break;

		default:
		setting[1] = 0x00;
		break;
	}

	err = imx073_i2c_write(sd, init, sizeof(init));
	if (err < 0)
		v4l_info(client, "%s:  write failed\n", __func__);

	err = imx073_i2c_write(sd, setting, sizeof(setting));
	if (err < 0)
		v4l_info(client, "%s:  write failed\n", __func__);

	return err;
}

static int imx073_s_ctrl(struct v4l2_subdev *sd, struct v4l2_control *ctrl)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct imx073_state *state = to_state(sd);
	int err = 0;
	int value = ctrl->value;

	mutex_lock(&state->i2c_lock);

	switch (ctrl->id) {
	case V4L2_CID_CAMERA_FLASH_MODE:
	Flash_setting(sd,ctrl);
	break;

	case V4L2_CID_CAMERA_FOCUS_MODE:
		dev_dbg(&client->dev, "%s: V4L2_CID_CAMERA_FOCUS_MODE  %d\n", __func__, value);
		state->focus_mode = value;
		//imx073_s_FocusMode(sd,ctrl);
	break;

	case V4L2_CID_CAMERA_FACE_DETECTION:
	FaceDetection_Setting(sd,ctrl);
	break;

	case V4L2_CID_CAMERA_BRIGHTNESS:
	imx073_s_Brightness(sd,ctrl);
	break;

	case V4L2_CID_CAMERA_SCENE_MODE:
	imx073_s_Scene(sd,ctrl);
	break;

	case V4L2_CID_CAMERA_ISO:
	imx073_s_ISO(sd,ctrl);
	break;

	case V4L2_CID_CAM_JPEG_QUALITY:
	imx073_s_JPGQuality(sd,ctrl);
	break;
    
	case V4L2_CID_CAMERA_WHITE_BALANCE:
	imx073_s_WB(sd,ctrl);
	break;

	case V4L2_CID_CAMERA_EFFECT:
	imx073_s_Effect(sd,ctrl);
	break;
/*
	case V4L2_CID_CAMERA_LEDONOFF:
	imx073_s_LedLight(sd,ctrl);
	break;

	case V4L2_CID_CAMERA_DOWNLOADFW:
	imx073_clock();
	imx073_reset(sd,0);
	imx073_s_res(sd,640);
	imx073_set_framerate(sd,15);
	imx073_repreview(sd);
	break;
*/
       case V4L2_CID_CAMERA_ANTI_SHAKE:
	AbtiShaking_Setting(sd,ctrl);
  	break;

	case V4L2_CID_CAMERA_WDR:
	WDB_setting(sd,ctrl);
	break;

	case V4L2_CID_CAMERA_GPS_TIMESTAMP:
	TimeStamp_setting(sd,ctrl);
	break;
/*
	case V4L2_CID_CAMERA_SMILEONOFF:
	imx073_s_SmileDetection(sd,ctrl);
	break;

	case V4L2_CID_CAMERA_BLINK_DETECTION:
	imx073_s_Blink(sd,ctrl);
	break;
*/	
	case V4L2_CID_CAMERA_METERING:
		break;
	case V4L2_CID_CAMERA_CONTRAST:
		imx073_s_Contrast(sd,ctrl);
		break;
	case V4L2_CID_CAMERA_SATURATION:
		imx073_s_Saturation(sd,ctrl);
		break;
	case V4L2_CID_CAMERA_SHARPNESS:
		dev_dbg(&client->dev, "%s: V4L2_CID_CAMERA_SHARPNESS\n", __func__);
		break;
	case V4L2_CID_CAMERA_SET_AUTO_FOCUS:
		if(state->focus_mode==FOCUS_MODE_AUTO)
		{
			if(value == AUTO_FOCUS_ON)
				imx073_s_Focus(sd,ctrl);
		}
		break;
	case V4L2_CID_CAMERA_GPS_LATITUDE:
	case V4L2_CID_CAMERA_GPS_LONGITUDE:
	case V4L2_CID_CAMERA_GPS_ALTITUDE:
	case V4L2_CID_CAMERA_OBJECT_POSITION_X:
	case V4L2_CID_CAMERA_OBJECT_POSITION_Y:
		break;
	case V4L2_CID_CAMERA_FRAME_RATE:
		state->fps = value;
		imx073_set_framerate(sd,value);
		break;
	case V4L2_CID_CAM_PREVIEW_ONOFF:
		if (state->check_previewdata == 0)
			err = 0;
		else
			err = -EIO;
		break;
	case V4L2_CID_CAMERA_CHECK_DATALINE:
	case V4L2_CID_CAMERA_CHECK_DATALINE_STOP:
		break;
	case V4L2_CID_CAMERA_RESET:
		dev_dbg(&client->dev, "%s: V4L2_CID_CAMERA_RESET\n", __func__);
		imx073_reset(sd,1);
		break;
	case V4L2_CID_EXPOSURE:
		dev_dbg(&client->dev, "%s: V4L2_CID_EXPOSURE\n",__func__);
		break;
	case V4L2_CID_CAMERA_ANTI_BANDING:
		dev_dbg(&client->dev, "%s: V4L2_CID_CAMERA_ANTI_BANDING\n", __func__);
		imx073_s_Antibanding(sd,value);
		break;
	case V4L2_CID_CAMERA_ZOOM:
		dev_dbg(&client->dev, "%s: V4L2_CID_CAMERA_ZOOM\n", __func__);
		imx073_s_Zoom(sd,value);
		break;

	default:
		dev_err(&client->dev, "%s: no such control id=%x, value=%d\n", __func__, ctrl->id,ctrl->value);
		/* err = -EINVAL; */
		break;
	}

	mutex_unlock(&state->i2c_lock);
	if (err < 0)
		dev_dbg(&client->dev, "%s: vidioc_s_ctrl failed\n", __func__);

	return err;

}


static int imx073_init(struct v4l2_subdev *sd, u32 val)
{
	struct imx073_state *state = to_state(sd);
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	state->userset.exposure_bias = 0;
	state->userset.ae_lock = 0;
	state->userset.awb_lock = 0;
	state->userset.auto_wb = 0;
	state->userset.manual_wb = 0;
	state->userset.wb_temp = 0;
	state->userset.effect = 0;
	state->userset.brightness= 0;
	state->userset.contrast = 0;
	state->userset.saturation = 0;
	state->userset.sharpness = 0;
	state->userset.focus_position = 25;
	state->userset.glamour = 0;
	state->userset.zoom = 0;
	state->userset.scene = 0;
	state->userset.fast_shutter = 0;

	state->initialized = STATE_INIT_PRIVEW;
	state->first_on = 1;
	state->state = STATE_CAPTURE_OFF;
	state->mode = CAMERA_PREVIEW;
	state->shutter = STATE_SHUTTER_OFF;


	v4l_info(client, "##########%s: camera initialization start, val=%d, preview width=%d, fps=%d\n", __func__,val,state->pix.width,state->fps);

	imx073_clock();
	imx073_reset(sd,val);
		
	if (val == 1)
		return 1;
		
	imx073_load_fw(sd);
	if(state->pix.width!=0)
		imx073_s_res(sd,state->pix.width);
	//if(state->fps ==15)	//only for vt mode
	//	imx073_set_framerate(sd,15);
	//imx073_repreview(sd);
	return 0;
}

/*
 * s_config subdev ops
 * With camera device, we need to re-initialize every single opening time
 * therefor,it is not necessary to be initialized on probe time.
 * except for version checking
 * NOTE: version checking is optional
 */
static int imx073_s_config(struct v4l2_subdev *sd, int irq, void *platform_data)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct imx073_state *state = to_state(sd);
	struct imx073_platform_data *pdata;

	dev_info(&client->dev, "fetching platform data\n");

	pdata = client->dev.platform_data;

	if (!pdata) {
		dev_err(&client->dev, "%s: no platform data\n", __func__);
		return -ENODEV;
	}

	/*
	 * Assign default format and resolution
	 * Use configured default information in platform data
	 * or without them, use default information in driver
	 */
	if (!(pdata->default_width && pdata->default_height)) {
		/* TODO: assign driver default resolution */
	} else {
		state->pix.width = pdata->default_width;
		state->pix.height = pdata->default_height;
	}

	if (!pdata->pixelformat)
		state->pix.pixelformat = DEFAULT_FMT;
	else
		state->pix.pixelformat = pdata->pixelformat;

	if (!pdata->freq)
		state->freq = 48000000;	/* 48MHz default */
	else
		state->freq = pdata->freq;

	if (!pdata->is_mipi) {
		state->is_mipi = 0;
		dev_info(&client->dev, "parallel mode\n");
	} else
		state->is_mipi = pdata->is_mipi;

	return 0;
}

#if 0
static int imx073_sleep(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = -EINVAL, i;

	v4l_info(client, "%s: sleep mode\n", __func__);
/*
	for (i = 0; i < IMX073_SLEEP_REGS; i++) {
		if (imx073_sleep_reg[i][0] == REG_DELAY) {
			mdelay(imx073_sleep_reg[i][1]);
			err = 0;
		} else {
			err = imx073_write(sd, imx073_sleep_reg[i][0], \
				imx073_sleep_reg[i][1]);
		}

		if (err < 0)
			v4l_info(client, "%s: register set failed\n", __func__);
	}

	if (err < 0) {
		v4l_err(client, "%s: sleep failed\n", __func__);
		return -EIO;
	}
*/
 	return 0;
}
#endif

static int imx073_wakeup(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	//int err = -EINVAL, i;

	v4l_info(client, "%s: wakeup mode\n", __func__);
/*
	for (i = 0; i < IMX073_WAKEUP_REGS; i++) {
		if (imx073_wakeup_reg[i][0] == REG_DELAY) {
			mdelay(imx073_wakeup_reg[i][1]);
			err = 0;
		} else {
			err = imx073_write(sd, imx073_wakeup_reg[i][0], \
				imx073_wakeup_reg[i][1]);
		}

		if (err < 0)
			v4l_info(client, "%s: register set failed\n", __func__);
	}

	if (err < 0) {
		v4l_err(client, "%s: wake up failed\n", __func__);
		return -EIO;
	}


	if (err < 0) {
		v4l_err(client, "%s: wake up failed\n", __func__);
		return -EIO;
	}
*/
	return 0;
}

static int imx073_stream_on(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = 0;
     struct imx073_state *state = to_state(sd);

	
	   dev_info(&client->dev, "%s: fmt width==%d,fmt height==%d\n", __func__,state->pix.width,state->pix.height);
//		state->pix.width = fmt->width;
//		state->pix.height = fmt->height;
//		state->pix.pixelformat = fmt->reserved[0];
	
	mutex_lock(&state->i2c_lock);
	switch(state->format_mode)
	{
        case  V4L2_PIX_FMT_MODE_PREVIEW:
			imx073_stop_preview(sd);
			imx073_s_res(sd,state->pix.width);
			imx073_repreview(sd);
			break;
	//		state->mode = CAMERA_PREVIEW;
		case V4L2_PIX_FMT_MODE_CAPTURE:	
//			imx073_s_captureres(sd,state->pix.width,state->pix.height);			
//			imx073_capture_init(sd);
			
			imx073_start_capture(sd);			
			break;
	}
	mutex_unlock(&state->i2c_lock);
	v4l_info(client, "%s: stream on\n", __func__);

	return err;
}

static int imx073_stream_off(struct v4l2_subdev *sd)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	int err = 0;
	imx073_stop_preview(sd);
	v4l_info(client, "%s:  stream off\n", __func__);
	return err;
}
static int imx073_s_stream(struct v4l2_subdev *sd, int enable)
{
#if 0
	return enable ? imx073_wakeup(sd) : imx073_sleep(sd);
#else
    	struct i2c_client *client = v4l2_get_subdevdata(sd);


	if(imx073_is_Initialized(sd))
	{
		dev_info(&client->dev, "%s: enable:%d finished\n", __func__,enable);
		if(enable)
		{
			imx073_stream_on(sd);
		}else{
			imx073_stream_off(sd);
		}
		return 0;
	}
	return -1;
#endif
}

static int imx073_s_gpio(struct v4l2_subdev *sd, u32 val)
{

	return 0;
}


static long imx073_ioctl(struct v4l2_subdev *sd, unsigned int cmd, void *arg)
{
	return 0;
}

static const struct v4l2_subdev_core_ops imx073_core_ops = {
	.init = imx073_init,	/* initializing API */
//	.s_config = imx073_s_config,	/* Fetch platform data */
	.queryctrl = imx073_queryctrl,
	.querymenu = imx073_querymenu,
	.g_ctrl = imx073_g_ctrl,
	.s_ctrl = imx073_s_ctrl,
	.load_fw = imx073_load_fw,
	.s_gpio = imx073_s_gpio,
	.reset = imx073_reset,
	.ioctl = imx073_ioctl,	
};

static const struct v4l2_subdev_video_ops imx073_video_ops = {
	.s_crystal_freq = imx073_s_crystal_freq,
	.g_mbus_fmt = imx073_g_fmt,
	.s_mbus_fmt = imx073_s_fmt,
	.enum_framesizes = imx073_enum_framesizes,
	.enum_frameintervals = imx073_enum_frameintervals,
	.enum_mbus_fmt = imx073_enum_fmt,
	.try_mbus_fmt = imx073_try_fmt,
	.g_parm = imx073_g_parm,
	.s_parm = imx073_s_parm,
 	.s_stream = imx073_s_stream,
};

static const struct v4l2_subdev_ops imx073_ops = {
	.core = &imx073_core_ops,
	.video = &imx073_video_ops,
};

/*
 * imx073_probe
 * Fetching platform data is being done with s_config subdev call.
 * In probe routine, we just register subdev device
 */
static int imx073_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct imx073_state *state;
	struct v4l2_subdev *sd;
	int ret;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);

	const struct imx073_platform_data *pdata =
		client->dev.platform_data;
	
/*
*cam_buck9, *cam_regulator5, *cam_regulator9, *cam_regulator17,\
					*cam_regulator24 , *cam_regulator26;
*/
	/* buck4 regulator on */
	printk("\t %s \n", __func__);
	cam_buck9 = regulator_get(NULL, "vdd11_isp_cam");
	if (IS_ERR(cam_buck9)) {
		printk(KERN_ERR "failed to get resource %s\n", "vdd11_isp_cam");
		return PTR_ERR(cam_buck9);
	}
	ret = regulator_enable(cam_buck9);
	if(ret < 0)
		printk(KERN_ERR "failed to enable %s\n", "vdd11_isp_cam");
	
#if 0
	/* ldo5 regulator on */
	cam_regulator5  = regulator_get(NULL, "vdd_ldo5");
	if (IS_ERR(cam_regulator5)) {
		printk(KERN_ERR "failed to get resource %s\n", "vdd_ldo5");
		return PTR_ERR(cam_regulator5);
	}
	ret = regulator_enable(cam_regulator5);
	if(ret < 0)
		printk(KERN_ERR "failed to enable %s\n", "vdd_ldo5");

	/* ldo9 regulator on */
	cam_regulator9  = regulator_get(NULL, "vdd_ldo9");
	if (IS_ERR(cam_regulator9)) {
		printk(KERN_ERR "failed to get resource %s\n", "vdd_ldo9");
		return PTR_ERR(cam_regulator9);
	}
	ret = regulator_enable(cam_regulator9);
	if(ret < 0)
		printk(KERN_ERR "failed to enable %s\n", "vdd_ldo9");

	/* ldo17 regulator on */
	cam_regulator17 = regulator_get(NULL, "vdd_ldo17");
	if (IS_ERR(cam_regulator17)) {
		printk(KERN_ERR "failed to get resource %s\n", "vdd_ldo17");
		return PTR_ERR(cam_regulator17);
	}
	ret = regulator_enable(cam_regulator17);
	if(ret < 0)
		printk(KERN_ERR "failed to enable %s\n", "vdd_ldo17");

	/* ldo24 regulator on */
	cam_regulator24= regulator_get(NULL, "vdd_ldo24");
	if (IS_ERR(cam_regulator24)) {
		printk(KERN_ERR "failed to get resource %s\n", "vdd_ldo24");
		return PTR_ERR(cam_regulator24);
	}
	ret = regulator_enable(cam_regulator24);
	if(ret < 0)
		printk(KERN_ERR "failed to enable %s\n", "vdd_ldo24");

	/* ldo26 regulator on */
		cam_regulator26 = regulator_get(NULL, "vdd_ldo26");
		if (IS_ERR(cam_regulator26)) {
			printk(KERN_ERR "failed to get resource %s\n", "vdd_ldo26");
			return PTR_ERR(cam_regulator26);
		}
		ret = regulator_enable(cam_regulator26);
		if(ret < 0)
			printk(KERN_ERR "failed to enable %s\n", "vdd_ldo26");

#endif

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(&adapter->dev,
			 "I2C-Adapter doesn't support I2C_FUNC_SMBUS_BYTE\n");
		return -EIO;
	}	
	
	s3c_gpio_cfgpin(EXYNOS4210_GPJ1(3), S3C_GPIO_SFN(2));

		
	state = kzalloc(sizeof(struct imx073_state), GFP_KERNEL);
	if (state == NULL)
		return -ENOMEM;

	sd = &state->sd;
	strcpy(sd->name, IMX073_DRIVER_NAME);
	mutex_init(&state->i2c_lock);
	if(pdata != NULL)
	{
		if (!(pdata->default_width && pdata->default_height)) {
				/* TODO: assign driver default resolution */
			} else {
				state->pix.width = pdata->default_width;
				state->pix.height = pdata->default_height;
			}
		
			if (!pdata->pixelformat)
				state->pix.pixelformat = DEFAULT_FMT;
			else
				state->pix.pixelformat = pdata->pixelformat;
		
			if (!pdata->freq)
				state->freq = 48000000; /* 48MHz default */
			else
				state->freq = pdata->freq;
		
			if (!pdata->is_mipi) {
				state->is_mipi = 0;
				dev_info(&client->dev, "parallel mode\n");
			} else
				state->is_mipi = pdata->is_mipi;

	}

	/* Registering subdev */
	v4l2_i2c_subdev_init(sd, client, &imx073_ops);

	dev_info(&client->dev, "imx073 has been probed\n");
	return 0;
}

static int imx073_remove(struct i2c_client *client)
{
	//u32 tmp;

	struct v4l2_subdev *sd = i2c_get_clientdata(client);
	
	v4l2_device_unregister_subdev(sd);
	kfree(to_state(sd));
#if 0
	/* buck9 regulator off */
	regulator_disable(cam_buck9);
	regulator_put(cam_buck9);
	/* ldo5 regulator off */
	regulator_disable(cam_regulator5);
	regulator_put(cam_regulator5);
	/* ldo9 regulator off */
	regulator_disable(cam_regulator9);
	regulator_put(cam_regulator9);
	/* ldo17 regulator off */
	regulator_disable(cam_regulator17);
	regulator_put(cam_regulator17);
	/* ldo13 regulator off */
	//regulator_disable(cam_regulator5);
	//regulator_put(cam_regulator5);
	/* ldo24 regulator off */
	regulator_disable(cam_regulator24);
	regulator_put(cam_regulator24);
	/* ldo26 regulator off */
	regulator_disable(cam_regulator26);
	regulator_put(cam_regulator26);
#endif

	s3c_gpio_cfgpin(EXYNOS4210_GPJ1(3), S3C_GPIO_INPUT);
	s3c_gpio_setpull(EXYNOS4210_GPJ1(3), S3C_GPIO_PULL_DOWN);

	s3c_gpio_cfgpin(EXYNOS4_GPF1(5), S3C_GPIO_INPUT);
	s3c_gpio_setpull(EXYNOS4_GPF1(5), S3C_GPIO_PULL_DOWN);
	
	s3c_gpio_cfgpin(EXYNOS4_GPF1(6), S3C_GPIO_INPUT);
	s3c_gpio_setpull(EXYNOS4_GPF1(6), S3C_GPIO_PULL_DOWN);

#if 0
	//s3c_gpio_cfgpin(S5PV210_GPH1(1), S3C_GPIO_INPUT);
	//s3c_gpio_setpull(S5PV210_GPH1(1), S3C_GPIO_PULL_DOWN);
	
	//s3c_gpio_cfgpin(S5PV210_GPH1(2), S3C_GPIO_INPUT);
	//s3c_gpio_setpull(S5PV210_GPH1(2), S3C_GPIO_PULL_DOWN);
	
	//s3c_gpio_cfgpin(S5PV210_GPH3(2), S3C_GPIO_INPUT);
	//s3c_gpio_setpull(S5PV210_GPH3(2), S3C_GPIO_PULL_DOWN);

    	s3c_gpio_cfgpin(S5PV210_GPH2(7), S3C_GPIO_INPUT);
	s3c_gpio_setpull(S5PV210_GPH2(7), S3C_GPIO_PULL_DOWN);
	
	s3c_gpio_cfgpin(S5PV210_GPH3(6), S3C_GPIO_INPUT);
	s3c_gpio_setpull(S5PV210_GPH3(6), S3C_GPIO_PULL_DOWN);
#endif

	return 0;
}
static int imx073_suspend(struct i2c_client *client, pm_message_t state)
{
	u32 tmp;
	
	/* buck9 regulator off */
	regulator_disable(cam_buck9);
	/* ldo5 regulator off */
	regulator_disable(cam_regulator5);
	/* ldo9 regulator off */
	regulator_disable(cam_regulator9);
	/* ldo17 regulator off */
	regulator_disable(cam_regulator17);
	/* ldo13 regulator off */
	//regulator_disable(cam_regulator5);
	/* ldo24 regulator off */
	regulator_disable(cam_regulator24);
	/* ldo26 regulator off */
	regulator_disable(cam_regulator26);

#if 0
	tmp=readl(S5PV210_GPE1CONPDN);
	writel(tmp & (~(0x3<<6)) | (0x2<<6), S5PV210_GPE1CONPDN);
	//s3c_gpio_cfgpin(S5PV210_GPE1(3), S3C_GPIO_INPUT);
	tmp=readl(S5PV210_GPE1PUDPDN);
	writel(tmp & (~(0x3<<6)) | (0x1<<6), S5PV210_GPE1PUDPDN);	
	//s3c_gpio_setpull(S5PV210_GPE1(3), S3C_GPIO_PULL_DOWN);
#endif
    s3c_gpio_cfgpin(EXYNOS4_GPF1(5), S3C_GPIO_INPUT);
	s3c_gpio_setpull(EXYNOS4_GPF1(5), S3C_GPIO_PULL_DOWN);	

	s3c_gpio_cfgpin(EXYNOS4_GPF1(6), S3C_GPIO_INPUT);
	s3c_gpio_setpull(EXYNOS4_GPF1(6), S3C_GPIO_PULL_DOWN);	

#if 0
	s3c_gpio_cfgpin(S5PV210_GPH1(1), S3C_GPIO_INPUT);
	s3c_gpio_setpull(S5PV210_GPH1(1), S3C_GPIO_PULL_DOWN);	
	
	s3c_gpio_cfgpin(S5PV210_GPH1(2), S3C_GPIO_INPUT);
	s3c_gpio_setpull(S5PV210_GPH1(2), S3C_GPIO_PULL_DOWN);	

	s3c_gpio_cfgpin(S5PV210_GPH3(2), S3C_GPIO_INPUT);
	s3c_gpio_setpull(S5PV210_GPH3(2), S3C_GPIO_PULL_DOWN);	

    	s3c_gpio_cfgpin(S5PV210_GPH2(7), S3C_GPIO_INPUT);
	s3c_gpio_setpull(S5PV210_GPH2(7), S3C_GPIO_PULL_DOWN);

	s3c_gpio_cfgpin(S5PV210_GPH3(6), S3C_GPIO_INPUT);
	s3c_gpio_setpull(S5PV210_GPH3(6), S3C_GPIO_PULL_DOWN);
#endif

	return 0;
}
static int imx073_resume(struct i2c_client *client)
{
	/* buck9 regulator on */
	regulator_enable(cam_buck9);
	/* ldo5 regulator on */
	regulator_enable(cam_regulator5);
	/* ldo9 regulator on */
	regulator_enable(cam_regulator9);
	/* ldo17 regulator on */
	regulator_enable(cam_regulator17);
	/* ldo13 regulator on */
	//regulator_enable(cam_regulator5);
	/* ldo24 regulator on */
	regulator_enable(cam_regulator24);
	/* ldo26 regulator on */
	regulator_enable(cam_regulator26);  
    
  	s3c_gpio_cfgpin(EXYNOS4_GPF1(5), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(EXYNOS4_GPF1(5), S3C_GPIO_PULL_NONE);
	gpio_set_value(EXYNOS4_GPF1(5), 0);	
	udelay(100);
	gpio_set_value(EXYNOS4_GPF1(5), 1);

    	s3c_gpio_cfgpin(EXYNOS4_GPF1(6), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(EXYNOS4_GPF1(6), S3C_GPIO_PULL_NONE);    
	gpio_set_value(EXYNOS4_GPF1(6), 0);	
	udelay(500);
	gpio_set_value(EXYNOS4_GPF1(6), 1);	
       
	return 0;
}

static void imx073_reset(struct v4l2_subdev *sd, u32 val)
{
	printk(KERN_ERR "########%s ######## (%d)\n",__func__,val);	
	// 8M reset
	s3c_gpio_cfgpin(EXYNOS4_GPF1(5), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(EXYNOS4_GPF1(5), S3C_GPIO_PULL_NONE);
    
	gpio_set_value(EXYNOS4_GPF1(5), 0);	
	udelay(500);
	gpio_set_value(EXYNOS4_GPF1(5), 1);	

//	if(val==0xff)
//		imx073_load_fw(sd);
	if(val==0xf7)
	{
		imx073_load_fw(sd);
		imx073_init_vga(sd);
		imx073_repreview(sd);
	}
}


int imx073_power(int isonoff)
{
	int err = -1;
	printk(KERN_ERR "########%s ########  %d \n",__func__,isonoff);
  #if 0  
	//0.3m cs to high
	s3c_gpio_cfgpin(S5PV210_GPH2(7), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(S5PV210_GPH2(7), S3C_GPIO_PULL_NONE);
	gpio_set_value(S5PV210_GPH2(7), 1);
	//0.3 reset to low
	s3c_gpio_cfgpin(S5PV210_GPH3(6), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(S5PV210_GPH3(6), S3C_GPIO_PULL_NONE);
	gpio_set_value(S5PV210_GPH3(6), 0);
	udelay(50);
#endif
	
	//8m cs
	s3c_gpio_cfgpin(EXYNOS4_GPF1(6), S3C_GPIO_OUTPUT);
	s3c_gpio_setpull(EXYNOS4_GPF1(6), S3C_GPIO_PULL_NONE);
       s3c_gpio_cfgpin(EXYNOS4_GPF0(3), S3C_GPIO_OUTPUT); 
       s3c_gpio_setpull(EXYNOS4_GPF0(3), S3C_GPIO_PULL_NONE);
	if(isonoff)
	{
	      gpio_set_value(EXYNOS4_GPF0(3), 1);

		gpio_set_value(EXYNOS4_GPF1(6), 0);	
		udelay(100);
		gpio_set_value(EXYNOS4_GPF1(6), 1);

		imx073powered = 1;
		//udelay(500);
	}else
	{
		gpio_set_value(EXYNOS4_GPF1(6), 0);
		imx073powered = 0;
	}

		
	return 0;
}

void imx073_clock(void)
{
#if 0

	unsigned int tempvalue = 0;
	printk(KERN_ERR "########%s ########\n",__func__);	
 
	tempvalue = readl(EXYNOS4_CLKDIV_CAM);  //fff04444  C520
        printk(KERN_ERR "########%x ########\n",tempvalue  );	
	
	tempvalue = (tempvalue &0xffff0fff);
	writel(tempvalue,EXYNOS4_CLKDIV_CAM);

	tempvalue = 0;
	tempvalue = readl(EXYNOS4_CLKSRC_CAM);  //11116666  C220
        printk(KERN_ERR "########%x ########\n",tempvalue  );	
	
	tempvalue = (tempvalue &0xffff0fff)|0x00010000;
	writel(tempvalue,EXYNOS4_CLKSRC_CAM);
#endif	

}



static const struct i2c_device_id imx073_id[] = {
	{ IMX073_DRIVER_NAME, 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, imx073_id);

static struct i2c_driver imx073_i2c_driver = {
	.driver = {
		.name	= IMX073_DRIVER_NAME,
	},
	.probe = imx073_probe,
	.remove = imx073_remove,
	.suspend = imx073_suspend,
	.resume = imx073_resume,
	.id_table = imx073_id,

};

static int __init imx073_mod_init(void)
{
	printk("++++++++++++++++++++++++++++++  imx mod init \n");
	return i2c_add_driver(&imx073_i2c_driver);
}

static void __exit imx073_mod_exit(void)
{
	printk("++++++++++++++++++++++++++++++imx073_mod_exit \n");

	i2c_del_driver(&imx073_i2c_driver);
}

module_init(imx073_mod_init);
module_exit(imx073_mod_exit);


MODULE_DESCRIPTION("Samsung Electronics IMX073 SXGA camera driver");
MODULE_AUTHOR("Li Meng<meng80.li@samsung.com>");
MODULE_LICENSE("GPL");

