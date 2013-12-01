/* linux/arch/arm/mach-exynos/mach-stuttgart.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/platform_device.h>
#include <linux/serial_core.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi_gpio.h>
#include <linux/clk.h>
#include <linux/lcd.h>
#include <linux/gpio.h>
#include <linux/gpio_event.h>
#include <linux/i2c.h>
#include <linux/pwm_backlight.h>
#include <linux/input.h>
#include <linux/mmc/host.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/max8649.h>
#include <linux/regulator/fixed.h>
#include <linux/mfd/max8997.h>
#include <linux/v4l2-mediabus.h>
#include <linux/memblock.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/reboot.h>

#define WRITEBACK_ENABLED
#define TEST_FLASH //Test flash torch,weichenli 2012.4.26

#if defined(CONFIG_S5P_MEM_CMA)
#include <linux/cma.h>
#endif
#ifdef CONFIG_ANDROID_PMEM
#include <linux/android_pmem.h>
#endif
#include <linux/i2c-gpio.h>
#ifdef CONFIG_SMM6260_MODEM
#include <mach/modem.h>
#endif
#include <asm/mach/arch.h>
#include <asm/mach-types.h>

#include <plat/regs-serial.h>
#include <plat/exynos4.h>
#include <plat/cpu.h>
#include <plat/clock.h>
#include <plat/keypad.h>
#include <plat/devs.h>
#include <plat/fb.h>
#include <plat/fb-s5p.h>
#include <plat/fb-core.h>
#include <plat/regs-fb-v4.h>
#include <plat/backlight.h>
#include <plat/gpio-cfg.h>
#include <plat/iic.h>
#include <plat/pd.h>
#include <plat/sdhci.h>
#include <plat/mshci.h>
#include <plat/ehci.h>
#include <plat/usbgadget.h>
#include <plat/s3c64xx-spi.h>
#if defined(CONFIG_VIDEO_FIMC)
#include <plat/fimc.h>
#elif defined(CONFIG_VIDEO_SAMSUNG_S5P_FIMC)
#include <plat/fimc-core.h>
#include <media/s5p_fimc.h>
#endif
#if defined(CONFIG_VIDEO_FIMC_MIPI)
#include <plat/csis.h>
#elif defined(CONFIG_VIDEO_S5P_MIPI_CSIS)
#include <plat/mipi_csis.h>
#endif
#include <plat/tvout.h>
#include <plat/media.h>
#include <plat/regs-srom.h>
#include <plat/sysmmu.h>
#include <plat/tv-core.h>
#include <media/s5k4ba_platform.h>
#include <media/s5k4ea_platform.h>
#include <media/exynos_flite.h>
#include <media/exynos_fimc_is.h>
#include <video/platform_lcd.h>
#include <media/m5mo_platform.h>
#include <media/m5mols.h>
#include <mach/map.h>
#include <mach/spi-clocks.h>
#include <mach/exynos-ion.h>
#include <mach/regs-pmu.h>
#ifdef CONFIG_TOUCHSCREEN_STUTTGART_ATMEL
#include <linux/atmel_qt602240.h>
#endif
#ifdef CONFIG_EXYNOS4_DEV_DWMCI
#include <linux/scatterlist.h>
#include <mach/dwmci.h>
#endif
#ifdef CONFIG_EXYNOS4_CONTENT_PATH_PROTECTION
#include <mach/secmem.h>
#endif
#include <mach/dev.h>
#include <mach/ppmu.h>
#if defined(CONFIG_VIDEO_SAMSUNG_S5P_MFC) || defined(CONFIG_VIDEO_MFC5X)
#include <plat/s5p-mfc.h>
#endif

#ifdef CONFIG_VIDEO_IMX073
#include<media/imx073_platform.h>
#endif

#ifdef CONFIG_FB_S5P_MIPI_DSIM
#include <mach/mipi_ddi.h>
#include <mach/dsim.h>
#include <../../../drivers/video/samsung/s3cfb.h>
#endif
#include <plat/fimg2d.h>
#include <mach/dev-sysmmu.h>

#ifdef CONFIG_VIDEO_SAMSUNG_S5P_FIMC
#include <plat/fimc-core.h>
#include <media/s5p_fimc.h>
#endif

#if defined(CONFIG_VIDEO_SAMSUNG_S5P_MFC)
#include <plat/s5p-mfc.h>
#endif

#ifdef CONFIG_VIDEO_JPEG_V2X
#include <plat/jpeg.h>
#endif

#ifdef CONFIG_REGULATOR_S5M8767
#include <linux/mfd/s5m87xx/s5m-core.h>
#include <linux/mfd/s5m87xx/s5m-pmic.h>
#endif

#ifdef CONFIG_REGULATOR_MAX77686
#include <linux/mfd/max77686.h>
#endif

#ifdef CONFIG_VIDEO_MT9M114
#include<media/mt9m114_platform.h>
#endif

#ifdef CONFIG_VIDEO_OV2659
#include<media/ov2659_platform.h>
#endif

int lcd_id;
#ifdef CONFIG_VIDEO_IMX073
extern int imx073_power(int isonoff);
#endif

#ifdef CONFIG_VIDEO_MT9M114
extern int mt9m114_power(int isonoff);
#endif

#ifdef CONFIG_VIDEO_OV2659
extern int ov2659_power(int isonoff);
#endif

//jeff
#ifdef CONFIG_KEYBOARD_GPIO_STUTTGART
#include <linux/input.h>
#include <linux/gpio_keys.h>
#endif

#ifdef CONFIG_BQ2415X_POWER
#include <linux/power/bq2415x_power.h>
#endif

#ifdef CONFIG_MAX8971_POWER
#include <linux/power/max8971-charger.h>
#endif

//jeff
#include <mach/stuttgart_gpio_cfg.h>

#ifdef CONFIG_EXYNOS_SETUP_THERMAL
#include <plat/s5p-tmu.h>
#endif

#ifdef CONFIG_VIDEO_IMX073
extern int imx073_power(int isonoff);
#endif

#define REG_INFORM4		(S5P_INFORM4)

/* Following are default values for UCON, ULCON and UFCON UART registers */
#define SMDK4X12_UCON_DEFAULT	(S3C2410_UCON_TXILEVEL |	\
				 S3C2410_UCON_RXILEVEL |	\
				 S3C2410_UCON_TXIRQMODE |	\
				 S3C2410_UCON_RXIRQMODE |	\
				 S3C2410_UCON_RXFIFO_TOI |	\
				 S3C2443_UCON_RXERR_IRQEN)

#define SMDK4X12_ULCON_DEFAULT	S3C2410_LCON_CS8

#define SMDK4X12_UFCON_DEFAULT	(S3C2410_UFCON_FIFOMODE |	\
				 S5PV210_UFCON_TXTRIG4 |	\
				 S5PV210_UFCON_RXTRIG4)

extern void herring_bt_uart_wake_peer(struct uart_port *port);

static struct s3c2410_uartcfg stuttgart_uartcfgs[] __initdata = {
	[0] = {
		.hwport		= 0,
		.flags		= 0,
		.ucon		= SMDK4X12_UCON_DEFAULT,
		.ulcon		= SMDK4X12_ULCON_DEFAULT,
		.ufcon		= SMDK4X12_UFCON_DEFAULT,
		.wake_peer	= herring_bt_uart_wake_peer,
	},
	[1] = {
		.hwport		= 1,
		.flags		= 0,
		.ucon		= SMDK4X12_UCON_DEFAULT,
		.ulcon		= SMDK4X12_ULCON_DEFAULT,
		.ufcon		= SMDK4X12_UFCON_DEFAULT,
	},
	[2] = {
		.hwport		= 2,
		.flags		= 0,
		.ucon		= SMDK4X12_UCON_DEFAULT,
		.ulcon		= SMDK4X12_ULCON_DEFAULT,
		.ufcon		= SMDK4X12_UFCON_DEFAULT,
	},
	[3] = {
		.hwport		= 3,
		.flags		= 0,
		.ucon		= SMDK4X12_UCON_DEFAULT,
		.ulcon		= SMDK4X12_ULCON_DEFAULT,
		.ufcon		= SMDK4X12_UFCON_DEFAULT,
	},
};




#ifdef CONFIG_EXYNOS_MEDIA_DEVICE
struct platform_device exynos_device_md0 = {
	.name = "exynos-mdev",
	.id = -1,
};
#endif


#if defined(CONFIG_VIDEO_FIMC) || defined(CONFIG_VIDEO_SAMSUNG_S5P_FIMC)
/*
 * External camera reset
 * Because the most of cameras take i2c bus signal, so that
 * you have to reset at the boot time for other i2c slave devices.
 * This function also called at fimc_init_camera()
 * Do optimization for cameras on your platform.
*/
#if defined(CONFIG_ITU_A) || defined(CONFIG_CSI_C) \
	|| defined(CONFIG_S5K3H2_CSI_C) || defined(CONFIG_S5K3H7_CSI_C) \
	|| defined(CONFIG_S5K6A3_CSI_C) \
	|| defined(CONFIG_S5P_S5K3H1_CSI_C) || defined(CONFIG_S5P_S5K3H2_CSI_C) \
	|| defined(CONFIG_S5P_S5K6A3_CSI_C)
static int stuttgart_cam0_reset(int dummy)
{
	int err;
	/* Camera A */

	err = gpio_request(EXYNOS4_GPF1(6), "GPF1"); //GPF1_6
	if (err)
		printk(KERN_ERR "#### failed to request GPF1_6 ####\n");
	
	s3c_gpio_cfgpin(EXYNOS4_GPF1(6), S3C_GPIO_SFN(1));
	s3c_gpio_setpull(EXYNOS4_GPF1(6), S3C_GPIO_PULL_NONE);
	gpio_direction_output(EXYNOS4_GPF1(6), 1);
	gpio_free(EXYNOS4_GPF1(6));

	err = gpio_request(EXYNOS4_GPF1(4), "GPF1"); //GPF1_4
	if (err)
		printk(KERN_ERR "#### failed to request GPF1_4 ####\n");
	
	s3c_gpio_cfgpin(EXYNOS4_GPF1(4), S3C_GPIO_SFN(1));
	s3c_gpio_setpull(EXYNOS4_GPF1(4), S3C_GPIO_PULL_NONE);
	gpio_direction_output(EXYNOS4_GPF1(4), 0);
	gpio_free(EXYNOS4_GPF1(4));

	msleep(5);
	err = gpio_request(EXYNOS4_GPF1(5), "GPF1"); //GPF1_5
	if (err)
		printk(KERN_ERR "#### failed to request GPF1_5 ####\n");
	
	s3c_gpio_cfgpin(EXYNOS4_GPF1(5), S3C_GPIO_SFN(1));
	s3c_gpio_setpull(EXYNOS4_GPF1(5), S3C_GPIO_PULL_NONE);
	gpio_direction_output(EXYNOS4_GPF1(5), 0);
	gpio_direction_output(EXYNOS4_GPF1(5), 1);
	gpio_free(EXYNOS4_GPF1(5));

	return 0;
}
#endif
#if defined(CONFIG_ITU_B) || defined(CONFIG_CSI_D) \
	|| defined(CONFIG_S5K3H2_CSI_D) || defined(CONFIG_S5K3H7_CSI_D) \
	|| defined(CONFIG_S5K6A3_CSI_D) \
	|| defined(CONFIG_S5P_S5K3H1_CSI_D) || defined(CONFIG_S5P_S5K3H2_CSI_D) \
	|| defined(CONFIG_S5P_S5K6A3_CSI_D)
static int stuttgart_cam1_reset(int dummy)
{
	int err;

	/* Camera B */
	err = gpio_request(EXYNOS4_GPX1(0), "GPX1");
	if (err)
		printk(KERN_ERR "#### failed to request GPX1_0 ####\n");

	s3c_gpio_setpull(EXYNOS4_GPX1(0), S3C_GPIO_PULL_NONE);
	gpio_direction_output(EXYNOS4_GPX1(0), 0);
	gpio_direction_output(EXYNOS4_GPX1(0), 1);
	gpio_free(EXYNOS4_GPX1(0));

	return 0;
}
#endif
#endif

#ifdef CONFIG_VIDEO_FIMC
#ifdef CONFIG_VIDEO_S5K4BA
static struct s5k4ba_platform_data s5k4ba_plat = {
	.default_width = 800,
	.default_height = 600,
	.pixelformat = V4L2_PIX_FMT_YUYV,
	.freq = 24000000,
	.is_mipi = 0,
};

static struct i2c_board_info s5k4ba_i2c_info = {
	I2C_BOARD_INFO("S5K4BA", 0x2d),
	.platform_data = &s5k4ba_plat,
};

static struct s3c_platform_camera s5k4ba = {
#ifdef CONFIG_ITU_A
	.id		= CAMERA_PAR_A,
	.clk_name	= "sclk_cam0",
	.i2c_busnum	= 4,
	.cam_power	= stuttgart_cam0_reset,
#endif
#ifdef CONFIG_ITU_B
	.id		= CAMERA_PAR_B,
	.clk_name	= "sclk_cam1",
	.i2c_busnum	= 5,
	.cam_power	= stuttgart_cam1_reset,
#endif
	.type		= CAM_TYPE_ITU,
	.fmt		= ITU_601_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_CBYCRY,
	.info		= &s5k4ba_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_YUYV,
	.srclk_name	= "xusbxti",
	.clk_rate	= 24000000,
	.line_length	= 1920,
	.width		= 1600,
	.height		= 1200,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 1600,
		.height	= 1200,
	},

	/* Polarity */
	.inv_pclk	= 0,
	.inv_vsync	= 1,
	.inv_href	= 0,
	.inv_hsync	= 0,
	.reset_camera	= 1,
	.initialized	= 0,
};
#endif
/* MIPI Camera */
#ifdef CONFIG_VIDEO_IMX073
static struct imx073_platform_data imx073_plat = {
	.default_width = 640,
	.default_height = 480,
	.pixelformat = V4L2_PIX_FMT_UYVY,
	.freq = 24000000,
	.is_mipi = 1,
};

static struct i2c_board_info  imx073_i2c_info = {
	I2C_BOARD_INFO("IMX073", (0x3c )),  //0x3c
	.platform_data = &imx073_plat,
//	.irq = IRQ_EINT(14),

};

static struct s3c_platform_camera imx073 = {

#ifdef CONFIG_CSI_C
	.id		= CAMERA_CSI_C,
	.clk_name	= "sclk_cam0",
//	.i2c_busnum	= 0,
//	.cam_power	= smdkv310_mipi_cam0_reset,
#endif
#ifdef CONFIG_CSI_D
	.id		= CAMERA_CSI_D,
	.clk_name	= "sclk_cam1",
//	.i2c_busnum	= 1,
//	.cam_power	= smdkv310_mipi_cam1_reset,
#endif
//	.id		= CAMERA_CSI_C,
	.type		= CAM_TYPE_MIPI,
	.fmt		= MIPI_CSI_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_CBYCRY,
	.i2c_busnum	= 6,
	.info		= &imx073_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_UYVY,
	.srclk_name	= "xusbxti",
//	.clk_name	= "sclk_cam0",
	.clk_rate	= 24000000,
	.line_length	= 1920,
	.width		= 640,
	.height		= 480,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 640,
		.height	= 480,
	},

	.mipi_lanes	= 2,
	.mipi_settle	= 8,
	.mipi_align	= 32,

	/* Polarity */
	.inv_pclk	= 0,
	.inv_vsync	= 1,
	.inv_href	= 0,
	.inv_hsync	= 0,
	.use_isp = false,


	.initialized	= 0,
	.cam_power	= imx073_power,
};


#endif
#ifdef CONFIG_VIDEO_MT9M114
static struct mt9m114_platform_data mt9m114_plat = {
	.default_width = 640,
	.default_height = 480,
	.pixelformat = V4L2_PIX_FMT_UYVY,
	.freq = 24000000,
	.is_mipi = 0,
};

static struct i2c_board_info  mt9m114_i2c_info = {
	I2C_BOARD_INFO("MT9M114", (0x90>>1)),  //0x3c
	.platform_data = &mt9m114_plat,
//	.irq = IRQ_EINT(14),

};

static struct s3c_platform_camera mt9m114 = {

#ifdef CONFIG_ITU_A
	.id		= CAMERA_PAR_A,
	.clk_name	= "sclk_cam0",
//	.i2c_busnum	= 0,
//	.cam_power	= smdkv310_mipi_cam0_reset,
#endif
#ifdef CONFIG_ITU_B
	.id		= CAMERA_PAR_B,
	.clk_name	= "sclk_cam1",
//	.i2c_busnum	= 1,
//	.cam_power	= smdkv310_mipi_cam1_reset,
#endif
//	.id		= CAMERA_CSI_C,
	.type		= CAM_TYPE_ITU,
	.fmt		= ITU_601_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_CBYCRY ,		//
	.i2c_busnum	= 6,
	.info		= &mt9m114_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_UYVY,
	.srclk_name	= "xusbxti",
//	.clk_name	= "sclk_cam0",
	.clk_rate	= 24000000,
	.line_length	= 1920,
	.width		= 640,
	.height		= 480,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 640,
		.height	= 480,
	},

#if 0
	/* Polarity */
	.inv_pclk	= 0,
	.inv_vsync	= 1,
	.inv_href	= 0,
	.inv_hsync	= 0,
	.reset_camera	= 1,
	.initialized	= 0,

#else
	.inv_pclk	= 0,
	.inv_vsync	= 0,
	.inv_href	= 0,
	.inv_hsync	= 1,
	.reset_camera	= 0,
	.initialized	= 0,

#endif
	.use_isp = false,
	.cam_power	= mt9m114_power,
};
#endif

#ifdef CONFIG_VIDEO_OV2659
static struct ov2659_platform_data ov2659_plat = {
	.default_width = 640,
	.default_height = 480,
	.pixelformat = V4L2_PIX_FMT_UYVY,
	.freq = 24000000,
	.is_mipi = 0,
};

static struct i2c_board_info  ov2659_i2c_info = {
	I2C_BOARD_INFO("OV2659", (0x60>>1)),  
	.platform_data = &ov2659_plat,
//	.irq = IRQ_EINT(14),

};

static struct s3c_platform_camera ov2659 = {

#ifdef CONFIG_ITU_A
	.id		= CAMERA_PAR_A,
	.clk_name	= "sclk_cam0",
//	.i2c_busnum	= 0,
//	.cam_power	= smdkv310_mipi_cam0_reset,
#endif
#ifdef CONFIG_ITU_B
	.id		= CAMERA_PAR_B,
	.clk_name	= "sclk_cam1",
//	.i2c_busnum	= 1,
//	.cam_power	= smdkv310_mipi_cam1_reset,
#endif
//	.id		= CAMERA_CSI_C,
	.type		= CAM_TYPE_ITU,
	.fmt		= ITU_601_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_CBYCRY ,		//
	.i2c_busnum	= 6,
	.info		= &ov2659_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_UYVY,
	.srclk_name	= "xusbxti",
//	.clk_name	= "sclk_cam0",
	.clk_rate	= 24000000,
	.line_length	= 1920,
	.width		= 800,
	.height		= 600,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 800,
		.height	= 600,
	},

#if 0
	/* Polarity */
	.inv_pclk	= 0,
	.inv_vsync	= 1,
	.inv_href	= 0,
	.inv_hsync	= 0,
	.reset_camera	= 1,
	.initialized	= 0,

#else
	.inv_pclk	= 0,
	.inv_vsync	= 0,
	.inv_href	= 0,
	.inv_hsync	= 1,
	.reset_camera	= 0,
	.initialized	= 0,

#endif
	.use_isp = false,
	.cam_power	= ov2659_power,
};
#endif


/* 2 MIPI Cameras */
#ifdef CONFIG_VIDEO_S5K4EA
static struct s5k4ea_platform_data s5k4ea_plat = {
	.default_width = 1920,
	.default_height = 1080,
	.pixelformat = V4L2_PIX_FMT_UYVY,
	.freq = 24000000,
	.is_mipi = 1,
};

static struct i2c_board_info s5k4ea_i2c_info = {
	I2C_BOARD_INFO("S5K4EA", 0x2d),
	.platform_data = &s5k4ea_plat,
};

static struct s3c_platform_camera s5k4ea = {
#ifdef CONFIG_CSI_C
	.id		= CAMERA_CSI_C,
	.clk_name	= "sclk_cam0",
	.i2c_busnum	= 4,
	.cam_power	= stuttgart_cam0_reset,
#endif
#ifdef CONFIG_CSI_D
	.id		= CAMERA_CSI_D,
	.clk_name	= "sclk_cam1",
	.i2c_busnum	= 5,
	.cam_power	= stuttgart_cam1_reset,
#endif
	.type		= CAM_TYPE_MIPI,
	.fmt		= MIPI_CSI_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_YCBYCR,
	.info		= &s5k4ea_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_UYVY,
	.srclk_name	= "xusbxti",
	.clk_rate	= 24000000,
	.line_length	= 1920,
	.width		= 1920,
	.height		= 1080,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 1920,
		.height	= 1080,
	},

	.mipi_lanes	= 2,
	.mipi_settle	= 12,
	.mipi_align	= 32,

	/* Polarity */
	.inv_pclk	= 0,
	.inv_vsync	= 1,
	.inv_href	= 0,
	.inv_hsync	= 0,

	.initialized	= 0,
};
#endif


#ifdef WRITEBACK_ENABLED
static struct i2c_board_info writeback_i2c_info = {
	I2C_BOARD_INFO("WriteBack", 0x0),
};

static struct s3c_platform_camera writeback = {
	.id		= CAMERA_WB,
	.fmt		= ITU_601_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_CBYCRY,
	.i2c_busnum	= 0,
	.info		= &writeback_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_YUV444,
	.line_length	= 800,
	.width		= 480,
	.height		= 800,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 480,
		.height	= 800,
	},

	.initialized	= 0,
};
#endif

#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
#ifdef CONFIG_VIDEO_S5K3H2
static struct i2c_board_info s5k3h2_sensor_info = {
	.type = "S5K3H2",
};

