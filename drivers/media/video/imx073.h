/* linux/drivers/media/video/imx073.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 * 		http://www.samsung.com/
 *
 * Driver for IMX073 (SXGA camera) from Samsung Electronics
 * 1/6" 1.3Mp CMOS Image Sensor SoC with an Embedded Image Processor
 * supporting MIPI CSI-2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __IMX073_H__
#define __IMX073_H__


//#define USE_POSTVIEW 

struct imx073_reg {
	unsigned short addr;
	unsigned short val;
};

struct imx073_regset_type {
	unsigned char *regset;
	int len;
};

/*
 * Macro
 */
#define REGSET_LENGTH(x)	(sizeof(x)/sizeof(imx073_reg))

/*
 * Host S/W Register interface (0x70000000-0x70002000)
 */
/* Initialization section */
#define IMX073_Speed_368Mbps		0
#define IMX073_Speed_464Mbps		1
#define IMX073_Speed_552Mbps		2
#define IMX073_Speed_648Mbps		3
#define IMX073_Speed_736Mbps		4
#define IMX073_Speed_832Mbps		5
#define IMX073_Speed_920Mbps		6

#define IMX0734Khz_0Mhz		0x0000
#define IMX0734Khz_46Mhz		0x2CEC
#define IMX0734Khz_58Mhz		0x38A4
#define IMX0734Khz_69Mhz		0x4362
#define IMX0734Khz_81Mhz		0x4F1A
#define IMX0734Khz_91Mhz		0x58DE
#define IMX0734Khz_92Mhz		0x59D8
#define IMX0734Khz_93Mhz		0x5AD2
#define IMX0734Khz_104Mhz		0x6590
#define IMX0734Khz_115Mhz		0x704E

#define IMX073FrTime_30fps		0x014D	/*  33.3ms -> 30 fps */
#define IMX073FrTime_15fps		0x029A	/*  66.6ms -> 15 fps */
#define IMX073FrTime_7P5fps		0x0535	/*  133.3ms -> 7.5 fps */
#define IMX073FrTime_1P5fps		0x1964	/*  650.0ms -> 1.5 fps */
/*=====================================*/
/*========Register map for IMX073 EVT1(Don't modify)===========*/
#define IMX073_REG_TC_IPRM_InClockLSBs				0x0238
#define IMX073_REG_TC_IPRM_InClockMSBs				0x023A
#define IMX073_REG_TC_IPRM_UseNPviClocks			0x0252
#define IMX073_REG_TC_IPRM_UseNMipiClocks			0x0254
#define IMX073_REG_TC_IPRM_NumberOfMipiLanes			0x0256
#define IMX073_REG_TC_IPRM_OpClk4KHz_0				0x025A
#define IMX073_REG_TC_IPRM_MinOutRate4KHz_0			0x025C
#define IMX073_REG_TC_IPRM_MaxOutRate4KHz_0			0x025E
#define IMX073_REG_TC_IPRM_InitParamsUpdated			0x026E
#define IMX073_REG_TC_GP_EnablePreview				0x0280
#define IMX073_REG_TC_GP_EnablePreviewChanged			0x0282
#define IMX073_REG_TC_GP_NewConfigSync				0x0290
#define IMX073_REG_TC_GP_ActivePrevConfig			0x02A4
#define IMX073_REG_TC_GP_PrevConfigChanged			0x02A6
#define IMX073_REG_TC_GP_PrevOpenAfterChange			0x02A8
#define IMX073_REG_0TC_PCFG_usWidth				0x02E2
#define IMX073_REG_0TC_PCFG_usHeight				0x02E4
#define IMX073_REG_0TC_PCFG_Format				0x02E6
#define IMX073_REG_0TC_PCFG_usMaxOut4KHzRate			0x02E8
#define IMX073_REG_0TC_PCFG_usMinOut4KHzRate			0x02EA
#define IMX073_REG_0TC_PCFG_PVIMask				0x02F0
#define IMX073_REG_0TC_PCFG_uClockInd				0x02F8
#define IMX073_REG_0TC_PCFG_FrRateQualityType			0x02FC
#define IMX073_REG_0TC_PCFG_usFrTimeType			0x02FA
#define IMX073_REG_0TC_PCFG_usMaxFrTimeMsecMult10		0x02FE
#define IMX073_REG_0TC_PCFG_usMinFrTimeMsecMult10		0x0300

#define IMX073_PCLK_MIN	IMX0734Khz_115Mhz
#define IMX073_PCLK_MAX	IMX0734Khz_115Mhz

#define IMX073_FrTime_MAX	IMX073FrTime_30fps