static struct s3c_platform_camera s5k3h2 = {
#ifdef CONFIG_S5K3H2_CSI_C
	.id		= CAMERA_CSI_C,
	.clk_name	= "sclk_cam0",
	.cam_power	= stuttgart_cam0_reset,
#endif
#ifdef CONFIG_S5K3H2_CSI_D
	.id		= CAMERA_CSI_D,
	.clk_name	= "sclk_cam1",
	.cam_power	= stuttgart_cam1_reset,
#endif
	.type		= CAM_TYPE_MIPI,
	.fmt		= MIPI_CSI_RAW10,
	.info		= &s5k3h2_sensor_info,
	.order422	= CAM_ORDER422_8BIT_YCBYCR,
	.pixelformat	= V4L2_PIX_FMT_UYVY,
	.line_length	= 1920,
	.width		= 1920,
	.height		= 1080,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 1920,
		.height	= 1080,
	},
	.srclk_name	= "xusbxti",
	.clk_rate	= 24000000,
	.mipi_lanes	= 2,
	.mipi_settle	= 12,
	.mipi_align	= 24,

	.initialized	= 0,
#ifdef CONFIG_S5K3H2_CSI_C
	.flite_id	= FLITE_IDX_A,
#endif
#ifdef CONFIG_S5K3H2_CSI_D
	.flite_id	= FLITE_IDX_B,
#endif
	.use_isp	= true,
#ifdef CONFIG_S5K3H2_CSI_C
	.sensor_index	= 1,
#endif
#ifdef CONFIG_S5K3H2_CSI_D
	.sensor_index	= 101,
#endif
};
#endif

#ifdef CONFIG_VIDEO_S5K3H7
static struct i2c_board_info s5k3h7_sensor_info = {
	.type = "S5K3H7",
};

static struct s3c_platform_camera s5k3h7 = {
#ifdef CONFIG_S5K3H7_CSI_C
	.id		= CAMERA_CSI_C,
	.clk_name	= "sclk_cam0",
	.cam_power	= stuttgart_cam0_reset,
#endif
#ifdef CONFIG_S5K3H7_CSI_D
	.id		= CAMERA_CSI_D,
	.clk_name	= "sclk_cam1",
	.cam_power	= stuttgart_cam1_reset,
#endif
	.type		= CAM_TYPE_MIPI,
	.fmt		= MIPI_CSI_RAW10,
	.info		= &s5k3h7_sensor_info,
	.order422	= CAM_ORDER422_8BIT_YCBYCR,
	.pixelformat	= V4L2_PIX_FMT_UYVY,
	.line_length	= 1920,
	.width		= 1920,
	.height		= 1080,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 1920,
		.height	= 1080,
	},
	.srclk_name	= "xusbxti",
	.clk_rate	= 24000000,
	.mipi_lanes	= 2,
	.mipi_settle	= 12,
	.mipi_align	= 24,

	.initialized	= 0,
#ifdef CONFIG_S5K3H7_CSI_C
	.flite_id	= FLITE_IDX_A,
#endif
#ifdef CONFIG_S5K3H7_CSI_D
	.flite_id	= FLITE_IDX_B,
#endif
	.use_isp	= true,
#ifdef CONFIG_S5K3H7_CSI_C
	.sensor_index	= 4,
#endif
#ifdef CONFIG_S5K3H7_CSI_D
	.sensor_index	= 104,
#endif
};
#endif
#ifdef CONFIG_VIDEO_S5K6A3
static struct s3c_platform_camera s5k6a3 = {
#ifdef CONFIG_S5K6A3_CSI_C
	.id		= CAMERA_CSI_C,
	.clk_name	= "sclk_cam0",
	.cam_power	= stuttgart_cam0_reset,
#endif
#ifdef CONFIG_S5K6A3_CSI_D
	.id		= CAMERA_CSI_D,
	.clk_name	= "sclk_cam1",
	.cam_power	= stuttgart_cam1_reset,
#endif
	.type		= CAM_TYPE_MIPI,
	.fmt		= MIPI_CSI_RAW10,
	.order422	= CAM_ORDER422_8BIT_YCBYCR,
	.pixelformat	= V4L2_PIX_FMT_UYVY,
	.line_length	= 1920,
	.width		= 1920,
	.height		= 1080,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 1920,
		.height	= 1080,
	},
	.srclk_name	= "xusbxti",
	.clk_rate	= 12000000,
	.mipi_lanes	= 1,
	.mipi_settle	= 12,
	.mipi_align	= 24,

	.initialized	= 0,
#ifdef CONFIG_S5K6A3_CSI_C
	.flite_id	= FLITE_IDX_A,
#endif
#ifdef CONFIG_S5K6A3_CSI_D
	.flite_id	= FLITE_IDX_B,
#endif
	.use_isp	= true,
#ifdef CONFIG_S5K6A3_CSI_C
	.sensor_index	= 2,
#endif
#ifdef CONFIG_S5K6A3_CSI_D
	.sensor_index	= 102,
#endif
};
#endif

#endif

/* legacy M5MOLS Camera driver configuration */
#ifdef CONFIG_VIDEO_M5MO
#define CAM_CHECK_ERR_RET(x, msg)	\
	if (unlikely((x) < 0)) { \
		printk(KERN_ERR "\nfail to %s: err = %d\n", msg, x); \
		return x; \
	}
#define CAM_CHECK_ERR(x, msg)	\
		if (unlikely((x) < 0)) { \
			printk(KERN_ERR "\nfail to %s: err = %d\n", msg, x); \
		}

static int m5mo_config_isp_irq(void)
{
	s3c_gpio_cfgpin(EXYNOS4_GPX2(6), S3C_GPIO_SFN(0xF));
	s3c_gpio_setpull(EXYNOS4_GPX2(6), S3C_GPIO_PULL_NONE);
	return 0;
}

static struct m5mo_platform_data m5mo_plat = {
	.default_width = 640, /* 1920 */
	.default_height = 480, /* 1080 */
	.pixelformat = V4L2_PIX_FMT_UYVY,
	.freq = 24000000,
	.is_mipi = 1,
	.config_isp_irq = m5mo_config_isp_irq,
	.irq = IRQ_EINT(22),
};

static struct i2c_board_info m5mo_i2c_info = {
	I2C_BOARD_INFO("M5MO", 0x1F),
	.platform_data = &m5mo_plat,
	.irq = IRQ_EINT(22),
};

static struct s3c_platform_camera m5mo = {
#ifdef CONFIG_CSI_C
	.id		= CAMERA_CSI_C,
	.clk_name	= "sclk_cam0",
	.i2c_busnum	= 4,
	.cam_power	= stuttgart_cam0_reset,
#endif
#ifdef CONFIG_CSI_D
	.id		= CAMERA_CSI_D,
	.clk_name	= "sclk_cam1",
	.i2c_busnum	= 5,
	.cam_power	= stuttgart_cam1_reset,
#endif
	.type		= CAM_TYPE_MIPI,
	.fmt		= MIPI_CSI_YCBCR422_8BIT,
	.order422	= CAM_ORDER422_8BIT_YCBYCR,
	.info		= &m5mo_i2c_info,
	.pixelformat	= V4L2_PIX_FMT_UYVY,
	.srclk_name	= "xusbxti", /* "mout_mpll" */
	.clk_rate	= 24000000, /* 48000000 */
	.line_length	= 1920,
	.width		= 640,
	.height		= 480,
	.window		= {
		.left	= 0,
		.top	= 0,
		.width	= 640,
		.height	= 480,
	},

	.mipi_lanes	= 2,
	.mipi_settle	= 12,
	.mipi_align	= 32,

	/* Polarity */
	.inv_pclk	= 1,
	.inv_vsync	= 1,
	.inv_href	= 0,
	.inv_hsync	= 0,
	.reset_camera	= 0,
	.initialized	= 0,
};
#endif

/* Interface setting */
static struct s3c_platform_fimc fimc_plat = {
/*	
#ifdef CONFIG_ITU_A
	.default_cam	= CAMERA_PAR_A,
#endif
#ifdef CONFIG_ITU_B
	.default_cam	= CAMERA_PAR_B,
#endif
#ifdef CONFIG_CSI_C
	.default_cam	= CAMERA_CSI_C,
#endif
#ifdef CONFIG_CSI_D
	.default_cam	= CAMERA_CSI_D,
#endif */

#ifdef CONFIG_ITU_A
		.default_cam	= CAMERA_PAR_A,
#elif defined(CONFIG_ITU_B)
		.default_cam	= CAMERA_PAR_B,
	
#elif defined(CONFIG_CSI_C)
		.default_cam	= CAMERA_CSI_C,

#elif defined(CONFIG_CSI_D)
		.default_cam	= CAMERA_CSI_D,
#endif

#ifdef WRITEBACK_ENABLED
	.default_cam	= CAMERA_WB,
#endif
	.camera		= {
#ifdef CONFIG_VIDEO_IMX073 
		&imx073,
#endif
#ifdef CONFIG_VIDEO_S5K3H7
			&s5k3h7,
#endif
#ifdef CONFIG_VIDEO_OV2659
		&ov2659,
#endif


#ifdef CONFIG_VIDEO_MT9M114
		&mt9m114,
#endif

#ifdef CONFIG_VIDEO_S5K3H1
		&s5k3h1,
#endif
#ifdef CONFIG_VIDEO_S5K3H2
		&s5k3h2,
#endif

#ifdef CONFIG_VIDEO_S5K6A3
		&s5k6a3,
#endif

#ifdef WRITEBACK_ENABLED
                &writeback,
#endif
	},
	.hw_ver		= 0x51,
};
#endif /* CONFIG_VIDEO_FIMC */

/* for mainline fimc interface */
#ifdef CONFIG_VIDEO_SAMSUNG_S5P_FIMC
#ifdef WRITEBACK_ENABLED
struct writeback_mbus_platform_data {
	int id;
	struct v4l2_mbus_framefmt fmt;
};

static struct i2c_board_info __initdata writeback_info = {
	I2C_BOARD_INFO("writeback", 0x0),
};
#endif

#ifdef CONFIG_VIDEO_S5K4BA
static struct s5k4ba_mbus_platform_data s5k4ba_mbus_plat = {
	.id		= 0,
	.fmt = {
		.width	= 1600,
		.height	= 1200,
		/*.code	= V4L2_MBUS_FMT_UYVY8_2X8, */
		.code	= V4L2_MBUS_FMT_VYUY8_2X8,
	},
	.clk_rate	= 24000000UL,
#ifdef CONFIG_ITU_A
	.set_power	= stuttgart_cam0_reset,
#endif
#ifdef CONFIG_ITU_B
	.set_power	= stuttgart_cam1_reset,
#endif
};

static struct i2c_board_info s5k4ba_info = {
	I2C_BOARD_INFO("S5K4BA", 0x2d),
	.platform_data = &s5k4ba_mbus_plat,
};
#endif

/* 2 MIPI Cameras */
#ifdef CONFIG_VIDEO_S5K4EA
static struct s5k4ea_mbus_platform_data s5k4ea_mbus_plat = {
#ifdef CONFIG_CSI_C
	.id		= 0,
	.set_power = stuttgart_cam0_reset,
#endif
#ifdef CONFIG_CSI_D
	.id		= 1,
	.set_power = stuttgart_cam1_reset,
#endif
	.fmt = {
		.width	= 1920,
		.height	= 1080,
		.code	= V4L2_MBUS_FMT_VYUY8_2X8,
	},
	.clk_rate	= 24000000UL,
};

static struct i2c_board_info s5k4ea_info = {
	I2C_BOARD_INFO("S5K4EA", 0x2d),
	.platform_data = &s5k4ea_mbus_plat,
};
#endif

#ifdef CONFIG_VIDEO_M5MOLS
static struct m5mols_platform_data m5mols_platdata = {
#ifdef CONFIG_CSI_C
	.gpio_rst = EXYNOS4_GPX1(2), /* ISP_RESET */
#endif
#ifdef CONFIG_CSI_D
	.gpio_rst = EXYNOS4_GPX1(0), /* ISP_RESET */
#endif
	.enable_rst = true, /* positive reset */
	.irq = IRQ_EINT(22),
};

static struct i2c_board_info m5mols_board_info = {
	I2C_BOARD_INFO("M5MOLS", 0x1F),
	.platform_data = &m5mols_platdata,
};

#endif

#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
#ifdef CONFIG_VIDEO_S5K3H1
static struct i2c_board_info s5k3h1_sensor_info = {
	.type = "S5K3H1",
};
#endif
#ifdef CONFIG_VIDEO_S5K3H2
static struct i2c_board_info s5k3h2_sensor_info = {
	.type = "S5K3H2",
};
#endif
#ifdef CONFIG_VIDEO_S5K3H7
static struct i2c_board_info s5k3h7_sensor_info = {
	.type = "S5K3H7",
};
#endif
#ifdef CONFIG_VIDEO_S5K6A3
static struct i2c_board_info s5k6a3_sensor_info = {
	.type = "S5K6A3",
};
#endif
#endif
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_LITE
/* This is for platdata of fimc-lite */
#ifdef CONFIG_VIDEO_S5K3H2
static struct s3c_platform_camera s5k3h2 = {
	.type  = CAM_TYPE_MIPI,
	.use_isp = true,
	.inv_pclk = 0,
	.inv_vsync = 0,
	.inv_href = 0,
	.inv_hsync = 0,
};
#endif

#ifdef CONFIG_VIDEO_S5K3H7
static struct s3c_platform_camera s5k3h7 = {
	.type  = CAM_TYPE_MIPI,
	.use_isp = true,
	.inv_pclk = 0,
	.inv_vsync = 0,
	.inv_href = 0,
	.inv_hsync = 0,
};
#endif
#ifdef CONFIG_VIDEO_S5K6A3
static struct s3c_platform_camera s5k6a3 = {
	.type  = CAM_TYPE_MIPI,
	.use_isp = true,
	.inv_pclk = 0,
	.inv_vsync = 0,
	.inv_href = 0,
	.inv_hsync = 0,
};
#endif
#endif
#endif /* CONFIG_VIDEO_SAMSUNG_S5P_FIMC */

#ifdef CONFIG_S3C64XX_DEV_SPI
static struct s3c64xx_spi_csinfo spi0_csi[] = {
	[0] = {
		.line = EXYNOS4_GPB(1),
		.set_level = gpio_set_value,
		.fb_delay = 0x0,
	},
};

static struct spi_board_info spi0_board_info[] __initdata = {
	{
		.modalias = "spidev",
		.platform_data = NULL,
		.max_speed_hz = 10*1000*1000,
		.bus_num = 0,
		.chip_select = 0,
		.mode = SPI_MODE_0,
		.controller_data = &spi0_csi[0],
	}
};

#ifndef CONFIG_FB_S5P_LMS501KF03
static struct s3c64xx_spi_csinfo spi1_csi[] = {
	[0] = {
		.line = EXYNOS4_GPB(5),
		.set_level = gpio_set_value,
		.fb_delay = 0x2,
	},
};

static struct spi_board_info spi1_board_info[] __initdata = {
	{
		.modalias = "spidev",
		.platform_data = NULL,
		.max_speed_hz = 1200000,
		.bus_num = 1,
		.chip_select = 0,
		.mode = SPI_MODE_3,
		.controller_data = &spi1_csi[0],
	}
};
#endif

static struct s3c64xx_spi_csinfo spi2_csi[] = {
	[0] = {
		.line = EXYNOS4_GPC1(2),
		.set_level = gpio_set_value,
		.fb_delay = 0x0,
	},
};

static struct spi_board_info spi2_board_info[] __initdata = {
	{
		.modalias = "spidev",
		.platform_data = NULL,
		.max_speed_hz = 10*1000*1000,
		.bus_num = 2,
		.chip_select = 0,
		.mode = SPI_MODE_0,
		.controller_data = &spi2_csi[0],
	}
};
#endif

#ifdef CONFIG_GPSLENOVO
static struct platform_device stuttgart_gps_device = {
	.name = "gps_device",
};
#endif

#ifdef CONFIG_FB_S5P

static struct s3cfb_lcd stuttgart_mipi_lcd_boe = {
	.width = 720,
	.height = 1280,
	.bpp = 24,

	.freq = 60,

    .timing = {
        .h_fp = 0x20,
        .h_bp = 0x3b,
        .h_sw = 0x09,
        .v_fp = 0x0b,
        .v_fpe = 1, 
        .v_bp = 0x0b,
        .v_bpe = 1, 
        .v_sw = 0x02,     
        .cmd_allow_len = 0xf,
    },

    .polarity = {
        .rise_vclk = 0,
		.inv_hsync = 0,
		.inv_vsync = 0,
		.inv_vden = 0,
	},
};

static struct s3cfb_lcd stuttgart_mipi_lcd_lg = { 
	.width  = 720,
	.height = 1280,
	.bpp    = 24, 
	.freq   = 60, 

	.timing = { 
		.h_fp = 14, 
		.h_bp = 112,
		.h_sw = 4,
		.v_fp = 8, 
		.v_fpe = 1,
		.v_bp = 12, 
		.v_bpe = 1,
		.v_sw = 2,
		.cmd_allow_len = 0xf,
	},  

	.polarity = { 
		.rise_vclk = 0,
		.inv_hsync = 0,
		.inv_vsync = 0,
		.inv_vden = 0,
	},  
};

static struct s3c_platform_fb fb_platform_data __initdata = {
	.hw_ver		= 0x70,
	.clk_name	= "sclk_lcd",
	.nr_wins	= 5,
	.default_win	= CONFIG_FB_S5P_DEFAULT_WINDOW,
	.swap		= FB_SWAP_HWORD | FB_SWAP_WORD,
};

static void lcd_cfg_gpio(void)
{
	int err;
        err = gpio_request(EXYNOS4_GPD0(1), "GPD0");
        if (err)
                printk(KERN_ERR "request gpio GPX0(5) failure, err (%d)\n", err);
        gpio_direction_output(EXYNOS4_GPD0(1), 1);
        s3c_gpio_setpull(EXYNOS4_GPD0(1), S3C_GPIO_PULL_NONE);
        gpio_free(EXYNOS4_GPD0(1));

	return;
}

static int reset_lcd(void)
{
	int err = 0;
	/* fire nRESET on power off */
	return 0;
}

static int lcd_power_on(void *pdev, int enable)
{
	return 1;
}

#define ID_PIN EXYNOS4_GPX3(3)
static void gpio_cfg(void)
{
	int err;

	err = gpio_request(ID_PIN, "GPX");
	if (err)
		printk(KERN_ERR "#### failed to request GPX3_3 ####\n");
	s3c_gpio_setpull(ID_PIN, S3C_GPIO_PULL_UP);
	gpio_direction_output(ID_PIN, 1);
	mdelay(1);
	s3c_gpio_setpull(ID_PIN, S3C_GPIO_PULL_NONE);
	gpio_direction_input(ID_PIN);
	mdelay(5);
	lcd_id = gpio_get_value(ID_PIN);
	printk("********************** lcd_id = %s ***************************\n", lcd_id ? "LCD_LG" : "LCD_BOE");
	gpio_free(ID_PIN);
}
static void __init mipi_fb_init(void)
{
	struct s5p_platform_dsim *dsim_pd = NULL;
	struct mipi_ddi_platform_data *mipi_ddi_pd = NULL;
	struct dsim_lcd_config *dsim_lcd_info = NULL;

	/* gpio pad configuration for rgb and spi interface. */
//	lcd_cfg_gpio();

	/*
	 * register lcd panel data.
	 */
	gpio_cfg();
	dsim_pd = (struct s5p_platform_dsim *)
		s5p_device_dsim.dev.platform_data;

	strcpy(dsim_pd->lcd_panel_name, "stuttgart_mipi_lcd");

	dsim_lcd_info = dsim_pd->dsim_lcd_info;
	//dsim_lcd_info->lcd_panel_info = (void *)&stuttgart_mipi_lcd;
	//lcd_id = gpio_get_value(ID_PIN);
	if (lcd_id == 0) {
		//boe
		dsim_lcd_info->lcd_panel_info = (void *)&stuttgart_mipi_lcd_boe;
	} else {
		//lg
		dsim_lcd_info->lcd_panel_info = (void *)&stuttgart_mipi_lcd_lg;
	}

	mipi_ddi_pd = (struct mipi_ddi_platform_data *)
		dsim_lcd_info->mipi_ddi_pd;
	mipi_ddi_pd->lcd_reset = reset_lcd;
	mipi_ddi_pd->lcd_power_on = lcd_power_on;

	platform_device_register(&s5p_device_dsim);

	s3cfb_set_platdata(&fb_platform_data);
	printk(KERN_INFO "platform data of %s lcd panel has been registered.\n",
			dsim_pd->lcd_panel_name);
}
#endif

static int exynos4_notifier_call(struct notifier_block *this,
					unsigned long code, void *_cmd)
{
	int mode = 0;

	if ((code == SYS_RESTART) && _cmd)
	{
		/* reboot to recovery mode */
		if (!strcmp((char *)_cmd, "recovery")) {
			mode = 0xABABABAA;
			printk("reboot to recovery\n");
		}
		/* reboot to bootloader(fastboot mode) */
		if (!strcmp((char *)_cmd, "bootloader")) {
			mode = 0xABABABAB;
			printk("reboot to bootloader\n");
		}
	}

	__raw_writel(mode, REG_INFORM4);

	return NOTIFY_DONE;
}

static struct notifier_block exynos4_reboot_notifier = {
	.notifier_call = exynos4_notifier_call,
};

#ifdef CONFIG_EXYNOS4_DEV_DWMCI
static void exynos_dwmci_cfg_gpio(int width)
{
	unsigned int gpio;

	for (gpio = EXYNOS4_GPK0(0); gpio < EXYNOS4_GPK0(2); gpio++) {
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_NONE);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV2);
	}

	switch (width) {
	case 8:
		for (gpio = EXYNOS4_GPK1(3); gpio <= EXYNOS4_GPK1(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(4));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV2);
		}
	case 4:
		for (gpio = EXYNOS4_GPK0(3); gpio <= EXYNOS4_GPK0(6); gpio++) {
			s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
			s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
			s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV2);
		}
		break;
	case 1:
		gpio = EXYNOS4_GPK0(3);
		s3c_gpio_cfgpin(gpio, S3C_GPIO_SFN(3));
		s3c_gpio_setpull(gpio, S3C_GPIO_PULL_UP);
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV2);
	default:
		break;
	}
}

static struct dw_mci_board exynos_dwmci_pdata __initdata = {
	.num_slots		= 1,
	.quirks			= DW_MCI_QUIRK_BROKEN_CARD_DETECTION | DW_MCI_QUIRK_HIGHSPEED,
	.bus_hz			= 100 * 1000 * 1000,
	.caps			= MMC_CAP_UHS_DDR50 | MMC_CAP_1_8V_DDR | MMC_CAP_8_BIT_DATA,
	.detect_delay_ms	= 200,
	.hclk_name		= "dwmci",
	.cclk_name		= "sclk_dwmci",
	.cfg_gpio		= exynos_dwmci_cfg_gpio,
};
#endif

#ifdef CONFIG_S3C_DEV_HSMMC
static struct s3c_sdhci_platdata stuttgart_hsmmc0_pdata __initdata = {
	.cd_type		= S3C_SDHCI_CD_INTERNAL,
	.clk_type		= S3C_SDHCI_CLK_DIV_EXTERNAL,
#ifdef CONFIG_EXYNOS4_SDHCI_CH0_8BIT
	.max_width		= 8,
	.host_caps		= MMC_CAP_8_BIT_DATA,
#endif
};
#endif

#ifdef CONFIG_S3C_DEV_HSMMC1
static struct s3c_sdhci_platdata stuttgart_hsmmc1_pdata __initdata = {
	.cd_type		= S3C_SDHCI_CD_INTERNAL,
	.clk_type		= S3C_SDHCI_CLK_DIV_EXTERNAL,
};
#endif

#ifdef CONFIG_S3C_DEV_HSMMC2
static struct s3c_sdhci_platdata stuttgart_hsmmc2_pdata __initdata = {
	.cd_type		= S3C_MSHCI_CD_PERMANENT,
	.clk_type		= S3C_SDHCI_CLK_DIV_EXTERNAL,
#ifdef CONFIG_EXYNOS4_SDHCI_CH2_8BIT
	.max_width		= 8,
	.host_caps		= MMC_CAP_8_BIT_DATA,
#endif
};
#endif

#ifdef CONFIG_S3C_DEV_HSMMC3
static struct s3c_sdhci_platdata stuttgart_hsmmc3_pdata __initdata = {
	.cd_type		= S3C_SDHCI_CD_EXTERNAL,
	.clk_type		= S3C_SDHCI_CLK_DIV_EXTERNAL,
};
#endif

#ifdef CONFIG_S5P_DEV_MSHC
static struct s3c_mshci_platdata exynos4_mshc_pdata __initdata = {
	.cd_type		= S3C_MSHCI_CD_PERMANENT,
	.has_wp_gpio		= true,
	.wp_gpio		= 0xffffffff,
#if defined(CONFIG_EXYNOS4_MSHC_8BIT) && \
	defined(CONFIG_EXYNOS4_MSHC_DDR)
	.max_width		= 8,
	.host_caps		= MMC_CAP_8_BIT_DATA | MMC_CAP_1_8V_DDR |
				  MMC_CAP_UHS_DDR50,
#elif defined(CONFIG_EXYNOS4_MSHC_8BIT)
	.max_width		= 8,
	.host_caps		= MMC_CAP_8_BIT_DATA,
#elif defined(CONFIG_EXYNOS4_MSHC_DDR)
	.host_caps		= MMC_CAP_1_8V_DDR | MMC_CAP_UHS_DDR50,
#endif
};
#endif

#ifdef CONFIG_USB_EHCI_S5P
static struct s5p_ehci_platdata stuttgart_ehci_pdata;

static void __init stuttgart_ehci_init(void)
{
	struct s5p_ehci_platdata *pdata = &stuttgart_ehci_pdata;

	s5p_ehci_set_platdata(pdata);
}
#endif

#ifdef CONFIG_USB_OHCI_S5P
static struct s5p_ohci_platdata stuttgart_ohci_pdata;

static void __init stuttgart_ohci_init(void)
{
	struct s5p_ohci_platdata *pdata = &stuttgart_ohci_pdata;

	s5p_ohci_set_platdata(pdata);
}
#endif

/* USB GADGET */
#ifdef CONFIG_USB_GADGET
static struct s5p_usbgadget_platdata stuttgart_usbgadget_pdata;

static void __init stuttgart_usbgadget_init(void)
{
	struct s5p_usbgadget_platdata *pdata = &stuttgart_usbgadget_pdata;

	s5p_usbgadget_set_platdata(pdata);
}
#endif

static struct regulator_consumer_supply max8952_supply =
	REGULATOR_SUPPLY("vdd_mif", NULL);