/*
 * User defined commands
 */
/* repeat time for reading status */
#define CMD_DELAY_LONG		400	
#define CMD_DELAY_SHORT	200	

/* Following order should not be changed */
enum image_size_imx073 {
	/* This SoC supports upto SXGA (1280*1024) */
#if 0
	QQVGA, /* 160*120*/
	QCIF, /* 176*144 */
	QVGA, /* 320*240 */
	CIF, /* 352*288 */
#endif
	VGA, /* 640*480 */
#if 0
	SVGA, /* 800*600 */
	HD720P, /* 1280*720 */
	SXGA, /* 1280*1024 */
#endif
};

/*
 * Following values describe controls of camera
 * in user aspect and must be match with index of imx073_regset[]
 * These values indicates each controls and should be used
 * to control each control
 */
enum imx073_control {
	IMX073_INIT,
	IMX073_EV,
	IMX073_AWB,
	IMX073_MWB,
	IMX073_EFFECT,
	IMX073_CONTRAST,
	IMX073_SATURATION,
	IMX073_SHARPNESS,
};
enum  imx_smile_ctrl {
	SMILE_ON = 0,
	SMILE_OFF ,	
};
enum imx_blink {
	OFF,
	BLINK_ON,
	BLINK_OFF,
};

#define IMX073_REGSET(x)	{	\
	.regset = x,			\
	.len = sizeof(x)/sizeof(imx073_reg),}

/*
 * User tuned register setting values
 */
unsigned char imx073_init_reg1[][2] = {

};

unsigned short imx073_init_reg2[][2] = {
};

unsigned char imx073_init_reg3[][4] = {
};

unsigned short imx073_init_reg4[][2] = {
};

unsigned char imx073_init_reg5[][4] = {
};

unsigned char imx073_init_jpeg[][4] = {
};

unsigned short imx073_init_reg6[][2] = {
};

unsigned char imx073_init_reg7[][4] = {
};

unsigned short imx073_init_reg8[][2] = {
};

unsigned char imx073_init_reg9[][4] = {
};

unsigned short imx073_init_reg10[][2] = {
};

unsigned char imx073_init_reg11[][4] = {
};

#define IMX073_INIT_REGS1	\
	(sizeof(imx073_init_reg1) / sizeof(imx073_init_reg1[0]))
#define IMX073_INIT_REGS2	\
	(sizeof(imx073_init_reg2) / sizeof(imx073_init_reg2[0]))
#define IMX073_INIT_REGS3	\
	(sizeof(imx073_init_reg3) / sizeof(imx073_init_reg3[0]))
#define IMX073_INIT_REGS4	\
	(sizeof(imx073_init_reg4) / sizeof(imx073_init_reg4[0]))
#define IMX073_INIT_JPEG	\
	(sizeof(imx073_init_jpeg) / sizeof(imx073_init_jpeg[0]))
#define IMX073_INIT_REGS5\
	(sizeof(imx073_init_reg5) / sizeof(imx073_init_reg5[0]))
#define IMX073_INIT_REGS6	\
	(sizeof(imx073_init_reg6) / sizeof(imx073_init_reg6[0]))
#define IMX073_INIT_REGS7	\
	(sizeof(imx073_init_reg7) / sizeof(imx073_init_reg7[0]))
#define IMX073_INIT_REGS8	\
	(sizeof(imx073_init_reg8) / sizeof(imx073_init_reg8[0]))
#define IMX073_INIT_REGS9	\
	(sizeof(imx073_init_reg9) / sizeof(imx073_init_reg9[0]))
#define IMX073_INIT_REGS10	\
	(sizeof(imx073_init_reg10) / sizeof(imx073_init_reg10[0]))
#define IMX073_INIT_REGS11	\
	(sizeof(imx073_init_reg11) / sizeof(imx073_init_reg11[0]))

unsigned short imx073_sleep_reg[][2] = {
};

#define IMX073_SLEEP_REGS	\
	(sizeof(imx073_sleep_reg) / sizeof(imx073_sleep_reg[0]))

unsigned short imx073_wakeup_reg[][2] = {
};

#define IMX073_WAKEUP_REGS	\
	(sizeof(imx073_wakeup_reg) / sizeof(imx073_wakeup_reg[0]))

/* Preview configuration preset #1 */
/* Preview configuration preset #2 */
/* Preview configuration preset #3 */
/* Preview configuration preset #4 */

/* Capture configuration preset #0 */
/* Capture configuration preset #1 */
/* Capture configuration preset #2 */
/* Capture configuration preset #3 */
/* Capture configuration preset #4 */