static struct regulator_init_data max8952_init_data = {
	.constraints	= {
		.name		= "vdd_mif range",
		.min_uV		= 850000,
		.max_uV		= 1100000,
		.always_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE,
		.state_mem	= {
			.uV		= 1100000,
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &max8952_supply,
};

static struct max8649_platform_data exynos4_max8952_info = {
	.mode		= 1,	/* VID1 = 0, VID0 = 1 */
	.extclk		= 0,
	.ramp_timing	= MAX8649_RAMP_32MV,
	.regulator	= &max8952_init_data,
};

/* max8997 */
static struct regulator_consumer_supply max8997_buck1 =
	REGULATOR_SUPPLY("vdd_arm", NULL);

static struct regulator_consumer_supply max8997_buck2 =
	REGULATOR_SUPPLY("vdd_int", NULL);

static struct regulator_consumer_supply max8997_buck3 =
	REGULATOR_SUPPLY("vdd_g3d", NULL);

static struct regulator_consumer_supply __initdata ldo2_consumer =
	REGULATOR_SUPPLY("vdd_ldo2", NULL);

static struct regulator_consumer_supply __initdata ldo3_consumer =
	REGULATOR_SUPPLY("vdd_ldo3", NULL);

static struct regulator_consumer_supply __initdata ldo4_consumer =
	REGULATOR_SUPPLY("vdd_ldo4", NULL);

static struct regulator_consumer_supply __initdata ldo5_consumer =
	REGULATOR_SUPPLY("vdd_ldo5", NULL);

static struct regulator_consumer_supply __initdata ldo6_consumer =
	REGULATOR_SUPPLY("vdd_ldo6", NULL);

static struct regulator_consumer_supply __initdata ldo7_consumer =
	REGULATOR_SUPPLY("vdd_ldo7", NULL);

static struct regulator_consumer_supply __initdata ldo8_consumer =
	REGULATOR_SUPPLY("vdd_ldo8", NULL);

static struct regulator_consumer_supply __initdata ldo9_consumer =
	REGULATOR_SUPPLY("vdd_ldo9", NULL);

static struct regulator_consumer_supply __initdata ldo10_consumer =
	REGULATOR_SUPPLY("vdd_ldo10", NULL);

static struct regulator_consumer_supply __initdata ldo11_consumer =
	REGULATOR_SUPPLY("vdd_ldo11", NULL);

static struct regulator_consumer_supply __initdata ldo14_consumer =
	REGULATOR_SUPPLY("vdd_ldo14", NULL);

static struct regulator_consumer_supply __initdata ldo21_consumer =
	REGULATOR_SUPPLY("vdd_ldo21", NULL);

static struct regulator_init_data __initdata max8997_ldo2_data = {
	.constraints	= {
		.name		= "vdd_ldo2 range",
		.min_uV		= 1000000,
		.max_uV		= 1000000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &ldo2_consumer,
};

static struct regulator_init_data __initdata max8997_ldo3_data = {
	.constraints	= {
		.name		= "vdd_ldo3 range",
		.min_uV		= 1000000,
		.max_uV		= 1000000,
		.apply_uV	= 1,
		.always_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &ldo3_consumer,
};

static struct regulator_init_data __initdata max8997_ldo4_data = {
	.constraints	= {
		.name		= "vdd_ldo4 range",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &ldo4_consumer,
};

static struct regulator_init_data __initdata max8997_ldo5_data = {
	.constraints	= {
		.name		= "vdd_ldo5 range",
		.min_uV		= 1000000,
		.max_uV		= 1000000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &ldo5_consumer,
};

static struct regulator_init_data __initdata max8997_ldo6_data = {
	.constraints	= {
		.name		= "vdd_ldo6 range",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &ldo6_consumer,
};

static struct regulator_init_data __initdata max8997_ldo7_data = {
	.constraints	= {
		.name		= "vdd_ldo7 range",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &ldo7_consumer,
};

static struct regulator_init_data __initdata max8997_ldo8_data = {
	.constraints	= {
		.name		= "vdd_ldo8 range",
		.min_uV		= 3300000,
		.max_uV		= 3300000,
		.apply_uV	= 1,
		.always_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &ldo8_consumer,
};

static struct regulator_init_data __initdata max8997_ldo9_data = {
	.constraints	= {
		.name		= "vdd_ldo9 range",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &ldo9_consumer,
};

static struct regulator_init_data __initdata max8997_ldo10_data = {
	.constraints	= {
		.name		= "vdd_ldo10 range",
		.min_uV		= 1000000,
		.max_uV		= 1000000,
		.apply_uV	= 1,
		.always_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &ldo10_consumer,
};

static struct regulator_init_data __initdata max8997_ldo11_data = {
	.constraints	= {
		.name		= "vdd_ldo11 range",
		.min_uV		= 2800000,
		.max_uV		= 2800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &ldo11_consumer,
};

static struct regulator_init_data __initdata max8997_ldo14_data = {
	.constraints	= {
		.name		= "vdd_ldo14 range",
		.min_uV		= 1800000,
		.max_uV		= 1800000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &ldo14_consumer,
};

static struct regulator_init_data __initdata max8997_ldo21_data = {
	.constraints	= {
		.name		= "vdd_ldo21 range",
		.min_uV		= 1200000,
		.max_uV		= 1200000,
		.apply_uV	= 1,
		.always_on	= 1,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &ldo21_consumer,
};

static struct regulator_init_data __initdata max8997_buck1_data = {
	.constraints	= {
		.name		= "vdd_arm range",
		.min_uV		= 850000,
		.max_uV		= 1500000,
		.always_on	= 1,
		.boot_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &max8997_buck1,
};

static struct regulator_init_data __initdata max8997_buck2_data = {
	.constraints	= {
		.name		= "vdd_int range",
		.min_uV		= 850000,
		.max_uV		= 1000000,
		.always_on	= 1,
		.boot_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &max8997_buck2,
};

static struct regulator_init_data __initdata max8997_buck3_data = {
	.constraints	= {
		.name		= "vdd_g3d range",
		.min_uV		= 800000,
		.max_uV		= 1200000,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &max8997_buck3,
};

static struct max8997_regulator_data __initdata max8997_regulators[] = {
	{ MAX8997_LDO2, &max8997_ldo2_data, },
	{ MAX8997_LDO3, &max8997_ldo3_data, },
	{ MAX8997_LDO4, &max8997_ldo4_data, },
	{ MAX8997_LDO5, &max8997_ldo5_data, },
	{ MAX8997_LDO6, &max8997_ldo6_data, },
	{ MAX8997_LDO7, &max8997_ldo7_data, },
	{ MAX8997_LDO8, &max8997_ldo8_data, },
	{ MAX8997_LDO9, &max8997_ldo9_data, },
	{ MAX8997_LDO10, &max8997_ldo10_data, },
	{ MAX8997_LDO11, &max8997_ldo11_data, },
	{ MAX8997_LDO14, &max8997_ldo14_data, },
	{ MAX8997_LDO21, &max8997_ldo21_data, },
	{ MAX8997_BUCK1, &max8997_buck1_data, },
	{ MAX8997_BUCK2, &max8997_buck2_data, },
	{ MAX8997_BUCK3, &max8997_buck3_data, },
};

static struct max8997_platform_data __initdata exynos4_max8997_info = {
	.num_regulators = ARRAY_SIZE(max8997_regulators),
	.regulators     = max8997_regulators,

	.buck1_voltage[0] = 1250000, /* 1.25V */
	.buck1_voltage[1] = 1100000, /* 1.1V */
	.buck1_voltage[2] = 1100000, /* 1.1V */
	.buck1_voltage[3] = 1100000, /* 1.1V */
	.buck1_voltage[4] = 1100000, /* 1.1V */
	.buck1_voltage[5] = 1100000, /* 1.1V */
	.buck1_voltage[6] = 1000000, /* 1.0V */
	.buck1_voltage[7] = 950000, /* 0.95V */

	.buck2_voltage[0] = 1000000, /* 1.0V */
	.buck2_voltage[1] = 1000000, /* 1.0V */
	.buck2_voltage[2] = 950000, /* 0.95V */
	.buck2_voltage[3] = 900000, /* 0.9V */
	.buck2_voltage[4] = 1000000, /* 1.0V */
	.buck2_voltage[5] = 1000000, /* 1.0V */
	.buck2_voltage[6] = 950000, /* 0.95V */
	.buck2_voltage[7] = 900000, /* 0.9V */

	.buck5_voltage[0] = 1100000, /* 1.1V */
	.buck5_voltage[1] = 1100000, /* 1.1V */
	.buck5_voltage[2] = 1100000, /* 1.1V */
	.buck5_voltage[3] = 1100000, /* 1.1V */
	.buck5_voltage[4] = 1100000, /* 1.1V */
	.buck5_voltage[5] = 1100000, /* 1.1V */
	.buck5_voltage[6] = 1100000, /* 1.1V */
	.buck5_voltage[7] = 1100000, /* 1.1V */
};

#ifdef CONFIG_REGULATOR_S5M8767
/* S5M8767 Regulator */
static int s5m_cfg_irq(void)
{
	/* AP_PMIC_IRQ: EINT26 */
	s3c_gpio_cfgpin(EXYNOS4_GPX3(2), S3C_GPIO_SFN(0xF));
	s3c_gpio_setpull(EXYNOS4_GPX3(2), S3C_GPIO_PULL_UP);
	return 0;
}
static struct regulator_consumer_supply s5m8767_buck1_consumer =
	REGULATOR_SUPPLY("vdd_mif", NULL);

static struct regulator_consumer_supply s5m8767_buck2_consumer =
	REGULATOR_SUPPLY("vdd_arm", NULL);

static struct regulator_consumer_supply s5m8767_buck3_consumer =
	REGULATOR_SUPPLY("vdd_int", NULL);

static struct regulator_consumer_supply s5m8767_buck4_consumer =
	REGULATOR_SUPPLY("vdd_g3d", NULL);

static struct regulator_init_data s5m8767_buck1_data = {
	.constraints	= {
		.name		= "vdd_mif range",
		.min_uV		= 950000,
		.max_uV		= 1100000,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s5m8767_buck1_consumer,
};

static struct regulator_init_data s5m8767_buck2_data = {
	.constraints	= {
		.name		= "vdd_arm range",
		.min_uV		=  925000,
		.max_uV		= 1450000,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				  REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s5m8767_buck2_consumer,
};

static struct regulator_init_data s5m8767_buck3_data = {
	.constraints	= {
		.name		= "vdd_int range",
		.min_uV		=  900000,
		.max_uV		= 1200000,
		.apply_uV	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.uV		= 1100000,
			.mode		= REGULATOR_MODE_NORMAL,
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &s5m8767_buck3_consumer,
};

static struct regulator_init_data s5m8767_buck4_data = {
	.constraints	= {
		.name		= "vdd_g3d range",
		.min_uV		= 750000,
		.max_uV		= 1500000,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
				REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &s5m8767_buck4_consumer,
};

static struct s5m_regulator_data pegasus_regulators[] = {
	{ S5M8767_BUCK1, &s5m8767_buck1_data },
	{ S5M8767_BUCK2, &s5m8767_buck2_data },
	{ S5M8767_BUCK3, &s5m8767_buck3_data },
	{ S5M8767_BUCK4, &s5m8767_buck4_data },
};

static struct s5m_platform_data exynos4_s5m8767_pdata = {
	.device_type		= S5M8767X,
	.irq_base		= IRQ_BOARD_START,
	.num_regulators		= ARRAY_SIZE(pegasus_regulators),
	.regulators		= pegasus_regulators,
	.cfg_pmic_irq		= s5m_cfg_irq,

	.buck2_voltage[0]	= 1250000,
	.buck2_voltage[1]	= 1200000,
	.buck2_voltage[2]	= 1150000,
	.buck2_voltage[3]	= 1100000,
	.buck2_voltage[4]	= 1050000,
	.buck2_voltage[5]	= 1000000,
	.buck2_voltage[6]	=  950000,
	.buck2_voltage[7]	=  900000,

	.buck3_voltage[0]	= 1100000,
	.buck3_voltage[1]	= 1000000,
	.buck3_voltage[2]	= 950000,
	.buck3_voltage[3]	= 900000,
	.buck3_voltage[4]	= 1100000,
	.buck3_voltage[5]	= 1000000,
	.buck3_voltage[6]	= 950000,
	.buck3_voltage[7]	= 900000,

	.buck4_voltage[0]	= 1200000,
	.buck4_voltage[1]	= 1150000,
	.buck4_voltage[2]	= 1200000,
	.buck4_voltage[3]	= 1100000,
	.buck4_voltage[4]	= 1100000,
	.buck4_voltage[5]	= 1100000,
	.buck4_voltage[6]	= 1100000,
	.buck4_voltage[7]	= 1100000,

	.buck_default_idx	= 3,
	.buck_gpios[0]		= EXYNOS4_GPX2(3),
	.buck_gpios[1]		= EXYNOS4_GPX2(4),
	.buck_gpios[2]		= EXYNOS4_GPX2(5),

	.buck_ramp_delay        = 50,
	.buck2_ramp_enable      = true,
	.buck3_ramp_enable      = true,
	.buck4_ramp_enable      = true,
};
/* End of S5M8767 */
#endif



/* jeff,  add for STUTTGART, MAX77686*/
#ifdef CONFIG_REGULATOR_MAX77686
static struct regulator_consumer_supply max77686_buck1_consumer = 
	REGULATOR_SUPPLY("vdd_mif", NULL);

static struct regulator_consumer_supply max77686_buck2_consumer = 
	REGULATOR_SUPPLY("vdd_arm", NULL);

static struct regulator_consumer_supply max77686_buck3_consumer = 
	REGULATOR_SUPPLY("vdd_int", NULL);

static struct regulator_consumer_supply max77686_buck4_consumer =
	REGULATOR_SUPPLY("vdd_g3d", NULL);

static struct regulator_consumer_supply max77686_buck8_consumer = 
	REGULATOR_SUPPLY("vdd_emmc", NULL);

static struct regulator_consumer_supply max77686_buck9_consumer = 
	REGULATOR_SUPPLY("vdd11_isp_cam", NULL);



static struct regulator_consumer_supply __initdata max77686_ldo1_consumers[] = {
	REGULATOR_SUPPLY("vdd_ldo1", NULL),
};
static struct regulator_consumer_supply __initdata max77686_ldo2_consumers[] = {
	REGULATOR_SUPPLY("vdd_ldo2", NULL),
};
static struct regulator_consumer_supply __initdata max77686_ldo3_consumers[] = {
	REGULATOR_SUPPLY("vdd_ldo3", NULL),
};
static struct regulator_consumer_supply __initdata max77686_ldo4_consumers[] = {
	REGULATOR_SUPPLY("vdd_ldo4", NULL),
};
static struct regulator_consumer_supply __initdata max77686_ldo5_consumers[] = {
	REGULATOR_SUPPLY("vdd_ldo5", NULL),
};
static struct regulator_consumer_supply __initdata max77686_ldo6_consumers[] = {
	REGULATOR_SUPPLY("vdd_ldo6", NULL),
};
static struct regulator_consumer_supply __initdata max77686_ldo7_consumers[] = {
	REGULATOR_SUPPLY("vdd_ldo7", NULL),
};
static struct regulator_consumer_supply __initdata max77686_ldo8_consumers[] = {
	REGULATOR_SUPPLY("vdd_ldo8", NULL),
};
static struct regulator_consumer_supply __initdata max77686_ldo9_consumers[] = {
	REGULATOR_SUPPLY("vdd_ldo9", NULL),
};
static struct regulator_consumer_supply __initdata max77686_ldo10_consumers[] = {
	REGULATOR_SUPPLY("vdd_ldo10", NULL),
};
static struct regulator_consumer_supply __initdata max77686_ldo11_consumers[] = {
	REGULATOR_SUPPLY("vdd_ldo11", NULL),
};
static struct regulator_consumer_supply __initdata max77686_ldo12_consumers[] = {
	/*REGULATOR_SUPPLY("vdd_ldo12", NULL),*/
	REGULATOR_SUPPLY("vdd33_uotg", "s5p-ehci"),	/* ehci-s5p.c */	
};
static struct regulator_consumer_supply __initdata max77686_ldo13_consumers[] = {
	REGULATOR_SUPPLY("vdd_ldo13", NULL),
};
static struct regulator_consumer_supply __initdata max77686_ldo14_consumers[] = {
	REGULATOR_SUPPLY("vdd_ldo14", NULL),
};
static struct regulator_consumer_supply __initdata max77686_ldo15_consumers[] = {
	/*REGULATOR_SUPPLY("vdd_ldo15", NULL),*/
	REGULATOR_SUPPLY("vdd10_ush", "s5p-ehci"),	/* ehci-s5p.c */
};
static struct regulator_consumer_supply __initdata max77686_ldo16_consumers[] = {
	/*REGULATOR_SUPPLY("vdd_ldo16", NULL),*/
	REGULATOR_SUPPLY("vdd18_hsic", "s5p-ehci"),	/* ehci-s5p.c */
};
static struct regulator_consumer_supply __initdata max77686_ldo17_consumers[] = {
	REGULATOR_SUPPLY("vdd_ldo17", NULL),
};
static struct regulator_consumer_supply __initdata max77686_ldo18_consumers[] = {
	REGULATOR_SUPPLY("vdd_ldo18", NULL),
};
static struct regulator_consumer_supply __initdata max77686_ldo19_consumers[] = {
	REGULATOR_SUPPLY("vdd_ldo19", NULL),
};
static struct regulator_consumer_supply __initdata max77686_ldo20_consumers[] = {
	REGULATOR_SUPPLY("vdd_ldo20", NULL),
};
static struct regulator_consumer_supply __initdata max77686_ldo21_consumers[] = {
	REGULATOR_SUPPLY("vdd_ldo21", NULL),
};
static struct regulator_consumer_supply __initdata max77686_ldo22_consumers[] = {
	REGULATOR_SUPPLY("vdd_ldo22", NULL),
};		
static struct regulator_consumer_supply __initdata max77686_ldo23_consumers[] = {
	REGULATOR_SUPPLY("vdd_ldo23", NULL),
};
static struct regulator_consumer_supply __initdata max77686_ldo24_consumers[] = {
	REGULATOR_SUPPLY("vdd_ldo24", NULL),
};
static struct regulator_consumer_supply __initdata max77686_ldo25_consumers[] = {
	REGULATOR_SUPPLY("vdd_ldo25", NULL),
};
static struct regulator_consumer_supply __initdata max77686_ldo26_consumers[] = {
	REGULATOR_SUPPLY("vdd_ldo26", NULL),
};
static struct regulator_consumer_supply __initdata max77686_en32khz_cp_consumers[] = {
	REGULATOR_SUPPLY("gps_32khz", NULL),
};
static struct regulator_consumer_supply __initdata max77686_enp32khz_consumers[] = {
	REGULATOR_SUPPLY("wlan_32khz", NULL),
	REGULATOR_SUPPLY("bt_32khz", NULL),
};

#define REGULATOR_INIT(_ldo, _name, _min_uV, _max_uV, _always_on, _ops_mask,\
		_disabled) \
	static struct regulator_init_data __initdata max77686_##_ldo##_data = {		\
		.constraints = {					\
			.name	= _name,				\
			.min_uV = _min_uV,				\
			.max_uV = _max_uV,				\
			.always_on	= _always_on,			\
			.boot_on	= _always_on,			\
			.apply_uV	= 1,				\
			.valid_ops_mask = _ops_mask,			\
			.state_mem	= {				\
				.disabled	= _disabled,		\
				.enabled	= !(_disabled),		\
			}						\
		},							\
		.num_consumer_supplies = ARRAY_SIZE(max77686_##_ldo##_consumers),   \
		.consumer_supplies = &max77686_##_ldo##_consumers[0],  \
	};

#define REGULATOR_INIT_NO_CONSUMER(_ldo, _name, _min_uV, _max_uV, _always_on, _ops_mask,\
		_disabled) \
	static struct regulator_init_data __initdata max77686_##_ldo##_data = {		\
		.constraints = {					\
			.name	= _name,				\
			.min_uV = _min_uV,				\
			.max_uV = _max_uV,				\
			.always_on	= _always_on,			\
			.boot_on	= _always_on,			\
			.apply_uV	= 1,				\
			.valid_ops_mask = _ops_mask,			\
			.state_mem	= {				\
				.disabled	= _disabled,		\
				.enabled	= !(_disabled),		\
			}						\
		},							\
		.num_consumer_supplies = 0,	\
		.consumer_supplies = NULL,			\
	};

/*jeff, note, now set most regulator boot on state,*/

 //ldo1, VDD_ALIVE_AP -> VDD_ALIVE, 1.1v, always on
REGULATOR_INIT_NO_CONSUMER(ldo1, "vdd_ldo1 range", 1100000, 1100000, 1,
		REGULATOR_CHANGE_STATUS, 0);
//ldo2, VDDQ_M_AP -> VDDQ_M1/M2, 1.2v, sleep:off, ctrl by PWRREQ
REGULATOR_INIT_NO_CONSUMER(ldo2, "vdd_ldo2 range", 1200000, 1200000, 1,
		REGULATOR_CHANGE_STATUS, 0); 
//ldo3, VDDQ_EXT_AP -> VDD_IO_1V8, 1.8v, always on
REGULATOR_INIT_NO_CONSUMER(ldo3, "vdd_ldo3 range", 1800000, 1800000, 1,
		REGULATOR_CHANGE_STATUS, 0); 

//ldo4, VDD28_SD, 2.8v, always on, NC?
REGULATOR_INIT(ldo4, "vdd_ldo4 range", 2800000, 2800000, 1,
		REGULATOR_CHANGE_STATUS, 0); 
//ldo5, VDD18_CODEC, 1.8v, always on,
REGULATOR_INIT(ldo5, "vdd_ldo5 range", 1800000, 1800000,1,
		REGULATOR_CHANGE_STATUS, 0);
 //ldo6, VDD10_MPLL_AP -> VDD10_MPLL, 1.0v, sleep:off, ctrl by PWRREQ
REGULATOR_INIT(ldo6, "vdd_ldo6 range", 1000000, 1000000, 1,
		REGULATOR_CHANGE_STATUS, 0);
//ldo7, VDD10_PLL_SUB_AP -> VDD10_VPLL/APLL/EPLL, 1.0v, sleep:off, ctrl by PWRREQ		
REGULATOR_INIT(ldo7, "vdd_ldo7 range", 1000000, 1000000, 1,
		REGULATOR_CHANGE_STATUS, 0); 
 //ldo8, VDD10_MIPI_HDMI_AP -> VDD10_MIPI_PLL/HDMI_PLL, 1.0v, sleep:off, ctrl by PWRREQ
REGULATOR_INIT_NO_CONSUMER(ldo8, "vdd_ldo8 range", 1000000, 1000000, 1,
		REGULATOR_CHANGE_STATUS, 0);
 //ldo9, VDD18_ISP_CAM, 1.8v, sleep:off
REGULATOR_INIT(ldo9, "vdd_ldo9 range", 1800000, 1800000, 0,
		REGULATOR_CHANGE_STATUS, 1);
//ldo10, VDD18_HDMI_MIPI_AP -> VDD18_HDMI_OSC/TS/MIPI, 1.8v, sleep:off, ctrl by PWRREQ		
REGULATOR_INIT(ldo10, "vdd_ldo10 range", 1800000, 1800000, 1,
		REGULATOR_CHANGE_STATUS, 0); 
//ldo11, VDD18_ABB1_AP -> VDD18_ABB, 1.8v,  sleep : ctrl by PWRREQ
REGULATOR_INIT(ldo11, "vdd_ldo11 range", 1800000, 1800000, 1,
		REGULATOR_CHANGE_STATUS, 0); 
 //ldo12, VDD33_UOTG_AP -> VDD33_UOTG, 3.3v,  sleep:off,
REGULATOR_INIT(ldo12, "vdd_ldo12 range", 3300000, 3300000, 0,
		REGULATOR_CHANGE_STATUS, 0); //jeff, need check
//ldo13, VDDQ_C2C_AP -> VDDQ_C2C, 1.8v. ?
REGULATOR_INIT(ldo13, "vdd_ldo13 range", 1800000, 1800000, 0,
		REGULATOR_CHANGE_STATUS, 1); 
//ldo14, VDD18_ABB0/2_AP -> VDD18_ABB0/ABB2, 1.8v.   sleep : ctrl by PWRREQ		
REGULATOR_INIT(ldo14, "vdd_ldo14 range", 1800000, 1800000, 1,
		REGULATOR_CHANGE_STATUS, 0); 
//ldo15, VDD10_HSIC_AP -> VDD10_HSIC, 1.0v. sleep:off
REGULATOR_INIT(ldo15, "vdd_ldo15 range", 1000000, 1000000, 0,
		REGULATOR_CHANGE_STATUS, 0);  //jeff, need check 
//ldo16, VDD18_HSIC_AP -> VDD18_HSIC , 1.8v. sleep:off 
REGULATOR_INIT(ldo16, "vdd_ldo16 range", 1800000, 1800000, 0,
		REGULATOR_CHANGE_STATUS, 0);   //jeff, need check 
//ldo17, VDD18_CAM , 1.8v. sleep:off
REGULATOR_INIT(ldo17, "vdd_ldo17 range", 1200000, 1200000, 0,
		REGULATOR_CHANGE_STATUS, 1); 
//ldo18, VDD18_CODEC , 1.8v. sleep:off
REGULATOR_INIT(ldo18, "vdd_ldo18 range", 1800000, 1800000, 1,
		REGULATOR_CHANGE_STATUS, 1);
//ldo19, VDD18_SUBCAM , 1.8v. sleep:off, boot:off
REGULATOR_INIT(ldo19, "vdd_ldo19 range", 1800000, 1800000, 0,
		REGULATOR_CHANGE_STATUS, 1);
//ldo20, VDDQ_PRE_AP , VDDQ_PRE, 1.8v. sleep:on
REGULATOR_INIT(ldo20, "vdd_ldo20 range", 1800000, 1800000, 1,
		REGULATOR_CHANGE_STATUS, 1);

//ldo21, VDDQ_MMC_AP -> VDDQ_MMC , 2.85v.  sleep:off,  Ctrl by EN21:XPWRRGTON, always on
REGULATOR_INIT(ldo21, "vdd_ldo21 range", 2850000, 2850000, 1,
		REGULATOR_CHANGE_STATUS, 1);
//ldo22, VDD28_SD , 2.8v.  sleep:off,  Ctrl by EN22: xmmc0CDn
REGULATOR_INIT(ldo22, "vdd_ldo22 range", 2800000, 2800000, 1,
		REGULATOR_CHANGE_STATUS, 1);

//ldo23, VDD28_LCD , 2.8v.  sleep:off,  boot
REGULATOR_INIT(ldo23, "vdd_ldo23 range", 2800000, 2800000, 0,
		REGULATOR_CHANGE_STATUS, 1);
//ldo24, VDD28_AF_CAM , 2.8v.  sleep:off,  
REGULATOR_INIT(ldo24, "vdd_ldo24 range", 2800000, 2800000, 0,
		REGULATOR_CHANGE_STATUS, 1);
//ldo25, VDD28_TP , 2.8v.  sleep:off,  boot: off
REGULATOR_INIT(ldo25, "vdd_ldo25 range", 2800000, 2800000, 0,
		REGULATOR_CHANGE_STATUS, 1);

//ldo26, VADD27_CAM , 2.7v.  sleep:off,  
REGULATOR_INIT(ldo26, "vdd_ldo26 range", 2700000, 2700000, 0,
		REGULATOR_CHANGE_STATUS, 1);

/* buck1, PVDD_MIF_AP, 1.1V, sleep:off, ctrl by PWRREQ */
static struct regulator_init_data __initdata max77686_buck1_data = {
	.constraints	= {
		.name		= "vdd_mif range",
		.min_uV		= 850000,
		.max_uV		= 1100000,
		.always_on	= 1,
		.boot_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS | REGULATOR_CHANGE_VOLTAGE,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &max77686_buck1_consumer,
};

/* buck2, PVDD_ARM_AP, 0.85~1.2v ?, sleep:off, ctrl by PWRREQ */
static struct regulator_init_data __initdata max77686_buck2_data = {
	.constraints	= {
		.name		= "vdd_arm range",
		.min_uV		= 875000,
		.max_uV		= 1375000,   
		.always_on	= 1,
		.boot_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &max77686_buck2_consumer,
};

/* buck3, PVDD_INT_AP, 0.85~1.2v?,  sleep:off, ctrl by PWRREQ */
static struct regulator_init_data __initdata max77686_buck3_data = {
	.constraints	= {
		.name		= "vdd_int range",
		.min_uV		= 850000,
		.max_uV		= 1050000,
		.always_on	= 1,
		.boot_on	= 1,		
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= & max77686_buck3_consumer,
};

/* buck4, PVDD_G3D_AP, 0.85~1.2v?, sleep:off */
static struct regulator_init_data __initdata max77686_buck4_data = {
	.constraints	= {
		.name		= "vdd_g3d range",
		.min_uV		= 850000,
		.max_uV		= 1200000,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &max77686_buck4_consumer,
};

/* buck5, PVDD_MEM_AP, 1.2v ?, sleep:on */
static struct regulator_init_data __initdata max77686_buck5_data = {
	.constraints	= {
		.name		= "buck5 range",
		.min_uV		= 1200000,
		.max_uV		= 1200000,
		.always_on	= 1,
		.boot_on	= 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 0,
	.consumer_supplies	= NULL,
};

/* buck6, 1.35v,  sleep:on */
static struct regulator_init_data __initdata max77686_buck6_data = {
	.constraints	= {
		.name		= "buck6 range",
		.min_uV		= 1350000,
		.max_uV		= 1350000,
		.always_on	= 1,
		.boot_on	= 1,		
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 0,
	.consumer_supplies	= NULL,
};

/* buck7, 2.0v,  sleep:on */
static struct regulator_init_data __initdata max77686_buck7_data = {
	.constraints	= {
		.name		= "buck7 range",
		.min_uV		= 2000000,
		.max_uV		= 2000000,
		.always_on	= 1,
		.boot_on	= 1,		
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.enabled	= 1,
		},
	},
	.num_consumer_supplies	= 0,
	.consumer_supplies	= NULL,
};

/* buck8, VDD_IO_EMMC, 2.85v,  sleep:off,  ctrl by PWRREQ */
static struct regulator_init_data __initdata max77686_buck8_data = {
	.constraints	= {
		.name		= "vdd_emmc range",
		.min_uV		= 2950000,
		.max_uV		= 2950000,	
		.boot_on	= 0,	//jeff
        .apply_uV   = 1,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &max77686_buck8_consumer,
};

/* buck9, VDD11_ISP_CAM, 1.2v,  sleep:off */
static struct regulator_init_data __initdata max77686_buck9_data = {
	.constraints	= {
		.name		= "vdd11_isp_cam range",
		.min_uV		= 1200000,
		.max_uV		= 1200000,
		.boot_on	= 0,
		.valid_ops_mask	= REGULATOR_CHANGE_STATUS,
		.state_mem	= {
			.disabled	= 1,
		},
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &max77686_buck9_consumer,
};


/* en32khz_ap, 32Khz for AP rtc,  sleep:on */
static struct regulator_init_data __initdata max77686_en32k_ap_data = {
	.constraints	= {
		.name		= "32khz_ap",
		//.always_on 	= 1,
	},
};

/* en32khz_cp, 32Khz for CP rtc,  sleep: depend on CP */
static struct regulator_init_data __initdata max77686_en32k_cp_data = {
	.constraints	= {
		.name		= "32khz_cp",
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 1,
	.consumer_supplies	= &max77686_en32khz_cp_consumers,
};

/* enp32khz, 32Khz for WLAN,  sleep: depend on wlan*/
static struct regulator_init_data __initdata max77686_enp32k_data = {
	.constraints	= {
		.name		= "32pkhz",
		//.always_on 	= 1,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies	= 2,
	.consumer_supplies	= &max77686_enp32khz_consumers,
};

static struct max8997_regulator_data __initdata max77686_regulators[] = {
	{ MAX77686_LDO1, &max77686_ldo1_data, },
	{ MAX77686_LDO2, &max77686_ldo2_data, },
	{ MAX77686_LDO3, &max77686_ldo3_data, },
	{ MAX77686_LDO4, &max77686_ldo4_data, },
	{ MAX77686_LDO5, &max77686_ldo5_data, },
	{ MAX77686_LDO6, &max77686_ldo6_data, },
	{ MAX77686_LDO7, &max77686_ldo7_data, },
	{ MAX77686_LDO8, &max77686_ldo8_data, },
	{ MAX77686_LDO9, &max77686_ldo9_data, },
	{ MAX77686_LDO10, &max77686_ldo10_data, },
	{ MAX77686_LDO11, &max77686_ldo11_data, },	
	{ MAX77686_LDO12, &max77686_ldo12_data, },
	{ MAX77686_LDO13, &max77686_ldo13_data, },
	{ MAX77686_LDO14, &max77686_ldo14_data, },
	{ MAX77686_LDO15, &max77686_ldo15_data, },
	{ MAX77686_LDO16, &max77686_ldo16_data, },
	{ MAX77686_LDO17, &max77686_ldo17_data, },
	{ MAX77686_LDO18, &max77686_ldo18_data, },
	{ MAX77686_LDO19, &max77686_ldo19_data, },	
	{ MAX77686_LDO20, &max77686_ldo20_data, },
	{ MAX77686_LDO21, &max77686_ldo21_data, },
	{ MAX77686_LDO22, &max77686_ldo22_data, },
	{ MAX77686_LDO23, &max77686_ldo23_data, },
	{ MAX77686_LDO24, &max77686_ldo24_data, },
	{ MAX77686_LDO25, &max77686_ldo25_data, },
	{ MAX77686_LDO26, &max77686_ldo26_data, },	
	{ MAX77686_BUCK1, &max77686_buck1_data, },
	{ MAX77686_BUCK2, &max77686_buck2_data, },
	{ MAX77686_BUCK3, &max77686_buck3_data, },
	{ MAX77686_BUCK4, &max77686_buck4_data, },
	{ MAX77686_BUCK5, &max77686_buck5_data, },
	{ MAX77686_BUCK6, &max77686_buck6_data, },
	{ MAX77686_BUCK7, &max77686_buck7_data, },
	{ MAX77686_BUCK8, &max77686_buck8_data, },
	{ MAX77686_BUCK9, &max77686_buck9_data, },
	{ MAX77686_EN32KHZ_AP, &max77686_en32k_ap_data, },	
	{ MAX77686_EN32KHZ_CP, &max77686_en32k_cp_data, },	
	{ MAX77686_P32KH, &max77686_enp32k_data,	},
};

struct max77686_opmode_data max77686_opmode_data[MAX77686_REG_MAX] = {
	[MAX77686_LDO2] = {MAX77686_LDO2, MAX77686_OPMODE_STANDBY},
	[MAX77686_LDO6] = {MAX77686_LDO6, MAX77686_OPMODE_STANDBY},
	[MAX77686_LDO7] = {MAX77686_LDO7, MAX77686_OPMODE_STANDBY},
	[MAX77686_LDO8] = {MAX77686_LDO8, MAX77686_OPMODE_STANDBY},
	[MAX77686_LDO10] = {MAX77686_LDO10, MAX77686_OPMODE_STANDBY},
	[MAX77686_LDO11] = {MAX77686_LDO11, MAX77686_OPMODE_STANDBY},
	[MAX77686_LDO12] = {MAX77686_LDO12, MAX77686_OPMODE_STANDBY},	/* UOTG_AP*/
	[MAX77686_LDO14] = {MAX77686_LDO14, MAX77686_OPMODE_STANDBY},
	[MAX77686_LDO15] = {MAX77686_LDO15, MAX77686_OPMODE_STANDBY},	/* HSIC */
	[MAX77686_LDO16] = {MAX77686_LDO16, MAX77686_OPMODE_STANDBY},	/* HSIC */
	[MAX77686_BUCK1] = {MAX77686_BUCK1, MAX77686_OPMODE_STANDBY},
	[MAX77686_BUCK2] = {MAX77686_BUCK2, MAX77686_OPMODE_STANDBY},
	[MAX77686_BUCK3] = {MAX77686_BUCK3, MAX77686_OPMODE_STANDBY},
	[MAX77686_BUCK4] = {MAX77686_BUCK4, MAX77686_OPMODE_STANDBY},	/* G3D */
};

static struct max77686_platform_data exynos4_max77686_info = {
	.num_regulators = ARRAY_SIZE(max77686_regulators),
	.regulators = max77686_regulators,
	.irq_gpio	= EXYNOS4_GPX3(2),
	.irq_base	= IRQ_BOARD_START,      /* jeff, if there are other irq_chip, we should defince IRQ_BASE at irqs_exynos4.h */
	.wakeup		= 1,

	.opmode_data = max77686_opmode_data,
	.ramp_rate = MAX77686_RAMP_RATE_27MV,

	.buck2_voltage[0] = 1300000,	/* 1.3V */
	.buck2_voltage[1] = 1000000,	/* 1.0V */
	.buck2_voltage[2] = 950000,	/* 0.95V */
	.buck2_voltage[3] = 900000,	/* 0.9V */
	.buck2_voltage[4] = 1000000,	/* 1.0V */
	.buck2_voltage[5] = 1000000,	/* 1.0V */
	.buck2_voltage[6] = 950000,	/* 0.95V */
	.buck2_voltage[7] = 900000,	/* 0.9V */

	.buck3_voltage[0] = 1037500,	/* 1.0375V */
	.buck3_voltage[1] = 1000000,	/* 1.0V */
	.buck3_voltage[2] = 950000,	/* 0.95V */
	.buck3_voltage[3] = 900000,	/* 0.9V */
	.buck3_voltage[4] = 1000000,	/* 1.0V */
	.buck3_voltage[5] = 1000000,	/* 1.0V */
	.buck3_voltage[6] = 950000,	/* 0.95V */
	.buck3_voltage[7] = 900000,	/* 0.9V */

	.buck4_voltage[0] = 1100000,	/* 1.1V */
	.buck4_voltage[1] = 1000000,	/* 1.0V */
	.buck4_voltage[2] = 950000,	/* 0.95V */
	.buck4_voltage[3] = 900000,	/* 0.9V */
	.buck4_voltage[4] = 1000000,	/* 1.0V */
	.buck4_voltage[5] = 1000000,	/* 1.0V */
	.buck4_voltage[6] = 950000,	/* 0.95V */
	.buck4_voltage[7] = 900000,	/* 0.9V */
};


#endif
/* End of MAX77686 */


#ifdef CONFIG_VIDEO_S5P_MIPI_CSIS
static struct regulator_consumer_supply mipi_csi_fixed_voltage_supplies[] = {
	REGULATOR_SUPPLY("mipi_csi", "s5p-mipi-csis.0"),
	REGULATOR_SUPPLY("mipi_csi", "s5p-mipi-csis.1"),
};

static struct regulator_init_data mipi_csi_fixed_voltage_init_data = {
	.constraints = {
		.always_on = 1,
	},
	.num_consumer_supplies	= ARRAY_SIZE(mipi_csi_fixed_voltage_supplies),
	.consumer_supplies	= mipi_csi_fixed_voltage_supplies,
};

static struct fixed_voltage_config mipi_csi_fixed_voltage_config = {
	.supply_name	= "DC_5V",
	.microvolts	= 5000000,
	.gpio		= -EINVAL,
	.init_data	= &mipi_csi_fixed_voltage_init_data,
};

static struct platform_device mipi_csi_fixed_voltage = {
	.name		= "reg-fixed-voltage",
	.id		= 3,
	.dev		= {
		.platform_data	= &mipi_csi_fixed_voltage_config,
	},
};
#endif

#ifdef CONFIG_VIDEO_M5MOLS
static struct regulator_consumer_supply m5mols_fixed_voltage_supplies[] = {
	REGULATOR_SUPPLY("core", NULL),
	REGULATOR_SUPPLY("dig_18", NULL),
	REGULATOR_SUPPLY("d_sensor", NULL),
	REGULATOR_SUPPLY("dig_28", NULL),
	REGULATOR_SUPPLY("a_sensor", NULL),
	REGULATOR_SUPPLY("dig_12", NULL),
};

static struct regulator_init_data m5mols_fixed_voltage_init_data = {
	.constraints = {
		.always_on = 1,
	},
	.num_consumer_supplies	= ARRAY_SIZE(m5mols_fixed_voltage_supplies),
	.consumer_supplies	= m5mols_fixed_voltage_supplies,
};

static struct fixed_voltage_config m5mols_fixed_voltage_config = {
	.supply_name	= "CAM_SENSOR",
	.microvolts	= 1800000,
	.gpio		= -EINVAL,
	.init_data	= &m5mols_fixed_voltage_init_data,
};

static struct platform_device m5mols_fixed_voltage = {
	.name		= "reg-fixed-voltage",
	.id		= 4,
	.dev		= {
		.platform_data	= &m5mols_fixed_voltage_config,
	},
};
#endif


#ifdef CONFIG_TOUCHSCREEN_STUTTGART_ATMEL
#define ATMEL_GPIO_IRQ  EXYNOS4_GPX0(5)
#define ATMEL_GPIO_RST EXYNOS4_GPF0(1)
#define ATMEL_GPIO_PULLUP EXYNOS4_GPZ(1)

static void mxt_cfg_irq(void)
{
        int err;

        err = gpio_request(EXYNOS4_GPX0(5), "GPX0");
        if (err)
                printk(KERN_ERR "request gpio GPX0(5) failure, err (%d)\n", err);
        gpio_direction_input(EXYNOS4_GPX0(5));
        s3c_gpio_setpull(EXYNOS4_GPX0(5), S3C_GPIO_PULL_NONE);
        /*
        s3c_gpio_cfgpin(EXYNOS4_GPX3(7), S3C_GPIO_PULL_UP);
        gpio_direction_output(EXYNOS4_GPX3(7), 1);
        */
        gpio_free(EXYNOS4_GPX0(5));
}
//#endif
//#ifdef CONFIG_TOUCHSCREEN_STUTTGART_ATMEL
#if 0
#define ATMEL_GPIO_IRQ  EXYNOS4_GPX3(7)
#define ATMEL_GPIO_RST EXYNOS4_GPF0(1)

static void mxt_cfg_irq(void)
{
        int err;

        err = gpio_request(EXYNOS4_GPX3(7), "GPX1");
        if (err)
                printk(KERN_ERR "request gpio GPX3(7) failure, err (%d)\n", err);
        gpio_direction_input(EXYNOS4_GPX3(7));
        s3c_gpio_setpull(EXYNOS4_GPX3(7), S3C_GPIO_PULL_UP);
        /*
        s3c_gpio_cfgpin(EXYNOS4_GPX3(7), S3C_GPIO_PULL_UP);
        gpio_direction_output(EXYNOS4_GPX3(7), 1);
        */
        gpio_free(EXYNOS4_GPX3(7));
}
#endif

static void mxt_init_platform_hw(void)
{
	int err;
	unsigned long *gpz_base = NULL;
	printk("mxt_init_platform_hw\n");
        err = gpio_request(ATMEL_GPIO_PULLUP, "GPZ");
        if (err < 0)
                printk(KERN_ERR "Failed to request GPIO%d (MaxTouch-reset) err=%d\n",
                        ATMEL_GPIO_PULLUP, err);
        gpio_direction_output(ATMEL_GPIO_PULLUP, 1);

	gpz_base = (unsigned long*)ioremap(0x03860000, 1024);
	writew(0x3 << 2, gpz_base + 0xc);
	iounmap(gpz_base);

        gpio_free(ATMEL_GPIO_PULLUP);
	mdelay(10);
	mxt_cfg_irq();
	err = gpio_request(ATMEL_GPIO_RST, "MaxTouch-reset");
	if (err < 0)
		printk(KERN_ERR "Failed to request GPIO%d (MaxTouch-reset) err=%d\n",
			ATMEL_GPIO_RST, err);

	err = gpio_direction_output(ATMEL_GPIO_RST, 0);
	if (err)
		printk(KERN_ERR "Failed to change direction, err=%d\n", err);

	msleep(10);
	/* maXTouch wants 40mSec minimum after reset to get organized */
	gpio_direction_output(ATMEL_GPIO_RST, 1);
	msleep(40);
}
static struct atmel_i2c_platform_data atmel_mxt_platform_data = 
{
	.version = 0x020,
	.abs_x_min = 0,
	.abs_x_max = 720,
	.abs_y_min = 0,
	.abs_y_max = 1280,
	.abs_pressure_min = 0,
	.abs_pressure_max = 255,
	.abs_width_min = 0,
	.abs_width_max = 20, 
	.gpio_irq = ATMEL_GPIO_IRQ,
	.power = mxt_init_platform_hw,

	.config_T6 = {0, 0, 0, 0, 0, 0}, 
	.config_T7 = {255, 255, 10}, 
	.config_T8 = {28, 0, 5, 5, 0, 0, 10, 40, 30, 230},
	.config_T9 = {139, 0, 0, 19, 11, 0, 16, 65, 3, 7, 0, 2, 1, 0, 5, 10, 20, 16, 0x46, 5, 0xcf, 2, 0, 0, 0, 0, 0, 0, 64, 0, 30, 20, 56, 52, 0}, 
	.config_T15 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 
	.config_T18 = {0, 0},
	.config_T19 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 
	.config_T23 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 
	.config_T25 = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, 

	.config_T38 = {0, 0, 0, 0, 0, 0, 0, 0},
	.config_T40 = {0, 0, 0, 0, 0},
	.config_T42 = {0, 0, 0, 0, 0, 0, 0, 0},
	.config_T46 = {0, 3, 32, 63, 0, 0, 0, 0, 0},  
	.config_T47 = {0, 20, 60, 5, 2, 50, 40, 0, 0, 40}, 
	.config_T48 = {3, 131, 194, 0, 0, 0, 0, 0, 5, 10, 0, 0, 0, 6, 6, 0, 0, 100, 10, 80, 10, 0, 20, 5, 0, 38, 0, 5, 0, 0, 0, 0, 0, 0, 0, 65, 3, 2, 1, 0, 5, 15, 20, 0, 0, 0, 0, 0, 0, 64, 0, 30, 20, 2}, 
	.object_crc = {0x1f, 0x2f, 0xe3},
};
#endif

#if defined(CONFIG_MPU_SENSORS_MPU3050) || defined(CONFIG_MPU_SENSORS_MPU3050_MODULE)
#include <linux/mpu.h>

#define SENSOR_MPU_NAME 	"mpu3050"
#define SENSOR_ACCEL_NAME	"mma845x"
#define SENSOR_COMPASS_NAME	"yas530"
#define GYRO_INT_GPIO		EXYNOS4_GPX0(3)
#define GYRO_IRQ				IRQ_EINT(3)

static int gyro_init_irq(void)
{
	int ret = 0;
	ret = gpio_request(GYRO_INT_GPIO, "gyro irq");
	if (ret) {
		printk("InvenSense fail to request irq gpio, return :%d\n",ret);
		goto err_request_cd;
	}
	
	s3c_gpio_cfgpin(GYRO_INT_GPIO, S3C_GPIO_SFN(0xf));
//	gpio_free(GYRO_INT_GPIO);

err_request_cd:
	return ret;
}

static struct mpu_platform_data mpu_data = {
	.int_config  = 0x10,
	.orientation = {  0,  -1,  0, 
			   1,  0,  0, 
			   0,  0, 1 }
};
/* accel */
#if defined(CONFIG_MPU_SENSORS_MMA845X) 
static struct ext_slave_platform_data accel_data = {
	.bus         = EXT_SLAVE_BUS_SECONDARY,
	.orientation = {  0,  1,  0, 
				  -1,  0,  0, 
				  0,  0, 1 }
};
#endif
#if defined(CONFIG_MPU_SENSORS_YAS530)
/* compass */
static struct ext_slave_platform_data compass_data = {
	 .bus         = EXT_SLAVE_BUS_PRIMARY,
	 .orientation = { 0, 1, 0, 
			  -1, 0, 0, 
			  0, 0, 1 }
};
#endif
#endif

#ifdef CONFIG_BQ2415X_POWER
static struct bq2415x_platform_data bq24153A_charger_info = {
	.stat_irq_gpio = EXYNOS4_GPX3(6),	/* EINT3_6, GPX3[6] on stuttgart*/
	.chg_det_irq_gpio = EXYNOS4_GPX2(4),	/* EINT2_4, GPX2[4] on stuttgart*/
};
#endif


#ifdef CONFIG_MAX8971_POWER
static struct max8971_platform_data max8971_charger_info = {
	.chgcc= 0x0A,              // Fast Charge Current, 500mA
	.fchgtime= 0x01,           // Fast Charge Time, 0x1:4 hours
	.chgrstrt= 0x0,           // Fast Charge Restart Threshold ,150mA
	.dcilmt = 0x14,             // Input Current Limit Selection, 500mA
	.topofftime = 0x3,        // Top Off Timer Setting ,30min
	.topofftshld =0x1,        // Done Current Threshold ,100mA
	.chgcv= 0x0,            // Charger Termination Voltage ,4.2V

	.regtemp = 0x0,           // Die temperature thermal regulation loop setpoint ,90 c
	.safetyreg = 0x0,          // JEITA Safety region selection
	.thm_config = 0x0,         // Thermal monitor configuration, 0: on

	.int_mask=0xe6,           // CHGINT_MASK

	//.irq_gpio=EXYNOS4_GPX3(6),  //jeff ,EINT(30)
};
#endif


static struct i2c_board_info i2c_devs0[] __initdata = {
#ifdef CONFIG_REGULATOR_S5M8767
	{
		I2C_BOARD_INFO("s5m87xx", 0xCC >> 1),
		.platform_data = &exynos4_s5m8767_pdata,
		.irq		= IRQ_EINT(26),
	},
#elif CONFIG_MFD_MAX8997
	{
		I2C_BOARD_INFO("max8997", 0x66),
		.platform_data	= &exynos4_max8997_info,
	},
#endif
#ifdef CONFIG_MFD_MAX77686
	{
		I2C_BOARD_INFO("max77686", 0x12 >>1),
		.platform_data	= &exynos4_max77686_info,
	},
#endif
#ifdef CONFIG_BQ2415X_POWER
	{
		I2C_BOARD_INFO("bq24153A", 0x6B),
		.platform_data	= &bq24153A_charger_info,
	},
#endif
#ifdef CONFIG_BATTERY_BQ27x00
	{
		I2C_BOARD_INFO("bq27500", 0x55),
	},
#endif

#ifdef CONFIG_MAX8971_POWER
	{
		I2C_BOARD_INFO("max8971", 0x35),
		.platform_data	= &max8971_charger_info,
		.irq = IRQ_EINT(30),
	}, 
#endif
#ifdef CONFIG_BATTERY_MAX17058
	{
		I2C_BOARD_INFO("max17058", 0x36),
		.irq = IRQ_EINT(29),
	}, 
#endif

};

static struct i2c_board_info i2c_devs1[] __initdata = {
#ifdef CONFIG_LEDS_STUTTGART_BACKLIGHT
	{
		I2C_BOARD_INFO("lm3532", 0x38),
	},
#endif
#ifdef CONFIG_TOUCHSCREEN_STUTTGART_ATMEL
	{
		I2C_BOARD_INFO("mxt224e", 0x4a),
		.platform_data = &atmel_mxt_platform_data,
		.irq =  IRQ_EINT(5),

	},
#endif
#ifdef  CONFIG_TOUCHSCREEN_FT5X06_I2C
    {    
        I2C_BOARD_INFO("ft5x0x_ts", 0x38),
        .irq = IRQ_EINT(5),
    }    
#endif
};

static struct i2c_board_info i2c_devs2[] __initdata = {
#ifdef CONFIG_VIDEO_TVOUT
	{
		I2C_BOARD_INFO("s5p_ddc", (0x74 >> 1)),
	},
#endif
};

static struct i2c_board_info i2c_devs3[] __initdata = {
	{
		I2C_BOARD_INFO("max8952", 0x60),
		.platform_data	= &exynos4_max8952_info,
	},
};

#if defined(CONFIG_INPUT_KXTIK)
#include <linux/input/kxtik.h>
#define KXTIK_DEVICE_MAP	4
#define KXTIK_MAP_X		((KXTIK_DEVICE_MAP-1)%2)
#define KXTIK_MAP_Y		(KXTIK_DEVICE_MAP%2)
#define KXTIK_NEG_X		(((KXTIK_DEVICE_MAP+2)/2)%2)
#define KXTIK_NEG_Y		(((KXTIK_DEVICE_MAP+5)/4)%2)
#define KXTIK_NEG_Z		((KXTIK_DEVICE_MAP-1)/4)

struct kxtik_platform_data kxtik_pdata = {
	.min_interval 	= 5,
	.poll_interval 	= 200,

	.axis_map_x 	= KXTIK_MAP_X,
	.axis_map_y 	= KXTIK_MAP_Y,
	.axis_map_z 	= 2,

	.negate_x 		= KXTIK_NEG_X,
	.negate_y 		= KXTIK_NEG_Y,
	.negate_z 		= KXTIK_NEG_Z,

	.res_12bit 		= RES_12BIT,
	.g_range  		= KXTIK_G_2G,
};
#endif

static struct i2c_gpio_platform_data gpio_i2c4_data  = {   
       .sda_pin = EXYNOS4_GPB(0),  
          .scl_pin = EXYNOS4_GPB(1),  
             .udelay = 1,
};
struct platform_device s3c_device_gpio_i2c4 = {    
       .name   = "i2c-gpio",   
          .id       = 4,  
             .dev = { 
                        .platform_data = &gpio_i2c4_data 
                               },
};
static struct i2c_board_info i2c_devs4[] __initdata = {
#if defined(CONFIG_MPU_SENSORS_MPU3050) || defined(CONFIG_MPU_SENSORS_MPU3050_MODULE)
       {
                  I2C_BOARD_INFO(SENSOR_MPU_NAME, 0x68),
                         .irq = (GYRO_IRQ),
                                .platform_data = &mpu_data,
                                   },  
#endif
#if defined(CONFIG_MPU_SENSORS_MMA845X) 
          {
                     I2C_BOARD_INFO(SENSOR_ACCEL_NAME, 0x1C),
                            .platform_data = &accel_data,
                               },  
#endif
#ifdef CONFIG_INPUT_YAS_ACCELEROMETER
	{
		I2C_BOARD_INFO("accelerometer", 0x0F),
	},
#endif
#ifdef CONFIG_INPUT_KXTIK
	{
		I2C_BOARD_INFO("kxtik", 0x0F),
		.platform_data = &kxtik_pdata,
	},
#endif
};

static struct i2c_gpio_platform_data gpio_i2c8_data  = {   
       .sda_pin = EXYNOS4212_GPM4(3), 
          .scl_pin = EXYNOS4212_GPM4(2), 
             .udelay = 100,
};

struct platform_device s3c_device_gpio_i2c8 = {    
       .name   = "i2c-gpio",   
          .id       = 8,  
             .dev = { 
                        .platform_data = &gpio_i2c8_data 
                               },
};

static struct i2c_gpio_platform_data gpio_i2c7_data  = {   
       .sda_pin = EXYNOS4_GPD0(2), 
          .scl_pin = EXYNOS4_GPD0(3), 
             .udelay = 1,
};
struct platform_device s3c_device_gpio_i2c7 = {    
       .name   = "i2c-gpio",   
          .id       = 7,  
             .dev = { 
                        .platform_data = &gpio_i2c7_data 
                               },
};

static struct i2c_board_info i2c_devs5[] __initdata = {
#ifdef CONFIG_SND_SOC_STUTTGART_MC_YAMAHA 
	{
		I2C_BOARD_INFO("mc_asoc_a", (0x3a)),
		.irq            = IRQ_EINT(13),
	},
	{
		I2C_BOARD_INFO("mc_asoc_d", (0x11)),
	},
#endif
#ifdef CONFIG_INPUT_YAS_MAGNETOMETER
    {
        I2C_BOARD_INFO("geomagnetic", 0x2E),
    },
#endif

};

static struct i2c_board_info i2c_devs6[] __initdata = {
};

static struct i2c_board_info i2c_devs7[] __initdata = {
	{
		I2C_BOARD_INFO("pixcir-ts", 0x5C),
		.irq		= IRQ_EINT(15),
	},
#if defined(CONFIG_MPU_SENSORS_YAS530) 
    {
        I2C_BOARD_INFO(SENSOR_COMPASS_NAME, 0x2E),
        .platform_data = &compass_data,
    },  
#endif
#ifdef CONFIG_PSENSOR_APDS990x
	{
		I2C_BOARD_INFO("apds990x", 0x39),
		.irq	= IRQ_EINT(1),
	},
#endif
};

#ifdef CONFIG_ANDROID_PMEM
static struct android_pmem_platform_data pmem_pdata = {
	.name		= "pmem",
	.no_allocator	= 1,
	.cached		= 0,
	.start		= 0,
	.size		= 0
};

static struct android_pmem_platform_data pmem_gpu1_pdata = {
	.name		= "pmem_gpu1",
	.no_allocator	= 1,
	.cached		= 0,
	.start		= 0,
	.size		= 0,
};

static struct platform_device pmem_device = {
	.name	= "android_pmem",
	.id	= 0,
	.dev	= {
		.platform_data = &pmem_pdata
	},
};

static struct platform_device pmem_gpu1_device = {
	.name	= "android_pmem",
	.id	= 1,
	.dev	= {
		.platform_data = &pmem_gpu1_pdata
	},
};

static void __init android_pmem_set_platdata(void)
{
#if defined(CONFIG_S5P_MEM_CMA)
	pmem_pdata.size = CONFIG_ANDROID_PMEM_MEMSIZE_PMEM * SZ_1K;
	pmem_gpu1_pdata.size = CONFIG_ANDROID_PMEM_MEMSIZE_PMEM_GPU1 * SZ_1K;
#endif
}
#endif

#ifdef CONFIG_BATTERY_SAMSUNG
static struct platform_device samsung_device_battery = {
	.name	= "samsung-fake-battery",
	.id	= -1,
};
#endif

//jeff
#ifdef CONFIG_KEYBOARD_GPIO_STUTTGART
struct gpio_keys_button stuttgart_button_table[] = {
	{
		.desc = "power",		
		.code = KEY_POWER,
		.gpio = EXYNOS4_GPX3(1),
		.active_low = 1,
		.type = EV_KEY,
		.can_disable = 0,
		.wakeup = 1,
	},{
		.desc = "volumedown",		
		.code = KEY_VOLUMEDOWN,
		.gpio = EXYNOS4_GPX1(1),
		.active_low = 1,
		.type = EV_KEY,
		.can_disable = 0,
		.wakeup = 0,
	}, {
		.desc = "volumeup",
		.code = KEY_VOLUMEUP,
		.gpio = EXYNOS4_GPX1(0),
		.active_low = 1,
		.type = EV_KEY,
		.can_disable = 0,
		.wakeup = 0,
	},
	/*wangkai, the two pins below are no longer used as keys from S3 EVT*/
	/*	
	{
		.desc = "search",
		.code = KEY_SEARCH,
		.gpio = EXYNOS4_GPX2(0),
		.active_low = 1,
		.type = EV_KEY,
		.can_disable = 0,
		.wakeup = 0,
	}, {
		.desc = "menu",
		.code = KEY_MENU,
		.gpio = EXYNOS4_GPX2(1),
		.active_low = 1,
		.type = EV_KEY,
		.can_disable = 0,
		.wakeup = 0,
	}, 
	*/
};

static struct gpio_keys_platform_data stuttgart_button_data = {
	.buttons  = stuttgart_button_table,
	.nbuttons = ARRAY_SIZE(stuttgart_button_table),
};

struct platform_device stuttgart_buttons = {
	.name   = "stuttgart-gpio-keypad",
	.dev  = {
		.platform_data = &stuttgart_button_data,
	},
	.id   = -1,
};
#endif

#if 0  /*jeff, not usd on stuttgart*/
static struct gpio_event_direct_entry stuttgart_keypad_key_map[] = {
	{
		.gpio   = EXYNOS4_GPX0(0),
		.code   = KEY_POWER,
	}
};

static struct gpio_event_input_info stuttgart_keypad_key_info = {
	.info.func              = gpio_event_input_func,
	.info.no_suspend        = true,
	.debounce_time.tv64	= 5 * NSEC_PER_MSEC,
	.type                   = EV_KEY,
	.keymap                 = stuttgart_keypad_key_map,
	.keymap_size            = ARRAY_SIZE(stuttgart_keypad_key_map)
};

static struct gpio_event_info *stuttgart_input_info[] = {
	&stuttgart_keypad_key_info.info,
};

static struct gpio_event_platform_data stuttgart_input_data = {
	.names  = {
		"stuttgart-keypad",
		NULL,
	},
	.info           = stuttgart_input_info,
	.info_count     = ARRAY_SIZE(stuttgart_input_info),
};

static struct platform_device stuttgart_input_device = {
	.name   = GPIO_EVENT_DEV_NAME,
	.id     = 0,
	.dev    = {
		.platform_data = &stuttgart_input_data,
	},
};

static void __init stuttgart_gpio_power_init(void)
{
	int err = 0;

	err = gpio_request_one(EXYNOS4_GPX0(0), 0, "GPX0");
	if (err) {
		printk(KERN_ERR "failed to request GPX0 for "
				"suspend/resume control\n");
		return;
	}
	s3c_gpio_setpull(EXYNOS4_GPX0(0), S3C_GPIO_PULL_NONE);

	gpio_free(EXYNOS4_GPX0(0));
}

static uint32_t stuttgart_keymap[] __initdata = {
	/* KEY(row, col, keycode) */
	KEY(1, 0, KEY_D), KEY(1, 1, KEY_A), KEY(1, 2, KEY_B),
	KEY(1, 3, KEY_E), KEY(1, 4, KEY_C)
};

static struct matrix_keymap_data stuttgart_keymap_data __initdata = {
	.keymap		= stuttgart_keymap,
	.keymap_size	= ARRAY_SIZE(stuttgart_keymap),
};

static struct samsung_keypad_platdata stuttgart_keypad_data __initdata = {
	.keymap_data	= &stuttgart_keymap_data,
	.rows		= 2,
	.cols		= 5,
};
#endif

#ifdef CONFIG_WAKEUP_ASSIST
static struct platform_device wakeup_assist_device = {
	.name   = "wakeup_assist",
};
#endif

#ifdef CONFIG_VIDEO_FIMG2D
static struct fimg2d_platdata fimg2d_data __initdata = {
	.hw_ver = 0x41,
	.parent_clkname = "mout_g2d0",
	.clkname = "sclk_fimg2d",
	.gate_clkname = "fimg2d",
	.clkrate = 201 * 1000000,	/* 200 Mhz */
};
#endif


#ifdef CONFIG_USB_EXYNOS_SWITCH
static struct s5p_usbswitch_platdata stuttgart_usbswitch_pdata;

static void __init stuttgart_usbswitch_init(void)
{
	struct s5p_usbswitch_platdata *pdata = &stuttgart_usbswitch_pdata;
	int err;

	pdata->gpio_host_detect = EXYNOS4_GPX3(5); /* low active */
	err = gpio_request_one(pdata->gpio_host_detect, GPIOF_IN, "HOST_DETECT");
	if (err) {
		printk(KERN_ERR "failed to request gpio_host_detect\n");
		return;
	}

	s3c_gpio_cfgpin(pdata->gpio_host_detect, S3C_GPIO_SFN(0xF));
	s3c_gpio_setpull(pdata->gpio_host_detect, S3C_GPIO_PULL_NONE);
	gpio_free(pdata->gpio_host_detect);

	pdata->gpio_device_detect = EXYNOS4_GPX3(4); /* high active */
	err = gpio_request_one(pdata->gpio_device_detect, GPIOF_IN, "DEVICE_DETECT");
	if (err) {
		printk(KERN_ERR "failed to request gpio_host_detect for\n");
		return;
	}

	s3c_gpio_cfgpin(pdata->gpio_device_detect, S3C_GPIO_SFN(0xF));
	s3c_gpio_setpull(pdata->gpio_device_detect, S3C_GPIO_PULL_NONE);
	gpio_free(pdata->gpio_device_detect);

	s5p_usbswitch_set_platdata(pdata);
}
#endif

#ifdef CONFIG_BUSFREQ_OPP
/* BUSFREQ to control memory/bus*/
static struct device_domain busfreq;
#endif

static struct platform_device exynos4_busfreq = {
	.id = -1,
	/* name = "exynos4-busfreq", */
	.name = "exynos-busfreq", /* jeff */
};


#ifdef CONFIG_BT
static struct resource bluesleep_resources[] = {
	{
		.name   = "gpio_host_wake", /* gpio: BT wake AP */
		.start  = EXYNOS4_GPX3(0),
		.end    = EXYNOS4_GPX3(0),
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "gpio_ext_wake", /* gpio: AP wake BT */
		.start  = EXYNOS4_GPF2(4),
		.end    = EXYNOS4_GPF2(4),
		.flags  = IORESOURCE_IO,
	},
	{
		.name   = "host_wake",	/* IRQ: BT wake AP */
		.start  = IRQ_EINT(24),
		.end    = IRQ_EINT(24),
		.flags  = IORESOURCE_IRQ,
	},
};

static struct platform_device msm_bluesleep_device = {
	.name = "bluesleep",
	.id = -1,
	.num_resources = ARRAY_SIZE(bluesleep_resources),
	.resource = bluesleep_resources,
};

static struct platform_device stuttgart_bt_power_device = {
	.name = "bt_power",
};
#endif

static struct platform_device *stuttgart_devices[] __initdata = {
#ifdef CONFIG_ANDROID_PMEM
	&pmem_device,
	&pmem_gpu1_device,
#endif
	/* Samsung Power Domain */
	&exynos4_device_pd[PD_MFC],
	&exynos4_device_pd[PD_G3D],
	&exynos4_device_pd[PD_LCD0],
	&exynos4_device_pd[PD_CAM],
	&exynos4_device_pd[PD_TV],
	&exynos4_device_pd[PD_GPS],
	&exynos4_device_pd[PD_GPS_ALIVE],
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
	&exynos4_device_pd[PD_ISP],
#endif
#ifdef CONFIG_FB_MIPI_DSIM
	&s5p_device_mipi_dsim,
#endif
#ifdef CONFIG_BT
	&msm_bluesleep_device,
	&stuttgart_bt_power_device,
#endif
	/* legacy fimd */
#ifdef CONFIG_FB_S5P
	&s3c_device_fb,
#endif
	&s3c_device_wdt,
	&s3c_device_rtc,
	&s3c_device_i2c0,
       //&s3c_device_gpio_i2c0,

	&s3c_device_i2c1,
	//&s3c_device_i2c2,
	//&s3c_device_i2c3,
	//&s3c_device_gpio_i2c4,
	&s3c_device_i2c4,
	&s3c_device_i2c5,
	&s3c_device_i2c6,
       &s3c_device_gpio_i2c8,
	&s3c_device_gpio_i2c7,
	&s3c_device_i2c7,
#ifdef CONFIG_SMM6260_MODEM
	&smm6260_modem,
#endif	 //haozz
#ifdef CONFIG_USB_EHCI_S5P
	&s5p_device_ehci,
#endif
#ifdef CONFIG_USB_OHCI_S5P
	&s5p_device_ohci,
#endif
#ifdef CONFIG_USB_GADGET
	&s3c_device_usbgadget,
#endif
#ifdef CONFIG_USB_ANDROID_RNDIS
	&s3c_device_rndis,
#endif
#ifdef CONFIG_USB_ANDROID
	&s3c_device_android_usb,
	&s3c_device_usb_mass_storage,
#endif
#ifdef CONFIG_S3C_DEV_HSMMC
	&s3c_device_hsmmc0,
#endif
#ifdef CONFIG_S3C_DEV_HSMMC1
	&s3c_device_hsmmc1,
#endif
#ifdef CONFIG_S3C_DEV_HSMMC2
	&s3c_device_hsmmc2,
#endif
#ifdef CONFIG_S3C_DEV_HSMMC3
	&s3c_device_hsmmc3,
#endif
#ifdef CONFIG_S5P_DEV_MSHC
	&s3c_device_mshci,
#endif
#ifdef CONFIG_EXYNOS4_DEV_DWMCI
	&exynos_device_dwmci,
#endif
#ifdef CONFIG_SND_SAMSUNG_AC97
	&exynos_device_ac97,
#endif
#ifdef CONFIG_SND_SAMSUNG_I2S
	&exynos_device_i2s0,
#endif
#ifdef CONFIG_SND_SAMSUNG_PCM
	&exynos_device_pcm0,
#endif
#ifdef CONFIG_SND_SAMSUNG_SPDIF
	&exynos_device_spdif,
#endif
#if defined(CONFIG_SND_SAMSUNG_RP) || defined(CONFIG_SND_SAMSUNG_ALP)
	&exynos_device_srp,
#endif
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
	&exynos4_device_fimc_is,
#endif
#ifdef CONFIG_VIDEO_TVOUT
	&s5p_device_tvout,
	&s5p_device_cec,
	&s5p_device_hpd,
#endif
#ifdef CONFIG_VIDEO_EXYNOS_TV
	&s5p_device_i2c_hdmiphy,
	&s5p_device_hdmi,
	&s5p_device_sdo,
	&s5p_device_mixer,
	&s5p_device_cec,
#endif
#if defined(CONFIG_VIDEO_FIMC)
	&s3c_device_fimc0,
	&s3c_device_fimc1,
	&s3c_device_fimc2,
	&s3c_device_fimc3,
/* CONFIG_VIDEO_SAMSUNG_S5P_FIMC is the feature for mainline */
#elif defined(CONFIG_VIDEO_SAMSUNG_S5P_FIMC)
	&s5p_device_fimc0,
	&s5p_device_fimc1,
	&s5p_device_fimc2,
	&s5p_device_fimc3,
#endif
#if defined(CONFIG_VIDEO_FIMC_MIPI)
	&s3c_device_csis0,
	&s3c_device_csis1,
#elif defined(CONFIG_VIDEO_S5P_MIPI_CSIS)
	&s5p_device_mipi_csis0,
	&s5p_device_mipi_csis1,
#endif
#ifdef CONFIG_VIDEO_S5P_MIPI_CSIS
	&mipi_csi_fixed_voltage,
#endif
#ifdef CONFIG_VIDEO_M5MOLS
	&m5mols_fixed_voltage,
#endif

#if defined(CONFIG_VIDEO_MFC5X) || defined(CONFIG_VIDEO_SAMSUNG_S5P_MFC)
	&s5p_device_mfc,
#endif
#ifdef CONFIG_S5P_SYSTEM_MMU
	&SYSMMU_PLATDEV(g2d_acp),
	&SYSMMU_PLATDEV(fimc0),
	&SYSMMU_PLATDEV(fimc1),
	&SYSMMU_PLATDEV(fimc2),
	&SYSMMU_PLATDEV(fimc3),
	&SYSMMU_PLATDEV(jpeg),
	&SYSMMU_PLATDEV(mfc_l),
	&SYSMMU_PLATDEV(mfc_r),
	&SYSMMU_PLATDEV(tv),
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
	&SYSMMU_PLATDEV(is_isp),
	&SYSMMU_PLATDEV(is_drc),
	&SYSMMU_PLATDEV(is_fd),
	&SYSMMU_PLATDEV(is_cpu),
#endif
#endif /* CONFIG_S5P_SYSTEM_MMU */
#ifdef CONFIG_ION_EXYNOS
	&exynos_device_ion,
#endif
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_LITE
	&exynos_device_flite0,
	&exynos_device_flite1,
#endif
#ifdef CONFIG_VIDEO_FIMG2D
	&s5p_device_fimg2d,
#endif
#ifdef CONFIG_EXYNOS_MEDIA_DEVICE
	&exynos_device_md0,
#endif
#ifdef CONFIG_VIDEO_JPEG_V2X
	&s5p_device_jpeg,
#endif
	&samsung_asoc_dma,
	&samsung_asoc_idma,
#ifdef CONFIG_BATTERY_SAMSUNG
	&samsung_device_battery,
#endif
	&samsung_device_keypad,
#ifdef CONFIG_WAKEUP_ASSIST
	&wakeup_assist_device,
#endif
	//&stuttgart_input_device,
//jeff
#ifdef CONFIG_KEYBOARD_GPIO_STUTTGART
       &stuttgart_buttons,
#endif
#ifdef CONFIG_S3C64XX_DEV_SPI
	//&exynos_device_spi0,

	&exynos_device_spi1,

	&exynos_device_spi2,
#endif
#ifdef CONFIG_EXYNOS_SETUP_THERMAL
	&s5p_device_tmu,
#endif
#ifdef CONFIG_S5P_DEV_ACE
	&s5p_device_ace,
#endif
	&exynos4_busfreq,

#ifdef CONFIG_GPSLENOVO
    &stuttgart_gps_device,
#endif
};

#ifdef CONFIG_EXYNOS_SETUP_THERMAL
/* below temperature base on the celcius degree */
struct s5p_platform_tmu exynos_tmu_data __initdata = {
        .ts = {
                .stop_1st_throttle  = 78,
                .start_1st_throttle = 80,
                .stop_2nd_throttle  = 87,
                .start_2nd_throttle = 103,
                .start_tripping     = 110, /* temp to do tripping */
                .start_emergency    = 120, /* To protect chip,forcely kernel panic */
                .stop_mem_throttle  = 80,
                .start_mem_throttle = 85,
                .stop_tc  = 13,
                .start_tc = 10,
        },
        .cpufreq = {
                .limit_1st_throttle  = 800000, /* 800MHz in KHz order */
                .limit_2nd_throttle  = 200000, /* 200MHz in KHz order */
        },
        .temp_compensate = {
                .arm_volt = 925000, /* vdd_arm in uV for temperature compensation */
                .bus_volt = 900000, /* vdd_bus in uV for temperature compensation */
                .g3d_volt = 900000, /* vdd_g3d in uV for temperature compensation */
        },
};
#endif

/*
 * bt power switch low level gpio and regulator control
 */
#ifdef CONFIG_BT
static int bluetooth_power(int on)
{
#define power 1 /* 1 for open regulator  */
#if power
	static int  reg_state = 0;
    struct regulator *bt32k_regulator;

		bt32k_regulator = regulator_get(NULL, "bt_32khz");
		if(IS_ERR(bt32k_regulator)){
			pr_err("%s: failed to get %s\n", __func__, "bt_32khz");
			return -1;
		};
#endif

		if(on) {
#if power
			/*
			 * reg_state equals to 0 means bt hasn't enable
			 * regulator
			 */
			if (!reg_state) {
				regulator_enable(bt32k_regulator);
				reg_state = 1;
			}
#endif
			gpio_set_value(EXYNOS4_GPF2(6),1);
			gpio_set_value(EXYNOS4_GPF2(5),1);
		} else {
			gpio_set_value(EXYNOS4_GPF2(6),0);
			gpio_set_value(EXYNOS4_GPF2(5),0);
#if power
			if(reg_state) {
				regulator_disable(bt32k_regulator);
				reg_state = 0;
			}
#endif
		}
#if power
		regulator_put(bt32k_regulator);
#endif
		printk(KERN_INFO"bluetooth power switch %d\n",on);

		return 0;
#undef power
}

static void __init bt_power_init(void)
{
        stuttgart_bt_power_device.dev.platform_data = &bluetooth_power;
}
#endif
//--
#if defined(CONFIG_VIDEO_TVOUT)
static struct s5p_platform_hpd hdmi_hpd_data __initdata = {

};
static struct s5p_platform_cec hdmi_cec_data __initdata = {

};
#endif

#ifdef CONFIG_VIDEO_EXYNOS_HDMI_CEC
static struct s5p_platform_cec hdmi_cec_data __initdata = {

};
#endif

#ifdef CONFIG_VIDEO_SAMSUNG_S5P_FIMC
static struct s5p_fimc_isp_info isp_info[] = {
#if defined(CONFIG_VIDEO_S5K4BA)
	{
		.board_info	= &s5k4ba_info,
		.clk_frequency  = 24000000UL,
		.bus_type	= FIMC_ITU_601,
#ifdef CONFIG_ITU_A
		.i2c_bus_num	= 0,
		.mux_id		= 0, /* A-Port : 0, B-Port : 1 */
#endif
#ifdef CONFIG_ITU_B
		.i2c_bus_num	= 1,
		.mux_id		= 1, /* A-Port : 0, B-Port : 1 */
#endif
		.flags		= FIMC_CLK_INV_VSYNC,
	},
#endif
#if defined(CONFIG_VIDEO_S5K4EA)
	{
		.board_info	= &s5k4ea_info,
		.clk_frequency  = 24000000UL,
		.bus_type	= FIMC_MIPI_CSI2,
#ifdef CONFIG_CSI_C
		.i2c_bus_num	= 0,
		.mux_id		= 0, /* A-Port : 0, B-Port : 1 */
#endif
#ifdef CONFIG_CSI_D
		.i2c_bus_num	= 1,
		.mux_id		= 1, /* A-Port : 0, B-Port : 1 */
#endif
		.flags		= FIMC_CLK_INV_VSYNC,
		.csi_data_align = 32,
	},
#endif
#if defined(CONFIG_VIDEO_M5MOLS)
	{
		.board_info	= &m5mols_board_info,
		.clk_frequency  = 24000000UL,
		.bus_type	= FIMC_MIPI_CSI2,
#ifdef CONFIG_CSI_C
		.i2c_bus_num	= 4,
		.mux_id		= 0, /* A-Port : 0, B-Port : 1 */
#endif
#ifdef CONFIG_CSI_D
		.i2c_bus_num	= 5,
		.mux_id		= 1, /* A-Port : 0, B-Port : 1 */
#endif
		.flags		= FIMC_CLK_INV_PCLK | FIMC_CLK_INV_VSYNC,
		.csi_data_align = 32,
	},
#endif
#if defined(CONFIG_VIDEO_IMX073)
	{
		.board_info	= &imx073_i2c_info,
		.clk_frequency  = 24000000UL,
		.bus_type	= FIMC_MIPI_CSI2,
#ifdef CONFIG_CSI_C
		.i2c_bus_num	= 6,
		.mux_id		= 0, /* A-Port : 0, B-Port : 1 */
#endif
#ifdef CONFIG_CSI_D
		.i2c_bus_num	= 1,
		.mux_id		= 1, /* A-Port : 0, B-Port : 1 */
#endif
		.flags		= FIMC_CLK_INV_PCLK | FIMC_CLK_INV_VSYNC, // ??????????
		.csi_data_align = 32,
	},
#endif
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
#if defined(CONFIG_VIDEO_S5K3H1)
	{
		.board_info	= &s5k3h1_sensor_info,
		.clk_frequency  = 24000000UL,
		.bus_type	= FIMC_MIPI_CSI2,
#ifdef CONFIG_S5P_S5K3H1_CSI_C
		.i2c_bus_num	= 0,
		.mux_id		= 0, /* A-Port : 0, B-Port : 1 */
		.flite_id	= FLITE_IDX_A,
		.cam_power	= stuttgart_cam0_reset,
#endif
#ifdef CONFIG_S5P_S5K3H1_CSI_D
		.i2c_bus_num	= 1,
		.mux_id		= 1, /* A-Port : 0, B-Port : 1 */
		.flite_id	= FLITE_IDX_B,
		.cam_power	= stuttgart_cam1_reset,
#endif
		.csi_data_align = 24,
		.use_isp	= true,
	},
#endif
#if defined(CONFIG_VIDEO_S5K3H2)
	{
		.board_info	= &s5k3h2_sensor_info,
		.clk_frequency  = 24000000UL,
		.bus_type	= FIMC_MIPI_CSI2,
#ifdef CONFIG_S5P_S5K3H2_CSI_C
		.i2c_bus_num	= 0,
		.mux_id		= 0, /* A-Port : 0, B-Port : 1 */
		.flite_id	= FLITE_IDX_A,
		.cam_power	= stuttgart_cam0_reset,
#endif
#ifdef CONFIG_S5P_S5K3H2_CSI_D
		.i2c_bus_num	= 1,
		.mux_id		= 1, /* A-Port : 0, B-Port : 1 */
		.flite_id	= FLITE_IDX_B,
		.cam_power	= stuttgart_cam1_reset,
#endif
		.csi_data_align = 24,
		.use_isp	= true,
	},
#endif
#if defined(CONFIG_VIDEO_S5K3H7)
	{
		.board_info	= &s5k3h7_sensor_info,
		.clk_frequency  = 24000000UL,
		.bus_type	= FIMC_MIPI_CSI2,
#ifdef CONFIG_S5K3H7_CSI_C
		.i2c_bus_num	= 0,
		.mux_id		= 0, /* A-Port : 0, B-Port : 1 */
		.flite_id	= FLITE_IDX_A,
		.cam_power	= stuttgart_cam0_reset,
#endif
#ifdef CONFIG_S5K3H7_CSI_D
		.i2c_bus_num	= 1,
		.mux_id		= 1, /* A-Port : 0, B-Port : 1 */
		.flite_id	= FLITE_IDX_B,
		.cam_power	= stuttgart_cam1_reset,
#endif
		.csi_data_align = 24,
		.use_isp	= true,
	},
#endif
#if defined(CONFIG_VIDEO_S5K6A3)
	{
		.board_info	= &s5k6a3_sensor_info,
		.clk_frequency  = 12000000UL,
		.bus_type	= FIMC_MIPI_CSI2,
#ifdef CONFIG_S5P_S5K6A3_CSI_C
		.i2c_bus_num	= 0,
		.mux_id		= 0, /* A-Port : 0, B-Port : 1 */
		.flite_id	= FLITE_IDX_A,
		.cam_power	= stuttgart_cam0_reset,
#endif
#ifdef CONFIG_S5P_S5K6A3_CSI_D
		.i2c_bus_num	= 1,
		.mux_id		= 1, /* A-Port : 0, B-Port : 1 */
		.flite_id	= FLITE_IDX_B,
		.cam_power	= stuttgart_cam1_reset,
#endif
		.csi_data_align = 12,
		.use_isp	= true,
	},
#endif
#endif
#if defined(WRITEBACK_ENABLED)
	{
		.board_info	= &writeback_info,
		.bus_type	= FIMC_LCD_WB,
		.i2c_bus_num	= 0,
		.mux_id		= 0, /* A-Port : 0, B-Port : 1 */
		.flags		= FIMC_CLK_INV_VSYNC,
	},
#endif
};

static void __init stuttgart_subdev_config(void)
{
	s3c_fimc0_default_data.isp_info[0] = &isp_info[0];
	s3c_fimc0_default_data.isp_info[0]->use_cam = true;
	/* support using two fimc as one sensore */
	{
		static struct s5p_fimc_isp_info camcording;
		memcpy(&camcording, &isp_info[0], sizeof(struct s5p_fimc_isp_info));
		s3c_fimc1_default_data.isp_info[0] = &camcording;
		s3c_fimc1_default_data.isp_info[0]->use_cam = false;
	}
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
#ifdef CONFIG_VIDEO_S5K3H2
#ifdef CONFIG_S5K3H2_CSI_C
	s5p_mipi_csis0_default_data.clk_rate	= 160000000;
	s5p_mipi_csis0_default_data.lanes 	= 2;
	s5p_mipi_csis0_default_data.alignment	= 24;
	s5p_mipi_csis0_default_data.hs_settle	= 12;
#endif
#ifdef CONFIG_S5K3H2_CSI_D
	s5p_mipi_csis1_default_data.clk_rate	= 160000000;
	s5p_mipi_csis1_default_data.lanes 	= 2;
	s5p_mipi_csis1_default_data.alignment	= 24;
	s5p_mipi_csis1_default_data.hs_settle	= 12;
#endif
#endif
#ifdef CONFIG_VIDEO_S5K3H7
#ifdef CONFIG_S5K3H7_CSI_C
	s5p_mipi_csis0_default_data.clk_rate	= 160000000;
	s5p_mipi_csis0_default_data.lanes 	= 2;
	s5p_mipi_csis0_default_data.alignment	= 24;
	s5p_mipi_csis0_default_data.hs_settle	= 12;
#endif
#ifdef CONFIG_S5K3H7_CSI_D
	s5p_mipi_csis1_default_data.clk_rate	= 160000000;
	s5p_mipi_csis1_default_data.lanes 	= 2;
	s5p_mipi_csis1_default_data.alignment	= 24;
	s5p_mipi_csis1_default_data.hs_settle	= 12;
#endif
#endif
#ifdef CONFIG_VIDEO_S5K6A3
#ifdef CONFIG_S5P_S5K6A3_CSI_C
	s5p_mipi_csis0_default_data.clk_rate	= 160000000;
	s5p_mipi_csis0_default_data.lanes 	= 1;
	s5p_mipi_csis0_default_data.alignment	= 24;
	s5p_mipi_csis0_default_data.hs_settle	= 12;
#endif
#ifdef CONFIG_S5P_S5K6A3_CSI_D
	s5p_mipi_csis1_default_data.clk_rate	= 160000000;
	s5p_mipi_csis1_default_data.lanes 	= 1;
	s5p_mipi_csis1_default_data.alignment	= 24;
	s5p_mipi_csis1_default_data.hs_settle	= 12;
#endif
#endif
#endif
}
static void __init stuttgart_camera_config(void)
{
	/* CAM A port(b0010) : PCLK, VSYNC, HREF, DATA[0-4] */
	s3c_gpio_cfgrange_nopull(EXYNOS4212_GPJ0(0), 8, S3C_GPIO_SFN(2));
	/* CAM A port(b0010) : DATA[5-7], CLKOUT(MIPI CAM also), FIELD */
	s3c_gpio_cfgrange_nopull(EXYNOS4212_GPJ1(0), 5, S3C_GPIO_SFN(2));
	/* CAM B port(b0011) : PCLK, DATA[0-6] */
	s3c_gpio_cfgrange_nopull(EXYNOS4212_GPM0(0), 8, S3C_GPIO_SFN(3));
	/* CAM B port(b0011) : FIELD, DATA[7]*/
	s3c_gpio_cfgrange_nopull(EXYNOS4212_GPM1(0), 2, S3C_GPIO_SFN(3));
	/* CAM B port(b0011) : VSYNC, HREF, CLKOUT*/
	s3c_gpio_cfgrange_nopull(EXYNOS4212_GPM2(0), 3, S3C_GPIO_SFN(3));

	/* note : driver strength to max is unnecessary */
#ifdef CONFIG_VIDEO_M5MOLS
	s3c_gpio_cfgpin(EXYNOS4_GPX2(6), S3C_GPIO_SFN(0xF));
	s3c_gpio_setpull(EXYNOS4_GPX2(6), S3C_GPIO_PULL_NONE);
#endif
}
#endif /* CONFIG_VIDEO_SAMSUNG_S5P_FIMC */

#ifdef CONFIG_VIDEO_EXYNOS_FIMC_LITE
static void __set_flite_camera_config(struct exynos_platform_flite *data,
					u32 active_index, u32 max_cam)
{
	data->active_cam_index = active_index;
	data->num_clients = max_cam;
}

static void __init stuttgart_set_camera_flite_platdata(void)
{
	int flite0_cam_index = 0;
	int flite1_cam_index = 0;
#ifdef CONFIG_VIDEO_S5K3H1
#ifdef CONFIG_S5K3H1_CSI_C
	exynos_flite0_default_data.cam[flite0_cam_index++] = &s5k3h1;
#endif
#ifdef CONFIG_S5K3H1_CSI_D
	exynos_flite1_default_data.cam[flite1_cam_index++] = &s5k3h1;
#endif
#endif
#ifdef CONFIG_VIDEO_S5K3H2
#ifdef CONFIG_S5K3H2_CSI_C
	exynos_flite0_default_data.cam[flite0_cam_index++] = &s5k3h2;
#endif
#ifdef CONFIG_S5K3H2_CSI_D
	exynos_flite1_default_data.cam[flite1_cam_index++] = &s5k3h2;
#endif
#endif
#ifdef CONFIG_VIDEO_S5K3H7
#ifdef CONFIG_S5K3H7_CSI_C
	exynos_flite0_default_data.cam[flite0_cam_index++] = &s5k3h7;
#endif
#ifdef CONFIG_S5K3H7_CSI_D
	exynos_flite1_default_data.cam[flite1_cam_index++] = &s5k3h7;
#endif
#endif
#ifdef CONFIG_VIDEO_S5K6A3
#ifdef CONFIG_S5K6A3_CSI_C
	exynos_flite0_default_data.cam[flite0_cam_index++] = &s5k6a3;
#endif
#ifdef CONFIG_S5K6A3_CSI_D
	exynos_flite1_default_data.cam[flite1_cam_index++] = &s5k6a3;
#endif
#endif
	__set_flite_camera_config(&exynos_flite0_default_data, 0, flite0_cam_index);
	__set_flite_camera_config(&exynos_flite1_default_data, 0, flite1_cam_index);
}
#endif

#if defined(CONFIG_S5P_MEM_CMA)
static void __init exynos4_cma_region_reserve(
			struct cma_region *regions_normal,
			struct cma_region *regions_secure)
{
	struct cma_region *reg;
	phys_addr_t paddr_last = 0xFFFFFFFF;

	for (reg = regions_normal; reg->size != 0; reg++) {
		phys_addr_t paddr;

		if (!IS_ALIGNED(reg->size, PAGE_SIZE)) {
			pr_err("S5P/CMA: size of '%s' is NOT page-aligned\n",
								reg->name);
			reg->size = PAGE_ALIGN(reg->size);
		}


		if (reg->reserved) {
			pr_err("S5P/CMA: '%s' alread reserved\n", reg->name);
			continue;
		}

		if (reg->alignment) {
			if ((reg->alignment & ~PAGE_MASK) ||
				(reg->alignment & ~reg->alignment)) {
				pr_err("S5P/CMA: Failed to reserve '%s': "
						"incorrect alignment 0x%08x.\n",
						reg->name, reg->alignment);
			continue;
			}
		} else {
			reg->alignment = PAGE_SIZE;
		}

		if (reg->start) {
			if (!memblock_is_region_reserved(reg->start, reg->size)
			    && (memblock_reserve(reg->start, reg->size) == 0)){
				reg->reserved = 1;
				pr_err("start mem reserved, name:%s,  addr:%#x, size:%d", reg->name, reg->start, reg->size);
			}
			else
				pr_err("S5P/CMA: Failed to reserve '%s'\n",
								reg->name);

			continue;
		}

		paddr = memblock_find_in_range(0, MEMBLOCK_ALLOC_ACCESSIBLE,
						reg->size, reg->alignment);
		if (paddr != MEMBLOCK_ERROR) {
			if (memblock_reserve(paddr, reg->size)) {
				pr_err("S5P/CMA: Failed to reserve '%s'\n",
								reg->name);
				continue;
			}

				reg->start = paddr;
				reg->reserved = 1;
		} else {
			pr_err("S5P/CMA: No free space in memory for '%s'\n",
								reg->name);
			}

		if (cma_early_region_register(reg)) {
			pr_err("S5P/CMA: Failed to register '%s'\n",
								reg->name);
			memblock_free(reg->start, reg->size);
		} else {
			paddr_last = min(paddr, paddr_last);
		}
		pr_err("mem reserved, name:%s,  addr:%#x, size:%d", reg->name, reg->start, reg->size);
	}

	if (regions_secure && regions_secure->size) {
		size_t size_secure = 0;
		size_t align_secure, size_region2, aug_size, order_region2;

		for (reg = regions_secure; reg->size != 0; reg++)
			size_secure += reg->size;

		reg--;

		/* Entire secure regions will be merged into 2
		 * consecutive regions. */
		align_secure = 1 <<
			(get_order((size_secure + 1) / 2) + PAGE_SHIFT);
		/* Calculation of a subregion size */
		size_region2 = size_secure - align_secure;
		order_region2 = get_order(size_region2) + PAGE_SHIFT;
		if (order_region2 < 20)
			order_region2 = 20; /* 1MB */
		order_region2 -= 3; /* divide by 8 */
		size_region2 = ALIGN(size_region2, 1 << order_region2);

		aug_size = align_secure + size_region2 - size_secure;
		if (aug_size > 0)
			reg->size += aug_size;

		size_secure = ALIGN(size_secure, align_secure);

		if (paddr_last >= memblock.current_limit) {
			paddr_last = memblock_find_in_range(0,
					MEMBLOCK_ALLOC_ACCESSIBLE,
					size_secure, reg->alignment);
		} else {
			paddr_last -= size_secure;
			paddr_last = round_down(paddr_last, align_secure);
		}

		if (paddr_last) {
			while (memblock_reserve(paddr_last, size_secure))
				paddr_last -= align_secure;

			do {
				reg->start = paddr_last;
				reg->reserved = 1;
				paddr_last += reg->size;

				if (cma_early_region_register(reg)) {
					memblock_free(reg->start, reg->size);
					pr_err("S5P/CMA: "
					"Failed to register secure region "
					"'%s'\n", reg->name);
				} else {
					size_secure -= reg->size;
				}
			} while (reg-- != regions_secure);

			if (size_secure > 0)
				memblock_free(paddr_last, size_secure);
		} else {
			pr_err("S5P/CMA: Failed to reserve secure regions\n");
		}
	}
}

static void __init exynos4_reserve_mem(void)
{
	static struct cma_region regions[] = {
#ifdef CONFIG_SEC_LOG
		{
			.name = "klog",
			.size = 1024 * SZ_1K,
			.start = 0x5ff00000,
		},
#endif
#ifdef CONFIG_ANDROID_PMEM_MEMSIZE_PMEM
		{
			.name = "pmem",
			.size = CONFIG_ANDROID_PMEM_MEMSIZE_PMEM * SZ_1K,
			.start = 0,
		},
#endif
#ifdef CONFIG_ANDROID_PMEM_MEMSIZE_PMEM_GPU1
		{
			.name = "pmem_gpu1",
			.size = CONFIG_ANDROID_PMEM_MEMSIZE_PMEM_GPU1 * SZ_1K,
			.start = 0,
		},
#endif
#ifndef CONFIG_VIDEOBUF2_ION
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_TV
		{
			.name = "tv",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_TV * SZ_1K,
			.start = 0
		},
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_JPEG
		{
			.name = "jpeg",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_JPEG * SZ_1K,
			.start = 0
		},
#endif
#ifdef CONFIG_AUDIO_SAMSUNG_MEMSIZE_SRP
		{
			.name = "srp",
			.size = CONFIG_AUDIO_SAMSUNG_MEMSIZE_SRP * SZ_1K,
			.start = 0,
		},
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMG2D
		{
			.name = "fimg2d",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMG2D * SZ_1K,
			.start = 0
		},
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMD
		{
			.name = "fimd",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMD * SZ_1K,
			.start = 0
		},
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMC0
		{
			.name = "fimc0",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMC0 * SZ_1K,
			.start = 0
		},
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMC2
		{
			.name = "fimc2",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMC2 * SZ_1K,
			.start = 0
		},
#endif
#if !defined(CONFIG_EXYNOS4_CONTENT_PATH_PROTECTION) && \
	defined(CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMC3)
		{
			.name = "fimc3",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMC3 * SZ_1K,
		},
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMC1
		{
			.name = "fimc1",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMC1 * SZ_1K,
			.start = 0
		},
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_MFC1
		{
#ifdef CONFIG_EXYNOS4_CONTENT_PATH_PROTECTION
			.name = "mfc-normal",
#else
			.name = "mfc1",
#endif
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_MFC1 * SZ_1K,
			{ .alignment = 1 << 17 },
		},
#endif
#if !defined(CONFIG_EXYNOS4_CONTENT_PATH_PROTECTION) && \
	defined(CONFIG_VIDEO_SAMSUNG_MEMSIZE_MFC0)
		{
			.name = "mfc0",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_MFC0 * SZ_1K,
			{ .alignment = 1 << 17 },
		},
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_MFC
		{
			.name = "mfc",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_MFC * SZ_1K,
			{ .alignment = 1 << 17 },
		},
#endif
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
		{
			.name = "fimc_is",
			.size = CONFIG_VIDEO_EXYNOS_MEMSIZE_FIMC_IS * SZ_1K,
			{
				.alignment = 1 << 26,
			},
			.start = 0
		},
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS_BAYER
		{
			.name = "fimc_is_isp",
			.size = CONFIG_VIDEO_EXYNOS_MEMSIZE_FIMC_IS_ISP * SZ_1K,
			.start = 0
		},
#endif
#endif
#if !defined(CONFIG_EXYNOS4_CONTENT_PATH_PROTECTION) && \
	defined(CONFIG_VIDEO_SAMSUNG_S5P_MFC)
		{
			.name		= "b2",
			.size		= 32 << 20,
			{ .alignment	= 128 << 10 },
		},
		{
			.name		= "b1",
			.size		= 32 << 20,
			{ .alignment	= 128 << 10 },
		},
		{
			.name		= "fw",
			.size		= 1 << 20,
			{ .alignment	= 128 << 10 },
		},
#endif
#else /* !CONFIG_VIDEOBUF2_ION */
#ifdef CONFIG_FB_S5P
#error CONFIG_FB_S5P is defined. Select CONFIG_FB_S3C, instead
#endif
		{
			.name	= "ion",
			.size	= CONFIG_ION_EXYNOS_CONTIGHEAP_SIZE * SZ_1K,
		},
#endif /* !CONFIG_VIDEOBUF2_ION */
		{
			.size = 0
		},
	};
#ifdef CONFIG_EXYNOS4_CONTENT_PATH_PROTECTION
	static struct cma_region regions_secure[] = {
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMD_VIDEO
		{
			.name = "video",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMD_VIDEO * SZ_1K,
		},
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMC3
		{
			.name = "fimc3",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_FIMC3 * SZ_1K,
		},
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_MEMSIZE_MFC0
		{
			.name = "mfc-secure",
			.size = CONFIG_VIDEO_SAMSUNG_MEMSIZE_MFC0 * SZ_1K,
		},
#endif
		{
			.name = "sectbl",
			.size = SZ_1M,
			{
				.alignment = SZ_64M,
			},
		},
		{
			.size = 0
		},
	};
#else /* !CONFIG_EXYNOS4_CONTENT_PATH_PROTECTION */
	struct cma_region *regions_secure = NULL;
#endif
	static const char map[] __initconst =
#ifdef CONFIG_EXYNOS_C2C
		"samsung-c2c=c2c_shdmem;"
#endif
		"android_pmem.0=pmem;android_pmem.1=pmem_gpu1;"
		"s3cfb.0/fimd=fimd;exynos4-fb.0/fimd=fimd;"
#ifdef CONFIG_EXYNOS4_CONTENT_PATH_PROTECTION
		"s3cfb.0/video=video;exynos4-fb.0/video=video;"
#endif
		"s3c-fimc.0=fimc0;s3c-fimc.1=fimc1;s3c-fimc.2=fimc2;s3c-fimc.3=fimc3;"
		"exynos4210-fimc.0=fimc0;exynos4210-fimc.1=fimc1;exynos4210-fimc.2=fimc2;exynos4210-fimc.3=fimc3;"
#ifdef CONFIG_VIDEO_MFC5X
		"s3c-mfc/A=mfc0,mfc-secure;"
		"s3c-mfc/B=mfc1,mfc-normal;"
		"s3c-mfc/AB=mfc;"
#endif
#ifdef CONFIG_VIDEO_SAMSUNG_S5P_MFC
		"s5p-mfc/f=fw;"
		"s5p-mfc/a=b1;"
		"s5p-mfc/b=b2;"
#endif
		"samsung-rp=srp;"
		"s5p-jpeg=jpeg;"
		"exynos4-fimc-is/f=fimc_is;"
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS_BAYER
		"exynos4-fimc-is/i=fimc_is_isp;"
#endif
		"s5p-mixer=tv;"
		"s5p-fimg2d=fimg2d;"
		"ion-exynos=ion,fimd,fimc0,fimc1,fimc2,fimc3,fw,b1,b2;"
#ifdef CONFIG_EXYNOS4_CONTENT_PATH_PROTECTION
		"s5p-smem/video=video;"
		"s5p-smem/sectbl=sectbl;"
#endif
		"s5p-smem/mfc=mfc0,mfc-secure;"
		"s5p-smem/fimc=fimc3;"
		"s5p-smem/mfc-shm=mfc1,mfc-normal;";

	cma_set_defaults(NULL, map);

	exynos4_cma_region_reserve(regions, regions_secure);
}
#endif

/* LCD Backlight data */
static struct samsung_bl_gpio_info stuttgart_bl_gpio_info = {
        .no = EXYNOS4_GPD0(1),
        .func = S3C_GPIO_SFN(2),
};

static struct platform_pwm_backlight_data stuttgart_bl_data = {
        .pwm_id = 1,
        .pwm_period_ns  = 1000,
};


static void __init stuttgart_map_io(void)
{
	clk_xusbxti.rate = 24000000;
	s5p_init_io(NULL, 0, S5P_VA_CHIPID);
	s3c24xx_init_clocks(24000000);
	s3c24xx_init_uarts(stuttgart_uartcfgs, ARRAY_SIZE(stuttgart_uartcfgs));

#if defined(CONFIG_S5P_MEM_CMA)
	exynos4_reserve_mem();
#endif
}

static void __init exynos_sysmmu_init(void)
{
	ASSIGN_SYSMMU_POWERDOMAIN(fimc0, &exynos4_device_pd[PD_CAM].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(fimc1, &exynos4_device_pd[PD_CAM].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(fimc2, &exynos4_device_pd[PD_CAM].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(fimc3, &exynos4_device_pd[PD_CAM].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(jpeg, &exynos4_device_pd[PD_CAM].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(mfc_l, &exynos4_device_pd[PD_MFC].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(mfc_r, &exynos4_device_pd[PD_MFC].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(tv, &exynos4_device_pd[PD_TV].dev);
#ifdef CONFIG_VIDEO_FIMG2D
	sysmmu_set_owner(&SYSMMU_PLATDEV(g2d_acp).dev, &s5p_device_fimg2d.dev);
#endif
#if defined(CONFIG_VIDEO_SAMSUNG_S5P_MFC) || defined(CONFIG_VIDEO_MFC5X)
	sysmmu_set_owner(&SYSMMU_PLATDEV(mfc_l).dev, &s5p_device_mfc.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(mfc_r).dev, &s5p_device_mfc.dev);
#endif
#if defined(CONFIG_VIDEO_FIMC)
	sysmmu_set_owner(&SYSMMU_PLATDEV(fimc0).dev, &s3c_device_fimc0.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(fimc1).dev, &s3c_device_fimc1.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(fimc2).dev, &s3c_device_fimc2.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(fimc3).dev, &s3c_device_fimc3.dev);
#elif defined(CONFIG_VIDEO_SAMSUNG_S5P_FIMC)
	sysmmu_set_owner(&SYSMMU_PLATDEV(fimc0).dev, &s5p_device_fimc0.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(fimc1).dev, &s5p_device_fimc1.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(fimc2).dev, &s5p_device_fimc2.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(fimc3).dev, &s5p_device_fimc3.dev);
#endif
#ifdef CONFIG_VIDEO_EXYNOS_TV
	sysmmu_set_owner(&SYSMMU_PLATDEV(tv).dev, &s5p_device_mixer.dev);
#endif
#ifdef CONFIG_VIDEO_TVOUT
	sysmmu_set_owner(&SYSMMU_PLATDEV(tv).dev, &s5p_device_tvout.dev);
#endif
#ifdef CONFIG_VIDEO_JPEG_V2X
	sysmmu_set_owner(&SYSMMU_PLATDEV(jpeg).dev, &s5p_device_jpeg.dev);
#endif
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
	ASSIGN_SYSMMU_POWERDOMAIN(is_isp, &exynos4_device_pd[PD_ISP].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(is_drc, &exynos4_device_pd[PD_ISP].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(is_fd, &exynos4_device_pd[PD_ISP].dev);
	ASSIGN_SYSMMU_POWERDOMAIN(is_cpu, &exynos4_device_pd[PD_ISP].dev);

	sysmmu_set_owner(&SYSMMU_PLATDEV(is_isp).dev,
						&exynos4_device_fimc_is.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(is_drc).dev,
						&exynos4_device_fimc_is.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(is_fd).dev,
						&exynos4_device_fimc_is.dev);
	sysmmu_set_owner(&SYSMMU_PLATDEV(is_cpu).dev,
						&exynos4_device_fimc_is.dev);
#endif
}

/* jeff, add gpio init code*/
#ifdef CONFIG_MACH_STUTTGART
void exynos4_config_gpio_table(int array_size, struct gpio_init_data * gpio_table)
{
	u32 i, gpio;

	for (i = 0; i < array_size; i++) {
		gpio = gpio_table[i].num;
		if (gpio <= EXYNOS4XXX_GPIO_END) {
			s3c_gpio_cfgpin(gpio, gpio_table[i].cfg);
			s3c_gpio_setpull(gpio, gpio_table[i].pud);

			if (gpio_table[i].val != S3C_GPIO_SETPIN_NONE)
				gpio_set_value(gpio, gpio_table[i].val);

			s5p_gpio_set_drvstr(gpio, gpio_table[i].drv);
		}
	}
}
void exynos4_config_sleep_gpio_table(int array_size, unsigned int (*gpio_table)[3])
{
	u32 i, gpio;

	for (i = 0; i < array_size; i++) {
		gpio = gpio_table[i][0];
		s5p_gpio_set_pd_cfg(gpio, gpio_table[i][1]);
		s5p_gpio_set_pd_pull(gpio, gpio_table[i][2]);
	}
}

static void stuttgart_init_gpio(void)
{
	exynos4_config_gpio_table(ARRAY_SIZE(stuttgart_init_gpio_table),
			stuttgart_init_gpio_table);
}

void stuttgart_init_sleep_gpio(void)
{
	exynos4_config_sleep_gpio_table(ARRAY_SIZE(stuttgart_sleep_gpio_table),
			stuttgart_sleep_gpio_table);
}
EXPORT_SYMBOL(stuttgart_init_sleep_gpio);

#endif
/* end */

#define EXYNOS4412_PS_HOLD_CONTROL_REG (S5P_VA_PMU + 0x330C)
static void stuttgart_power_off(void)
{
	msleep(10);
	/* PS_HOLD output High --> Low */
	writel(readl(EXYNOS4412_PS_HOLD_CONTROL_REG) & 0xFFFFFEFF,
			EXYNOS4412_PS_HOLD_CONTROL_REG);

	while(1);
}

static void __init stuttgart_machine_init(void)
{
#ifdef CONFIG_S3C64XX_DEV_SPI
	unsigned int gpio;
	struct clk *sclk = NULL;
	struct clk *prnt = NULL;
	//struct device *spi0_dev = &exynos_device_spi0.dev;

	struct device *spi1_dev = &exynos_device_spi1.dev;

	struct device *spi2_dev = &exynos_device_spi2.dev;
#endif
#if defined(CONFIG_EXYNOS_DEV_PD) && defined(CONFIG_PM_RUNTIME)
	exynos_pd_disable(&exynos4_device_pd[PD_MFC].dev);
	exynos_pd_disable(&exynos4_device_pd[PD_G3D].dev);
	//exynos_pd_disable(&exynos4_device_pd[PD_LCD0].dev);
	exynos_pd_disable(&exynos4_device_pd[PD_CAM].dev);
	exynos_pd_disable(&exynos4_device_pd[PD_TV].dev);
	exynos_pd_disable(&exynos4_device_pd[PD_GPS].dev);
	exynos_pd_disable(&exynos4_device_pd[PD_GPS_ALIVE].dev);
	exynos_pd_disable(&exynos4_device_pd[PD_ISP].dev);
#elif defined(CONFIG_EXYNOS_DEV_PD)
	/*
	 * These power domains should be always on
	 * without runtime pm support.
	 */
	exynos_pd_enable(&exynos4_device_pd[PD_MFC].dev);
	exynos_pd_enable(&exynos4_device_pd[PD_G3D].dev);
	exynos_pd_enable(&exynos4_device_pd[PD_LCD0].dev);
	exynos_pd_enable(&exynos4_device_pd[PD_CAM].dev);
	exynos_pd_enable(&exynos4_device_pd[PD_TV].dev);
	exynos_pd_enable(&exynos4_device_pd[PD_GPS].dev);
	exynos_pd_enable(&exynos4_device_pd[PD_GPS_ALIVE].dev);
	exynos_pd_enable(&exynos4_device_pd[PD_ISP].dev);
#endif
	/* jeff, init stuttgart startup and sleep gpio */
	stuttgart_init_gpio();

	pm_power_off = stuttgart_power_off ;

	s3c_i2c0_set_platdata(NULL);
	i2c_register_board_info(0, i2c_devs0, ARRAY_SIZE(i2c_devs0));

	s3c_i2c1_set_platdata(NULL);
	i2c_register_board_info(1, i2c_devs1, ARRAY_SIZE(i2c_devs1));

	//s3c_i2c2_set_platdata(NULL);
	//i2c_register_board_info(2, i2c_devs2, ARRAY_SIZE(i2c_devs2));

	//s3c_i2c3_set_platdata(NULL);
	//i2c_register_board_info(3, i2c_devs3, ARRAY_SIZE(i2c_devs3));

	s3c_i2c4_set_platdata(NULL);
	i2c_register_board_info(4, i2c_devs4, ARRAY_SIZE(i2c_devs4));
	
	s3c_i2c5_set_platdata(NULL);
	i2c_register_board_info(5, i2c_devs5, ARRAY_SIZE(i2c_devs5));

	s3c_i2c6_set_platdata(NULL);
//	i2c_register_board_info(6, i2c_devs6, ARRAY_SIZE(i2c_devs6));

	s3c_i2c6_set_platdata(NULL);
	i2c_register_board_info(6, i2c_devs6, ARRAY_SIZE(i2c_devs6));


	s3c_i2c7_set_platdata(NULL);
	i2c_register_board_info(7, i2c_devs7, ARRAY_SIZE(i2c_devs7));

#ifdef CONFIG_ANDROID_PMEM
	android_pmem_set_platdata();
#endif

#ifdef CONFIG_FB_S5P
	s3cfb_set_platdata(NULL);
#if defined(CONFIG_FB_S5P_MIPI_DSIM)
	mipi_fb_init();
#endif
#ifdef CONFIG_FB_S5P_MIPI_DSIM
	s5p_device_dsim.dev.parent = &exynos4_device_pd[PD_LCD0].dev;
#endif

#ifdef CONFIG_EXYNOS_DEV_PD
	s3c_device_fb.dev.parent = &exynos4_device_pd[PD_LCD0].dev;
#endif
#endif
#ifdef CONFIG_USB_EHCI_S5P
	stuttgart_ehci_init();
#endif
#ifdef CONFIG_USB_OHCI_S5P
	stuttgart_ohci_init();
#endif
#ifdef CONFIG_USB_GADGET
	stuttgart_usbgadget_init();
#endif
#ifdef CONFIG_USB_EXYNOS_SWITCH
	stuttgart_usbswitch_init();
#endif

        samsung_bl_set(&stuttgart_bl_gpio_info, &stuttgart_bl_data);

#ifdef CONFIG_EXYNOS4_DEV_DWMCI
	exynos_dwmci_set_platdata(&exynos_dwmci_pdata);
#endif

#ifdef CONFIG_VIDEO_EXYNOS_FIMC_IS
	exynos4_fimc_is_set_platdata(NULL);
#ifdef CONFIG_EXYNOS_DEV_PD
	exynos4_device_fimc_is.dev.parent = &exynos4_device_pd[PD_ISP].dev;
#endif
#endif
#ifdef CONFIG_S3C_DEV_HSMMC
	s3c_sdhci0_set_platdata(&stuttgart_hsmmc0_pdata);
#endif
#ifdef CONFIG_S3C_DEV_HSMMC1
	s3c_sdhci1_set_platdata(&stuttgart_hsmmc1_pdata);
#endif
#ifdef CONFIG_S3C_DEV_HSMMC2
	s3c_sdhci2_set_platdata(&stuttgart_hsmmc2_pdata);
#endif
#ifdef CONFIG_S3C_DEV_HSMMC3
	s3c_sdhci3_set_platdata(&stuttgart_hsmmc3_pdata);
#endif
#ifdef CONFIG_S5P_DEV_MSHC
	s3c_mshci_set_platdata(&exynos4_mshc_pdata);
#endif
#if defined(CONFIG_VIDEO_EXYNOS_TV) && defined(CONFIG_VIDEO_EXYNOS_HDMI)
	dev_set_name(&s5p_device_hdmi.dev, "exynos4-hdmi");
	clk_add_alias("hdmi", "s5p-hdmi", "hdmi", &s5p_device_hdmi.dev);
	clk_add_alias("hdmiphy", "s5p-hdmi", "hdmiphy", &s5p_device_hdmi.dev);
#ifdef CONFIG_VIDEO_EXYNOS_HDMI_CEC
	s5p_hdmi_cec_set_platdata(&hdmi_cec_data);
#endif
#endif
#ifdef CONFIG_VIDEO_EXYNOS_FIMC_LITE
	stuttgart_set_camera_flite_platdata();
	s3c_set_platdata(&exynos_flite0_default_data,
			sizeof(exynos_flite0_default_data), &exynos_device_flite0);
	s3c_set_platdata(&exynos_flite1_default_data,
			sizeof(exynos_flite1_default_data), &exynos_device_flite1);
#endif
#ifdef CONFIG_EXYNOS_SETUP_THERMAL
	s5p_tmu_set_platdata(&exynos_tmu_data);
#endif
#ifdef CONFIG_VIDEO_FIMC
	s3c_fimc0_set_platdata(&fimc_plat);
	s3c_fimc1_set_platdata(&fimc_plat);
	s3c_fimc2_set_platdata(NULL);
	s3c_fimc3_set_platdata(NULL);
#ifdef CONFIG_EXYNOS_DEV_PD
	s3c_device_fimc0.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s3c_device_fimc1.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s3c_device_fimc2.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s3c_device_fimc3.dev.parent = &exynos4_device_pd[PD_CAM].dev;
#ifdef CONFIG_EXYNOS4_CONTENT_PATH_PROTECTION
	secmem.parent = &exynos4_device_pd[PD_CAM].dev;
#endif
#endif
#ifdef CONFIG_VIDEO_FIMC_MIPI
	s3c_csis0_set_platdata(NULL);
	s3c_csis1_set_platdata(NULL);
#ifdef CONFIG_EXYNOS_DEV_PD
	s3c_device_csis0.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s3c_device_csis1.dev.parent = &exynos4_device_pd[PD_CAM].dev;
#endif
#endif

#if defined(CONFIG_ITU_A) || defined(CONFIG_CSI_C) \
	|| defined(CONFIG_S5K3H1_CSI_C) || defined(CONFIG_S5K3H2_CSI_C) \
	|| defined(CONFIG_S5K6A3_CSI_C)
	stuttgart_cam0_reset(1);
#endif
#if defined(CONFIG_ITU_B) || defined(CONFIG_CSI_D) \
	|| defined(CONFIG_S5K3H1_CSI_D) || defined(CONFIG_S5K3H2_CSI_D) \
	|| defined(CONFIG_S5K6A3_CSI_D)
	stuttgart_cam1_reset(1);
#endif
#endif /* CONFIG_VIDEO_FIMC */

#ifdef CONFIG_VIDEO_SAMSUNG_S5P_FIMC
	stuttgart_camera_config();
	stuttgart_subdev_config();

	dev_set_name(&s5p_device_fimc0.dev, "s3c-fimc.0");
	dev_set_name(&s5p_device_fimc1.dev, "s3c-fimc.1");
	dev_set_name(&s5p_device_fimc2.dev, "s3c-fimc.2");
	dev_set_name(&s5p_device_fimc3.dev, "s3c-fimc.3");

	clk_add_alias("fimc", "exynos4210-fimc.0", "fimc", &s5p_device_fimc0.dev);
	clk_add_alias("sclk_fimc", "exynos4210-fimc.0", "sclk_fimc",
			&s5p_device_fimc0.dev);
	clk_add_alias("fimc", "exynos4210-fimc.1", "fimc", &s5p_device_fimc1.dev);
	clk_add_alias("sclk_fimc", "exynos4210-fimc.1", "sclk_fimc",
			&s5p_device_fimc1.dev);
	clk_add_alias("fimc", "exynos4210-fimc.2", "fimc", &s5p_device_fimc2.dev);
	clk_add_alias("sclk_fimc", "exynos4210-fimc.2", "sclk_fimc",
			&s5p_device_fimc2.dev);
	clk_add_alias("fimc", "exynos4210-fimc.3", "fimc", &s5p_device_fimc3.dev);
	clk_add_alias("sclk_fimc", "exynos4210-fimc.3", "sclk_fimc",
			&s5p_device_fimc3.dev);

	s3c_fimc_setname(0, "exynos4210-fimc");
	s3c_fimc_setname(1, "exynos4210-fimc");
	s3c_fimc_setname(2, "exynos4210-fimc");
	s3c_fimc_setname(3, "exynos4210-fimc");
	/* FIMC */
	s3c_set_platdata(&s3c_fimc0_default_data,
			 sizeof(s3c_fimc0_default_data), &s5p_device_fimc0);
	s3c_set_platdata(&s3c_fimc1_default_data,
			 sizeof(s3c_fimc1_default_data), &s5p_device_fimc1);
	s3c_set_platdata(&s3c_fimc2_default_data,
			 sizeof(s3c_fimc2_default_data), &s5p_device_fimc2);
	s3c_set_platdata(&s3c_fimc3_default_data,
			 sizeof(s3c_fimc3_default_data), &s5p_device_fimc3);
#ifdef CONFIG_EXYNOS_DEV_PD
	s5p_device_fimc0.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s5p_device_fimc1.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s5p_device_fimc2.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s5p_device_fimc3.dev.parent = &exynos4_device_pd[PD_CAM].dev;
#endif
#ifdef CONFIG_VIDEO_S5P_MIPI_CSIS
	dev_set_name(&s5p_device_mipi_csis0.dev, "s3c-csis.0");
	dev_set_name(&s5p_device_mipi_csis1.dev, "s3c-csis.1");
	clk_add_alias("csis", "s5p-mipi-csis.0", "csis",
			&s5p_device_mipi_csis0.dev);
	clk_add_alias("sclk_csis", "s5p-mipi-csis.0", "sclk_csis",
			&s5p_device_mipi_csis0.dev);
	clk_add_alias("csis", "s5p-mipi-csis.1", "csis",
			&s5p_device_mipi_csis1.dev);
	clk_add_alias("sclk_csis", "s5p-mipi-csis.1", "sclk_csis",
			&s5p_device_mipi_csis1.dev);
	dev_set_name(&s5p_device_mipi_csis0.dev, "s5p-mipi-csis.0");
	dev_set_name(&s5p_device_mipi_csis1.dev, "s5p-mipi-csis.1");

	s3c_set_platdata(&s5p_mipi_csis0_default_data,
			sizeof(s5p_mipi_csis0_default_data), &s5p_device_mipi_csis0);
	s3c_set_platdata(&s5p_mipi_csis1_default_data,
			sizeof(s5p_mipi_csis1_default_data), &s5p_device_mipi_csis1);
#ifdef CONFIG_EXYNOS_DEV_PD
	s5p_device_mipi_csis0.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	s5p_device_mipi_csis1.dev.parent = &exynos4_device_pd[PD_CAM].dev;
#endif
#endif
#if defined(CONFIG_ITU_A) || defined(CONFIG_CSI_C) \
	|| defined(CONFIG_S5K3H1_CSI_C) || defined(CONFIG_S5K3H2_CSI_C) \
	|| defined(CONFIG_S5K6A3_CSI_C)
	stuttgart_cam0_reset(1);
#endif
#if defined(CONFIG_ITU_B) || defined(CONFIG_CSI_D) \
	|| defined(CONFIG_S5K3H1_CSI_D) || defined(CONFIG_S5K3H2_CSI_D) \
	|| defined(CONFIG_S5K6A3_CSI_D)
	stuttgart_cam1_reset(1);
#endif
#endif

#if defined(CONFIG_VIDEO_TVOUT)
	s5p_hdmi_hpd_set_platdata(&hdmi_hpd_data);
	s5p_hdmi_cec_set_platdata(&hdmi_cec_data);
#ifdef CONFIG_EXYNOS_DEV_PD
	s5p_device_tvout.dev.parent = &exynos4_device_pd[PD_TV].dev;
	exynos4_device_pd[PD_TV].dev.parent= &exynos4_device_pd[PD_LCD0].dev;
#endif
#endif

#ifdef CONFIG_VIDEO_JPEG_V2X
#ifdef CONFIG_EXYNOS_DEV_PD
	s5p_device_jpeg.dev.parent = &exynos4_device_pd[PD_CAM].dev;
	exynos4_jpeg_setup_clock(&s5p_device_jpeg.dev, 160000000);
#endif
#endif

#ifdef CONFIG_ION_EXYNOS
	exynos_ion_set_platdata();
#endif

#if defined(CONFIG_VIDEO_MFC5X) || defined(CONFIG_VIDEO_SAMSUNG_S5P_MFC)
#ifdef CONFIG_EXYNOS_DEV_PD
	s5p_device_mfc.dev.parent = &exynos4_device_pd[PD_MFC].dev;
#endif
	exynos4_mfc_setup_clock(&s5p_device_mfc.dev, 267 * MHZ);
#endif

#if defined(CONFIG_VIDEO_SAMSUNG_S5P_MFC)
	dev_set_name(&s5p_device_mfc.dev, "s3c-mfc");
	clk_add_alias("mfc", "s5p-mfc", "mfc", &s5p_device_mfc.dev);
	s5p_mfc_setname(&s5p_device_mfc, "s5p-mfc");
#endif

#ifdef CONFIG_VIDEO_FIMG2D
	s5p_fimg2d_set_platdata(&fimg2d_data);
#endif

#ifdef CONFIG_KEYBOARD_SAMSUNG
	samsung_keypad_set_platdata(&stuttgart_keypad_data);
#endif

	exynos_sysmmu_init();

	//stuttgart_gpio_power_init();

	platform_add_devices(stuttgart_devices, ARRAY_SIZE(stuttgart_devices));


#ifdef CONFIG_S3C64XX_DEV_SPI
#if 0 /*SPI0 gpios are used for I2C4*/
	sclk = clk_get(spi0_dev, "dout_spi0");
	if (IS_ERR(sclk))
		dev_err(spi0_dev, "failed to get sclk for SPI-0\n");
	prnt = clk_get(spi0_dev, "mout_mpll_user");
	if (IS_ERR(prnt))
		dev_err(spi0_dev, "failed to get prnt\n");
	if (clk_set_parent(sclk, prnt))
		printk(KERN_ERR "Unable to set parent %s of clock %s.\n",
				prnt->name, sclk->name);

	clk_set_rate(sclk, 100 * 1000 * 1000);
	clk_put(sclk);
	clk_put(prnt);

	if (!gpio_request(EXYNOS4_GPB(1), "SPI_CS0")) {
		gpio_direction_output(EXYNOS4_GPB(1), 1);
		s3c_gpio_cfgpin(EXYNOS4_GPB(1), S3C_GPIO_SFN(1));
		s3c_gpio_setpull(EXYNOS4_GPB(1), S3C_GPIO_PULL_UP);
		exynos_spi_set_info(0, EXYNOS_SPI_SRCCLK_SCLK,
			ARRAY_SIZE(spi0_csi));
	}

	for (gpio = EXYNOS4_GPB(0); gpio < EXYNOS4_GPB(4); gpio++)
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV3);

	spi_register_board_info(spi0_board_info, ARRAY_SIZE(spi0_board_info));
#endif

	sclk = clk_get(spi1_dev, "dout_spi1");
	if (IS_ERR(sclk))
		dev_err(spi1_dev, "failed to get sclk for SPI-1\n");
	prnt = clk_get(spi1_dev, "mout_mpll_user");
	if (IS_ERR(prnt))
		dev_err(spi1_dev, "failed to get prnt\n");
	if (clk_set_parent(sclk, prnt))
		printk(KERN_ERR "Unable to set parent %s of clock %s.\n",
				prnt->name, sclk->name);

	clk_set_rate(sclk, 100 * 1000 * 1000);
	clk_put(sclk);
	clk_put(prnt);

	if (!gpio_request(EXYNOS4_GPB(5), "SPI_CS1")) {
		gpio_direction_output(EXYNOS4_GPB(5), 1);
		s3c_gpio_cfgpin(EXYNOS4_GPB(5), S3C_GPIO_SFN(1));
		s3c_gpio_setpull(EXYNOS4_GPB(5), S3C_GPIO_PULL_UP);
		exynos_spi_set_info(1, EXYNOS_SPI_SRCCLK_SCLK,
			ARRAY_SIZE(spi1_csi));
	}

	for (gpio = EXYNOS4_GPB(4); gpio < EXYNOS4_GPB(8); gpio++)
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV3);

	spi_register_board_info(spi1_board_info, ARRAY_SIZE(spi1_board_info));


	sclk = clk_get(spi2_dev, "dout_spi2");
	if (IS_ERR(sclk))
		dev_err(spi2_dev, "failed to get sclk for SPI-2\n");
	prnt = clk_get(spi2_dev, "mout_mpll_user");
	if (IS_ERR(prnt))
		dev_err(spi2_dev, "failed to get prnt\n");
	if (clk_set_parent(sclk, prnt))
		printk(KERN_ERR "Unable to set parent %s of clock %s.\n",
				prnt->name, sclk->name);


	clk_set_rate(sclk, 100 * 1000 * 1000);
	clk_put(sclk);
	clk_put(prnt);

	if (!gpio_request(EXYNOS4_GPC1(2), "SPI_CS2")) {
		gpio_direction_output(EXYNOS4_GPC1(2), 1);
		s3c_gpio_cfgpin(EXYNOS4_GPC1(2), S3C_GPIO_SFN(1));
		s3c_gpio_setpull(EXYNOS4_GPC1(2), S3C_GPIO_PULL_UP);
		exynos_spi_set_info(2, EXYNOS_SPI_SRCCLK_SCLK,
			ARRAY_SIZE(spi2_csi));
	}

	for (gpio = EXYNOS4_GPC1(2); gpio < EXYNOS4_GPC1(5); gpio++)
		s5p_gpio_set_drvstr(gpio, S5P_GPIO_DRVSTR_LV3);

	spi_register_board_info(spi2_board_info, ARRAY_SIZE(spi2_board_info));
#else
    /* jeff, spi1 default clksrc is mpll*/
    {
        struct clk *sclk = NULL;
        struct clk *prnt = NULL;
        sclk = clk_get(NULL, "dout_spi1");
        if (IS_ERR(sclk))
            pr_err("failed to get sclk for SPI-1\n");
        prnt = clk_get(NULL, "xusbxti");
        if (IS_ERR(prnt))
            pr_err( "failed to get prnt\n");
        if (clk_set_parent(sclk, prnt))
            printk(KERN_ERR "Unable to set parent %s of clock %s.\n",
                prnt->name, sclk->name);

        clk_put(sclk);
        clk_put(prnt);
    }
#endif
#ifdef CONFIG_BUSFREQ_OPP
	dev_add(&busfreq, &exynos4_busfreq.dev);
	ppmu_init(&exynos_ppmu[PPMU_DMC0], &exynos4_busfreq.dev);
	ppmu_init(&exynos_ppmu[PPMU_DMC1], &exynos4_busfreq.dev);
	ppmu_init(&exynos_ppmu[PPMU_CPU], &exynos4_busfreq.dev);
#endif
	//Add for bt
#ifdef CONFIG_BT
	bt_power_init();
#endif

	/* register reboot notifier for reboot to bootloader */
	register_reboot_notifier(&exynos4_reboot_notifier);

#ifdef TEST_FLASH
	s3c_gpio_setpull(EXYNOS4212_GPM3(7), S3C_GPIO_OUTPUT);
	gpio_set_value(EXYNOS4212_GPM3(7), 1);
#endif

}

MACHINE_START(STUTTGART, "STUTTGART")
	.boot_params	= S5P_PA_SDRAM + 0x100,
	.init_irq	= exynos4_init_irq,
	.map_io		= stuttgart_map_io,
	.init_machine	= stuttgart_machine_init,
	.timer		= &exynos4_timer,
MACHINE_END