#if 0
/*
 * EV bias
 */

static const struct imx073_reg imx073_ev_m6[] = {
};

static const struct imx073_reg imx073_ev_m5[] = {
};

static const struct imx073_reg imx073_ev_m4[] = {
};

static const struct imx073_reg imx073_ev_m3[] = {
};

static const struct imx073_reg imx073_ev_m2[] = {
};

static const struct imx073_reg imx073_ev_m1[] = {
};

static const struct imx073_reg imx073_ev_default[] = {
};

static const struct imx073_reg imx073_ev_p1[] = {
};

static const struct imx073_reg imx073_ev_p2[] = {
};

static const struct imx073_reg imx073_ev_p3[] = {
};

static const struct imx073_reg imx073_ev_p4[] = {
};

static const struct imx073_reg imx073_ev_p5[] = {
};

static const struct imx073_reg imx073_ev_p6[] = {
};

/* Order of this array should be following the querymenu data */
static const unsigned char *imx073_regs_ev_bias[] = {
	(unsigned char *)imx073_ev_m6, (unsigned char *)imx073_ev_m5,
	(unsigned char *)imx073_ev_m4, (unsigned char *)imx073_ev_m3,
	(unsigned char *)imx073_ev_m2, (unsigned char *)imx073_ev_m1,
	(unsigned char *)imx073_ev_default, (unsigned char *)imx073_ev_p1,
	(unsigned char *)imx073_ev_p2, (unsigned char *)imx073_ev_p3,
	(unsigned char *)imx073_ev_p4, (unsigned char *)imx073_ev_p5,
	(unsigned char *)imx073_ev_p6,
};

/*
 * Color Effect (COLORFX)
 */
static const struct imx073_reg imx073_color_sepia[] = {
};

static const struct imx073_reg imx073_color_aqua[] = {
};

static const struct imx073_reg imx073_color_monochrome[] = {
};

static const struct imx073_reg imx073_color_negative[] = {
};

static const struct imx073_reg imx073_color_sketch[] = {
};

/* Order of this array should be following the querymenu data */
static const unsigned char *imx073_regs_color_effect[] = {
	(unsigned char *)imx073_color_sepia,
	(unsigned char *)imx073_color_aqua,
	(unsigned char *)imx073_color_monochrome,
	(unsigned char *)imx073_color_negative,
	(unsigned char *)imx073_color_sketch,
};

/*
 * Contrast bias
 */
static const struct imx073_reg imx073_contrast_m2[] = {
};

static const struct imx073_reg imx073_contrast_m1[] = {
};

static const struct imx073_reg imx073_contrast_default[] = {
};

static const struct imx073_reg imx073_contrast_p1[] = {
};

static const struct imx073_reg imx073_contrast_p2[] = {
};

static const unsigned char *imx073_regs_contrast_bias[] = {
	(unsigned char *)imx073_contrast_m2,
	(unsigned char *)imx073_contrast_m1,
	(unsigned char *)imx073_contrast_default,
	(unsigned char *)imx073_contrast_p1,
	(unsigned char *)imx073_contrast_p2,
};

/*
 * Saturation bias
 */
static const struct imx073_reg imx073_saturation_m2[] = {
};

static const struct imx073_reg imx073_saturation_m1[] = {
};

static const struct imx073_reg imx073_saturation_default[] = {
};

static const struct imx073_reg imx073_saturation_p1[] = {
};

static const struct imx073_reg imx073_saturation_p2[] = {
};

static const unsigned char *imx073_regs_saturation_bias[] = {
	(unsigned char *)imx073_saturation_m2,
	(unsigned char *)imx073_saturation_m1,
	(unsigned char *)imx073_saturation_default,
	(unsigned char *)imx073_saturation_p1,
	(unsigned char *)imx073_saturation_p2,
};

/*
 * Sharpness bias
 */
static const struct imx073_reg imx073_sharpness_m2[] = {
};

static const struct imx073_reg imx073_sharpness_m1[] = {
};

static const struct imx073_reg imx073_sharpness_default[] = {
};

static const struct imx073_reg imx073_sharpness_p1[] = {
};

static const struct imx073_reg imx073_sharpness_p2[] = {
};

static const unsigned char *imx073_regs_sharpness_bias[] = {
	(unsigned char *)imx073_sharpness_m2,
	(unsigned char *)imx073_sharpness_m1,
	(unsigned char *)imx073_sharpness_default,
	(unsigned char *)imx073_sharpness_p1,
	(unsigned char *)imx073_sharpness_p2,
};
#endif

#endif
