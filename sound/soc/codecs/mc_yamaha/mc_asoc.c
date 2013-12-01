/*
 * MC ASoC codec driver
 *
 * Copyright (c) 2011-2012 Yamaha Corporation
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.	In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *	claim that you wrote the original software. If you use this software
 *	in a product, an acknowledgment in the product documentation would be
 *	appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *	misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/switch.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/sysdev.h>
#include <linux/io.h>
#include <linux/wakelock.h>
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>
#include <mach/regs-clock.h>
#include <mach/regs-pmu-4212.h>
#include <sound/hwdep.h>
#include <sound/initval.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <sound/jack.h>
#include "mcresctrl.h"
#include "mc_asoc_cfg.h"
#include "mc_asoc_path_cfg.h"
#include "mc_asoc.h"
#include "mc_asoc_priv.h"
#include "mc_asoc_parser.h"
#include "ladonhf30.h"
#include "ladonhf31.h"

#define MC_ASOC_DRIVER_VERSION		"3.1.1"

#define MC_ASOC_I2S_RATE			(SNDRV_PCM_RATE_8000_48000)
#define MC_ASOC_I2S_FORMATS			(SNDRV_PCM_FMTBIT_S16_LE | \
									 SNDRV_PCM_FMTBIT_S20_3LE | \
									 SNDRV_PCM_FMTBIT_S24_3LE)

#define MC_ASOC_PCM_RATE			(SNDRV_PCM_RATE_8000 | SNDRV_PCM_RATE_16000)
#define MC_ASOC_PCM_FORMATS			(SNDRV_PCM_FMTBIT_S8 | \
									 SNDRV_PCM_FMTBIT_S16_LE | \
									 SNDRV_PCM_FMTBIT_S16_BE | \
									 SNDRV_PCM_FMTBIT_A_LAW | \
									 SNDRV_PCM_FMTBIT_MU_LAW)

#define MC_ASOC_HWDEP_ID 			"mc_asoc"
#define MC_ASOC_POWER_SET
#define	DIO_0						(0)
#define	DIO_1						(1)
#define	DIO_2						(2)
#define	LIN1						(3)
#define	LIN2						(4)
#define	LOUT1						(3)
#define	LOUT2						(4)
#define	LIN1_LOUT1					(3)
#define	LIN2_LOUT1					(4)
#define	LIN1_LOUT2					(5)
#define	LIN2_LOUT2					(6)
#define	AE_SRC_NONE					(0)
#define	AE_SRC_DIR0					(1)
#define	AE_SRC_DIR1					(2)
#define	AE_SRC_DIR2					(3)
#define	AE_SRC_MIX					(4)
#define	AE_SRC_PDM					(5)
#define	AE_SRC_ADC0					(6)
#define	AE_SRC_ADC1					(7)
#define	AE_SRC_DTMF					(8)
#define	AE_SRC_CDSP					(9)

#define mc_asoc_is_in_playback(p)	((p)->stream & (1 << SNDRV_PCM_STREAM_PLAYBACK))
#define mc_asoc_is_in_capture(p)	((p)->stream & (1 << SNDRV_PCM_STREAM_CAPTURE))
#ifdef KERNEL_2_6_35_OMAP
#define get_port_id(id) ((id)/2)
#else
#define get_port_id(id) (id-1)
#endif


struct mc_asoc_info_store {
	UINT32	get;
	UINT32	set;
	size_t	offset;
	UINT32	flags;
};
static struct mc_asoc_info_store	mc_asoc_info_store_tbl[]	= {
	{MCDRV_GET_PLL,			MCDRV_SET_PLL,			offsetof(struct mc_asoc_data, pll_store),		0},

	{MCDRV_GET_DIGITALIO,	MCDRV_SET_DIGITALIO,	offsetof(struct mc_asoc_data, dio_store),		0x1ff},
	{MCDRV_GET_DAC,			MCDRV_SET_DAC,			offsetof(struct mc_asoc_data, dac_store),		0x7},
	{MCDRV_GET_ADC,			MCDRV_SET_ADC,			offsetof(struct mc_asoc_data, adc_store),		0x7},
	{MCDRV_GET_ADC_EX,		MCDRV_SET_ADC_EX,		offsetof(struct mc_asoc_data, adc_ex_store),	0x3f},
	{MCDRV_GET_SP,			MCDRV_SET_SP,			offsetof(struct mc_asoc_data, sp_store),		0},
	{MCDRV_GET_DNG,			MCDRV_SET_DNG,			offsetof(struct mc_asoc_data, dng_store),		0x7f3f3f},
	{MCDRV_GET_SYSEQ,		MCDRV_SET_SYSEQ,		offsetof(struct mc_asoc_data, syseq_store),		0x3},
	{0,						MCDRV_SET_AUDIOENGINE,	offsetof(struct mc_asoc_data, ae_store),		0x1ff},
	{MCDRV_GET_PDM,			MCDRV_SET_PDM,		 	offsetof(struct mc_asoc_data, pdm_store),		0x7f},
	{MCDRV_GET_PDM_EX,		MCDRV_SET_PDM_EX,	 	offsetof(struct mc_asoc_data, pdm_ex_store),	0x3f},
	{MCDRV_GET_PATH,		MCDRV_SET_PATH,		 	offsetof(struct mc_asoc_data, path_store),		0},
	{MCDRV_GET_VOLUME, 		MCDRV_SET_VOLUME,	 	offsetof(struct mc_asoc_data, vol_store),		0},
	{MCDRV_GET_DTMF,		MCDRV_SET_DTMF,		 	offsetof(struct mc_asoc_data, dtmf_store),		0xff},
	{MCDRV_GET_DITSWAP,		MCDRV_SET_DITSWAP,	 	offsetof(struct mc_asoc_data, ditswap_store),	0x3},
	{MCDRV_GET_HSDET,		MCDRV_SET_HSDET,	 	offsetof(struct mc_asoc_data, hsdet_store),		0x1fffffff},
};
#define MC_ASOC_N_INFO_STORE \
			(sizeof(mc_asoc_info_store_tbl) / sizeof(struct mc_asoc_info_store))

/* volmap for Digital Volumes */
static SINT16	mc_asoc_vol_digital[]	= {
	0xa000, 0xb600, 0xb700, 0xb800, 0xb900, 0xba00, 0xbb00, 0xbc00,
	0xbd00, 0xbe00, 0xbf00, 0xc000, 0xc100, 0xc200, 0xc300, 0xc400,
	0xc500, 0xc600, 0xc700, 0xc800, 0xc900, 0xca00, 0xcb00, 0xcc00,
	0xcd00, 0xce00, 0xcf00, 0xd000, 0xd100, 0xd200, 0xd300, 0xd400,
	0xd500, 0xd600, 0xd700, 0xd800, 0xd900, 0xda00, 0xdb00, 0xdc00,
	0xdd00, 0xde00, 0xdf00, 0xe000, 0xe100, 0xe200, 0xe300, 0xe400,
	0xe500, 0xe600, 0xe700, 0xe800, 0xe900, 0xea00, 0xeb00, 0xec00,
	0xed00, 0xee00, 0xef00, 0xf000, 0xf100, 0xf200, 0xf300, 0xf400,
	0xf500, 0xf600, 0xf700, 0xf800, 0xf900, 0xfa00, 0xfb00, 0xfc00,
	0xfd00, 0xfe00, 0xff00, 0x0000, 0x0100, 0x0200, 0x0300, 0x0400,
	0x0500, 0x0600, 0x0700, 0x0800, 0x0900, 0x0a00, 0x0b00, 0x0c00,
	0x0d00, 0x0e00, 0x0f00, 0x1000, 0x1100, 0x1200,
};

/* volmap for ADC Analog Volume */
static SINT16	mc_asoc_vol_adc[]	= {
	0xa000, 0xe500, 0xe680, 0xe800, 0xe980, 0xeb00, 0xec80, 0xee00,
	0xef80, 0xf100, 0xf280, 0xf400, 0xf580, 0xf700, 0xf880, 0xfa00,
	0xfb80, 0xfd00, 0xfe80, 0x0000, 0x0180, 0x0300, 0x0480, 0x0600,
	0x0780, 0x0900, 0x0a80, 0x0c00, 0x0d80, 0x0f00, 0x1080, 0x1200,
};

/* volmap for LINE/MIC Input Volumes */
static SINT16	mc_asoc_vol_ain[]	= {
	0xa000, 0xe200, 0xe380, 0xe500, 0xe680, 0xe800, 0xe980, 0xeb00,
	0xec80, 0xee00, 0xef80, 0xf100, 0xf280, 0xf400, 0xf580, 0xf700,
	0xf880, 0xfa00, 0xfb80, 0xfd00, 0xfe80, 0x0000, 0x0180, 0x0300,
	0x0480, 0x0600, 0x0780, 0x0900, 0x0a80, 0x0c00, 0x0d80, 0x0f00,
};

/* volmap for HP/SP/RC Output Volumes */
static SINT16	mc_asoc_vol_hpsprc[]	= {
	0xa000, 0xdc00, 0xe400, 0xe800, 0xea00, 0xec00, 0xee00, 0xf000,
	0xf100, 0xf200, 0xf300, 0xf400, 0xf500, 0xf600, 0xf700, 0xf800,
	0xf880, 0xf900, 0xf980, 0xfa00, 0xfa80, 0xfb00, 0xfb80, 0xfc00,
	0xfc80, 0xfd00, 0xfd80, 0xfe00, 0xfe80, 0xff00, 0xff80, 0x0000,
};

/* volmap for LINE Output Volumes */
static SINT16	mc_asoc_vol_aout[]	= {
	0xa000, 0xe200, 0xe300, 0xe400, 0xe500, 0xe600, 0xe700, 0xe800,
	0xe900, 0xea00, 0xeb00, 0xec00, 0xed00, 0xee00, 0xef00, 0xf000,
	0xf100, 0xf200, 0xf300, 0xf400, 0xf500, 0xf600, 0xf700, 0xf800,
	0xf900, 0xfa00, 0xfb00, 0xfc00, 0xfd00, 0xfe00, 0xff00, 0x0000,
};

/* volmap for MIC Gain Volumes */
static SINT16	mc_asoc_vol_micgain[]	= {
	0x0f00, 0x1400, 0x1900, 0x1e00,
};

/* volmap for HP Gain Volume */
static SINT16	mc_asoc_vol_hpgain[]	= {
	0x0000, 0x0180, 0x0300, 0x0600,
};

struct mc_asoc_vreg_info {
	size_t	offset;
	SINT16	*volmap;
	UINT8	channels;
};
static struct mc_asoc_vreg_info	mc_asoc_vreg_map[MC_ASOC_N_VOL_REG]	= {
	{offsetof(MCDRV_VOL_INFO, aswD_Ad0),		mc_asoc_vol_digital,	AD0_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Adc1),		mc_asoc_vol_digital,	AD1_DVOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Aeng6),		mc_asoc_vol_digital,	AENG6_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Pdm),		mc_asoc_vol_digital,	PDM_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Dtmfb),		mc_asoc_vol_digital,	DTMF_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Dir0),		mc_asoc_vol_digital,	DIO0_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Dir1),		mc_asoc_vol_digital,	DIO1_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Dir2),		mc_asoc_vol_digital,	DIO2_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Ad0Att),		mc_asoc_vol_digital,	AD0_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Adc1Att),	mc_asoc_vol_digital,	AD1_DVOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_DtmfAtt),	mc_asoc_vol_digital,	DTFM_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Dir0Att),	mc_asoc_vol_digital,	DIO0_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Dir1Att),	mc_asoc_vol_digital,	DIO1_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Dir2Att),	mc_asoc_vol_digital,	DIO2_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_SideTone),	mc_asoc_vol_digital,	PDM_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_DacMaster),	mc_asoc_vol_digital,	DAC_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_DacVoice),	mc_asoc_vol_digital,	DAC_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_DacAtt),		mc_asoc_vol_digital,	DAC_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Dit0),		mc_asoc_vol_digital,	DIO0_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Dit1),		mc_asoc_vol_digital,	DIO1_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Dit2),		mc_asoc_vol_digital,	DIO2_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Dit21),		mc_asoc_vol_digital,	DIO2_VOL_CHANNELS},

	{(size_t)-1,								mc_asoc_vol_digital,	DIO0_VOL_CHANNELS},
	{(size_t)-1,								mc_asoc_vol_digital,	DIO1_VOL_CHANNELS},
	{(size_t)-1,								mc_asoc_vol_digital,	AD0_VOL_CHANNELS},
	{(size_t)-1,								mc_asoc_vol_digital,	AD0_VOL_CHANNELS},

	{offsetof(MCDRV_VOL_INFO, aswA_Ad0),		mc_asoc_vol_adc,		AD0_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswA_Ad1),		mc_asoc_vol_adc,		AD1_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswA_Lin1),		mc_asoc_vol_ain,		LIN1_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswA_Lin2),		mc_asoc_vol_ain,		LIN2_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswA_Mic1),		mc_asoc_vol_ain,		MIC1_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswA_Mic2),		mc_asoc_vol_ain,		MIC2_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswA_Mic3),		mc_asoc_vol_ain,		MIC3_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswA_Hp),			mc_asoc_vol_hpsprc,		HP_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswA_Sp),			mc_asoc_vol_hpsprc,		SP_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswA_Rc),			mc_asoc_vol_hpsprc,		RC_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswA_Lout1),	 	mc_asoc_vol_aout,		LOUT1_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswA_Lout2),	 	mc_asoc_vol_aout,		LOUT2_VOL_CHANNELS},

	{(size_t)-1,								mc_asoc_vol_adc,		AD0_VOL_CHANNELS},

	{offsetof(MCDRV_VOL_INFO, aswA_Mic1Gain),	mc_asoc_vol_micgain,	MIC1_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswA_Mic2Gain),	mc_asoc_vol_micgain,	MIC2_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswA_Mic3Gain),	mc_asoc_vol_micgain,	MIC3_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswA_HpGain),		mc_asoc_vol_hpgain,		HPGAIN_VOL_CHANNELS},

	{offsetof(MCDRV_VOL_INFO, aswD_Adc0AeMix),	mc_asoc_vol_digital,	MIX2_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Adc1AeMix),	mc_asoc_vol_digital,	MIX2_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Dir0AeMix),	mc_asoc_vol_digital,	MIX2_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Dir1AeMix),	mc_asoc_vol_digital,	MIX2_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_Dir2AeMix),	mc_asoc_vol_digital,	MIX2_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_PdmAeMix),	mc_asoc_vol_digital,	MIX2_VOL_CHANNELS},
	{offsetof(MCDRV_VOL_INFO, aswD_MixAeMix),	mc_asoc_vol_digital,	MIX2_VOL_CHANNELS}
};

#ifdef ALSA_VER_1_0_23
#else
struct mc_asoc_priv {
	struct mc_asoc_data		data;
/*	struct mc_asoc_setup	setup;*/
};
#endif

static struct i2c_client	*mc_asoc_i2c_d;
static struct i2c_client	*mc_asoc_i2c_a;

static UINT8	mc_asoc_hwid	= 0;
static UINT8	mc_asoc_hwid_a	= 0;

static UINT8	mc_asoc_cdsp_ae_ex_flag	= MC_ASOC_CDSP_AE_EX_OFF;
static UINT8	mc_asoc_ae_dir			= MC_ASOC_AE_NONE;
static UINT8	mc_asoc_hold			= YMC_NOTITY_HOLD_OFF;

static MCDRV_VOL_INFO	mc_asoc_vol_info_mute;

#if (defined ALSA_VER_1_0_23) || (defined KERNEL_2_6_35_36_COMMON)||(defined MC_ASOC_POWER_SET)
static struct snd_soc_codec	*mc_asoc_codec	= 0;
#endif

static UINT8	mc_asoc_main_mic		= MAIN_MIC;
static UINT8	mc_asoc_sub_mic			= SUB_MIC;
static UINT8	mc_asoc_hs_mic			= HEADSET_MIC;
static UINT8	mc_asoc_mic1_bias		= MIC1_BIAS;
static UINT8	mc_asoc_mic2_bias		= MIC2_BIAS;
static UINT8	mc_asoc_mic3_bias		= MIC3_BIAS;
static UINT8	mc_asoc_cap_path		= CAP_PATH_8KHZ;
static UINT8	mc_asoc_ap_cap_port_rate= MCDRV_FS_48000;

static UINT8	mc_asoc_ap_play_port	= (UINT8)-1;
static UINT8	mc_asoc_ap_cap_port		= (UINT8)-1;
static UINT8	mc_asoc_cp_port			= (UINT8)-1;
static UINT8	mc_asoc_bt_port			= (UINT8)-1;

static int	mc_asoc_set_bias_level(struct snd_soc_codec *codec, enum snd_soc_bias_level level);
unsigned char mc_asoc_audio_mode_incall(void);

/*
 * Function
 */
#if (defined ALSA_VER_1_0_23) || (defined KERNEL_2_6_35_36_COMMON) ||(defined MC_ASOC_POWER_SET)
static struct snd_soc_codec* mc_asoc_get_codec_data(void)
{
	return mc_asoc_codec;
}

static void mc_asoc_set_codec_data(struct snd_soc_codec *codec)
{
	mc_asoc_codec	= codec;
}
#endif

static struct mc_asoc_data* mc_asoc_get_mc_asoc(const struct snd_soc_codec *codec)
{
#if (defined ALSA_VER_1_0_23)
	if(codec == NULL) {
		return NULL;
	}
	return codec->private_data;
#elif (defined KERNEL_2_6_35_36_COMMON)
	if(codec == NULL) {
		return NULL;
	}
	return snd_soc_codec_get_drvdata((struct snd_soc_codec *)codec);
#else
	struct mc_asoc_priv	*priv;

	if(codec == NULL) {
		return NULL;
	}
	priv	= snd_soc_codec_get_drvdata((struct snd_soc_codec *)codec);
	if(priv != NULL) {
		return &priv->data;
	}
#endif
	return NULL;
}

/* deliver i2c access to machdep */
struct i2c_client* mc_asoc_get_i2c_client(int slave)
{
	if (slave == 0x11){
		return mc_asoc_i2c_d;
	}
	if (slave == 0x3a){
		return mc_asoc_i2c_a;
	}

#ifdef ALSA_VER_1_0_23
	if(mc_asoc_codec != NULL) {
		return mc_asoc_codec->control_data;
	}
#endif
	return NULL;
}

int mc_asoc_map_drv_error(int err)
{
	switch (err) {
	case MCDRV_SUCCESS:
		return 0;
	case MCDRV_ERROR_ARGUMENT:
		return -EINVAL;
	case MCDRV_ERROR_STATE:
		return -EBUSY;
	case MCDRV_ERROR_TIMEOUT:
		return -EIO;
	default:
		/* internal error */
		return -EIO;
	}
}


static int read_cache(
	struct snd_soc_codec *codec,
	unsigned int reg)
{
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON) || defined(KERNEL_2_6_35_OMAP)
	if(reg < MC_ASOC_N_REG) {
		return ((u16 *)codec->reg_cache)[reg];
	}
	return -EINVAL;
#else
	int		ret;
	unsigned int	val;

	ret	= snd_soc_cache_read(codec, reg, &val);
	if (ret != 0) {
		dev_err(codec->dev, "Cache read to %x failed: %d\n", reg, ret);
		return -EIO;
	}
	return val;
#endif
}

static int write_cache(
	struct snd_soc_codec *codec, 
	unsigned int reg,
	unsigned int value)
{
	if(reg == MC_ASOC_INCALL_MIC_SP
	|| reg == MC_ASOC_INCALL_MIC_RC
	|| reg == MC_ASOC_INCALL_MIC_HP
	|| reg == MC_ASOC_INCALL_MIC_LO1
	|| reg == MC_ASOC_INCALL_MIC_LO2
	|| reg == MC_ASOC_VOICE_RECORDING) {
dbg_info("write cache(%d, %d)\n", reg, value);
	}

#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON) || defined(KERNEL_2_6_35_OMAP)
	if(reg < MC_ASOC_N_REG) {
		((u16 *)codec->reg_cache)[reg]	= value;
	} else {
		return -EINVAL;
	}
	return 0;
#else
	return (snd_soc_cache_write(codec, reg, value));
#endif
}

static int get_port_block(UINT8 port)
{
	switch(port) {
	case	DIO_0:
		return MCDRV_SRC_DIR0_BLOCK;
	case	DIO_1:
		return MCDRV_SRC_DIR1_BLOCK;
	case	DIO_2:
		return MCDRV_SRC_DIR2_BLOCK;
	default:
		return -1;
	}
}
static int get_port_block_on(UINT8 port)
{
	switch(port) {
	case	DIO_0:
		return MCDRV_SRC3_DIR0_ON;
	case	DIO_1:
		return MCDRV_SRC3_DIR1_ON;
	case	DIO_2:
		return MCDRV_SRC3_DIR2_ON;
	default:
		return -1;
	}
}
static size_t get_port_srconoff_offset(UINT8 port)
{
	switch(port) {
	case	DIO_0:
		return offsetof(MCDRV_PATH_INFO, asDit0);
	case	DIO_1:
		return offsetof(MCDRV_PATH_INFO, asDit1);
	case	DIO_2:
		return offsetof(MCDRV_PATH_INFO, asDit2);
	default:
		return -1;
	}
}
static int get_ap_play_port(void)
{
	switch(mc_asoc_ap_play_port) {
	case	DIO_0:
	case	DIO_1:
	case	DIO_2:
		return mc_asoc_ap_play_port;
	default:
		return -1;
	}
}
static int get_ap_cap_port(void)
{
	switch(mc_asoc_ap_cap_port) {
	case	DIO_0:
	case	DIO_1:
	case	DIO_2:
		return mc_asoc_ap_cap_port;
	default:
		return -1;
	}
}
static int get_cp_port(void)
{
	switch(mc_asoc_cp_port) {
	case	DIO_0:
	case	DIO_1:
	case	DIO_2:
		return mc_asoc_cp_port;
	default:
		return -1;
	}
}

static int get_bt_port(void)
{
	return mc_asoc_bt_port;
}
static int get_mic_block_on(UINT8 mic)
{
	switch(mic) {
	case	MIC_1:
		return MCDRV_SRC0_MIC1_ON;
	case	MIC_2:
		return MCDRV_SRC0_MIC2_ON;
	case	MIC_3:
		return MCDRV_SRC0_MIC3_ON;
	default:
		return -1;
	}
}

static int get_main_mic_block_on(void)
{
	return get_mic_block_on(mc_asoc_main_mic);
}
static int get_sub_mic_block_on(void)
{
	return get_mic_block_on(mc_asoc_sub_mic);
}
static int get_hs_mic_block_on(void)
{
	return get_mic_block_on(mc_asoc_hs_mic);
}


static UINT8 get_incall_mic(
	struct snd_soc_codec *codec,
	int	output_path
)
{
	switch(output_path) {
	case	MC_ASOC_OUTPUT_PATH_SP:
		return read_cache(codec, MC_ASOC_INCALL_MIC_SP);
		break;
	case	MC_ASOC_OUTPUT_PATH_RC:
	case	MC_ASOC_OUTPUT_PATH_SP_RC:
		return read_cache(codec, MC_ASOC_INCALL_MIC_RC);
		break;
	case	MC_ASOC_OUTPUT_PATH_HP:
	case	MC_ASOC_OUTPUT_PATH_SP_HP:
		return read_cache(codec, MC_ASOC_INCALL_MIC_HP);
		break;
	case	MC_ASOC_OUTPUT_PATH_LO1:
	case	MC_ASOC_OUTPUT_PATH_SP_LO1:
		return read_cache(codec, MC_ASOC_INCALL_MIC_LO1);
		break;
	case	MC_ASOC_OUTPUT_PATH_LO2:
	case	MC_ASOC_OUTPUT_PATH_SP_LO2:
		return read_cache(codec, MC_ASOC_INCALL_MIC_LO2);
		break;
	case	MC_ASOC_OUTPUT_PATH_HS:
	case	MC_ASOC_OUTPUT_PATH_BT:
	case	MC_ASOC_OUTPUT_PATH_SP_BT:
		return MC_ASOC_INCALL_MIC_MAINMIC;
	default:
		break;
	}
	return -EIO;
}

struct mc_asoc_mixer_ctl_info {
	int		audio_mode_play;
	int		audio_mode_cap;
	int		output_path;
	int		input_path;
	int		incall_mic;
	int		mainmic_play;
	int		submic_play;
	int		msmic_play;
	int		hsmic_play;
	int		btmic_play;
	int		lin1_play;
	int		lin2_play;
	int		dtmf_control;
	int		dtmf_output;
};

static int get_mixer_ctl_info(
	struct snd_soc_codec *codec,
	struct mc_asoc_mixer_ctl_info	*mixer_ctl_info
)
{
	TRACE_FUNC();

	if((mixer_ctl_info->audio_mode_play = read_cache(codec, MC_ASOC_AUDIO_MODE_PLAY)) < 0) {
		return -EIO;
	}
	if((mixer_ctl_info->audio_mode_cap = read_cache(codec, MC_ASOC_AUDIO_MODE_CAP)) < 0) {
		return -EIO;
	}
	if((mixer_ctl_info->output_path = read_cache(codec, MC_ASOC_OUTPUT_PATH)) < 0) {
		return -EIO;
	}
	if((mixer_ctl_info->input_path = read_cache(codec, MC_ASOC_INPUT_PATH)) < 0) {
		return -EIO;
	}
	if((mixer_ctl_info->incall_mic = get_incall_mic(codec, mixer_ctl_info->output_path)) < 0) {
		return -EIO;
	}
	if((mixer_ctl_info->dtmf_control = read_cache(codec, MC_ASOC_DTMF_CONTROL)) < 0) {
		return -EIO;
	}
	if((mixer_ctl_info->dtmf_output = read_cache(codec, MC_ASOC_DTMF_OUTPUT)) < 0) {
		return -EIO;
	}

	if((mixer_ctl_info->mainmic_play = read_cache(codec, MC_ASOC_MAINMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((mixer_ctl_info->submic_play = read_cache(codec, MC_ASOC_SUBMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((mixer_ctl_info->msmic_play = read_cache(codec, MC_ASOC_2MIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((mixer_ctl_info->hsmic_play = read_cache(codec, MC_ASOC_HSMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((mixer_ctl_info->btmic_play = read_cache(codec, MC_ASOC_BTMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((mixer_ctl_info->lin1_play = read_cache(codec, MC_ASOC_LIN1_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((mixer_ctl_info->lin2_play = read_cache(codec, MC_ASOC_LIN2_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	return 0;
}

static void get_voicecall_index(
	int	audio_mode_play,
	int	audio_mode_cap,
	int	output_path,
	int	input_path,
	int	incall_mic,
	int	msmic_play,
	int	*base,
	int	*user
)
{
	*base	= 0;

	if(((audio_mode_play == MC_ASOC_AUDIO_MODE_INCOMM) && (audio_mode_cap == MC_ASOC_AUDIO_MODE_INCOMM))
	|| (((audio_mode_play == MC_ASOC_AUDIO_MODE_INCALL) || (audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO_INCALL))
		&& ((audio_mode_cap == MC_ASOC_AUDIO_MODE_INCALL) || (audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)))) {
		switch(output_path) {
		case	MC_ASOC_OUTPUT_PATH_SP:
			if((incall_mic == 0) || (incall_mic == 1)) {
				/*	MainMIC or SubMIC	*/
				*user	= 2;
			}
			else {
				/*	2MIC	*/
				*user	= 3;
				*base	= 1;
			}
			break;
		case	MC_ASOC_OUTPUT_PATH_RC:
		case	MC_ASOC_OUTPUT_PATH_SP_RC:
			if((incall_mic == 0) || (incall_mic == 1)) {
				/*	MainMIC or SubMIC	*/
				*user	= 4;
			}
			else {
				/*	2MIC	*/
				*user	= 5;
				*base	= 1;
			}
			break;
		case	MC_ASOC_OUTPUT_PATH_HP:
		case	MC_ASOC_OUTPUT_PATH_SP_HP:
			if((incall_mic == 0) || (incall_mic == 1)) {
				/*	MainMIC or SubMIC	*/
				*user	= 6;
			}
			else {
				/*	2MIC	*/
				*user	= 7;
				*base	= 1;
			}
			break;
		case	MC_ASOC_OUTPUT_PATH_LO1:
		case	MC_ASOC_OUTPUT_PATH_SP_LO1:
			if((incall_mic == 0) || (incall_mic == 1)) {
				/*	MainMIC or SubMIC	*/
				*user	= 8;
			}
			else {
				/*	2MIC	*/
				*user	= 9;
				*base	= 1;
			}
			break;
		case	MC_ASOC_OUTPUT_PATH_LO2:
		case	MC_ASOC_OUTPUT_PATH_SP_LO2:
			if((incall_mic == 0) || (incall_mic == 1)) {
				/*	MainMIC or SubMIC	*/
				*user	= 10;
			}
			else {
				/*	2MIC	*/
				*user	= 11;
				*base	= 1;
			}
			break;
		case	MC_ASOC_OUTPUT_PATH_HS:
			*user	= 12;
			break;
		case	MC_ASOC_OUTPUT_PATH_BT:
		case	MC_ASOC_OUTPUT_PATH_SP_BT:
			*user	= 13;
			break;

		default:
			break;
		}
		return;
	}

	if((audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO)
	|| (audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)) {
		if(input_path == MC_ASOC_INPUT_PATH_2MIC) {
			*base	= 1;
			return;
		}
		else if((input_path == MC_ASOC_INPUT_PATH_MAINMIC)
			 || (input_path == MC_ASOC_INPUT_PATH_SUBMIC)
			 || (input_path == MC_ASOC_INPUT_PATH_HS)
			 || (input_path == MC_ASOC_INPUT_PATH_LIN1)
			 || (input_path == MC_ASOC_INPUT_PATH_LIN2)) {
			return;
		}
	}
	if(msmic_play == 1) {
		*base	= 1;
	}
}

static void set_vol_mute_flg(size_t offset, UINT8 lr, UINT8 mute)
{
	SINT16	*vp	= (SINT16*)((void*)&mc_asoc_vol_info_mute+offset);

	if(offset == (size_t)-1) {
		return;
	}
	if(mute == 1) {
		*(vp+lr)	= MCDRV_LOGICAL_VOL_MUTE;
	}
	else {
		*(vp+lr)	= 0;
	}
}

static UINT8 get_vol_mute_flg(size_t offset, UINT8 lr)
{
	SINT16	*vp;

	if(offset == (size_t)-1) {
		return 1;	/*	mute	*/
	}

	vp	= (SINT16*)((void*)&mc_asoc_vol_info_mute+offset);
	return (*(vp+lr) != 0);
}

static int set_volume(struct snd_soc_codec *codec)
{
	MCDRV_VOL_INFO	vol_info;
	SINT16	*vp;
	int		err, i;
	int		reg;
	int		cache;

	TRACE_FUNC();

	memset(&vol_info, 0, sizeof(MCDRV_VOL_INFO));

	for(reg = MC_ASOC_DVOL_AD0; reg < MC_ASOC_N_VOL_REG; reg++) {
		if(mc_asoc_vreg_map[reg].offset != (size_t)-1) {
			vp		= (SINT16 *)((void *)&vol_info + mc_asoc_vreg_map[reg].offset);
			cache	= read_cache(codec, reg);
			if(cache < 0) {
				return -EIO;
			}

			for (i = 0; i < mc_asoc_vreg_map[reg].channels; i++, vp++) {
				unsigned int	v	= (cache >> (i*8)) & 0xff;
				int		sw, vol;
				SINT16	db;
				sw	= (reg < MC_ASOC_AVOL_MIC1_GAIN) ? (v & 0x80) : 1;
				if(reg == MC_ASOC_DVOL_AENG6) {
					if(sw != 0) {
						sw	= read_cache(codec, MC_ASOC_VOICE_RECORDING);
					}
				}
				switch(get_bt_port()) {
				case	DIO_0:
					if(reg == MC_ASOC_DVOL_DIR0) {
						if(sw != 0) {
							sw	= read_cache(codec, MC_ASOC_VOICE_RECORDING);
						}
					}
					break;
				case	DIO_1:
					if(reg == MC_ASOC_DVOL_DIR1) {
						if(sw != 0) {
							sw	= read_cache(codec, MC_ASOC_VOICE_RECORDING);
						}
					}
					break;
				case	DIO_2:
					if(reg == MC_ASOC_DVOL_DIR2) {
						if(sw != 0) {
							sw	= read_cache(codec, MC_ASOC_VOICE_RECORDING);
						}
					}
					break;
				default:
					break;
				}
				vol	= sw ? (v & 0x7f) : 0;
				if(get_vol_mute_flg(mc_asoc_vreg_map[reg].offset, i) != 0) {
					db	= mc_asoc_vreg_map[reg].volmap[0];
				}
				else {
					db	= mc_asoc_vreg_map[reg].volmap[vol];
				}
				*vp	= db | MCDRV_VOL_UPDATE;
			}
		}
	}

	err	= _McDrv_Ctrl(MCDRV_SET_VOLUME, &vol_info, 0);
	if (err != MCDRV_SUCCESS) {
		dev_err(codec->dev, "%d: Error in MCDRV_SET_VOLUME\n", err);
		return -EIO;
	}

	return 0;
}

static int set_vreg_map(
	struct snd_soc_codec	*codec,
	const MCDRV_PATH_INFO	*path_info,
	int	cdsp_onoff,
	int	preset_idx
)
{
	int		input_path;
	int		ain_play	= 0;
	struct mc_asoc_data	*mc_asoc	= mc_asoc_get_mc_asoc(codec);
	int		mainmic_play, submic_play, msmic_play, hsmic_play, btmic_play, lin1_play, lin2_play;
	UINT8	bCh;

	TRACE_FUNC();

	if(mc_asoc == NULL) {
		return -EINVAL;
	}

	if((input_path = read_cache(codec, MC_ASOC_INPUT_PATH)) < 0) {
		return -EIO;
	}
	if((mainmic_play = read_cache(codec, MC_ASOC_MAINMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((submic_play = read_cache(codec, MC_ASOC_SUBMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((msmic_play = read_cache(codec, MC_ASOC_2MIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((hsmic_play = read_cache(codec, MC_ASOC_HSMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((btmic_play = read_cache(codec, MC_ASOC_BTMIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((lin1_play = read_cache(codec, MC_ASOC_LIN1_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}
	if((lin2_play = read_cache(codec, MC_ASOC_LIN2_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}

	if((mainmic_play == 1)
	|| (submic_play == 1)
	|| (msmic_play == 1)
	|| (hsmic_play == 1)
	|| (lin1_play == 1)
	|| (lin2_play == 1)) {
		ain_play	= 1;
	}
	dbg_info("ain_play=%d\n", ain_play);

	mc_asoc_vreg_map[MC_ASOC_AVOL_AIN].offset			= (size_t)-1;
	mc_asoc_vreg_map[MC_ASOC_DVOL_AIN].offset			= (size_t)-1;
	mc_asoc_vreg_map[MC_ASOC_DVOL_AINPLAYBACK].offset	= (size_t)-1;
	mc_asoc_vreg_map[MC_ASOC_AVOL_AD0].offset			= offsetof(MCDRV_VOL_INFO, aswA_Ad0);
	mc_asoc_vreg_map[MC_ASOC_DVOL_AD0].offset			= offsetof(MCDRV_VOL_INFO, aswD_Ad0);
	mc_asoc_vreg_map[MC_ASOC_DVOL_AD0_ATT].offset		= offsetof(MCDRV_VOL_INFO, aswD_Ad0Att);
	mc_asoc_vreg_map[MC_ASOC_DVOL_AD1_ATT].offset		= offsetof(MCDRV_VOL_INFO, aswD_Adc1Att);
	mc_asoc_vreg_map[MC_ASOC_DVOL_DIR0].offset			= offsetof(MCDRV_VOL_INFO, aswD_Dir0);
	mc_asoc_vreg_map[MC_ASOC_DVOL_DIR1].offset			= offsetof(MCDRV_VOL_INFO, aswD_Dir1);
	mc_asoc_vreg_map[MC_ASOC_DVOL_DIR2].offset			= offsetof(MCDRV_VOL_INFO, aswD_Dir2);
	mc_asoc_vreg_map[MC_ASOC_DVOL_DIR0_ATT].offset		= offsetof(MCDRV_VOL_INFO, aswD_Dir0Att);
	mc_asoc_vreg_map[MC_ASOC_DVOL_DIR1_ATT].offset		= offsetof(MCDRV_VOL_INFO, aswD_Dir1Att);
	mc_asoc_vreg_map[MC_ASOC_DVOL_DIR2_ATT].offset		= offsetof(MCDRV_VOL_INFO, aswD_Dir2Att);
	mc_asoc_vreg_map[MC_ASOC_DVOL_DIT2_1].offset		= offsetof(MCDRV_VOL_INFO, aswD_Dit21);

	switch(get_ap_play_port()) {
	case	DIO_0:
		mc_asoc_vreg_map[MC_ASOC_DVOL_DIR0_ATT].offset	= (size_t)-1;
		mc_asoc_vreg_map[MC_ASOC_DVOL_MASTER].offset	= offsetof(MCDRV_VOL_INFO, aswD_Dir0Att);
		break;
	case	DIO_1:
		mc_asoc_vreg_map[MC_ASOC_DVOL_DIR1_ATT].offset	= (size_t)-1;
		mc_asoc_vreg_map[MC_ASOC_DVOL_MASTER].offset	= offsetof(MCDRV_VOL_INFO, aswD_Dir1Att);
		break;
	case	DIO_2:
		mc_asoc_vreg_map[MC_ASOC_DVOL_DIR2_ATT].offset	= (size_t)-1;
		mc_asoc_vreg_map[MC_ASOC_DVOL_MASTER].offset	= offsetof(MCDRV_VOL_INFO, aswD_Dir2Att);
		break;
	default:
		mc_asoc_vreg_map[MC_ASOC_DVOL_MASTER].offset	= (size_t)-1;
		if(mc_asoc_ap_play_port == LIN1) {
			for(bCh = 0; bCh < ADC0_PATH_CHANNELS; bCh++) {
				if((path_info->asAdc0[bCh].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK] & 0x15) != 0) {
					mc_asoc_vreg_map[MC_ASOC_DVOL_AD0_ATT].offset	= (size_t)-1;
					mc_asoc_vreg_map[MC_ASOC_DVOL_MASTER].offset	= offsetof(MCDRV_VOL_INFO, aswD_Ad0Att);
				}
			}
			for(bCh = 0; bCh < ADC1_PATH_CHANNELS; bCh++) {
				if((path_info->asAdc1[bCh].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & 0x10) != 0) {
					mc_asoc_vreg_map[MC_ASOC_DVOL_AD1_ATT].offset	= (size_t)-1;
					mc_asoc_vreg_map[MC_ASOC_DVOL_MASTER].offset	= offsetof(MCDRV_VOL_INFO, aswD_Adc1Att);
				}
			}
			break;
		}
		if(mc_asoc_ap_play_port == LIN2) {
			for(bCh = 0; bCh < ADC0_PATH_CHANNELS; bCh++) {
				if((path_info->asAdc0[bCh].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK] & 0x15) != 0) {
					mc_asoc_vreg_map[MC_ASOC_DVOL_AD0_ATT].offset	= (size_t)-1;
					mc_asoc_vreg_map[MC_ASOC_DVOL_MASTER].offset	= offsetof(MCDRV_VOL_INFO, aswD_Ad0Att);
				}
			}
			for(bCh = 0; bCh < ADC1_PATH_CHANNELS; bCh++) {
				if((path_info->asAdc1[bCh].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & 0x10) != 0) {
					mc_asoc_vreg_map[MC_ASOC_DVOL_AD1_ATT].offset	= (size_t)-1;
					mc_asoc_vreg_map[MC_ASOC_DVOL_MASTER].offset	= offsetof(MCDRV_VOL_INFO, aswD_Adc1Att);
				}
			}
		}
		break;
	}

	if(get_cp_port() == -1) {
		mc_asoc_vreg_map[MC_ASOC_DVOL_VOICE].offset	= (size_t)-1;
		if((preset_idx >= 12)
		&& (preset_idx < 24)) {
			/*	incall	*/
			if((mc_asoc_cp_port == LIN1_LOUT1)
			|| (mc_asoc_cp_port == LIN1_LOUT2)) {
				for(bCh = 0; bCh < ADC0_PATH_CHANNELS; bCh++) {
					if((path_info->asAdc0[bCh].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK] & 0x15) != 0) {
						mc_asoc_vreg_map[MC_ASOC_DVOL_AD0_ATT].offset	= (size_t)-1;
						mc_asoc_vreg_map[MC_ASOC_DVOL_VOICE].offset		= offsetof(MCDRV_VOL_INFO, aswD_Ad0Att);
						return 0;
					}
				}
				for(bCh = 0; bCh < ADC1_PATH_CHANNELS; bCh++) {
					if((path_info->asAdc1[bCh].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & 0x10) != 0) {
						mc_asoc_vreg_map[MC_ASOC_DVOL_AD1_ATT].offset	= (size_t)-1;
						mc_asoc_vreg_map[MC_ASOC_DVOL_VOICE].offset		= offsetof(MCDRV_VOL_INFO, aswD_Adc1Att);
						return 0;
					}
				}
			}
			if((mc_asoc_cp_port == LIN2_LOUT1)
			|| (mc_asoc_cp_port == LIN2_LOUT2)) {
				for(bCh = 0; bCh < ADC0_PATH_CHANNELS; bCh++) {
					if((path_info->asAdc0[bCh].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK] & 0x15) != 0) {
						mc_asoc_vreg_map[MC_ASOC_DVOL_AD0_ATT].offset	= (size_t)-1;
						mc_asoc_vreg_map[MC_ASOC_DVOL_VOICE].offset		= offsetof(MCDRV_VOL_INFO, aswD_Ad0Att);
						return 0;
					}
				}
				for(bCh = 0; bCh < ADC1_PATH_CHANNELS; bCh++) {
					if((path_info->asAdc1[bCh].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & 0x10) != 0) {
						mc_asoc_vreg_map[MC_ASOC_DVOL_AD1_ATT].offset	= (size_t)-1;
						mc_asoc_vreg_map[MC_ASOC_DVOL_VOICE].offset		= offsetof(MCDRV_VOL_INFO, aswD_Adc1Att);
						return 0;
					}
				}
			}
		}
	}
	else if((preset_idx >= 12)
		 && (preset_idx < 24)) {
		/*	incall	*/
		if(cdsp_onoff == PRESET_PATH_CDSP_ON) {
			mc_asoc_vreg_map[MC_ASOC_DVOL_DIT2_1].offset	= (size_t)-1;
			mc_asoc_vreg_map[MC_ASOC_DVOL_VOICE].offset		= offsetof(MCDRV_VOL_INFO, aswD_Dit21);
			return 0;
		}
		switch(get_cp_port()) {
		case	-1:
			mc_asoc_vreg_map[MC_ASOC_DVOL_VOICE].offset	= (size_t)-1;
			if((mc_asoc_cp_port == LIN1_LOUT1)
			|| (mc_asoc_cp_port == LIN1_LOUT2)) {
				for(bCh = 0; bCh < ADC0_PATH_CHANNELS; bCh++) {
					if((path_info->asAdc0[bCh].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK] & 0x15) != 0) {
						mc_asoc_vreg_map[MC_ASOC_DVOL_AD0_ATT].offset	= (size_t)-1;
						mc_asoc_vreg_map[MC_ASOC_DVOL_VOICE].offset		= offsetof(MCDRV_VOL_INFO, aswD_Ad0Att);
					}
				}
				for(bCh = 0; bCh < ADC1_PATH_CHANNELS; bCh++) {
					if((path_info->asAdc1[bCh].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK] & 0x10) != 0) {
						mc_asoc_vreg_map[MC_ASOC_DVOL_AD1_ATT].offset	= (size_t)-1;
						mc_asoc_vreg_map[MC_ASOC_DVOL_VOICE].offset		= offsetof(MCDRV_VOL_INFO, aswD_Adc1Att);
					}
				}
				break;
			}
			if((mc_asoc_cp_port == LIN2_LOUT1)
			|| (mc_asoc_cp_port == LIN2_LOUT2)) {
				for(bCh = 0; bCh < ADC0_PATH_CHANNELS; bCh++) {
					if((path_info->asAdc0[bCh].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK] & 0x15) != 0) {
						mc_asoc_vreg_map[MC_ASOC_DVOL_AD0_ATT].offset	= (size_t)-1;
						mc_asoc_vreg_map[MC_ASOC_DVOL_VOICE].offset		= offsetof(MCDRV_VOL_INFO, aswD_Ad0Att);
					}
				}
				for(bCh = 0; bCh < ADC1_PATH_CHANNELS; bCh++) {
					if((path_info->asAdc1[bCh].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK] & 0x10) != 0) {
						mc_asoc_vreg_map[MC_ASOC_DVOL_AD1_ATT].offset	= (size_t)-1;
						mc_asoc_vreg_map[MC_ASOC_DVOL_VOICE].offset		= offsetof(MCDRV_VOL_INFO, aswD_Adc1Att);
					}
				}
			}
			break;
		case	DIO_0:
			mc_asoc_vreg_map[MC_ASOC_DVOL_DIR0_ATT].offset	= (size_t)-1;
			mc_asoc_vreg_map[MC_ASOC_DVOL_VOICE].offset		= offsetof(MCDRV_VOL_INFO, aswD_Dir0Att);
			break;
		case	DIO_1:
			mc_asoc_vreg_map[MC_ASOC_DVOL_DIR1_ATT].offset	= (size_t)-1;
			mc_asoc_vreg_map[MC_ASOC_DVOL_VOICE].offset		= offsetof(MCDRV_VOL_INFO, aswD_Dir1Att);
			break;
		case	DIO_2:
			mc_asoc_vreg_map[MC_ASOC_DVOL_DIR2_ATT].offset	= (size_t)-1;
			mc_asoc_vreg_map[MC_ASOC_DVOL_VOICE].offset		= offsetof(MCDRV_VOL_INFO, aswD_Dir2Att);
			break;
		default:
			break;
		}
	}

	if(preset_idx < 12) {
		/*	!incall	*/
		if(ain_play != 0) {
			if((preset_idx < 4)
			|| (input_path == MC_ASOC_INPUT_PATH_BT)
			|| ((mainmic_play == 1) && (input_path == MC_ASOC_INPUT_PATH_MAINMIC))
			|| ((submic_play == 1) && (input_path == MC_ASOC_INPUT_PATH_SUBMIC))
			|| ((msmic_play == 1) && (input_path == MC_ASOC_INPUT_PATH_2MIC))
			|| ((hsmic_play == 1) && (input_path == MC_ASOC_INPUT_PATH_HS))
			|| ((lin1_play == 1) && (input_path == MC_ASOC_INPUT_PATH_LIN1))
			|| ((lin2_play == 1) && (input_path == MC_ASOC_INPUT_PATH_LIN2))) {
				mc_asoc_vreg_map[MC_ASOC_AVOL_AIN].offset	= offsetof(MCDRV_VOL_INFO, aswA_Ad0);
				mc_asoc_vreg_map[MC_ASOC_DVOL_AIN].offset	= offsetof(MCDRV_VOL_INFO, aswD_Ad0);
				mc_asoc_vreg_map[MC_ASOC_AVOL_AD0].offset	= (size_t)-1;
				mc_asoc_vreg_map[MC_ASOC_DVOL_AD0].offset	= (size_t)-1;
			}
			mc_asoc_vreg_map[MC_ASOC_DVOL_AINPLAYBACK].offset	= offsetof(MCDRV_VOL_INFO, aswD_Ad0Att);
			mc_asoc_vreg_map[MC_ASOC_DVOL_AD0_ATT].offset		= (size_t)-1;
		}
		if(btmic_play != 0) {
			switch(get_bt_port()) {
			case	DIO_0:
				if(mc_asoc_vreg_map[MC_ASOC_AVOL_AIN].offset == -1) {
					mc_asoc_vreg_map[MC_ASOC_DVOL_AIN].offset			= offsetof(MCDRV_VOL_INFO, aswD_Dir0);
					mc_asoc_vreg_map[MC_ASOC_DVOL_DIR0].offset			= (size_t)-1;
				}
				if(mc_asoc_vreg_map[MC_ASOC_DVOL_AINPLAYBACK].offset == -1) {
					mc_asoc_vreg_map[MC_ASOC_DVOL_AINPLAYBACK].offset	= offsetof(MCDRV_VOL_INFO, aswD_Dir0Att);
					mc_asoc_vreg_map[MC_ASOC_DVOL_DIR0_ATT].offset		= (size_t)-1;
				}
				break;
			case	DIO_1:
				if(mc_asoc_vreg_map[MC_ASOC_AVOL_AIN].offset == -1) {
					mc_asoc_vreg_map[MC_ASOC_DVOL_AIN].offset			= offsetof(MCDRV_VOL_INFO, aswD_Dir1);
					mc_asoc_vreg_map[MC_ASOC_DVOL_DIR1].offset			= (size_t)-1;
				}
				if(mc_asoc_vreg_map[MC_ASOC_DVOL_AINPLAYBACK].offset == -1) {
					mc_asoc_vreg_map[MC_ASOC_DVOL_AINPLAYBACK].offset	= offsetof(MCDRV_VOL_INFO, aswD_Dir1Att);
					mc_asoc_vreg_map[MC_ASOC_DVOL_DIR1_ATT].offset		= (size_t)-1;
				}
				break;
			case	DIO_2:
				if(mc_asoc_vreg_map[MC_ASOC_AVOL_AIN].offset == -1) {
					mc_asoc_vreg_map[MC_ASOC_DVOL_AIN].offset			= offsetof(MCDRV_VOL_INFO, aswD_Dir2);
					mc_asoc_vreg_map[MC_ASOC_DVOL_DIR2].offset			= (size_t)-1;
				}
				if(mc_asoc_vreg_map[MC_ASOC_DVOL_AINPLAYBACK].offset == -1) {
					mc_asoc_vreg_map[MC_ASOC_DVOL_AINPLAYBACK].offset	= offsetof(MCDRV_VOL_INFO, aswD_Dir2Att);
					mc_asoc_vreg_map[MC_ASOC_DVOL_DIR2_ATT].offset		= (size_t)-1;
				}
				break;
			default:
				break;
			}
		}
	}
	return 0;
}

static UINT16 dsp_onoff(
	UINT16	*dsp_onoff,
	int		output_path
)
{
	UINT16	onoff	= dsp_onoff[0];

	switch(output_path) {
	case	MC_ASOC_OUTPUT_PATH_SP:
		if(dsp_onoff[YMC_DSP_OUTPUT_SP] != 0x8000) {
			onoff	= dsp_onoff[YMC_DSP_OUTPUT_SP];
		}
		break;

	case	MC_ASOC_OUTPUT_PATH_RC:
	case	MC_ASOC_OUTPUT_PATH_SP_RC:
		if(dsp_onoff[YMC_DSP_OUTPUT_RC] != 0x8000) {
			onoff	= dsp_onoff[YMC_DSP_OUTPUT_RC];
		}
		break;

	case	MC_ASOC_OUTPUT_PATH_HP:
	case	MC_ASOC_OUTPUT_PATH_HS:
	case	MC_ASOC_OUTPUT_PATH_SP_HP:
		if(dsp_onoff[YMC_DSP_OUTPUT_HP] != 0x8000) {
			onoff	= dsp_onoff[YMC_DSP_OUTPUT_HP];
		}
		break;

	case	MC_ASOC_OUTPUT_PATH_LO1:
	case	MC_ASOC_OUTPUT_PATH_SP_LO1:
		if(dsp_onoff[YMC_DSP_OUTPUT_LO1] != 0x8000) {
			onoff	= dsp_onoff[YMC_DSP_OUTPUT_LO1];
		}
		break;

	case	MC_ASOC_OUTPUT_PATH_LO2:
	case	MC_ASOC_OUTPUT_PATH_SP_LO2:
		if(dsp_onoff[YMC_DSP_OUTPUT_LO2] != 0x8000) {
			onoff	= dsp_onoff[YMC_DSP_OUTPUT_LO2];
		}
		break;

	case	MC_ASOC_OUTPUT_PATH_BT:
	case	MC_ASOC_OUTPUT_PATH_SP_BT:
		if(dsp_onoff[YMC_DSP_OUTPUT_BT] != 0x8000) {
			onoff	= dsp_onoff[YMC_DSP_OUTPUT_BT];
		}
		break;

	default:
		break;
	}
	return onoff;
}

static UINT8 ae_onoff(
	UINT8	*ae_onoff,
	int		output_path
)
{
	UINT8	onoff	= ae_onoff[0];

	switch(output_path) {
	case	MC_ASOC_OUTPUT_PATH_SP:
		if(ae_onoff[YMC_DSP_OUTPUT_SP] != 0x80) {
			onoff	= ae_onoff[YMC_DSP_OUTPUT_SP];
		}
		break;

	case	MC_ASOC_OUTPUT_PATH_RC:
	case	MC_ASOC_OUTPUT_PATH_SP_RC:
		if(ae_onoff[YMC_DSP_OUTPUT_RC] != 0x80) {
			onoff	= ae_onoff[YMC_DSP_OUTPUT_RC];
		}
		break;

	case	MC_ASOC_OUTPUT_PATH_HP:
	case	MC_ASOC_OUTPUT_PATH_HS:
	case	MC_ASOC_OUTPUT_PATH_SP_HP:
		if(ae_onoff[YMC_DSP_OUTPUT_HP] != 0x80) {
			onoff	= ae_onoff[YMC_DSP_OUTPUT_HP];
		}
		break;

	case	MC_ASOC_OUTPUT_PATH_LO1:
	case	MC_ASOC_OUTPUT_PATH_SP_LO1:
		if(ae_onoff[YMC_DSP_OUTPUT_LO1] != 0x80) {
			onoff	= ae_onoff[YMC_DSP_OUTPUT_LO1];
		}
		break;

	case	MC_ASOC_OUTPUT_PATH_LO2:
	case	MC_ASOC_OUTPUT_PATH_SP_LO2:
		if(ae_onoff[YMC_DSP_OUTPUT_LO2] != 0x80) {
			onoff	= ae_onoff[YMC_DSP_OUTPUT_LO2];
		}
		break;

	case	MC_ASOC_OUTPUT_PATH_BT:
	case	MC_ASOC_OUTPUT_PATH_SP_BT:
		if(ae_onoff[YMC_DSP_OUTPUT_BT] != 0x80) {
			onoff	= ae_onoff[YMC_DSP_OUTPUT_BT];
		}
		break;

	default:
		break;
	}
	return onoff;
}

static void set_audioengine_param(
	struct mc_asoc_data	*mc_asoc,
	MCDRV_AE_INFO	*ae_info,
	int	inout
)
{
	TRACE_FUNC();

	if(inout < MC_ASOC_DSP_N_OUTPUT) {
		if((mc_asoc->block_onoff[inout][MC_ASOC_DSP_BLOCK_BEX] == MC_ASOC_DSP_BLOCK_OFF)
		|| ((mc_asoc->block_onoff[inout][MC_ASOC_DSP_BLOCK_BEX] == MC_ASOC_DSP_BLOCK_THRU)
			&&
			(mc_asoc->block_onoff[0][MC_ASOC_DSP_BLOCK_BEX] != MC_ASOC_DSP_BLOCK_ON))
		|| (mc_asoc->block_onoff[inout][MC_ASOC_DSP_BLOCK_WIDE] == MC_ASOC_DSP_BLOCK_OFF)
		|| ((mc_asoc->block_onoff[inout][MC_ASOC_DSP_BLOCK_WIDE] == MC_ASOC_DSP_BLOCK_THRU)
			&&
			(mc_asoc->block_onoff[0][MC_ASOC_DSP_BLOCK_WIDE] != MC_ASOC_DSP_BLOCK_ON))) {
			ae_info->bOnOff	&= ~MCDRV_BEXWIDE_ON;
		}
		else {
			ae_info->bOnOff	|= MCDRV_BEXWIDE_ON;
			if(mc_asoc->block_onoff[inout][MC_ASOC_DSP_BLOCK_BEX] == MC_ASOC_DSP_BLOCK_ON) {
				memcpy(ae_info->abBex, mc_asoc->ae_info[inout].abBex, BEX_PARAM_SIZE);
			}
			else {
				memcpy(ae_info->abBex, mc_asoc->ae_info[0].abBex, BEX_PARAM_SIZE);
			}
			if(mc_asoc->block_onoff[inout][MC_ASOC_DSP_BLOCK_WIDE] == MC_ASOC_DSP_BLOCK_ON) {
				memcpy(ae_info->abWide, mc_asoc->ae_info[inout].abWide, WIDE_PARAM_SIZE);
			}
			else {
				memcpy(ae_info->abWide, mc_asoc->ae_info[0].abWide, WIDE_PARAM_SIZE);
			}
		}

		if((mc_asoc->block_onoff[inout][MC_ASOC_DSP_BLOCK_DRC] == MC_ASOC_DSP_BLOCK_OFF)
		|| ((mc_asoc->block_onoff[inout][MC_ASOC_DSP_BLOCK_DRC] == MC_ASOC_DSP_BLOCK_THRU)
			&&
			(mc_asoc->block_onoff[0][MC_ASOC_DSP_BLOCK_DRC] != MC_ASOC_DSP_BLOCK_ON))) {
			ae_info->bOnOff	&= ~MCDRV_DRC_ON;
		}
		else {
			ae_info->bOnOff	|= MCDRV_DRC_ON;
			if(mc_asoc->block_onoff[inout][MC_ASOC_DSP_BLOCK_DRC] == MC_ASOC_DSP_BLOCK_ON) {
				memcpy(ae_info->abDrc, mc_asoc->ae_info[inout].abDrc, DRC_PARAM_SIZE);
				memcpy(ae_info->abDrc2, mc_asoc->ae_info[inout].abDrc2, DRC2_PARAM_SIZE);
			}
			else {
				memcpy(ae_info->abDrc, mc_asoc->ae_info[0].abDrc, DRC_PARAM_SIZE);
				memcpy(ae_info->abDrc2, mc_asoc->ae_info[0].abDrc2, DRC2_PARAM_SIZE);
			}
		}

		if((mc_asoc->block_onoff[inout][MC_ASOC_DSP_BLOCK_EQ5] == MC_ASOC_DSP_BLOCK_OFF)
		|| ((mc_asoc->block_onoff[inout][MC_ASOC_DSP_BLOCK_EQ5] == MC_ASOC_DSP_BLOCK_THRU)
			&&
			(mc_asoc->block_onoff[0][MC_ASOC_DSP_BLOCK_EQ5] != MC_ASOC_DSP_BLOCK_ON))) {
			ae_info->bOnOff	&= ~MCDRV_EQ5_ON;
		}
		else {
			ae_info->bOnOff	|= MCDRV_EQ5_ON;
			if(mc_asoc->block_onoff[inout][MC_ASOC_DSP_BLOCK_EQ5] == MC_ASOC_DSP_BLOCK_ON) {
				memcpy(ae_info->abEq5, mc_asoc->ae_info[inout].abEq5, EQ5_PARAM_SIZE);
			}
			else {
				memcpy(ae_info->abEq5, mc_asoc->ae_info[0].abEq5, EQ5_PARAM_SIZE);
			}
		}

		if((mc_asoc->block_onoff[inout][MC_ASOC_DSP_BLOCK_EQ3] == MC_ASOC_DSP_BLOCK_OFF)
		|| ((mc_asoc->block_onoff[inout][MC_ASOC_DSP_BLOCK_EQ3] == MC_ASOC_DSP_BLOCK_THRU)
			&&
			(mc_asoc->block_onoff[0][MC_ASOC_DSP_BLOCK_EQ3] != MC_ASOC_DSP_BLOCK_ON))) {
			ae_info->bOnOff	&= ~MCDRV_EQ3_ON;
		}
		else {
			ae_info->bOnOff	|= MCDRV_EQ3_ON;
			if(mc_asoc->block_onoff[inout][MC_ASOC_DSP_BLOCK_EQ3] == MC_ASOC_DSP_BLOCK_ON) {
				memcpy(ae_info->abEq3, mc_asoc->ae_info[inout].abEq3, EQ3_PARAM_SIZE);
			}
			else {
				memcpy(ae_info->abEq3, mc_asoc->ae_info[0].abEq3, EQ3_PARAM_SIZE);
			}
		}
	}
	else {
		if((mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_BEX] == MC_ASOC_DSP_BLOCK_OFF)
		|| ((mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_BEX] == MC_ASOC_DSP_BLOCK_THRU)
			&&
			(mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_BASE][MC_ASOC_DSP_BLOCK_BEX] != MC_ASOC_DSP_BLOCK_ON))
		|| (mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_WIDE] == MC_ASOC_DSP_BLOCK_OFF)
		|| ((mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_WIDE] == MC_ASOC_DSP_BLOCK_THRU)
			&&
			(mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_BASE][MC_ASOC_DSP_BLOCK_WIDE] != MC_ASOC_DSP_BLOCK_ON))) {
			ae_info->bOnOff	&= ~MCDRV_BEXWIDE_ON;
		}
		else {
			ae_info->bOnOff	|= MCDRV_BEXWIDE_ON;
			if(mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_BEX] == MC_ASOC_DSP_BLOCK_ON) {
				memcpy(ae_info->abBex, mc_asoc->ae_info[MC_ASOC_DSP_INPUT_EXT].abBex, BEX_PARAM_SIZE);
			}
			else {
				memcpy(ae_info->abBex, mc_asoc->ae_info[MC_ASOC_DSP_INPUT_BASE].abBex, BEX_PARAM_SIZE);
			}
			if(mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_WIDE] == MC_ASOC_DSP_BLOCK_ON) {
				memcpy(ae_info->abWide, mc_asoc->ae_info[MC_ASOC_DSP_INPUT_EXT].abWide, WIDE_PARAM_SIZE);
			}
			else {
				memcpy(ae_info->abWide, mc_asoc->ae_info[MC_ASOC_DSP_INPUT_BASE].abWide, WIDE_PARAM_SIZE);
			}
		}

		if((mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_DRC] == MC_ASOC_DSP_BLOCK_OFF)
		|| ((mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_DRC] == MC_ASOC_DSP_BLOCK_THRU)
			&&
			(mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_BASE][MC_ASOC_DSP_BLOCK_DRC] != MC_ASOC_DSP_BLOCK_ON))) {
			ae_info->bOnOff	&= ~MCDRV_DRC_ON;
		}
		else {
			ae_info->bOnOff	|= MCDRV_DRC_ON;
			if(mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_DRC] == MC_ASOC_DSP_BLOCK_ON) {
				memcpy(ae_info->abDrc, mc_asoc->ae_info[MC_ASOC_DSP_INPUT_EXT].abDrc, DRC_PARAM_SIZE);
				memcpy(ae_info->abDrc2, mc_asoc->ae_info[MC_ASOC_DSP_INPUT_EXT].abDrc2, DRC2_PARAM_SIZE);
			}
			else {
				memcpy(ae_info->abDrc, mc_asoc->ae_info[MC_ASOC_DSP_INPUT_BASE].abDrc, DRC_PARAM_SIZE);
				memcpy(ae_info->abDrc2, mc_asoc->ae_info[MC_ASOC_DSP_INPUT_BASE].abDrc2, DRC2_PARAM_SIZE);
			}
		}

		if((mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_EQ5] == MC_ASOC_DSP_BLOCK_OFF)
		|| ((mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_EQ5] == MC_ASOC_DSP_BLOCK_THRU)
			&&
			(mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_BASE][MC_ASOC_DSP_BLOCK_EQ5] != MC_ASOC_DSP_BLOCK_ON))) {
			ae_info->bOnOff	&= ~MCDRV_EQ5_ON;
		}
		else {
			ae_info->bOnOff	|= MCDRV_EQ5_ON;
			if(mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_EQ5] == MC_ASOC_DSP_BLOCK_ON) {
				memcpy(ae_info->abEq5, mc_asoc->ae_info[MC_ASOC_DSP_INPUT_EXT].abEq5, EQ5_PARAM_SIZE);
			}
			else {
				memcpy(ae_info->abEq5, mc_asoc->ae_info[MC_ASOC_DSP_INPUT_BASE].abEq5, EQ5_PARAM_SIZE);
			}
		}

		if((mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_EQ3] == MC_ASOC_DSP_BLOCK_OFF)
		|| ((mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_EQ3] == MC_ASOC_DSP_BLOCK_THRU)
			&&
			(mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_BASE][MC_ASOC_DSP_BLOCK_EQ3] != MC_ASOC_DSP_BLOCK_ON))) {
			ae_info->bOnOff	&= ~MCDRV_EQ3_ON;
		}
		else {
			ae_info->bOnOff	|= MCDRV_EQ3_ON;
			if(mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_EQ3] == MC_ASOC_DSP_BLOCK_ON) {
				memcpy(ae_info->abEq3, mc_asoc->ae_info[MC_ASOC_DSP_INPUT_EXT].abEq3, EQ3_PARAM_SIZE);
			}
			else {
				memcpy(ae_info->abEq3, mc_asoc->ae_info[MC_ASOC_DSP_INPUT_BASE].abEq3, EQ3_PARAM_SIZE);
			}
		}
	}
}

static int set_audioengine(
	struct snd_soc_codec *codec
)
{
	MCDRV_AE_INFO	ae_info;
	int		ret;
	int		err;
	int		audio_mode	= 0;
	int		audio_mode_play;
	int		audio_mode_cap;
	int		output_path;
	struct mc_asoc_data	*mc_asoc	= mc_asoc_get_mc_asoc(codec);

	TRACE_FUNC();

	if((codec == NULL) || (mc_asoc == NULL)) {
		return -EINVAL;
	}

	memcpy(&ae_info, &mc_asoc->ae_store, sizeof(MCDRV_AE_INFO));

	if((audio_mode_play = read_cache(codec, MC_ASOC_AUDIO_MODE_PLAY)) < 0) {
		return -EIO;
	}
	if((audio_mode_cap = read_cache(codec, MC_ASOC_AUDIO_MODE_CAP)) < 0) {
		return -EIO;
	}

	if((audio_mode_play == MC_ASOC_AUDIO_MODE_INCOMM)
	&& (audio_mode_cap == MC_ASOC_AUDIO_MODE_INCOMM)) {
		audio_mode	= 3;	/*	Incommunication	*/
	}
	else if((audio_mode_play == MC_ASOC_AUDIO_MODE_INCALL)
		 || (audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)) {
		if((audio_mode_cap == MC_ASOC_AUDIO_MODE_INCALL)
		|| (audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)) {
			audio_mode	= 2;	/*	Incall	*/
		}
	}

	if(mc_asoc_ae_dir == MC_ASOC_AE_PLAY) {
		if((ret = read_cache(codec, MC_ASOC_OUTPUT_PATH)) < 0) {
			return ret;
		}
		output_path	= ret;

		switch(output_path) {
		case	MC_ASOC_OUTPUT_PATH_SP:
			if((dsp_onoff(mc_asoc->dsp_onoff, output_path) & (1<<audio_mode)) == 0) {
				ae_info.bOnOff	= 0;
			}
			else {
				set_audioengine_param(mc_asoc, &ae_info, YMC_DSP_OUTPUT_SP);
			}
			break;
		case	MC_ASOC_OUTPUT_PATH_RC:
		case	MC_ASOC_OUTPUT_PATH_SP_RC:
			if((dsp_onoff(mc_asoc->dsp_onoff, output_path) & (1<<audio_mode)) == 0) {
				ae_info.bOnOff	= 0;
			}
			else {
				set_audioengine_param(mc_asoc, &ae_info, YMC_DSP_OUTPUT_RC);
			}
			break;
		case	MC_ASOC_OUTPUT_PATH_HP:
		case	MC_ASOC_OUTPUT_PATH_HS:
		case	MC_ASOC_OUTPUT_PATH_SP_HP:
			if((dsp_onoff(mc_asoc->dsp_onoff, output_path) & (1<<audio_mode)) == 0) {
				ae_info.bOnOff	= 0;
			}
			else {
				set_audioengine_param(mc_asoc, &ae_info, YMC_DSP_OUTPUT_HP);
			}
			break;
		case	MC_ASOC_OUTPUT_PATH_LO1:
		case	MC_ASOC_OUTPUT_PATH_SP_LO1:
			if((dsp_onoff(mc_asoc->dsp_onoff, output_path) & (1<<audio_mode)) == 0) {
				ae_info.bOnOff	= 0;
			}
			else {
				set_audioengine_param(mc_asoc, &ae_info, YMC_DSP_OUTPUT_LO1);
			}
			break;
		case	MC_ASOC_OUTPUT_PATH_LO2:
		case	MC_ASOC_OUTPUT_PATH_SP_LO2:
			if((dsp_onoff(mc_asoc->dsp_onoff, output_path) & (1<<audio_mode)) == 0) {
				ae_info.bOnOff	= 0;
			}
			else {
				set_audioengine_param(mc_asoc, &ae_info, YMC_DSP_OUTPUT_LO2);
			}
			break;
		case	MC_ASOC_OUTPUT_PATH_BT:
		case	MC_ASOC_OUTPUT_PATH_SP_BT:
			if((dsp_onoff(mc_asoc->dsp_onoff, output_path) & (1<<audio_mode)) == 0) {
				ae_info.bOnOff	= 0;
			}
			else {
				set_audioengine_param(mc_asoc, &ae_info, YMC_DSP_OUTPUT_BT);
			}
			break;

		default:
			return -EIO;
		}
	}
	else if(mc_asoc_ae_dir == MC_ASOC_AE_CAP) {
		if(((mc_asoc->dsp_onoff[MC_ASOC_DSP_INPUT_EXT] != 0x8000)
			&& ((mc_asoc->dsp_onoff[MC_ASOC_DSP_INPUT_EXT] & (1<<audio_mode)) == 0))
		|| ((mc_asoc->dsp_onoff[MC_ASOC_DSP_INPUT_EXT] == 0x8000)
			&& ((mc_asoc->dsp_onoff[MC_ASOC_DSP_INPUT_BASE] & (1<<audio_mode)) == 0))) {
			ae_info.bOnOff	= 0;
		}
		else {
			set_audioengine_param(mc_asoc, &ae_info, MC_ASOC_DSP_INPUT_BASE);
		}
	}

	if(memcmp(&ae_info, &mc_asoc->ae_store, sizeof(MCDRV_AE_INFO)) == 0) {
		dbg_info("memcmp\n");
		return 0;
	}
	err	= _McDrv_Ctrl(MCDRV_SET_AUDIOENGINE, &ae_info, 0x1ff);
	if(err == MCDRV_SUCCESS) {
		memcpy(&mc_asoc->ae_store, &ae_info, sizeof(MCDRV_AE_INFO));
	}
	else {
		err	= mc_asoc_map_drv_error(err);
	}
	return err;
}

static int set_audioengine_ex(
	struct snd_soc_codec *codec
)
{
	static MC_AE_EX_STORE	ae_ex;
	int		i;
	int		ret;
	int		err	= 0;
	int		audio_mode	= 0;
	int		audio_mode_play;
	int		audio_mode_cap;
	int		output_path;
	int		input_path;
	int		lin1_play, lin2_play;
	struct mc_asoc_data	*mc_asoc	= mc_asoc_get_mc_asoc(codec);
	MCDRV_PATH_INFO	path_info, path_info_cur;

	TRACE_FUNC();

	if((codec == NULL) || (mc_asoc == NULL)) {
		return -EINVAL;
	}

	if((mc_asoc_hwid != MC_ASOC_MC3_HW_ID) && (mc_asoc_hwid != MC_ASOC_MA2_HW_ID)) {
		return -EIO;
	}

	memset(&ae_ex, 0, sizeof(MC_AE_EX_STORE));

	if((audio_mode_play = read_cache(codec, MC_ASOC_AUDIO_MODE_PLAY)) < 0) {
		return -EIO;
	}
	if((audio_mode_cap = read_cache(codec, MC_ASOC_AUDIO_MODE_CAP)) < 0) {
		return -EIO;
	}

	if((audio_mode_play == MC_ASOC_AUDIO_MODE_INCOMM)
	&& (audio_mode_cap == MC_ASOC_AUDIO_MODE_INCOMM)) {
		audio_mode	= 3;	/*	Incommunication	*/
	}
	else if((audio_mode_play == MC_ASOC_AUDIO_MODE_INCALL)
		 || (audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)) {
		if((audio_mode_cap == MC_ASOC_AUDIO_MODE_INCALL)
		|| (audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)) {
			audio_mode	= 2;	/*	Incall	*/
		}
	}

	if((ret = read_cache(codec, MC_ASOC_INPUT_PATH)) < 0) {
		return ret;
	}
	input_path	= ret;
	if((ret = read_cache(codec, MC_ASOC_LIN1_PLAYBACK_PATH)) < 0) {
		return ret;
	}
	lin1_play	= ret;
	if((ret = read_cache(codec, MC_ASOC_LIN2_PLAYBACK_PATH)) < 0) {
		return ret;
	}
	lin2_play	= ret;
	if((ret = read_cache(codec, MC_ASOC_OUTPUT_PATH)) < 0) {
		return ret;
	}
	output_path	= ret;

	if(mc_asoc_ae_dir == MC_ASOC_AE_PLAY) {
		dbg_info("mc_asoc->dsp_onoff=0x%x, mc_asoc_ae_dir=%d\n",
			dsp_onoff(mc_asoc->dsp_onoff, output_path), mc_asoc_ae_dir);

		if((dsp_onoff(mc_asoc->dsp_onoff, output_path) & (1<<audio_mode)) != 0) {
			switch(output_path) {
			case	MC_ASOC_OUTPUT_PATH_SP:
				if((mc_asoc->block_onoff[1][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_OFF)
				|| ((mc_asoc->block_onoff[1][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_THRU)
					&&
					(mc_asoc->block_onoff[0][MC_ASOC_DSP_BLOCK_EXTENSION] != MC_ASOC_DSP_BLOCK_ON))) {
				}
				else {
					if(mc_asoc->block_onoff[1][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_ON) {
						memcpy(&ae_ex, &mc_asoc->aeex_prm[1], sizeof(MC_AE_EX_STORE));
					}
					else {
						memcpy(&ae_ex, &mc_asoc->aeex_prm[0], sizeof(MC_AE_EX_STORE));
					}
				}
				break;
			case	MC_ASOC_OUTPUT_PATH_RC:
			case	MC_ASOC_OUTPUT_PATH_SP_RC:
				if((mc_asoc->block_onoff[2][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_OFF)
				|| ((mc_asoc->block_onoff[2][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_THRU)
					&&
					(mc_asoc->block_onoff[0][MC_ASOC_DSP_BLOCK_EXTENSION] != MC_ASOC_DSP_BLOCK_ON))) {
				}
				else {
					if(mc_asoc->block_onoff[2][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_ON) {
						memcpy(&ae_ex, &mc_asoc->aeex_prm[2], sizeof(MC_AE_EX_STORE));
					}
					else {
						memcpy(&ae_ex, &mc_asoc->aeex_prm[0], sizeof(MC_AE_EX_STORE));
					}
				}
				break;
			case	MC_ASOC_OUTPUT_PATH_HP:
			case	MC_ASOC_OUTPUT_PATH_HS:
			case	MC_ASOC_OUTPUT_PATH_SP_HP:
				if((mc_asoc->block_onoff[3][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_OFF)
				|| ((mc_asoc->block_onoff[3][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_THRU)
					&&
					(mc_asoc->block_onoff[0][MC_ASOC_DSP_BLOCK_EXTENSION] != MC_ASOC_DSP_BLOCK_ON))) {
				}
				else {
					if(mc_asoc->block_onoff[3][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_ON) {
						memcpy(&ae_ex, &mc_asoc->aeex_prm[3], sizeof(MC_AE_EX_STORE));
					}
					else {
						memcpy(&ae_ex, &mc_asoc->aeex_prm[0], sizeof(MC_AE_EX_STORE));
					}
				}
				break;
			case	MC_ASOC_OUTPUT_PATH_LO1:
			case	MC_ASOC_OUTPUT_PATH_SP_LO1:
				if((mc_asoc->block_onoff[4][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_OFF)
				|| ((mc_asoc->block_onoff[4][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_THRU)
					&&
					(mc_asoc->block_onoff[0][MC_ASOC_DSP_BLOCK_EXTENSION] != MC_ASOC_DSP_BLOCK_ON))) {
				}
				else {
					if(mc_asoc->block_onoff[4][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_ON) {
						memcpy(&ae_ex, &mc_asoc->aeex_prm[4], sizeof(MC_AE_EX_STORE));
					}
					else {
						memcpy(&ae_ex, &mc_asoc->aeex_prm[0], sizeof(MC_AE_EX_STORE));
					}
				}
				break;
			case	MC_ASOC_OUTPUT_PATH_LO2:
			case	MC_ASOC_OUTPUT_PATH_SP_LO2:
				if((mc_asoc->block_onoff[5][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_OFF)
				|| ((mc_asoc->block_onoff[5][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_THRU)
					&&
					(mc_asoc->block_onoff[0][MC_ASOC_DSP_BLOCK_EXTENSION] != MC_ASOC_DSP_BLOCK_ON))) {
				}
				else {
					if(mc_asoc->block_onoff[5][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_ON) {
						memcpy(&ae_ex, &mc_asoc->aeex_prm[5], sizeof(MC_AE_EX_STORE));
					}
					else {
						memcpy(&ae_ex, &mc_asoc->aeex_prm[0], sizeof(MC_AE_EX_STORE));
					}
				}
				break;
			case	MC_ASOC_OUTPUT_PATH_BT:
			case	MC_ASOC_OUTPUT_PATH_SP_BT:
				if((mc_asoc->block_onoff[6][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_OFF)
				|| ((mc_asoc->block_onoff[6][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_THRU)
					&&
					(mc_asoc->block_onoff[0][MC_ASOC_DSP_BLOCK_EXTENSION] != MC_ASOC_DSP_BLOCK_ON))) {
				}
				else {
					if(mc_asoc->block_onoff[6][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_ON) {
						memcpy(&ae_ex, &mc_asoc->aeex_prm[6], sizeof(MC_AE_EX_STORE));
					}
					else {
						memcpy(&ae_ex, &mc_asoc->aeex_prm[0], sizeof(MC_AE_EX_STORE));
					}
				}
				break;

			default:
				return -EIO;
				break;
			}
		}
	}
	else if(mc_asoc_ae_dir == MC_ASOC_AE_CAP) {
		dbg_info("mc_asoc->dsp_onoff[BASE]=0x%x, mc_asoc->dsp_onoff[EXT]=0x%x, mc_asoc_ae_dir=%d\n",
			mc_asoc->dsp_onoff[MC_ASOC_DSP_INPUT_BASE],
			mc_asoc->dsp_onoff[MC_ASOC_DSP_INPUT_EXT],
			mc_asoc_ae_dir);

		if((mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_OFF)
		|| ((mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_THRU)
			&&
			(mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_BASE][MC_ASOC_DSP_BLOCK_EXTENSION] != MC_ASOC_DSP_BLOCK_ON))) {
		}
		else {
			if(mc_asoc->block_onoff[MC_ASOC_DSP_INPUT_EXT][MC_ASOC_DSP_BLOCK_EXTENSION] == MC_ASOC_DSP_BLOCK_ON) {
				if((mc_asoc->dsp_onoff[MC_ASOC_DSP_INPUT_EXT] & (1<<audio_mode)) != 0) {
					memcpy(&ae_ex, &mc_asoc->aeex_prm[MC_ASOC_DSP_INPUT_EXT], sizeof(MC_AE_EX_STORE));
				}
			}
			else {
				if((mc_asoc->dsp_onoff[MC_ASOC_DSP_INPUT_BASE] & (1<<audio_mode)) != 0) {
					memcpy(&ae_ex, &mc_asoc->aeex_prm[MC_ASOC_DSP_INPUT_BASE], sizeof(MC_AE_EX_STORE));
				}
			}
		}
	}

	if(memcmp(&ae_ex, &mc_asoc->ae_ex_store, sizeof(MC_AE_EX_STORE)) == 0) {
		dbg_info("memcmp\n");
		return 0;
	}

	if((ae_ex.size != mc_asoc->ae_ex_store.size)
	|| (memcmp(&ae_ex.prog, &mc_asoc->ae_ex_store.prog, ae_ex.size))) {
		memset(&mc_asoc->ae_ex_store, 0, sizeof(MC_AE_EX_STORE));
		if(ae_ex.size == 0) {
			err	= _McDrv_Ctrl(MCDRV_SET_AUDIOENGINE_EX, NULL, 0);
		}
		else {
			if((err = _McDrv_Ctrl(MCDRV_GET_PATH, &path_info_cur, 0)) != MCDRV_SUCCESS) {
				return mc_asoc_map_drv_error(err);
			}
			memset(&path_info, 0, sizeof(MCDRV_PATH_INFO));
			for(i = 0; i < AE_PATH_CHANNELS; i++) {
				memset(path_info.asAe[i].abSrcOnOff, 0xaa, SOURCE_BLOCK_NUM);
			}
			if((err = _McDrv_Ctrl(MCDRV_SET_PATH, &path_info, 0)) != MCDRV_SUCCESS) {
				return mc_asoc_map_drv_error(err);
			}
			err	= _McDrv_Ctrl(MCDRV_SET_AUDIOENGINE_EX, ae_ex.prog, ae_ex.size);
		}
		if(err == MCDRV_SUCCESS) {
			memcpy(mc_asoc->ae_ex_store.prog, ae_ex.prog, ae_ex.size);
			mc_asoc->ae_ex_store.size	= ae_ex.size;
		}
		else {
			return mc_asoc_map_drv_error(err);
		}
		if(ae_ex.size > 0) {
			for(i = 0; i < ae_ex.param_count && err == 0; i++) {
				err	= _McDrv_Ctrl(MCDRV_SET_EX_PARAM, &ae_ex.exparam[i], 0);
				if(err == MCDRV_SUCCESS) {
					memcpy(&mc_asoc->ae_ex_store.exparam[i], &ae_ex.exparam[i], sizeof(MCDRV_EXPARAM));
					mc_asoc->ae_ex_store.param_count	= i+1;
				}
				else {
					return mc_asoc_map_drv_error(err);
				}
			}
			if(err == MCDRV_SUCCESS) {
				if((err = _McDrv_Ctrl(MCDRV_SET_PATH, &path_info_cur, 0)) != MCDRV_SUCCESS) {
					return mc_asoc_map_drv_error(err);
				}
			}
		}
	}
	else {
		if(mc_asoc_ae_dir == MC_ASOC_AE_PLAY) {
			if(ae_ex.param_count > mc_asoc->aeex_prm[0].param_count) {
				if((err = _McDrv_Ctrl(MCDRV_GET_PATH, &path_info_cur, 0)) != MCDRV_SUCCESS) {
					return mc_asoc_map_drv_error(err);
				}
				memset(&path_info, 0, sizeof(MCDRV_PATH_INFO));
				for(i = 0; i < AE_PATH_CHANNELS; i++) {
					memset(path_info.asAe[i].abSrcOnOff, 0xaa, SOURCE_BLOCK_NUM);
				}
				if((err = _McDrv_Ctrl(MCDRV_SET_PATH, &path_info, 0)) != MCDRV_SUCCESS) {
					return mc_asoc_map_drv_error(err);
				}
				for(i = mc_asoc->aeex_prm[0].param_count; i < ae_ex.param_count && err == 0; i++) {
					err	= _McDrv_Ctrl(MCDRV_SET_EX_PARAM, &ae_ex.exparam[i], 0);
					if(err == MCDRV_SUCCESS) {
						memcpy(&mc_asoc->ae_ex_store.exparam[i], &ae_ex.exparam[i], sizeof(MCDRV_EXPARAM));
						mc_asoc->ae_ex_store.param_count	= i+1;
					}
					else {
						return mc_asoc_map_drv_error(err);
					}
				}
				if((err = _McDrv_Ctrl(MCDRV_SET_PATH, &path_info_cur, 0)) != MCDRV_SUCCESS) {
					return mc_asoc_map_drv_error(err);
				}
			}
		}
		else if(mc_asoc_ae_dir == MC_ASOC_AE_CAP) {
			if(ae_ex.param_count > mc_asoc->aeex_prm[MC_ASOC_DSP_INPUT_BASE].param_count) {
				if((err = _McDrv_Ctrl(MCDRV_GET_PATH, &path_info_cur, 0)) != MCDRV_SUCCESS) {
					return mc_asoc_map_drv_error(err);
				}
				memset(&path_info, 0, sizeof(MCDRV_PATH_INFO));
				for(i = 0; i < AE_PATH_CHANNELS; i++) {
					memset(path_info.asAe[i].abSrcOnOff, 0xaa, SOURCE_BLOCK_NUM);
				}
				if((err = _McDrv_Ctrl(MCDRV_SET_PATH, &path_info, 0)) != MCDRV_SUCCESS) {
					return mc_asoc_map_drv_error(err);
				}
				for(i = mc_asoc->aeex_prm[MC_ASOC_DSP_INPUT_BASE].param_count; i < ae_ex.param_count && err == 0; i++) {
					err	= _McDrv_Ctrl(MCDRV_SET_EX_PARAM, &ae_ex.exparam[i], 0);
					if(err == MCDRV_SUCCESS) {
						memcpy(&mc_asoc->ae_ex_store.exparam[i], &ae_ex.exparam[i], sizeof(MCDRV_EXPARAM));
						mc_asoc->ae_ex_store.param_count	= i+1;
					}
					else {
						return mc_asoc_map_drv_error(err);
					}
				}
				if((err = _McDrv_Ctrl(MCDRV_SET_PATH, &path_info_cur, 0)) != MCDRV_SUCCESS) {
					return mc_asoc_map_drv_error(err);
				}
			}
		}
	}
	if(err == 0) {
		if(mc_asoc->ae_ex_store.size > 0) {
			mc_asoc_cdsp_ae_ex_flag	= MC_ASOC_AE_EX_ON;
		}
		else {
			mc_asoc_cdsp_ae_ex_flag	= MC_ASOC_CDSP_AE_EX_OFF;
		}
	}
	return err;
}

static int set_cdsp_off(
	struct mc_asoc_data	*mc_asoc
)
{
	int		i, j;
	int		err;
	MCDRV_PATH_INFO	path_info;

	TRACE_FUNC();

	if(mc_asoc_cdsp_ae_ex_flag != MC_ASOC_CDSP_ON) {
		return 0;
	}

	if((err = _McDrv_Ctrl(MCDRV_GET_PATH, &path_info, 0)) != MCDRV_SUCCESS) {
		return mc_asoc_map_drv_error(err);
	}
	for(i = 0; i < CDSP_PATH_CHANNELS; i++) {
		for(j = 0; j < SOURCE_BLOCK_NUM; j++) {
			if((path_info.asCdsp[i].abSrcOnOff[j] & 0x55) != 0) {
				memset(&path_info, 0, sizeof(MCDRV_PATH_INFO));
				for(i = 0; i < PEAK_PATH_CHANNELS; i++) {
					path_info.asPeak[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_OFF;
				}
				for(i = 0; i < DIT0_PATH_CHANNELS; i++) {
					path_info.asDit0[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_OFF;
				}
				for(i = 0; i < DIT1_PATH_CHANNELS; i++) {
					path_info.asDit1[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_OFF;
				}
				for(i = 0; i < DIT2_PATH_CHANNELS; i++) {
					path_info.asDit2[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_OFF;
				}
				for(i = 0; i < DAC_PATH_CHANNELS; i++) {
					path_info.asDac[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]		|= MCDRV_SRC6_CDSP_OFF;
				}
				for(i = 0; i < AE_PATH_CHANNELS; i++) {
					path_info.asAe[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]		|= MCDRV_SRC6_CDSP_OFF;
				}
				for(i = 0; i < MIX_PATH_CHANNELS; i++) {
					path_info.asMix[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]		|= MCDRV_SRC6_CDSP_OFF;
				}
				for(i = 0; i < MIX2_PATH_CHANNELS; i++) {
					path_info.asMix2[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_OFF;
				}
				for(i = 0; i < CDSP_PATH_CHANNELS; i++) {
					memset(path_info.asCdsp[i].abSrcOnOff, 0xaa, SOURCE_BLOCK_NUM);
					path_info.asCdsp[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_OFF;
				}
				if((err = _McDrv_Ctrl(MCDRV_SET_PATH, &path_info, 0)) != MCDRV_SUCCESS) {
					return mc_asoc_map_drv_error(err);
				}
				break;
			}
		}
	}
	if((err = _McDrv_Ctrl(MCDRV_SET_CDSP, NULL, 0)) != MCDRV_SUCCESS) {
		return mc_asoc_map_drv_error(err);
	}
	mc_asoc->cdsp_store.size	= 0;
	mc_asoc->cdsp_store.prog	= NULL;
	mc_asoc_cdsp_ae_ex_flag		= MC_ASOC_CDSP_AE_EX_OFF;

	return 0;
}

static int is_cdsp_used(
	const MCDRV_PATH_INFO	*path_info
)
{
	int		i, j;

	TRACE_FUNC();

	for(i = 0; i < CDSP_PATH_CHANNELS; i++) {
		for(j = 0; j < SOURCE_BLOCK_NUM; j++) {
			if((path_info->asCdsp[i].abSrcOnOff[j] & 0x55) != 0) {
				return 1;
			}
		}
	}
	for(i = 0; i < PEAK_PATH_CHANNELS; i++) {
		if((path_info->asPeak[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) != 0) {
			return 1;
		}
	}
	for(i = 0; i < DIT0_PATH_CHANNELS; i++) {
		if((path_info->asDit0[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) != 0) {
			return 1;
		}
	}
	for(i = 0; i < DIT1_PATH_CHANNELS; i++) {
		if((path_info->asDit1[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) != 0) {
			return 1;
		}
	}
	for(i = 0; i < DIT2_PATH_CHANNELS; i++) {
		if((path_info->asDit2[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) != 0) {
			return 1;
		}
	}
	for(i = 0; i < DAC_PATH_CHANNELS; i++) {
		if((path_info->asDac[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) != 0) {
			return 1;
		}
	}
	for(i = 0; i < AE_PATH_CHANNELS; i++) {
		if((path_info->asAe[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) != 0) {
			return 1;
		}
	}
	for(i = 0; i < MIX_PATH_CHANNELS; i++) {
		if((path_info->asMix[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) != 0) {
			return 1;
		}
	}
	for(i = 0; i < MIX2_PATH_CHANNELS; i++) {
		if((path_info->asMix2[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK] & MCDRV_SRC6_CDSP_ON) != 0) {
			return 1;
		}
	}

	return 0;
}

static int set_cdsp_param(struct snd_soc_codec *codec)
{
	int		i;
	int		ret;
	int		err	= 0;
	int		audio_mode_play;
	int		audio_mode_cap;
	int		output_path;
	int		input_path;
	int		incall_mic;
	int		msmic_play;
	int		base	= 0;
	int		user	= 0;
	struct mc_asoc_data	*mc_asoc	= mc_asoc_get_mc_asoc(codec);
	UINT8	*abLadonHf;
	size_t	sizLadonHf;

	TRACE_FUNC();

	if((codec == NULL) || (mc_asoc == NULL)) {
		return -EINVAL;
	}

	if((mc_asoc_hwid != MC_ASOC_MC3_HW_ID) && (mc_asoc_hwid != MC_ASOC_MA2_HW_ID)) {
		return -EIO;
	}

	if((ret = read_cache(codec, MC_ASOC_OUTPUT_PATH)) < 0) {
		return ret;
	}
	output_path	= ret;
	if((ret = read_cache(codec, MC_ASOC_INPUT_PATH)) < 0) {
		return ret;
	}
	input_path	= ret;
	if((ret = get_incall_mic(codec, output_path)) < 0) {
		return ret;
	}
	incall_mic	= ret;

	if((audio_mode_play = read_cache(codec, MC_ASOC_AUDIO_MODE_PLAY)) < 0) {
		return -EIO;
	}
	if((audio_mode_cap = read_cache(codec, MC_ASOC_AUDIO_MODE_CAP)) < 0) {
		return -EIO;
	}
	if((msmic_play = read_cache(codec, MC_ASOC_2MIC_PLAYBACK_PATH)) < 0) {
		return -EIO;
	}

	get_voicecall_index(audio_mode_play, audio_mode_cap, output_path, input_path, incall_mic, msmic_play, &base, &user);

	if(mc_asoc->dsp_voicecall.onoff[base] == 0) {
		/*	off	*/
		return set_cdsp_off(mc_asoc);
	}

	if((mc_asoc_hwid_a == MCDRV_HWID_15H)
	|| (mc_asoc_hwid_a == MCDRV_HWID_16H)) {
		abLadonHf	= (UINT8*)gabLadonHf31;
		sizLadonHf	= sizeof(gabLadonHf31);
	}
	else {
		abLadonHf	= (UINT8*)gabLadonHf30;
		sizLadonHf	= sizeof(gabLadonHf30);
	}

	if((mc_asoc->cdsp_store.size == 0)
	|| (mc_asoc->cdsp_store.prog == NULL)) {
		err	= _McDrv_Ctrl(MCDRV_SET_CDSP, abLadonHf, sizLadonHf);
		if(err != MCDRV_SUCCESS) {
			return mc_asoc_map_drv_error(err);
		}
		mc_asoc->cdsp_store.prog	= abLadonHf;
		mc_asoc->cdsp_store.size	= sizLadonHf;

		mc_asoc->cdsp_store.param_count	= 0;
		for(i = 0; i < mc_asoc->dsp_voicecall.param_count[base] && err == 0; i++) {
			err	= _McDrv_Ctrl(MCDRV_SET_CDSP_PARAM, &mc_asoc->dsp_voicecall.cdspparam[base][i], 0);
			if(err == MCDRV_SUCCESS) {
				memcpy(&mc_asoc->cdsp_store.cdspparam[mc_asoc->cdsp_store.param_count++],
						&mc_asoc->dsp_voicecall.cdspparam[base][i],
						sizeof(MCDRV_CDSPPARAM));
			}
			else {
				_McDrv_Ctrl(MCDRV_SET_CDSP, NULL, 0);
				return mc_asoc_map_drv_error(err);
			}
		}
	}

	if((user > 0) && (mc_asoc->dsp_voicecall.onoff[user] == 1) && (mc_asoc->cdsp_store.size > 0)) {
		dbg_info("user=%d\n", user);
		for(i = 0; i < mc_asoc->dsp_voicecall.param_count[user] && err == 0; i++) {
			err	= _McDrv_Ctrl(MCDRV_SET_CDSP_PARAM, &mc_asoc->dsp_voicecall.cdspparam[user][i], 0);
			if(err == MCDRV_SUCCESS) {
				memcpy(&mc_asoc->cdsp_store.cdspparam[mc_asoc->cdsp_store.param_count++],
						&mc_asoc->dsp_voicecall.cdspparam[user][i],
						sizeof(MCDRV_CDSPPARAM));
			}
			else {
				err	= mc_asoc_map_drv_error(err);
			}
		}
	}
	if(err == 0) {
		mc_asoc_cdsp_ae_ex_flag	= MC_ASOC_CDSP_ON;
	}
	return err;
}

static void mask_AnaOut_AnaPlay_src(
	MCDRV_CHANNEL	*asOut,
	UINT8			channels,
	const struct mc_asoc_mixer_ctl_info	*mixer_ctl_info
)
{
	int		main_mic_block_on	= get_main_mic_block_on();
	int		sub_mic_block_on	= get_sub_mic_block_on();
	int		hs_mic_block_on		= get_hs_mic_block_on();

	if((mixer_ctl_info->mainmic_play != 1)
	&& (mixer_ctl_info->msmic_play != 1)) {
		if(main_mic_block_on != -1) {
			asOut[0].abSrcOnOff[0]	&= ~main_mic_block_on;
			if(channels > 1) {
				asOut[1].abSrcOnOff[0]	&= ~main_mic_block_on;
			}
		}
	}
	if((mixer_ctl_info->submic_play != 1)
	&& (mixer_ctl_info->msmic_play != 1)) {
		if(sub_mic_block_on != -1) {
			asOut[0].abSrcOnOff[0]	&= ~sub_mic_block_on;
			if(channels > 1) {
				asOut[1].abSrcOnOff[0]	&= ~sub_mic_block_on;
			}
		}
	}
	if(mixer_ctl_info->hsmic_play != 1) {
		if(hs_mic_block_on != -1) {
			asOut[0].abSrcOnOff[0]	&= ~hs_mic_block_on;
			if(channels > 1) {
				asOut[1].abSrcOnOff[0]	&= ~hs_mic_block_on;
			}
		}
	}
	if(mixer_ctl_info->lin1_play != 1) {
		asOut[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK]	&= ~MCDRV_SRC1_LINE1_L_ON;
		asOut[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK]	&= ~MCDRV_SRC1_LINE1_M_ON;
		if(channels > 1) {
			asOut[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK]	&= ~MCDRV_SRC1_LINE1_R_ON;
		}
		else {
			asOut[0].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK]	&= ~MCDRV_SRC1_LINE1_R_ON;
		}
	}
	if(mixer_ctl_info->lin2_play != 1) {
		asOut[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK]	&= ~MCDRV_SRC2_LINE2_L_ON;
		asOut[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK]	&= ~MCDRV_SRC2_LINE2_M_ON;
		if(channels > 1) {
			asOut[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK]	&= ~MCDRV_SRC2_LINE2_R_ON;
		}
		else {
			asOut[0].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK]	&= ~MCDRV_SRC2_LINE2_R_ON;
		}
	}
}

static void mask_AnaOut_src_direct(
	MCDRV_CHANNEL	*asOut,
	UINT8			channels,
	const struct mc_asoc_mixer_ctl_info	*mixer_ctl_info,
	int	preset_idx
)
{
	int		main_mic_block_on	= get_main_mic_block_on();
	int		sub_mic_block_on	= get_sub_mic_block_on();
	int		hs_mic_block_on		= get_hs_mic_block_on();
	UINT8	bCh;

	if(preset_idx < 12) {
		/*	!incall	*/
		if((preset_idx == 4)
		|| (preset_idx == 6)
		|| (preset_idx == 8)
		|| (preset_idx == 10)) {
			/*	in capture	*/
			if(((mixer_ctl_info->mainmic_play == 1) && (mixer_ctl_info->input_path == MC_ASOC_INPUT_PATH_MAINMIC))
			|| ((mixer_ctl_info->submic_play == 1) && (mixer_ctl_info->input_path == MC_ASOC_INPUT_PATH_SUBMIC))
			|| ((mixer_ctl_info->msmic_play == 1) && (mixer_ctl_info->input_path == MC_ASOC_INPUT_PATH_2MIC))
			|| ((mixer_ctl_info->hsmic_play == 1) && (mixer_ctl_info->input_path == MC_ASOC_INPUT_PATH_HS))
			|| ((mixer_ctl_info->lin1_play == 1) && (mixer_ctl_info->input_path == MC_ASOC_INPUT_PATH_LIN1))
			|| ((mixer_ctl_info->lin2_play == 1) && (mixer_ctl_info->input_path == MC_ASOC_INPUT_PATH_LIN2))) {
				mask_AnaOut_AnaPlay_src(asOut, channels, mixer_ctl_info);
			}
			else if(get_ap_cap_port() == -1) {
				switch(mixer_ctl_info->input_path) {
				case	MC_ASOC_INPUT_PATH_MAINMIC:
					for(bCh = 0; bCh < channels; bCh++) {
						if(main_mic_block_on != -1) {
							asOut[bCh].abSrcOnOff[0]	&= (main_mic_block_on|0xAA);
						}
						else {
							asOut[bCh].abSrcOnOff[0]					= 0xAA;
						}
						asOut[bCh].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK]	= 0xAA;
						asOut[bCh].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK]	= 0xAA;
					}
					break;
				case	MC_ASOC_INPUT_PATH_SUBMIC:
					for(bCh = 0; bCh < channels; bCh++) {
						if(sub_mic_block_on != -1) {
							asOut[bCh].abSrcOnOff[0]	&= (sub_mic_block_on|0xAA);
						}
						else {
							asOut[bCh].abSrcOnOff[0]					= 0xAA;
						}
						asOut[bCh].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK]	= 0xAA;
						asOut[bCh].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK]	= 0xAA;
					}
					break;
				case	MC_ASOC_INPUT_PATH_2MIC:
					for(bCh = 0; bCh < channels; bCh++) {
						asOut[bCh].abSrcOnOff[0]	&= (main_mic_block_on|sub_mic_block_on|0xAA);
						asOut[bCh].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK]	= 0xAA;
						asOut[bCh].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK]	= 0xAA;
					}
					break;
				case	MC_ASOC_INPUT_PATH_HS:
					for(bCh = 0; bCh < channels; bCh++) {
						if(hs_mic_block_on != -1) {
							asOut[bCh].abSrcOnOff[0]	&= (hs_mic_block_on|0xAA);
						}
						else {
							asOut[bCh].abSrcOnOff[0]					= 0xAA;
						}
						asOut[bCh].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK]	= 0xAA;
						asOut[bCh].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK]	= 0xAA;
					}
					break;
				case	MC_ASOC_INPUT_PATH_LIN1:
					for(bCh = 0; bCh < channels; bCh++) {
						asOut[bCh].abSrcOnOff[0]						= 0xAA;
						asOut[bCh].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK]	= 0xAA;
					}
					break;
				case	MC_ASOC_INPUT_PATH_LIN2:
					for(bCh = 0; bCh < channels; bCh++) {
						asOut[bCh].abSrcOnOff[0]						= 0xAA;
						asOut[bCh].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK]	= 0xAA;
					}
					break;
				default:
					break;
				}
			}
		}
		else {
			if((preset_idx == 7)
			|| (preset_idx == 9)
			|| (preset_idx == 11)) {
				if(get_ap_cap_port() == -1) {
				 	return;
				 }
			}
			mask_AnaOut_AnaPlay_src(asOut, channels, mixer_ctl_info);
		}
	}
	else {
		switch(mixer_ctl_info->output_path) {
		case	MC_ASOC_OUTPUT_PATH_HS:
			for(bCh = 0; bCh < channels; bCh++) {
				if(hs_mic_block_on != -1) {
					asOut[bCh].abSrcOnOff[0]	&= (hs_mic_block_on|0xAA);
				}
				else {
					asOut[bCh].abSrcOnOff[0]	= 0xAA;
				}
			}
			break;
		case	MC_ASOC_OUTPUT_PATH_BT:
		case	MC_ASOC_OUTPUT_PATH_SP_BT:
			return;
		default:
			if(mixer_ctl_info->incall_mic == 0) {
				/*	MainMic	*/
				for(bCh = 0; bCh < channels; bCh++) {
					if(main_mic_block_on != -1) {
						asOut[bCh].abSrcOnOff[0]	&= (main_mic_block_on|0xAA);
					}
					else {
						asOut[bCh].abSrcOnOff[0]	= 0xAA;
					}
				}
			}
			else if(mixer_ctl_info->incall_mic == 1) {
				/*	SubMic	*/
				for(bCh = 0; bCh < channels; bCh++) {
					if(sub_mic_block_on != -1) {
						asOut[bCh].abSrcOnOff[0]	&= (sub_mic_block_on|0xAA);
					}
					else {
						asOut[bCh].abSrcOnOff[0]	= 0xAA;
					}
				}
			}
			else {
				/*	2Mic	*/
				if(main_mic_block_on != -1) {
					asOut[0].abSrcOnOff[0]	&= (main_mic_block_on|0xAA);
				}
				else {
					asOut[0].abSrcOnOff[0]	= 0xAA;
				}
				if(channels > 1) {
					if(sub_mic_block_on != -1) {
						asOut[1].abSrcOnOff[0]	&= (sub_mic_block_on|0xAA);
					}
					else {
						asOut[1].abSrcOnOff[0]	= 0xAA;
					}
				}
			}
		}
		if((mixer_ctl_info->audio_mode_play != MC_ASOC_AUDIO_MODE_AUDIO_INCALL)
		|| (mc_asoc_ap_play_port != LIN1)) {
			for(bCh = 0; bCh < channels; bCh++) {
				asOut[bCh].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK]	= 0xAA;
			}
		}
		if((mixer_ctl_info->audio_mode_play != MC_ASOC_AUDIO_MODE_AUDIO_INCALL)
		|| (mc_asoc_ap_play_port != LIN2)) {
			for(bCh = 0; bCh < channels; bCh++) {
				asOut[bCh].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK]	= 0xAA;
			}
		}
	}
}

static void mask_AnaOut_src(
	MCDRV_PATH_INFO	*path_info,
	const struct mc_asoc_mixer_ctl_info	*mixer_ctl_info,
	int	preset_idx
)
{
	int		i;
	UINT8	bCh;

	TRACE_FUNC();

	if((mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_SP)
	|| (mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_SP_RC)
	|| (mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_SP_HP)
	|| (mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_SP_LO1)
	|| (mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_SP_LO2)
	|| (mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_SP_BT)) {
		mask_AnaOut_src_direct(path_info->asSpOut, SP_PATH_CHANNELS, mixer_ctl_info, preset_idx);
	}
	else {
		for(i = 0; i < SOURCE_BLOCK_NUM; i++) {
			for(bCh = 0; bCh < SP_PATH_CHANNELS; bCh++) {
				path_info->asSpOut[bCh].abSrcOnOff[i]	= 0xAA;
			}
		}
	}

	if((mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_RC)
	|| (mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_SP_RC)) {
		mask_AnaOut_src_direct(path_info->asRcOut, RC_PATH_CHANNELS, mixer_ctl_info, preset_idx);
	}
	else {
		for(i = 0; i < SOURCE_BLOCK_NUM; i++) {
			for(bCh = 0; bCh < RC_PATH_CHANNELS; bCh++) {
				path_info->asRcOut[bCh].abSrcOnOff[i]	= 0xAA;
			}
		}
	}

	if((mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_HP)
	|| (mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_HS)
	|| (mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_SP_HP)) {
		mask_AnaOut_src_direct(path_info->asHpOut, HP_PATH_CHANNELS, mixer_ctl_info, preset_idx);
	}
	else {
		for(i = 0; i < SOURCE_BLOCK_NUM; i++) {
			for(bCh = 0; bCh < HP_PATH_CHANNELS; bCh++) {
				path_info->asHpOut[bCh].abSrcOnOff[i]	= 0xAA;
			}
		}
	}

	if((mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_LO1)
	|| (mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_SP_LO1)) {
		mask_AnaOut_src_direct(path_info->asLout1, LOUT1_PATH_CHANNELS, mixer_ctl_info, preset_idx);
	}
	else {
		if(preset_idx < 12) {
			if(mc_asoc_ap_cap_port == LOUT1) {
				if(preset_idx <= 3) {
					for(i = 0; i < SOURCE_BLOCK_NUM; i++) {
						for(bCh = 0; bCh < LOUT1_PATH_CHANNELS; bCh++) {
							path_info->asLout1[bCh].abSrcOnOff[i]	= 0xAA;
						}
					}
				}
				else if(preset_idx == 6) {
					for(bCh = 0; bCh < LOUT1_PATH_CHANNELS; bCh++) {
						path_info->asLout1[bCh].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	= 0xAA;
					}
					mask_AnaOut_src_direct(path_info->asLout1, LOUT1_PATH_CHANNELS, mixer_ctl_info, preset_idx);
				}
				else if(preset_idx == 7) {
					for(bCh = 0; bCh < LOUT1_PATH_CHANNELS; bCh++) {
						path_info->asLout1[bCh].abSrcOnOff[0]						= 0xAA;
						path_info->asLout1[bCh].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK]	= 0xAA;
						path_info->asLout1[bCh].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK]	= 0xAA;
					}
				}
				else if((preset_idx == 8)
					 || (preset_idx == 10)) {
					mask_AnaOut_src_direct(path_info->asLout1, LOUT1_PATH_CHANNELS, mixer_ctl_info, preset_idx);
				}
			}
			else {
				for(i = 0; i < SOURCE_BLOCK_NUM; i++) {
					for(bCh = 0; bCh < LOUT1_PATH_CHANNELS; bCh++) {
						path_info->asLout1[bCh].abSrcOnOff[i]	= 0xAA;
					}
				}
			}
		}
		else if((preset_idx == 24)
			 || (preset_idx == 26)) {
			for(bCh = 0; bCh < LOUT1_PATH_CHANNELS; bCh++) {
				path_info->asLout1[bCh].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	= 0xAA;
			}
			mask_AnaOut_src_direct(path_info->asLout1, LOUT1_PATH_CHANNELS, mixer_ctl_info, preset_idx);
		}
		else if(preset_idx == 25) {
		}
		else {
			if((mc_asoc_cp_port == LIN1_LOUT1)
			|| (mc_asoc_cp_port == LIN2_LOUT1)) {
				mask_AnaOut_src_direct(path_info->asLout1, LOUT1_PATH_CHANNELS, mixer_ctl_info, preset_idx);
			}
			else if(mc_asoc_ap_cap_port == LOUT1) {
				if((preset_idx == 18)
				|| (preset_idx == 21)) {
					for(i = 0; i < SOURCE_BLOCK_NUM; i++) {
						for(bCh = 0; bCh < LOUT1_PATH_CHANNELS; bCh++) {
							path_info->asLout1[bCh].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK]	= 0xAA;
							path_info->asLout1[bCh].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK]	= 0xAA;
						}
					}
					mask_AnaOut_src_direct(path_info->asLout1, LOUT1_PATH_CHANNELS, mixer_ctl_info, preset_idx);
				}
				else {
					for(i = 0; i < SOURCE_BLOCK_NUM; i++) {
						for(bCh = 0; bCh < LOUT1_PATH_CHANNELS; bCh++) {
							path_info->asLout1[bCh].abSrcOnOff[i]	= 0xAA;
						}
					}
				}
			}
			else {
				for(i = 0; i < SOURCE_BLOCK_NUM; i++) {
					for(bCh = 0; bCh < LOUT1_PATH_CHANNELS; bCh++) {
						path_info->asLout1[bCh].abSrcOnOff[i]	= 0xAA;
					}
				}
			}
		}
	}

	if((mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_LO2)
	|| (mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_SP_LO2)) {
		mask_AnaOut_src_direct(path_info->asLout2, LOUT2_PATH_CHANNELS, mixer_ctl_info, preset_idx);
	}
	else {
		if(preset_idx < 12) {
			if(mc_asoc_ap_cap_port == LOUT2) {
				if(preset_idx <= 3) {
					for(i = 0; i < SOURCE_BLOCK_NUM; i++) {
						for(bCh = 0; bCh < LOUT2_PATH_CHANNELS; bCh++) {
							path_info->asLout2[bCh].abSrcOnOff[i]	= 0xAA;
						}
					}
				}
				else if(preset_idx == 6) {
					for(bCh = 0; bCh < LOUT1_PATH_CHANNELS; bCh++) {
						path_info->asLout2[bCh].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	= 0xAA;
					}
					mask_AnaOut_src_direct(path_info->asLout2, LOUT2_PATH_CHANNELS, mixer_ctl_info, preset_idx);
				}
				else if(preset_idx == 7) {
					for(bCh = 0; bCh < LOUT1_PATH_CHANNELS; bCh++) {
						path_info->asLout2[bCh].abSrcOnOff[0]						= 0xAA;
						path_info->asLout2[bCh].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK]	= 0xAA;
						path_info->asLout2[bCh].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK]	= 0xAA;
					}
				}
				else if((preset_idx == 8)
					 || (preset_idx == 10)) {
					mask_AnaOut_src_direct(path_info->asLout2, LOUT2_PATH_CHANNELS, mixer_ctl_info, preset_idx);
				}
			}
			else {
				for(i = 0; i < SOURCE_BLOCK_NUM; i++) {
					for(bCh = 0; bCh < LOUT2_PATH_CHANNELS; bCh++) {
						path_info->asLout2[bCh].abSrcOnOff[i]	= 0xAA;
					}
				}
			}
		}
		else if((preset_idx == 24)
			 || (preset_idx == 26)) {
			for(bCh = 0; bCh < LOUT1_PATH_CHANNELS; bCh++) {
				path_info->asLout2[bCh].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]	= 0xAA;
			}
			mask_AnaOut_src_direct(path_info->asLout2, LOUT2_PATH_CHANNELS, mixer_ctl_info, preset_idx);
		}
		else if(preset_idx == 25) {
		}
		else {
			if((mc_asoc_cp_port == LIN1_LOUT2)
			|| (mc_asoc_cp_port == LIN2_LOUT2)) {
				mask_AnaOut_src_direct(path_info->asLout2, LOUT2_PATH_CHANNELS, mixer_ctl_info, preset_idx);
			}
			else if(mc_asoc_ap_cap_port == LOUT2) {
				if((preset_idx == 18)
				|| (preset_idx == 21)) {
					for(i = 0; i < SOURCE_BLOCK_NUM; i++) {
						for(bCh = 0; bCh < LOUT1_PATH_CHANNELS; bCh++) {
							path_info->asLout2[bCh].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK]	= 0xAA;
							path_info->asLout2[bCh].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK]	= 0xAA;
						}
					}
					mask_AnaOut_src_direct(path_info->asLout2, LOUT2_PATH_CHANNELS, mixer_ctl_info, preset_idx);
				}
				else {
					for(i = 0; i < SOURCE_BLOCK_NUM; i++) {
						for(bCh = 0; bCh < LOUT2_PATH_CHANNELS; bCh++) {
							path_info->asLout2[bCh].abSrcOnOff[i]	= 0xAA;
						}
					}
				}
			}
			else {
				for(i = 0; i < SOURCE_BLOCK_NUM; i++) {
					for(bCh = 0; bCh < LOUT2_PATH_CHANNELS; bCh++) {
						path_info->asLout2[bCh].abSrcOnOff[i]	= 0xAA;
					}
				}
			}
		}
	}
}

static void mask_BTOut_src(
	MCDRV_PATH_INFO	*path_info,
	int	output_path
)
{
	int		i;
	UINT8	bCh;

	TRACE_FUNC();

	if((output_path != MC_ASOC_OUTPUT_PATH_BT)
	&& (output_path != MC_ASOC_OUTPUT_PATH_SP_BT)) {
		switch(get_bt_port()) {
		case	DIO_0:
			for(i = 0; i < SOURCE_BLOCK_NUM; i++) {
				for(bCh = 0; bCh < DIT0_PATH_CHANNELS; bCh++) {
					path_info->asDit0[bCh].abSrcOnOff[i]	= 0xAA;
				}
			}
			break;
		case	DIO_1:
			for(i = 0; i < SOURCE_BLOCK_NUM; i++) {
				for(bCh = 0; bCh < DIT1_PATH_CHANNELS; bCh++) {
					path_info->asDit1[bCh].abSrcOnOff[i]	= 0xAA;
				}
			}
			break;
		case	DIO_2:
			for(i = 0; i < SOURCE_BLOCK_NUM; i++) {
				for(bCh = 0; bCh < DIT2_PATH_CHANNELS; bCh++) {
					path_info->asDit2[bCh].abSrcOnOff[i]	= 0xAA;
				}
			}
			break;
		default:
			break;
		}
	}
}

static void mask_ADC_src(
	MCDRV_PATH_INFO		*path_info,
	const struct mc_asoc_mixer_ctl_info	*mixer_ctl_info,
	int	cdsp_onoff,
	int	preset_idx
)
{
	int		main_mic_block_on	= get_main_mic_block_on();
	int		sub_mic_block_on	= get_sub_mic_block_on();
	int		hs_mic_block_on		= get_hs_mic_block_on();

	TRACE_FUNC();

	if(preset_idx < 12) {
		/*	!incall	*/
		if((preset_idx == 4)
		|| (preset_idx == 6)
		|| (preset_idx == 8)
		|| (preset_idx == 10)) {
			/*	in capture	*/
			if((mixer_ctl_info->input_path != MC_ASOC_INPUT_PATH_MAINMIC)
			&& (mixer_ctl_info->input_path != MC_ASOC_INPUT_PATH_2MIC)) {
				if(main_mic_block_on != -1) {
					path_info->asAdc0[0].abSrcOnOff[0]	&= ~main_mic_block_on;
					path_info->asAdc0[1].abSrcOnOff[0]	&= ~main_mic_block_on;
				}
			}
			if((mixer_ctl_info->input_path != MC_ASOC_INPUT_PATH_SUBMIC)
			&&  (mixer_ctl_info->input_path != MC_ASOC_INPUT_PATH_2MIC)) {
				if(sub_mic_block_on != -1) {
					path_info->asAdc0[0].abSrcOnOff[0]	&= ~sub_mic_block_on;
					path_info->asAdc0[1].abSrcOnOff[0]	&= ~sub_mic_block_on;
				}
			}
			if(mixer_ctl_info->input_path != MC_ASOC_INPUT_PATH_HS) {
				if(hs_mic_block_on != -1) {
					path_info->asAdc0[0].abSrcOnOff[0]	&= ~hs_mic_block_on;
					path_info->asAdc0[1].abSrcOnOff[0]	&= ~hs_mic_block_on;
				}
			}
			if(mixer_ctl_info->input_path == MC_ASOC_INPUT_PATH_2MIC) {
					path_info->asAdc0[0].abSrcOnOff[0]	&= ~sub_mic_block_on;
					path_info->asAdc0[1].abSrcOnOff[0]	&= ~main_mic_block_on;
			}
			if(mixer_ctl_info->input_path != MC_ASOC_INPUT_PATH_LIN1) {
				path_info->asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK]	&= ~MCDRV_SRC1_LINE1_L_ON;
				path_info->asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK]	&= ~MCDRV_SRC1_LINE1_R_ON;
			}
			if(mixer_ctl_info->input_path != MC_ASOC_INPUT_PATH_LIN2) {
				path_info->asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK]	&= ~MCDRV_SRC2_LINE2_L_ON;
				path_info->asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK]	&= ~MCDRV_SRC2_LINE2_R_ON;
			}
		}
		else {
			if((mixer_ctl_info->mainmic_play != 1)
			&& (mixer_ctl_info->msmic_play != 1)) {
				if(main_mic_block_on != -1) {
					path_info->asAdc0[0].abSrcOnOff[0]	&= ~main_mic_block_on;
					path_info->asAdc0[1].abSrcOnOff[0]	&= ~main_mic_block_on;
					path_info->asAdc1[0].abSrcOnOff[0]	&= ~main_mic_block_on;
				}
			}
			if((mixer_ctl_info->submic_play != 1)
			&& (mixer_ctl_info->msmic_play != 1)) {
				if(sub_mic_block_on != -1) {
					path_info->asAdc0[0].abSrcOnOff[0]	&= ~sub_mic_block_on;
					path_info->asAdc0[1].abSrcOnOff[0]	&= ~sub_mic_block_on;
					path_info->asAdc1[0].abSrcOnOff[0]	&= ~sub_mic_block_on;
				}
			}
			if(mixer_ctl_info->hsmic_play != 1) {
				if(hs_mic_block_on != -1) {
					path_info->asAdc0[0].abSrcOnOff[0]	&= ~hs_mic_block_on;
					path_info->asAdc0[1].abSrcOnOff[0]	&= ~hs_mic_block_on;
					path_info->asAdc1[0].abSrcOnOff[0]	&= ~hs_mic_block_on;
				}
			}
			if(mixer_ctl_info->lin1_play != 1) {
				path_info->asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK]	&= ~MCDRV_SRC1_LINE1_L_ON;
				path_info->asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK]	&= ~MCDRV_SRC1_LINE1_M_ON;
				path_info->asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK]	&= ~MCDRV_SRC1_LINE1_R_ON;
				path_info->asAdc1[0].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK]	&= ~MCDRV_SRC1_LINE1_M_ON;
			}
			if(mixer_ctl_info->lin2_play != 1) {
				path_info->asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK]	&= ~MCDRV_SRC2_LINE2_L_ON;
				path_info->asAdc0[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK]	&= ~MCDRV_SRC2_LINE2_M_ON;
				path_info->asAdc0[1].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK]	&= ~MCDRV_SRC2_LINE2_R_ON;
				path_info->asAdc1[0].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK]	&= ~MCDRV_SRC2_LINE2_M_ON;
			}
		}
	}
	else {
		/*	incall or incommunication	*/
		if((mixer_ctl_info->output_path != MC_ASOC_OUTPUT_PATH_BT)
		&& (mixer_ctl_info->output_path != MC_ASOC_OUTPUT_PATH_SP_BT)) {
			if(mixer_ctl_info->output_path != MC_ASOC_OUTPUT_PATH_HS) {
				if(hs_mic_block_on != -1) {
					path_info->asAdc0[0].abSrcOnOff[0]	&= ~hs_mic_block_on;
					path_info->asAdc0[1].abSrcOnOff[0]	&= ~hs_mic_block_on;
				}
				if((mixer_ctl_info->incall_mic != MC_ASOC_INCALL_MIC_MAINMIC)
				&& (mixer_ctl_info->incall_mic != MC_ASOC_INCALL_MIC_2MIC)) {
					if(main_mic_block_on != -1) {
						path_info->asAdc0[0].abSrcOnOff[0]	&= ~main_mic_block_on;
						path_info->asAdc0[1].abSrcOnOff[0]	&= ~main_mic_block_on;
					}
				}
				if((mixer_ctl_info->incall_mic != MC_ASOC_INCALL_MIC_SUBMIC)
				&& (mixer_ctl_info->incall_mic != MC_ASOC_INCALL_MIC_2MIC)) {
					if(sub_mic_block_on != -1) {
						path_info->asAdc0[0].abSrcOnOff[0]	&= ~sub_mic_block_on;
						path_info->asAdc0[1].abSrcOnOff[0]	&= ~sub_mic_block_on;
					}
				}
				if(mixer_ctl_info->incall_mic == MC_ASOC_INCALL_MIC_2MIC) {
					path_info->asAdc0[0].abSrcOnOff[0]	&= ~sub_mic_block_on;
					path_info->asAdc0[1].abSrcOnOff[0]	&= ~main_mic_block_on;
				}
			}
			else {
				if(main_mic_block_on != -1) {
					path_info->asAdc0[0].abSrcOnOff[0]	&= ~main_mic_block_on;
					path_info->asAdc0[1].abSrcOnOff[0]	&= ~main_mic_block_on;
				}
				if(sub_mic_block_on != -1) {
					path_info->asAdc0[0].abSrcOnOff[0]	&= ~sub_mic_block_on;
					path_info->asAdc0[1].abSrcOnOff[0]	&= ~sub_mic_block_on;
				}
			}
		}
	}
}

static void add_path_info(
	MCDRV_PATH_INFO	*dst_path_info,
	MCDRV_PATH_INFO	*src_path_info
)
{
	UINT8	bCh, bBlock;

	TRACE_FUNC();

	for(bCh = 0; bCh < HP_PATH_CHANNELS; bCh++) {
		for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++) {
			dst_path_info->asHpOut[bCh].abSrcOnOff[bBlock]	|= src_path_info->asHpOut[bCh].abSrcOnOff[bBlock];
		}
	}
	for(bCh = 0; bCh < SP_PATH_CHANNELS; bCh++) {
		for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++) {
			dst_path_info->asSpOut[bCh].abSrcOnOff[bBlock]	|= src_path_info->asSpOut[bCh].abSrcOnOff[bBlock];
		}
	}
	for(bCh = 0; bCh < RC_PATH_CHANNELS; bCh++) {
		for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++) {
			dst_path_info->asRcOut[bCh].abSrcOnOff[bBlock]	|= src_path_info->asRcOut[bCh].abSrcOnOff[bBlock];
		}
	}
	for(bCh = 0; bCh < LOUT1_PATH_CHANNELS; bCh++) {
		for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++) {
			dst_path_info->asLout1[bCh].abSrcOnOff[bBlock]	|= src_path_info->asLout1[bCh].abSrcOnOff[bBlock];
		}
	}
	for(bCh = 0; bCh < LOUT2_PATH_CHANNELS; bCh++) {
		for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++) {
			dst_path_info->asLout2[bCh].abSrcOnOff[bBlock]	|= src_path_info->asLout2[bCh].abSrcOnOff[bBlock];
		}
	}
	for(bCh = 0; bCh < PEAK_PATH_CHANNELS; bCh++) {
		for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++) {
			dst_path_info->asPeak[bCh].abSrcOnOff[bBlock]	|= src_path_info->asPeak[bCh].abSrcOnOff[bBlock];
		}
	}
	for(bCh = 0; bCh < DIT0_PATH_CHANNELS; bCh++) {
		for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++) {
			dst_path_info->asDit0[bCh].abSrcOnOff[bBlock]	|= src_path_info->asDit0[bCh].abSrcOnOff[bBlock];
		}
	}
	for(bCh = 0; bCh < DIT1_PATH_CHANNELS; bCh++) {
		for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++) {
			dst_path_info->asDit1[bCh].abSrcOnOff[bBlock]	|= src_path_info->asDit1[bCh].abSrcOnOff[bBlock];
		}
	}
	for(bCh = 0; bCh < DIT2_PATH_CHANNELS; bCh++) {
		for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++) {
			dst_path_info->asDit2[bCh].abSrcOnOff[bBlock]	|= src_path_info->asDit2[bCh].abSrcOnOff[bBlock];
		}
	}
	for(bCh = 0; bCh < DAC_PATH_CHANNELS; bCh++) {
		for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++) {
			dst_path_info->asDac[bCh].abSrcOnOff[bBlock]	|= src_path_info->asDac[bCh].abSrcOnOff[bBlock];
		}
	}
	for(bCh = 0; bCh < AE_PATH_CHANNELS; bCh++) {
		for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++) {
			dst_path_info->asAe[bCh].abSrcOnOff[bBlock]	|= src_path_info->asAe[bCh].abSrcOnOff[bBlock];
		}
	}
	for(bCh = 0; bCh < CDSP_PATH_CHANNELS; bCh++) {
		for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++) {
			dst_path_info->asCdsp[bCh].abSrcOnOff[bBlock]	|= src_path_info->asCdsp[bCh].abSrcOnOff[bBlock];
		}
	}
	for(bCh = 0; bCh < ADC0_PATH_CHANNELS; bCh++) {
		for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++) {
			dst_path_info->asAdc0[bCh].abSrcOnOff[bBlock]	|= src_path_info->asAdc0[bCh].abSrcOnOff[bBlock];
		}
	}
	for(bCh = 0; bCh < ADC1_PATH_CHANNELS; bCh++) {
		for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++) {
			dst_path_info->asAdc1[bCh].abSrcOnOff[bBlock]	|= src_path_info->asAdc1[bCh].abSrcOnOff[bBlock];
		}
	}
	for(bCh = 0; bCh < MIX_PATH_CHANNELS; bCh++) {
		for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++) {
			dst_path_info->asMix[bCh].abSrcOnOff[bBlock]	|= src_path_info->asMix[bCh].abSrcOnOff[bBlock];
		}
	}
	for(bCh = 0; bCh < BIAS_PATH_CHANNELS; bCh++) {
		for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++) {
			dst_path_info->asBias[bCh].abSrcOnOff[bBlock]	|= src_path_info->asBias[bCh].abSrcOnOff[bBlock];
		}
	}
	for(bCh = 0; bCh < MIX2_PATH_CHANNELS; bCh++) {
		for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++) {
			dst_path_info->asMix2[bCh].abSrcOnOff[bBlock]	|= src_path_info->asMix2[bCh].abSrcOnOff[bBlock];
		}
	}
}

static void exchange_ADCtoPDM(
	MCDRV_PATH_INFO	*path_info
)
{
	UINT8	bCh;

	TRACE_FUNC();

	for(bCh = 0; bCh < PEAK_PATH_CHANNELS; bCh++) {
		if((path_info->asPeak[bCh].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK] & MCDRV_SRC4_ADC0_ON) != 0) {
			path_info->asPeak[bCh].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	&= (UINT8)~MCDRV_SRC4_ADC0_ON;
			path_info->asPeak[bCh].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_OFF;
			path_info->asPeak[bCh].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]	|= MCDRV_SRC4_PDM_ON;
		}
	}
	for(bCh = 0; bCh < DIT0_PATH_CHANNELS; bCh++) {
		if((path_info->asDit0[bCh].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK] & MCDRV_SRC4_ADC0_ON) != 0) {
			path_info->asDit0[bCh].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	&= (UINT8)~MCDRV_SRC4_ADC0_ON;
			path_info->asDit0[bCh].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_OFF;
			path_info->asDit0[bCh].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]	|= MCDRV_SRC4_PDM_ON;
		}
	}
	for(bCh = 0; bCh < DIT1_PATH_CHANNELS; bCh++) {
		if((path_info->asDit1[bCh].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK] & MCDRV_SRC4_ADC0_ON) != 0) {
			path_info->asDit1[bCh].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	&= (UINT8)~MCDRV_SRC4_ADC0_ON;
			path_info->asDit1[bCh].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_OFF;
			path_info->asDit1[bCh].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]	|= MCDRV_SRC4_PDM_ON;
		}
	}
	for(bCh = 0; bCh < DIT2_PATH_CHANNELS; bCh++) {
		if((path_info->asDit2[bCh].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK] & MCDRV_SRC4_ADC0_ON) != 0) {
			path_info->asDit2[bCh].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	&= (UINT8)~MCDRV_SRC4_ADC0_ON;
			path_info->asDit2[bCh].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_OFF;
			path_info->asDit2[bCh].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]	|= MCDRV_SRC4_PDM_ON;
		}
	}
	for(bCh = 0; bCh < DAC_PATH_CHANNELS; bCh++) {
		if((path_info->asDac[bCh].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK] & MCDRV_SRC4_ADC0_ON) != 0) {
			path_info->asDac[bCh].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	&= (UINT8)~MCDRV_SRC4_ADC0_ON;
			path_info->asDac[bCh].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_OFF;
			path_info->asDac[bCh].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]	|= MCDRV_SRC4_PDM_ON;
		}
	}
	for(bCh = 0; bCh < AE_PATH_CHANNELS; bCh++) {
		if((path_info->asAe[bCh].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK] & MCDRV_SRC4_ADC0_ON) != 0) {
			path_info->asAe[bCh].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	&= (UINT8)~MCDRV_SRC4_ADC0_ON;
			path_info->asAe[bCh].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_OFF;
			path_info->asAe[bCh].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]	|= MCDRV_SRC4_PDM_ON;
		}
	}
	for(bCh = 0; bCh < CDSP_PATH_CHANNELS; bCh++) {
		if((path_info->asCdsp[bCh].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK] & MCDRV_SRC4_ADC0_ON) != 0) {
			path_info->asCdsp[bCh].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	&= (UINT8)~MCDRV_SRC4_ADC0_ON;
			path_info->asCdsp[bCh].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_OFF;
			path_info->asCdsp[bCh].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]	|= MCDRV_SRC4_PDM_ON;
		}
	}
	for(bCh = 0; bCh < MIX_PATH_CHANNELS; bCh++) {
		if((path_info->asMix[bCh].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK] & MCDRV_SRC4_ADC0_ON) != 0) {
			path_info->asMix[bCh].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	&= (UINT8)~MCDRV_SRC4_ADC0_ON;
			path_info->asMix[bCh].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_OFF;
			path_info->asMix[bCh].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]	|= MCDRV_SRC4_PDM_ON;
		}
	}
	for(bCh = 0; bCh < MIX2_PATH_CHANNELS; bCh++) {
		if((path_info->asMix2[bCh].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK] & MCDRV_SRC4_ADC0_ON) != 0) {
			path_info->asMix2[bCh].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	&= (UINT8)~MCDRV_SRC4_ADC0_ON;
			path_info->asMix2[bCh].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_OFF;
			path_info->asMix2[bCh].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]	|= MCDRV_SRC4_PDM_ON;
		}
	}
}

static void set_ain_play_path(
	MCDRV_PATH_INFO	*path_info,
	const struct mc_asoc_mixer_ctl_info	*mixer_ctl_info,
	int	cdsp_onoff,
	int	preset_idx,
	int	ignore_input_path
)
{
	int		idx	= AnalogPathMapping[cdsp_onoff][preset_idx];

	TRACE_FUNC();

	if(idx >= ARRAY_SIZE(AnalogInputPath)) {
		dbg_info("\n********\nAnalogPathMapping err\n********\n");
		return;
	}

	if(mixer_ctl_info->mainmic_play == 1) {
		if((ignore_input_path != 0)
		|| (mixer_ctl_info->input_path == MC_ASOC_INPUT_PATH_MAINMIC)) {
			add_path_info(path_info, (MCDRV_PATH_INFO*)&AnalogInputPath[idx]);
			if(get_main_mic_block_on() == -1) {
				exchange_ADCtoPDM(path_info);
			}
			mask_ADC_src(path_info, mixer_ctl_info, cdsp_onoff, preset_idx);
			mask_BTOut_src(path_info, mixer_ctl_info->output_path);
			return;
		}
	}
	if(mixer_ctl_info->submic_play == 1) {
		if((ignore_input_path != 0)
		|| (mixer_ctl_info->input_path == MC_ASOC_INPUT_PATH_SUBMIC)) {
			if(get_sub_mic_block_on() == -1) {
				add_path_info(path_info, (MCDRV_PATH_INFO*)&AnalogInputPath[idx]);
				exchange_ADCtoPDM(path_info);
			}
			else {
				add_path_info(path_info, (MCDRV_PATH_INFO*)&AnalogInputPath[idx]);
			}
			mask_ADC_src(path_info, mixer_ctl_info, cdsp_onoff, preset_idx);
			mask_BTOut_src(path_info, mixer_ctl_info->output_path);
			return;
		}
	}
	if(mixer_ctl_info->hsmic_play == 1) {
		if((ignore_input_path != 0)
		|| (mixer_ctl_info->input_path == MC_ASOC_INPUT_PATH_HS)) {
			if(get_hs_mic_block_on() == -1) {
				add_path_info(path_info, (MCDRV_PATH_INFO*)&AnalogInputPath[idx]);
				exchange_ADCtoPDM(path_info);
			}
			else {
				add_path_info(path_info, (MCDRV_PATH_INFO*)&AnalogInputPath[idx]);
			}
			mask_ADC_src(path_info, mixer_ctl_info, cdsp_onoff, preset_idx);
			mask_BTOut_src(path_info, mixer_ctl_info->output_path);
			return;
		}
	}
	if(mixer_ctl_info->msmic_play == 1) {
		if((ignore_input_path != 0)
		|| (mixer_ctl_info->input_path == MC_ASOC_INPUT_PATH_2MIC)) {
			add_path_info(path_info, (MCDRV_PATH_INFO*)&AnalogInputPath[idx]);
			mask_ADC_src(path_info, mixer_ctl_info, cdsp_onoff, preset_idx);
			mask_BTOut_src(path_info, mixer_ctl_info->output_path);
			return;
		}
	}
	if(mixer_ctl_info->lin1_play == 1) {
		if((ignore_input_path != 0)
		|| (mixer_ctl_info->input_path == MC_ASOC_INPUT_PATH_LIN1)) {
			add_path_info(path_info, (MCDRV_PATH_INFO*)&AnalogInputPath[idx]);
			mask_ADC_src(path_info, mixer_ctl_info, cdsp_onoff, preset_idx);
			mask_BTOut_src(path_info, mixer_ctl_info->output_path);
			return;
		}
	}
	if(mixer_ctl_info->lin2_play == 1) {
		if((ignore_input_path != 0)
		|| (mixer_ctl_info->input_path == MC_ASOC_INPUT_PATH_LIN2)) {
			add_path_info(path_info, (MCDRV_PATH_INFO*)&AnalogInputPath[idx]);
			mask_ADC_src(path_info, mixer_ctl_info, cdsp_onoff, preset_idx);
			mask_BTOut_src(path_info, mixer_ctl_info->output_path);
			return;
		}
	}
}

static int get_path_preset_idx(
	const struct mc_asoc_mixer_ctl_info	*mixer_ctl_info
)
{
	TRACE_FUNC();

	if((mixer_ctl_info->audio_mode_play == MC_ASOC_AUDIO_MODE_INCOMM)
	&& (mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_INCOMM)) {
		if(mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_BT) {
			return 25;
		}
		else if(mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_SP_BT) {
			return 26;
		}
		else {
			return 24;
		}
	}

	if(mixer_ctl_info->audio_mode_play == MC_ASOC_AUDIO_MODE_INCALL) {
		if(mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_INCALL) {
			if(mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_BT) {
				return 13;
			}
			else if(mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_SP_BT) {
				return 14;
			}
			else {
				return 12;
			}
		}
		else if(mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO_INCALL) {
			if(mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_BT) {
				return 19;
			}
			else if(mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_SP_BT) {
				return 20;
			}
			else {
				return 18;
			}
		}
	}

	if(mixer_ctl_info->audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO_INCALL) {
		if(mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_INCALL) {
			if(mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_BT) {
				return 16;
			}
			else if(mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_SP_BT) {
				return 17;
			}
			else {
				return 15;
			}
		}
		else if(mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO_INCALL) {
			if(mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_BT) {
				return 22;
			}
			else if(mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_SP_BT) {
				return 23;
			}
			else {
				return 21;
			}
		}
	}

	if(((mixer_ctl_info->audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO)
		&& (mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_OFF))
	|| ((mixer_ctl_info->audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO)
		&& (mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_INCALL))
	|| ((mixer_ctl_info->audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO)
		&& (mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_INCOMM))
	|| ((mixer_ctl_info->audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)
		&& (mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_OFF))
	|| ((mixer_ctl_info->audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)
		&& (mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_INCOMM))) {
		if(mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_BT) {
			return 2;
		}
		else if(mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_SP_BT) {
			return 3;
		}
		else {
			return 1;
		}
	}
	else if(((mixer_ctl_info->audio_mode_play == MC_ASOC_AUDIO_MODE_OFF)
			&& (mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO))
		 || ((mixer_ctl_info->audio_mode_play == MC_ASOC_AUDIO_MODE_OFF)
		 	&& (mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO_INCALL))
		 || ((mixer_ctl_info->audio_mode_play == MC_ASOC_AUDIO_MODE_INCALL)
		 	&& (mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO))
		 || ((mixer_ctl_info->audio_mode_play == MC_ASOC_AUDIO_MODE_INCOMM)
		 	&& (mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO))
		 || ((mixer_ctl_info->audio_mode_play == MC_ASOC_AUDIO_MODE_INCOMM)
		 	&& (mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO_INCALL))) {
		if((mixer_ctl_info->input_path != MC_ASOC_INPUT_PATH_VOICECALL)
		&& (mixer_ctl_info->input_path != MC_ASOC_INPUT_PATH_VOICEUPLINK)
		&& (mixer_ctl_info->input_path != MC_ASOC_INPUT_PATH_VOICEDOWNLINK)) {
			if(mixer_ctl_info->input_path == MC_ASOC_INPUT_PATH_BT) {
				return 5;
			}
			else {
				return 4;
			}
		}
		else {
			return 4;
		}
	}
	else if(((mixer_ctl_info->audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO)
			&& (mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO))
		 || ((mixer_ctl_info->audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO)
		 	&& (mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO_INCALL))
		 || ((mixer_ctl_info->audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)
		 	&& (mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO))) {
		if((mixer_ctl_info->input_path != MC_ASOC_INPUT_PATH_VOICECALL)
		&& (mixer_ctl_info->input_path != MC_ASOC_INPUT_PATH_VOICEUPLINK)
		&& (mixer_ctl_info->input_path != MC_ASOC_INPUT_PATH_VOICEDOWNLINK)) {
			if(mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_BT) {
				if(mixer_ctl_info->input_path == MC_ASOC_INPUT_PATH_BT) {
					return 9;
				}
				else {
					return 8;
				}
			}
			else if(mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_SP_BT) {
				if(mixer_ctl_info->input_path == MC_ASOC_INPUT_PATH_BT) {
					return 11;
				}
				else {
					return 10;
				}
			}
			else {
				if(mixer_ctl_info->input_path == MC_ASOC_INPUT_PATH_BT) {
					return 7;
				}
				else {
					return 6;
				}
			}
		}
		else {
			if(mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_BT) {
				return 8;
			}
			else if(mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_SP_BT) {
				return 10;
			}
			else {
				return 6;
			}
		}
	}

	return 0;
}

static void set_BIAS(
	MCDRV_PATH_INFO	*path_info
)
{
	int		i;
	int		mic_bias[3]	= {mc_asoc_mic1_bias, mc_asoc_mic2_bias, mc_asoc_mic3_bias};
	UINT8	bBlock[3]	= {MCDRV_SRC_MIC1_BLOCK, MCDRV_SRC_MIC2_BLOCK, MCDRV_SRC_MIC3_BLOCK};
	UINT8	bOn[3]		= {MCDRV_SRC0_MIC1_ON, MCDRV_SRC0_MIC2_ON, MCDRV_SRC0_MIC3_ON};
	UINT8	bCh;

	for(i = 0; i < 3; i++) {
		switch(mic_bias[i]) {
		case	BIAS_ON_ALWAYS:
			path_info->asBias[0].abSrcOnOff[bBlock[i]]	|= bOn[i];
			break;
		case	BIAS_OFF:
			path_info->asBias[0].abSrcOnOff[bBlock[i]]	&= ~bOn[i];
			break;
		case	BIAS_SYNC_MIC:
			path_info->asBias[0].abSrcOnOff[bBlock[i]]	&= ~bOn[i];
			for(bCh = 0; bCh < HP_PATH_CHANNELS; bCh++) {
				if((path_info->asHpOut[bCh].abSrcOnOff[bBlock[i]] & bOn[i]) != 0) {
					path_info->asBias[0].abSrcOnOff[bBlock[i]]	|= bOn[i];
					break;
				}
			}
			for(bCh = 0; bCh < SP_PATH_CHANNELS; bCh++) {
				if((path_info->asSpOut[bCh].abSrcOnOff[bBlock[i]] & bOn[i]) != 0) {
					path_info->asBias[0].abSrcOnOff[bBlock[i]]	|= bOn[i];
					break;
				}
			}
			for(bCh = 0; bCh < RC_PATH_CHANNELS; bCh++) {
				if((path_info->asRcOut[bCh].abSrcOnOff[bBlock[i]] & bOn[i]) != 0) {
					path_info->asBias[0].abSrcOnOff[bBlock[i]]	|= bOn[i];
					break;
				}
			}
			for(bCh = 0; bCh < LOUT1_PATH_CHANNELS; bCh++) {
				if((path_info->asLout1[bCh].abSrcOnOff[bBlock[i]] & bOn[i]) != 0) {
					path_info->asBias[0].abSrcOnOff[bBlock[i]]	|= bOn[i];
					break;
				}
			}
			for(bCh = 0; bCh < LOUT2_PATH_CHANNELS; bCh++) {
				if((path_info->asLout2[bCh].abSrcOnOff[bBlock[i]] & bOn[i]) != 0) {
					path_info->asBias[0].abSrcOnOff[bBlock[i]]	|= bOn[i];
					break;
				}
			}
			for(bCh = 0; bCh < ADC0_PATH_CHANNELS; bCh++) {
				if((path_info->asAdc0[bCh].abSrcOnOff[bBlock[i]] & bOn[i]) != 0) {
					path_info->asBias[0].abSrcOnOff[bBlock[i]]	|= bOn[i];
					break;
				}
			}
			for(bCh = 0; bCh < ADC1_PATH_CHANNELS; bCh++) {
				if((path_info->asAdc1[bCh].abSrcOnOff[bBlock[i]] & bOn[i]) != 0) {
					path_info->asBias[0].abSrcOnOff[bBlock[i]]	|= bOn[i];
					break;
				}
			}
			break;
		default:
			break;
		}
	}
}

static void swap_DIT_APCP(
	MCDRV_PATH_INFO		*path_info,
	int	ap_cap_port,
	int	cp_port
)
{
	MCDRV_CHANNEL	*asDitAP;
	MCDRV_CHANNEL	*asDitCP;
	int		i;

	TRACE_FUNC();

	if(ap_cap_port != cp_port) {
		asDitAP	= (MCDRV_CHANNEL*)((void*)path_info+get_port_srconoff_offset(ap_cap_port));
		asDitCP	= (MCDRV_CHANNEL*)((void*)path_info+get_port_srconoff_offset(cp_port));
		for(i = 0; i < SOURCE_BLOCK_NUM; i++) {
			asDitCP[0].abSrcOnOff[i]	|= asDitAP[0].abSrcOnOff[i];
			asDitAP[0].abSrcOnOff[i]	= 0xAA;
		}
	}
}

static void get_path_info(
	struct mc_asoc_data	*mc_asoc,
	MCDRV_PATH_INFO		*path_info,
	const struct mc_asoc_mixer_ctl_info	*mixer_ctl_info,
	int					cdsp_onoff,
	int					preset_idx
)
{
	int		ain_play	= 0;
	int		ap_cap_port	= get_ap_cap_port();
	UINT8	mute_DIT	= 0;

	TRACE_FUNC();

	if((mixer_ctl_info->mainmic_play == 1)
	|| (mixer_ctl_info->submic_play == 1)
	|| (mixer_ctl_info->msmic_play == 1)
	|| (mixer_ctl_info->hsmic_play == 1)
	|| (mixer_ctl_info->lin1_play == 1)
	|| (mixer_ctl_info->lin2_play == 1)) {
		ain_play	= 1;
	}
	else {
		ain_play	= 0;
	}

	if(mixer_ctl_info->dtmf_control == 1) {
		if((mixer_ctl_info->dtmf_output == MC_ASOC_DTMF_OUTPUT_SP)
		&& (mixer_ctl_info->output_path != MC_ASOC_OUTPUT_PATH_SP)) {
		}
		else {
			if(DtmfPathMapping[preset_idx] < ARRAY_SIZE(DtmfPath)) {
				add_path_info(path_info, (MCDRV_PATH_INFO*)&DtmfPath[DtmfPathMapping[preset_idx]]);
				mask_AnaOut_src(path_info, mixer_ctl_info, preset_idx);
			}
			else {
				dbg_info("\n********\nDtmfPathMapping err\n********\n");
			}
		}
	}

	if((mixer_ctl_info->audio_mode_play == MC_ASOC_AUDIO_MODE_INCOMM)
	&& (mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_INCOMM)) {
		if(mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_BT) {
			add_path_info(path_info, (MCDRV_PATH_INFO*)&stPresetPathInfo[cdsp_onoff][preset_idx]);
		}
		else if(mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_SP_BT) {
			add_path_info(path_info, (MCDRV_PATH_INFO*)&stPresetPathInfo[cdsp_onoff][preset_idx]);
			mask_AnaOut_src(path_info, mixer_ctl_info, preset_idx);
		}
		else {
			add_path_info(path_info, (MCDRV_PATH_INFO*)&stPresetPathInfo[cdsp_onoff][preset_idx]);
			if((mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_HS)
			|| (mixer_ctl_info->incall_mic == MC_ASOC_INCALL_MIC_2MIC)
			|| ((mixer_ctl_info->incall_mic == MC_ASOC_INCALL_MIC_MAINMIC) && (mc_asoc_main_mic != MIC_PDM))
			|| ((mixer_ctl_info->incall_mic == MC_ASOC_INCALL_MIC_SUBMIC) && (mc_asoc_sub_mic != MIC_PDM))) {
			}
			else {
				/*	ADC->PDM	*/
				exchange_ADCtoPDM(path_info);
			}
			mask_ADC_src(path_info, mixer_ctl_info, cdsp_onoff, preset_idx);
			mask_AnaOut_src(path_info, mixer_ctl_info, preset_idx);
		}
		if(mc_asoc_ap_cap_port_rate == MCDRV_FS_8000) {
			if(mc_asoc_cap_path == DIO_PORT0) {
				swap_DIT_APCP(path_info, ap_cap_port, DIO_0);
			}
			else if(mc_asoc_cap_path == DIO_PORT1) {
				swap_DIT_APCP(path_info, ap_cap_port, DIO_1);
			}
			else if(mc_asoc_cap_path == DIO_PORT2) {
				swap_DIT_APCP(path_info, ap_cap_port, DIO_2);
			}
		}
		return;
	}

	if(mixer_ctl_info->audio_mode_play == MC_ASOC_AUDIO_MODE_INCALL) {
		if(mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_INCALL) {
			if(mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_BT) {
				add_path_info(path_info, (MCDRV_PATH_INFO*)&stPresetPathInfo[cdsp_onoff][preset_idx]);
			}
			else if(mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_SP_BT) {
				add_path_info(path_info, (MCDRV_PATH_INFO*)&stPresetPathInfo[cdsp_onoff][preset_idx]);
				mask_AnaOut_src(path_info, mixer_ctl_info, preset_idx);
			}
			else {
				add_path_info(path_info, (MCDRV_PATH_INFO*)&stPresetPathInfo[cdsp_onoff][preset_idx]);
				if((mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_HS)
				|| (mixer_ctl_info->incall_mic == MC_ASOC_INCALL_MIC_2MIC)
				|| ((mixer_ctl_info->incall_mic == MC_ASOC_INCALL_MIC_MAINMIC) && (mc_asoc_main_mic != MIC_PDM))
				|| ((mixer_ctl_info->incall_mic == MC_ASOC_INCALL_MIC_SUBMIC) && (mc_asoc_sub_mic != MIC_PDM))) {
				}
				else {
					/*	ADC->PDM	*/
					exchange_ADCtoPDM(path_info);
				}
				mask_ADC_src(path_info, mixer_ctl_info, cdsp_onoff, preset_idx);
				mask_AnaOut_src(path_info, mixer_ctl_info, preset_idx);
			}
			return;
		}
		else if(mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO_INCALL) {
			if(mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_BT) {
				add_path_info(path_info, (MCDRV_PATH_INFO*)&stPresetPathInfo[cdsp_onoff][preset_idx]);
			}
			else if(mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_SP_BT) {
				add_path_info(path_info, (MCDRV_PATH_INFO*)&stPresetPathInfo[cdsp_onoff][preset_idx]);
				mask_AnaOut_src(path_info, mixer_ctl_info, preset_idx);
			}
			else {
				add_path_info(path_info, (MCDRV_PATH_INFO*)&stPresetPathInfo[cdsp_onoff][preset_idx]);
				if((mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_HS)
				|| (mixer_ctl_info->incall_mic == MC_ASOC_INCALL_MIC_2MIC)
				|| ((mixer_ctl_info->incall_mic == MC_ASOC_INCALL_MIC_MAINMIC) && (mc_asoc_main_mic != MIC_PDM))
				|| ((mixer_ctl_info->incall_mic == MC_ASOC_INCALL_MIC_SUBMIC) && (mc_asoc_sub_mic != MIC_PDM))) {
				}
				else {
					/*	ADC->PDM	*/
					exchange_ADCtoPDM(path_info);
				}
				mask_ADC_src(path_info, mixer_ctl_info, cdsp_onoff, preset_idx);
				mask_AnaOut_src(path_info, mixer_ctl_info, preset_idx);
			}
			return;
		}
	}

	if(mixer_ctl_info->audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO_INCALL) {
		if(mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_INCALL) {
			if(mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_BT) {
				add_path_info(path_info, (MCDRV_PATH_INFO*)&stPresetPathInfo[cdsp_onoff][preset_idx]);
			}
			else if(mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_SP_BT) {
				add_path_info(path_info, (MCDRV_PATH_INFO*)&stPresetPathInfo[cdsp_onoff][preset_idx]);
				mask_AnaOut_src(path_info, mixer_ctl_info, preset_idx);
			}
			else {
				add_path_info(path_info, (MCDRV_PATH_INFO*)&stPresetPathInfo[cdsp_onoff][preset_idx]);
				if((mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_HS)
				|| (mixer_ctl_info->incall_mic == MC_ASOC_INCALL_MIC_2MIC)
				|| ((mixer_ctl_info->incall_mic == MC_ASOC_INCALL_MIC_MAINMIC) && (mc_asoc_main_mic != MIC_PDM))
				|| ((mixer_ctl_info->incall_mic == MC_ASOC_INCALL_MIC_SUBMIC) && (mc_asoc_sub_mic != MIC_PDM))) {
				}
				else {
					/*	ADC->PDM	*/
					exchange_ADCtoPDM(path_info);
				}
				mask_ADC_src(path_info, mixer_ctl_info, cdsp_onoff, preset_idx);
				mask_AnaOut_src(path_info, mixer_ctl_info, preset_idx);
			}
			return;
		}
		else if(mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO_INCALL) {
			if(mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_BT) {
				add_path_info(path_info, (MCDRV_PATH_INFO*)&stPresetPathInfo[cdsp_onoff][preset_idx]);
			}
			else if(mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_SP_BT) {
				add_path_info(path_info, (MCDRV_PATH_INFO*)&stPresetPathInfo[cdsp_onoff][preset_idx]);
				mask_AnaOut_src(path_info, mixer_ctl_info, preset_idx);
			}
			else {
				add_path_info(path_info, (MCDRV_PATH_INFO*)&stPresetPathInfo[cdsp_onoff][preset_idx]);
				if((mixer_ctl_info->output_path == MC_ASOC_OUTPUT_PATH_HS)
				|| (mixer_ctl_info->incall_mic == MC_ASOC_INCALL_MIC_2MIC)
				|| ((mixer_ctl_info->incall_mic == MC_ASOC_INCALL_MIC_MAINMIC) && (mc_asoc_main_mic != MIC_PDM))
				|| ((mixer_ctl_info->incall_mic == MC_ASOC_INCALL_MIC_SUBMIC) && (mc_asoc_sub_mic != MIC_PDM))) {
				}
				else {
					/*	ADC->PDM	*/
					exchange_ADCtoPDM(path_info);
				}
				mask_ADC_src(path_info, mixer_ctl_info, cdsp_onoff, preset_idx);
				mask_AnaOut_src(path_info, mixer_ctl_info, preset_idx);
			}
			return;
		}
	}

	if(mixer_ctl_info->btmic_play == 1) {
		if(BtPathMapping[cdsp_onoff][preset_idx] < ARRAY_SIZE(BtInputPath)) {
			add_path_info(path_info, (MCDRV_PATH_INFO*)&BtInputPath[BtPathMapping[cdsp_onoff][preset_idx]]);
			mask_BTOut_src(path_info, mixer_ctl_info->output_path);
		}
		else {
			dbg_info("\n********\nBtPathMapping err\n********\n");
		}
	}

	if(((mixer_ctl_info->audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO)
		&& (mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_OFF))
	|| ((mixer_ctl_info->audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO)
		&& (mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_INCALL))
	|| ((mixer_ctl_info->audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO)
		&& (mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_INCOMM))
	|| ((mixer_ctl_info->audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)
		&& (mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_OFF))
	|| ((mixer_ctl_info->audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)
		&& (mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_INCOMM))) {
		if(ain_play == 1) {
			set_ain_play_path(path_info, mixer_ctl_info, cdsp_onoff, preset_idx, 1);
		}
		add_path_info(path_info, (MCDRV_PATH_INFO*)&stPresetPathInfo[cdsp_onoff][preset_idx]);
	}
	else if(((mixer_ctl_info->audio_mode_play == MC_ASOC_AUDIO_MODE_OFF)
			&& (mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO))
		 || ((mixer_ctl_info->audio_mode_play == MC_ASOC_AUDIO_MODE_OFF)
		 	&& (mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO_INCALL))
		 || ((mixer_ctl_info->audio_mode_play == MC_ASOC_AUDIO_MODE_INCALL)
		 	&& (mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO))
		 || ((mixer_ctl_info->audio_mode_play == MC_ASOC_AUDIO_MODE_INCOMM)
		 	&& (mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO))
		 || ((mixer_ctl_info->audio_mode_play == MC_ASOC_AUDIO_MODE_INCOMM)
		 	&& (mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO_INCALL))) {
		if((mixer_ctl_info->input_path != MC_ASOC_INPUT_PATH_VOICECALL)
		&& (mixer_ctl_info->input_path != MC_ASOC_INPUT_PATH_VOICEUPLINK)
		&& (mixer_ctl_info->input_path != MC_ASOC_INPUT_PATH_VOICEDOWNLINK)) {
			if(mixer_ctl_info->input_path == MC_ASOC_INPUT_PATH_BT) {
				if(ain_play == 1) {
					set_ain_play_path(path_info, mixer_ctl_info, cdsp_onoff, preset_idx, 1);
				}
			}
			else {
				if(ain_play == 1) {
					set_ain_play_path(path_info, mixer_ctl_info, cdsp_onoff, preset_idx, 0);
				}
			}
			add_path_info(path_info, (MCDRV_PATH_INFO*)&stPresetPathInfo[cdsp_onoff][preset_idx]);
			mask_ADC_src(path_info, mixer_ctl_info, cdsp_onoff, preset_idx);
		}
		else {
			add_path_info(path_info, (MCDRV_PATH_INFO*)&stPresetPathInfo[cdsp_onoff][preset_idx]);
			mute_DIT	= 1;
		}
	}
	else if(((mixer_ctl_info->audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO)
			&& (mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO))
		 || ((mixer_ctl_info->audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO)
		 	&& (mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO_INCALL))
		 || ((mixer_ctl_info->audio_mode_play == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)
		 	&& (mixer_ctl_info->audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO))) {
		if((mixer_ctl_info->input_path != MC_ASOC_INPUT_PATH_VOICECALL)
		&& (mixer_ctl_info->input_path != MC_ASOC_INPUT_PATH_VOICEUPLINK)
		&& (mixer_ctl_info->input_path != MC_ASOC_INPUT_PATH_VOICEDOWNLINK)) {
			if(ain_play == 1) {
				if(mixer_ctl_info->input_path == MC_ASOC_INPUT_PATH_BT) {
					set_ain_play_path(path_info, mixer_ctl_info, cdsp_onoff, preset_idx, 1);
				}
				else {
					set_ain_play_path(path_info, mixer_ctl_info, cdsp_onoff, preset_idx, 0);
				}
			}
			add_path_info(path_info, (MCDRV_PATH_INFO*)&stPresetPathInfo[cdsp_onoff][preset_idx]);
			if(get_ap_play_port() != -1) {
				mask_ADC_src(path_info, mixer_ctl_info, cdsp_onoff, preset_idx);
			}
		}
		else {
			add_path_info(path_info, (MCDRV_PATH_INFO*)&stPresetPathInfo[cdsp_onoff][preset_idx]);
			mute_DIT	= 1;
		}
	}
	else {
		if(ain_play == 1) {
			set_ain_play_path(path_info, mixer_ctl_info, cdsp_onoff, preset_idx, 1);
		}
	}

	mask_AnaOut_src(path_info, mixer_ctl_info, preset_idx);
	if((preset_idx < 4)
	|| (ap_cap_port != get_bt_port())) {
		mask_BTOut_src(path_info, mixer_ctl_info->output_path);
	}

	if(mc_asoc_cap_path != DIO_DEF) {
		if(preset_idx >= 4) {
			if(mc_asoc_ap_cap_port_rate == MCDRV_FS_8000) {
				if(mc_asoc_cap_path == DIO_PORT0) {
					swap_DIT_APCP(path_info, ap_cap_port, DIO_0);
					if(mute_DIT != 0) {
						set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT0].offset, 0, 1);
						set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT0].offset, 1, 1);
						mute_DIT	= 0;
					}
				}
				else if(mc_asoc_cap_path == DIO_PORT1) {
					swap_DIT_APCP(path_info, ap_cap_port, DIO_1);
					if(mute_DIT != 0) {
						set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT1].offset, 0, 1);
						set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT1].offset, 1, 1);
						mute_DIT	= 0;
					}
				}
				else if(mc_asoc_cap_path == DIO_PORT2) {
					swap_DIT_APCP(path_info, ap_cap_port, DIO_2);
					if(mute_DIT != 0) {
						set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT2].offset, 0, 1);
						set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT2].offset, 1, 1);
						mute_DIT	= 0;
					}
				}
			}
		}
	}

	if(mute_DIT != 0) {
		switch(ap_cap_port) {
		case	DIO_0:
			set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT0].offset, 0, 1);
			set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT0].offset, 1, 1);
			break;
		case	DIO_1:
			set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT1].offset, 0, 1);
			set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT1].offset, 1, 1);
			break;
		case	DIO_2:
			set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT2].offset, 0, 1);
			set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT2].offset, 1, 1);
			break;
		default:
			break;
		}
	}

	return;
}

static int is_same_cdsp_param(
	struct mc_asoc_data	*mc_asoc,
	int	base,
	int	user
)
{
	int		i;

	TRACE_FUNC();

	if(mc_asoc->cdsp_store.param_count < mc_asoc->dsp_voicecall.param_count[base]) {
		dbg_info("cdsp_store.param_count < dsp_voicecall.param_count[base]\n");
		return 0;
	}
	for(i = 0; i < mc_asoc->dsp_voicecall.param_count[base]; i++) {
		dbg_info("cdsp_store.cdspparam[%d]=%02X %02X %02X %02X ...\n",
			i,
			mc_asoc->cdsp_store.cdspparam[i].bCommand,
			mc_asoc->cdsp_store.cdspparam[i].abParam[0],
			mc_asoc->cdsp_store.cdspparam[i].abParam[1],
			mc_asoc->cdsp_store.cdspparam[i].abParam[2]);
		dbg_info("dsp_voicecall.cdspparam[base][%d]=%02X %02X %02X %02X ...\n",
			i,
			mc_asoc->dsp_voicecall.cdspparam[base][i].bCommand,
			mc_asoc->dsp_voicecall.cdspparam[base][i].abParam[0],
			mc_asoc->dsp_voicecall.cdspparam[base][i].abParam[1],
			mc_asoc->dsp_voicecall.cdspparam[base][i].abParam[2]);
		if(memcmp(&mc_asoc->cdsp_store.cdspparam[i],
					&mc_asoc->dsp_voicecall.cdspparam[base][i],
					sizeof(MCDRV_CDSPPARAM)) != 0) {
			return 0;
		}
	}

	if(user > 0) {
		if(mc_asoc->dsp_voicecall.param_count[user] == 0) {
			if(mc_asoc->cdsp_store.param_count != mc_asoc->dsp_voicecall.param_count[base]) {
				dbg_info("cdsp_store.param_count != dsp_voicecall.param_count[base]\n");
				return 0;
			}
		}
		if(mc_asoc->cdsp_store.param_count <
			(mc_asoc->dsp_voicecall.param_count[base]+mc_asoc->dsp_voicecall.param_count[user])) {
			dbg_info("cdsp_store.param_count < (dsp_voicecall.param_count[base]+dsp_voicecall.param_count[user])\n");
			return 0;
		}
		for(i = 0; i < mc_asoc->dsp_voicecall.param_count[user]; i++) {
			dbg_info("cdsp_store.cdspparam[%d]=%02X %02X %02X %02X ...\n",
				mc_asoc->cdsp_store.param_count-mc_asoc->dsp_voicecall.param_count[user]+i,
				mc_asoc->cdsp_store.cdspparam[mc_asoc->cdsp_store.param_count-mc_asoc->dsp_voicecall.param_count[user]+i].bCommand,
				mc_asoc->cdsp_store.cdspparam[mc_asoc->cdsp_store.param_count-mc_asoc->dsp_voicecall.param_count[user]+i].abParam[0],
				mc_asoc->cdsp_store.cdspparam[mc_asoc->cdsp_store.param_count-mc_asoc->dsp_voicecall.param_count[user]+i].abParam[1],
				mc_asoc->cdsp_store.cdspparam[mc_asoc->cdsp_store.param_count-mc_asoc->dsp_voicecall.param_count[user]+i].abParam[2]);
			dbg_info("dsp_voicecall.cdspparam[user][%d]=%02X %02X %02X %02X ...\n",
				i,
				mc_asoc->dsp_voicecall.cdspparam[user][i].bCommand,
				mc_asoc->dsp_voicecall.cdspparam[user][i].abParam[0],
				mc_asoc->dsp_voicecall.cdspparam[user][i].abParam[1],
				mc_asoc->dsp_voicecall.cdspparam[user][i].abParam[2]);
			if(memcmp(
				&mc_asoc->cdsp_store.cdspparam[mc_asoc->cdsp_store.param_count-mc_asoc->dsp_voicecall.param_count[user]+i],
				&mc_asoc->dsp_voicecall.cdspparam[user][i],
				sizeof(MCDRV_CDSPPARAM)) != 0) {
				return 0;
			}
		}
	}
	else {
		if(mc_asoc->cdsp_store.param_count != mc_asoc->dsp_voicecall.param_count[base]) {
			dbg_info("cdsp_store.param_count:%d != dsp_voicecall.param_count[base]:%d\n",
			mc_asoc->cdsp_store.param_count, mc_asoc->dsp_voicecall.param_count[base]);
			return 0;
		}
	}
	return 1;
}

static void skip_AE(
	MCDRV_PATH_INFO	*path_info
)
{
	MCDRV_CHANNEL	*asAe;
	UINT8	bCh;
	int		i;

	TRACE_FUNC();

	if(mc_asoc_ae_dir == MC_ASOC_AE_NONE) {
		asAe	= (MCDRV_CHANNEL*)((void*)path_info+offsetof(MCDRV_PATH_INFO, asAe));

		for(bCh = 0; bCh < PEAK_PATH_CHANNELS; bCh++) {
			if((path_info->asPeak[bCh].abSrcOnOff[MCDRV_SRC_AE_BLOCK] & MCDRV_SRC6_AE_ON) != 0) {
				for(i = 0; i < SOURCE_BLOCK_NUM; i++) {
					path_info->asPeak[bCh].abSrcOnOff[i]	|= asAe[0].abSrcOnOff[i];
				}
				path_info->asPeak[bCh].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	&= ~MCDRV_SRC6_AE_ON;
				path_info->asPeak[bCh].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_OFF;
			}
		}
		for(bCh = 0; bCh < DIT0_PATH_CHANNELS; bCh++) {
			if((path_info->asDit0[bCh].abSrcOnOff[MCDRV_SRC_AE_BLOCK] & MCDRV_SRC6_AE_ON) != 0) {
				for(i = 0; i < SOURCE_BLOCK_NUM; i++) {
					path_info->asDit0[bCh].abSrcOnOff[i]	|= asAe[0].abSrcOnOff[i];
				}
				path_info->asDit0[bCh].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	&= ~MCDRV_SRC6_AE_ON;
				path_info->asDit0[bCh].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_OFF;
			}
		}
		for(bCh = 0; bCh < DIT1_PATH_CHANNELS; bCh++) {
			if((path_info->asDit1[bCh].abSrcOnOff[MCDRV_SRC_AE_BLOCK] & MCDRV_SRC6_AE_ON) != 0) {
				for(i = 0; i < SOURCE_BLOCK_NUM; i++) {
					path_info->asDit1[bCh].abSrcOnOff[i]	|= asAe[0].abSrcOnOff[i];
				}
				path_info->asDit1[bCh].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	&= ~MCDRV_SRC6_AE_ON;
				path_info->asDit1[bCh].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_OFF;
			}
		}
		for(bCh = 0; bCh < DIT2_PATH_CHANNELS; bCh++) {
			if((path_info->asDit2[bCh].abSrcOnOff[MCDRV_SRC_AE_BLOCK] & MCDRV_SRC6_AE_ON) != 0) {
				for(i = 0; i < SOURCE_BLOCK_NUM; i++) {
					path_info->asDit2[bCh].abSrcOnOff[i]	|= asAe[0].abSrcOnOff[i];
				}
				path_info->asDit2[bCh].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	&= ~MCDRV_SRC6_AE_ON;
				path_info->asDit2[bCh].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_OFF;
			}
		}
		for(bCh = 0; bCh < DAC_PATH_CHANNELS; bCh++) {
			if((path_info->asDac[bCh].abSrcOnOff[MCDRV_SRC_AE_BLOCK] & MCDRV_SRC6_AE_ON) != 0) {
				for(i = 0; i < SOURCE_BLOCK_NUM; i++) {
					path_info->asDac[bCh].abSrcOnOff[i]	|= asAe[0].abSrcOnOff[i];
				}
				path_info->asDac[bCh].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	&= ~MCDRV_SRC6_AE_ON;
				path_info->asDac[bCh].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_OFF;
			}
		}
		for(bCh = 0; bCh < CDSP_PATH_CHANNELS; bCh++) {
			if((path_info->asCdsp[bCh].abSrcOnOff[MCDRV_SRC_AE_BLOCK] & MCDRV_SRC6_AE_ON) != 0) {
				for(i = 0; i < SOURCE_BLOCK_NUM; i++) {
					path_info->asCdsp[bCh].abSrcOnOff[i]	|= asAe[0].abSrcOnOff[i];
				}
				path_info->asCdsp[bCh].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	&= ~MCDRV_SRC6_AE_ON;
				path_info->asCdsp[bCh].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_OFF;
			}
		}
		for(bCh = 0; bCh < MIX_PATH_CHANNELS; bCh++) {
			if((path_info->asMix[bCh].abSrcOnOff[MCDRV_SRC_AE_BLOCK] & MCDRV_SRC6_AE_ON) != 0) {
				for(i = 0; i < SOURCE_BLOCK_NUM; i++) {
					path_info->asMix[bCh].abSrcOnOff[i]	|= asAe[0].abSrcOnOff[i];
				}
				path_info->asMix[bCh].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	&= ~MCDRV_SRC6_AE_ON;
				path_info->asMix[bCh].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_OFF;
			}
		}
		for(bCh = 0; bCh < MIX2_PATH_CHANNELS; bCh++) {
			if((path_info->asMix2[bCh].abSrcOnOff[MCDRV_SRC_AE_BLOCK] & MCDRV_SRC6_AE_ON) != 0) {
				for(i = 0; i < SOURCE_BLOCK_NUM; i++) {
					path_info->asMix2[bCh].abSrcOnOff[i]	|= asAe[0].abSrcOnOff[i];
				}
				path_info->asMix2[bCh].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	&= ~MCDRV_SRC6_AE_ON;
				path_info->asMix2[bCh].abSrcOnOff[MCDRV_SRC_AE_BLOCK]	|= MCDRV_SRC6_AE_OFF;
			}
		}

		for(i = 0; i < SOURCE_BLOCK_NUM; i++) {
			asAe[0].abSrcOnOff[i]	= 0xAA;
		}
	}
}

static int connect_path(
	struct snd_soc_codec *codec
)
{
	int		err;
	struct mc_asoc_mixer_ctl_info	mixer_ctl_info;
	int		base	= 0;
	int		user	= 0;
	struct mc_asoc_data	*mc_asoc	= mc_asoc_get_mc_asoc(codec);
	MCDRV_PATH_INFO	path_info;
	MCDRV_DAC_INFO	dac_info;
	UINT8	bMasterSwap, bVoiceSwap;
	int		preset_idx	= 0;
	int		cdsp_onoff	= PRESET_PATH_CDSP_OFF;
	UINT32	dDACUpdateFlg	= 0;
	int		i, j;
	UINT8		bAeUsed	= 0;

	TRACE_FUNC();

	if(mc_asoc == NULL) {
        printk("[%s][%d]return -EINVAL\n",__func__, __LINE__);
		return -EINVAL;
	}

	if(get_mixer_ctl_info(codec, &mixer_ctl_info) < 0) {
        printk("[%s][%d]return -EIO\n",__func__, __LINE__);
		return -EIO;
	}

	preset_idx	= get_path_preset_idx(&mixer_ctl_info);
	if((preset_idx < 0) || (preset_idx > PRESET_PATH_N)) {
        printk("[%s][%d]return -EIO\n",__func__, __LINE__);
		return -EIO;
	}

	if((mc_asoc_hwid == MC_ASOC_MC3_HW_ID) || (mc_asoc_hwid == MC_ASOC_MA2_HW_ID)) {
		if(PRESET_PATH_CDSP_AVAILABLE != 0) {
			get_voicecall_index(mixer_ctl_info.audio_mode_play,
								mixer_ctl_info.audio_mode_cap,
								mixer_ctl_info.output_path,
								mixer_ctl_info.input_path,
								mixer_ctl_info.incall_mic,
								mixer_ctl_info.msmic_play,
								&base, &user);
			dbg_info("base=%d, user=%d\n", base, user);
			if(mc_asoc->dsp_voicecall.onoff[base] == 1) {
				if((1 < ARRAY_SIZE(stPresetPathInfo))
				&& (1 < ARRAY_SIZE(AnalogPathMapping))
				&& (1 < ARRAY_SIZE(BtPathMapping))
				&& (1 < ARRAY_SIZE(DacSwapParamMapping))) {
					cdsp_onoff	= PRESET_PATH_CDSP_ON;
				}
			}
		}
	}
	printk("cdsp_onoff=%d, preset_idx=%d\n", cdsp_onoff, preset_idx);

	set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT0].offset, 0, 0);
	set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT0].offset, 1, 0);
	set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT1].offset, 0, 0);
	set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT1].offset, 1, 0);
	set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT2].offset, 0, 0);
	set_vol_mute_flg(mc_asoc_vreg_map[MC_ASOC_DVOL_DIT2].offset, 1, 0);

	memcpy(&path_info, &stPresetPathInfo[0][0], sizeof(MCDRV_PATH_INFO));
	get_path_info(mc_asoc, &path_info, &mixer_ctl_info, cdsp_onoff, preset_idx);
	set_BIAS(&path_info);

	switch(AeParamMapping[preset_idx]) {
	case	AE_PARAM_OFF:
		mc_asoc_ae_dir	= MC_ASOC_AE_NONE;
		break;
	case	PLAYBACK_AE_PARAM:
		mc_asoc_ae_dir	= MC_ASOC_AE_PLAY;
		break;
	case	CAPTURE_AE_PARAM:
		mc_asoc_ae_dir	= MC_ASOC_AE_CAP;
		break;
	default:
        printk("[%s][%d]return -EIO\n",__func__, __LINE__);
		return -EIO;
	}
	if(ae_onoff(mc_asoc->ae_onoff, mixer_ctl_info.output_path) == MC_ASOC_AENG_OFF) {
		mc_asoc_ae_dir	= MC_ASOC_AE_NONE;
	}
	if(mc_asoc_ae_dir != MC_ASOC_AE_NONE) {
		for(i = 0; i < AE_PATH_CHANNELS && bAeUsed == 0; i++) {
			for(j = 0; j < SOURCE_BLOCK_NUM; j++) {
				if((path_info.asAe[i].abSrcOnOff[j] & 0x55) != 0) {
					bAeUsed	= 1;
					break;
				}
			}
		}
		if(bAeUsed == 0)
			mc_asoc_ae_dir	= MC_ASOC_AE_NONE;
	}
	else {
		skip_AE(&path_info);
	}

	bMasterSwap	= DacSwapParamMapping[cdsp_onoff][preset_idx][0];
	bVoiceSwap	= DacSwapParamMapping[cdsp_onoff][preset_idx][1];

	if((err = set_vreg_map(codec, &path_info, cdsp_onoff, preset_idx)) < 0) {
        printk("[%s][%d]err = %#x\n",__func__, __LINE__,err);
		return err;
	}

	if((mc_asoc_hwid == MC_ASOC_MC3_HW_ID) || (mc_asoc_hwid == MC_ASOC_MA2_HW_ID)) {
		if(is_cdsp_used(&path_info) == 1) {
			if(cdsp_onoff == PRESET_PATH_CDSP_ON) {
				if(mc_asoc_cdsp_ae_ex_flag == MC_ASOC_AE_EX_ON) {
					if((err = _McDrv_Ctrl(MCDRV_SET_AUDIOENGINE_EX, NULL, 0)) != MCDRV_SUCCESS) {
						return mc_asoc_map_drv_error(err);
					}
					mc_asoc->ae_ex_store.size	= 0;
				}
				if(mc_asoc_cdsp_ae_ex_flag == MC_ASOC_CDSP_ON) {
					if(is_same_cdsp_param(mc_asoc, base, user) == 0) {
						if((err = set_cdsp_off(mc_asoc)) < 0) {
                            printk("[%s][%d]err = %#x\n",__func__, __LINE__,err);
							return err;
						}
						if((err = set_cdsp_param(codec)) < 0) {
                            printk("[%s][%d]err = %#x\n",__func__, __LINE__,err);
							return err;
						}
					}
				}
				else {
					if((err = set_cdsp_param(codec)) < 0) {
                            printk("[%s][%d]err = %#x\n",__func__, __LINE__,err);
						return err;
					}
				}
			}
			else {
				if(mc_asoc_cdsp_ae_ex_flag == MC_ASOC_CDSP_ON) {
					if((err = set_cdsp_off(mc_asoc)) < 0) {
                            printk("[%s][%d]err = %#x\n",__func__, __LINE__,err);
						return err;
					}
				}
                printk("[%s][%d]return -EIO\n",__func__, __LINE__);
				return -EIO;
			}
		}
		else {
			if((err = set_cdsp_off(mc_asoc)) < 0) {
                            printk("[%s][%d]err = %#x\n",__func__, __LINE__,err);
				return err;
			}
		}
	}

	/*	AE_EX	*/
	if((mc_asoc_hwid == MC_ASOC_MC3_HW_ID) || (mc_asoc_hwid == MC_ASOC_MA2_HW_ID)) {
		if(mc_asoc_cdsp_ae_ex_flag != MC_ASOC_CDSP_ON) {
			if((err = set_audioengine_ex(codec)) != 0) {
                            printk("[%s][%d]err = %#x\n",__func__, __LINE__,err);
				return err;
			}
		}
	}
	if((err = set_audioengine(codec)) < 0) {
                            printk("[%s][%d]err = %#x\n",__func__, __LINE__,err);
		return err;
	}

	if((err = _McDrv_Ctrl(MCDRV_GET_DAC, (void *)&dac_info, 0)) < MCDRV_SUCCESS) {
		return mc_asoc_map_drv_error(err);
	}
	if((dac_info.bMasterSwap == MCDRV_DSWAP_OFF) && (bMasterSwap != dac_info.bMasterSwap)) {
		dac_info.bMasterSwap	= bMasterSwap;
		dDACUpdateFlg			|= MCDRV_DAC_MSWP_UPDATE_FLAG;
	}
	if((dac_info.bVoiceSwap == MCDRV_DSWAP_OFF) && (bVoiceSwap != dac_info.bVoiceSwap)) {
		dac_info.bVoiceSwap	= bVoiceSwap;
		dDACUpdateFlg			|= MCDRV_DAC_VSWP_UPDATE_FLAG;
	}
	if(dDACUpdateFlg != 0) {
		if((err = _McDrv_Ctrl(MCDRV_SET_DAC, (void *)&dac_info, dDACUpdateFlg)) < MCDRV_SUCCESS) {
                            printk("[%s][%d]err = %#x\n",__func__, __LINE__,err);
			return mc_asoc_map_drv_error(err);
		}
	}

	set_volume(codec);
	if((err = _McDrv_Ctrl(MCDRV_SET_PATH, &path_info, 0)) != MCDRV_SUCCESS) {
                            printk("[%s][%d]err = %#x\n",__func__, __LINE__,err);
		return mc_asoc_map_drv_error(err);
	}
#if 0
#ifdef MC_ASOC_TEST
	if((err = _McDrv_Ctrl(MCDRV_SET_PATH, &path_info, 0)) != MCDRV_SUCCESS) {
		return mc_asoc_map_drv_error(err);
	}
#endif
#endif

	dDACUpdateFlg	= 0;
	if(bMasterSwap != dac_info.bMasterSwap) {
		dac_info.bMasterSwap	= bMasterSwap;
		dDACUpdateFlg			|= MCDRV_DAC_MSWP_UPDATE_FLAG;
	}
	if(bVoiceSwap != dac_info.bVoiceSwap) {
		dac_info.bVoiceSwap	= bVoiceSwap;
		dDACUpdateFlg			|= MCDRV_DAC_VSWP_UPDATE_FLAG;
	}
	if(dDACUpdateFlg != 0) {
		if((err = _McDrv_Ctrl(MCDRV_SET_DAC, (void *)&dac_info, dDACUpdateFlg)) < MCDRV_SUCCESS) {
                            printk("[%s][%d]err = %#x\n",__func__, __LINE__,err);
			return mc_asoc_map_drv_error(err);
		}
	}

	return err;
}


/*
 * DAI (PCM interface)
 */
/* SRC_RATE settings @ 73728000Hz (ideal PLL output) */
static int	mc_asoc_src_rate[][SNDRV_PCM_STREAM_LAST+1]	= {
	/* DIR, DIT */
	{32768, 4096},					/* MCDRV_FS_48000 */
	{30106, 4458},					/* MCDRV_FS_44100 */
	{21845, 6144},					/* MCDRV_FS_32000 */
	{0, 0},							/* N/A */
	{0, 0},							/* N/A */
	{15053, 8916},					/* MCDRV_FS_22050 */
	{10923, 12288},					/* MCDRV_FS_16000 */
	{0, 0},							/* N/A */
	{0, 0},							/* N/A */
	{7526, 17833},					/* MCDRV_FS_11025 */
	{5461, 24576},					/* MCDRV_FS_8000 */
};
#define mc_asoc_fs_to_srcrate(rate,dir)	mc_asoc_src_rate[(rate)][(dir)];

static int is_dio_modified(
	const MCDRV_DIO_PORT	*port,
	const struct mc_asoc_setup	*setup,
	int	id,
	int	mode,
	UINT32	update
)
{
	int		i, err;
	MCDRV_DIO_INFO	cur_dio;

	if((err = _McDrv_Ctrl(MCDRV_GET_DIGITALIO, &cur_dio, 0)) != MCDRV_SUCCESS) {
		return mc_asoc_map_drv_error(err);
	}

	if((update & (MCDRV_DIO0_COM_UPDATE_FLAG|MCDRV_DIO1_COM_UPDATE_FLAG|MCDRV_DIO2_COM_UPDATE_FLAG)) != 0) {
		if((cur_dio.asPortInfo[id].sDioCommon.bMasterSlave	!= port->sDioCommon.bMasterSlave)
		|| (cur_dio.asPortInfo[id].sDioCommon.bAutoFs		!= port->sDioCommon.bAutoFs)
		|| (cur_dio.asPortInfo[id].sDioCommon.bFs			!= port->sDioCommon.bFs)
		|| (cur_dio.asPortInfo[id].sDioCommon.bBckFs		!= port->sDioCommon.bBckFs)
		|| (cur_dio.asPortInfo[id].sDioCommon.bInterface	!= port->sDioCommon.bInterface)
		|| (cur_dio.asPortInfo[id].sDioCommon.bBckInvert	!= port->sDioCommon.bBckInvert)) {
			return 1;
		}
		if(mode == MCDRV_DIO_PCM) {
			if((cur_dio.asPortInfo[id].sDioCommon.bPcmHizTim	!= port->sDioCommon.bPcmHizTim)
			|| (cur_dio.asPortInfo[id].sDioCommon.bPcmClkDown	!= port->sDioCommon.bPcmClkDown)
			|| (cur_dio.asPortInfo[id].sDioCommon.bPcmFrame		!= port->sDioCommon.bPcmFrame)
			|| (cur_dio.asPortInfo[id].sDioCommon.bPcmHighPeriod!= port->sDioCommon.bPcmHighPeriod)) {
				return 1;
			}
		}
	}

	if((update & (MCDRV_DIO0_DIR_UPDATE_FLAG|MCDRV_DIO1_DIR_UPDATE_FLAG|MCDRV_DIO2_DIR_UPDATE_FLAG)) != 0) {
		if(cur_dio.asPortInfo[id].sDir.wSrcRate	!= port->sDir.wSrcRate) {
			return 1;
		}
		if (mode == MCDRV_DIO_DA) {
			if((cur_dio.asPortInfo[id].sDir.sDaFormat.bBitSel	!= port->sDir.sDaFormat.bBitSel)
			|| (cur_dio.asPortInfo[id].sDir.sDaFormat.bMode		!= port->sDir.sDaFormat.bMode)) {
				return 1;
			}
		}
		else {
			if((cur_dio.asPortInfo[id].sDir.sPcmFormat.bMono	!= port->sDir.sPcmFormat.bMono)
			|| (cur_dio.asPortInfo[id].sDir.sPcmFormat.bOrder	!= port->sDir.sPcmFormat.bOrder)
			|| (cur_dio.asPortInfo[id].sDir.sPcmFormat.bLaw		!= port->sDir.sPcmFormat.bLaw)
			|| (cur_dio.asPortInfo[id].sDir.sPcmFormat.bBitSel	!= port->sDir.sPcmFormat.bBitSel)) {
				return 1;
			}
			if (setup->pcm_extend[id]) {
				if(cur_dio.asPortInfo[id].sDir.sPcmFormat.bOrder	!= port->sDir.sPcmFormat.bOrder) {
					return 1;
				}
			}
		}
		for (i = 0; i < DIO_CHANNELS; i++) {
			if(cur_dio.asPortInfo[id].sDir.abSlot[i]	!= port->sDir.abSlot[i]) {
				return 1;
			}
		}
	}

	if((update & (MCDRV_DIO0_DIT_UPDATE_FLAG|MCDRV_DIO1_DIT_UPDATE_FLAG|MCDRV_DIO2_DIT_UPDATE_FLAG)) != 0) {
		if(cur_dio.asPortInfo[id].sDit.wSrcRate	!= port->sDit.wSrcRate) {
			return 1;
		}
		if (mode == MCDRV_DIO_DA) {
			if((cur_dio.asPortInfo[id].sDit.sDaFormat.bBitSel	!= port->sDit.sDaFormat.bBitSel)
			|| (cur_dio.asPortInfo[id].sDit.sDaFormat.bMode		!= port->sDit.sDaFormat.bMode)) {
				return 1;
			}
		}
		else {
			if((cur_dio.asPortInfo[id].sDit.sPcmFormat.bMono	!= port->sDit.sPcmFormat.bMono)
			|| (cur_dio.asPortInfo[id].sDit.sPcmFormat.bOrder	!= port->sDit.sPcmFormat.bOrder)
			|| (cur_dio.asPortInfo[id].sDit.sPcmFormat.bLaw		!= port->sDit.sPcmFormat.bLaw)
			|| (cur_dio.asPortInfo[id].sDit.sPcmFormat.bBitSel	!= port->sDit.sPcmFormat.bBitSel)) {
				return 1;
			}
			if (setup->pcm_extend[id]) {
				if(cur_dio.asPortInfo[id].sDir.sPcmFormat.bOrder	!= port->sDir.sPcmFormat.bOrder) {
					return 1;
				}
			}
		}
		for (i = 0; i < DIO_CHANNELS; i++) {
			if(cur_dio.asPortInfo[id].sDit.abSlot[i]	!= port->sDit.abSlot[i]) {
				return 1;
			}
		}
	}
	return 0;
}

static int setup_dai(
	struct snd_soc_codec	*codec,
	struct mc_asoc_data	*mc_asoc,
	int	id,
	int	mode,
	int	dir
)
{
	MCDRV_DIO_INFO	dio;
	MCDRV_DIO_PORT	*port	= &dio.asPortInfo[id];
	struct mc_asoc_setup		*setup		= &mc_asoc->setup;
	struct mc_asoc_port_params	*port_prm	= &mc_asoc->port[id];
	UINT32	update	= 0;
	int		i, err, modify;
	MCDRV_PATH_INFO	path_info, tmp_path_info;
	MCDRV_CHANNEL	*asDit;
	int		port_block, port_block_on;
	int		channels	= 0;

	if((err = _McDrv_Ctrl(MCDRV_GET_PATH, &path_info, 0)) != MCDRV_SUCCESS) {
		return mc_asoc_map_drv_error(err);
	}

	asDit	= (MCDRV_CHANNEL*)((void*)&path_info+get_port_srconoff_offset(id));

	if((get_cp_port() == id) || (get_bt_port() == id)) {
		port_block	= get_port_block(id);
		port_block_on	= get_port_block_on(id);
		for(i = 0; i < DIT0_PATH_CHANNELS; i++) {
			if((path_info.asDit0[i].abSrcOnOff[port_block] & port_block_on) != 0) {
				return 0;
			}
		}
		for(i = 0; i < DIT1_PATH_CHANNELS; i++) {
			if((path_info.asDit1[i].abSrcOnOff[port_block] & port_block_on) != 0) {
				return 0;
			}
		}
		for(i = 0; i < DIT2_PATH_CHANNELS; i++) {
			if((path_info.asDit2[i].abSrcOnOff[port_block] & port_block_on) != 0) {
				return 0;
			}
		}
		for(i = 0; i < DAC_PATH_CHANNELS; i++) {
			if((path_info.asDac[i].abSrcOnOff[port_block] & port_block_on) != 0) {
				return 0;
			}
		}
		for(i = 0; i < AE_PATH_CHANNELS; i++) {
			if((path_info.asAe[i].abSrcOnOff[port_block] & port_block_on) != 0) {
				return 0;
			}
		}
		for(i = 0; i < MIX_PATH_CHANNELS; i++) {
			if((path_info.asMix[i].abSrcOnOff[port_block] & port_block_on) != 0) {
				return 0;
			}
		}
		for(i = 0; i < PEAK_PATH_CHANNELS; i++) {
			if((path_info.asPeak[i].abSrcOnOff[port_block] & port_block_on) != 0) {
				return 0;
			}
		}
		for(i = 0; i < CDSP_PATH_CHANNELS; i++) {
			if((path_info.asCdsp[i].abSrcOnOff[port_block] & port_block_on) != 0) {
				return 0;
			}
		}
		for(i = 0; i < MIX2_PATH_CHANNELS; i++) {
			if((path_info.asMix2[i].abSrcOnOff[port_block] & port_block_on) != 0) {
				return 0;
			}
		}
		for(i = 0; i < SOURCE_BLOCK_NUM; i++) {
			if((asDit[0].abSrcOnOff[i] & 0x55) != 0) {
				return 0;
			}
		}
	}

	memset(&dio, 0, sizeof(MCDRV_DIO_INFO));

	if (port_prm->stream == 0) {
		port->sDioCommon.bMasterSlave	= port_prm->master;
		port->sDioCommon.bAutoFs		= MCDRV_AUTOFS_OFF;
		port->sDioCommon.bFs			= port_prm->rate;
		port->sDioCommon.bBckFs			= port_prm->bckfs;
		port->sDioCommon.bInterface		= mode;
		port->sDioCommon.bBckInvert		= port_prm->inv;
		if (mode == MCDRV_DIO_PCM) {
			port->sDioCommon.bPcmHizTim		= setup->pcm_hiz_redge[id];
			port->sDioCommon.bPcmClkDown	= port_prm->pcm_clkdown;
			port->sDioCommon.bPcmFrame		= port_prm->format;
			port->sDioCommon.bPcmHighPeriod	= setup->pcm_hperiod[id];
		}
		if (dir == SNDRV_PCM_STREAM_PLAYBACK) {
			switch(id) {
			case	DIO_0:
				update |= MCDRV_DIO0_COM_UPDATE_FLAG;
				break;
			case	DIO_1:
				update |= MCDRV_DIO1_COM_UPDATE_FLAG;
				break;
			case	DIO_2:
				update |= MCDRV_DIO2_COM_UPDATE_FLAG;
				break;
			default:
				break;
			}
		}
		else {
			switch(id) {
			case	DIO_0:
				update |= MCDRV_DIO0_COM_UPDATE_FLAG;
				break;
			case	DIO_1:
				update |= MCDRV_DIO1_COM_UPDATE_FLAG;
				break;
			case	DIO_2:
				update |= MCDRV_DIO2_COM_UPDATE_FLAG;
				break;
			default:
				break;
			}
		}
	}

	if (dir == SNDRV_PCM_STREAM_PLAYBACK) {
		port->sDir.wSrcRate	= mc_asoc_fs_to_srcrate(port_prm->rate, dir);
		if (mode == MCDRV_DIO_DA) {
			port->sDir.sDaFormat.bBitSel	= port_prm->bits[dir];
			port->sDir.sDaFormat.bMode		= port_prm->format;
		} 
		else {
			port->sDir.sPcmFormat.bMono		= port_prm->pcm_mono[dir];
			port->sDir.sPcmFormat.bOrder	= port_prm->pcm_order[dir];
			if (setup->pcm_extend[id]) {
				port->sDir.sPcmFormat.bOrder |=
					(1 << setup->pcm_extend[id]);
			}
			port->sDir.sPcmFormat.bLaw		= port_prm->pcm_law[dir];
			port->sDir.sPcmFormat.bBitSel	= port_prm->bits[dir];
		}
		for (i = 0; i < DIO_CHANNELS; i++) {
			if ((i > 0) && port_prm->channels == 1) {
				port->sDir.abSlot[i]	= port->sDir.abSlot[i-1];
			} 
			else {
				port->sDir.abSlot[i]	= setup->slot[id][dir][i];
			}
		}
		switch(id) {
		case	DIO_0:
			update |= MCDRV_DIO0_DIR_UPDATE_FLAG;
			break;
		case	DIO_1:
			update |= MCDRV_DIO1_DIR_UPDATE_FLAG;
			break;
		case	DIO_2:
			update |= MCDRV_DIO2_DIR_UPDATE_FLAG;
			break;
		default:
			break;
		}
	}

	if (dir == SNDRV_PCM_STREAM_CAPTURE) {
		port->sDit.wSrcRate	= mc_asoc_fs_to_srcrate(port_prm->rate, dir);
		if (mode == MCDRV_DIO_DA) {
			port->sDit.sDaFormat.bBitSel	= port_prm->bits[dir];
			port->sDit.sDaFormat.bMode		= port_prm->format;
		} 
		else {
			port->sDit.sPcmFormat.bMono		= port_prm->pcm_mono[dir];
			port->sDit.sPcmFormat.bOrder	= port_prm->pcm_order[dir];
			if (setup->pcm_extend[id]) {
				port->sDit.sPcmFormat.bOrder |= (1 << setup->pcm_extend[id]);
			}
			port->sDit.sPcmFormat.bLaw		= port_prm->pcm_law[dir];
			port->sDit.sPcmFormat.bBitSel	= port_prm->bits[dir];
		}
		for (i = 0; i < DIO_CHANNELS; i++) {
			port->sDit.abSlot[i]	= setup->slot[id][dir][i];
		}
		switch(id) {
		case	DIO_0:
			update |= MCDRV_DIO0_DIT_UPDATE_FLAG;
			break;
		case	DIO_1:
			update |= MCDRV_DIO1_DIT_UPDATE_FLAG;
			break;
		case	DIO_2:
			update |= MCDRV_DIO2_DIT_UPDATE_FLAG;
			break;
		default:
			break;
		}
	}

	if((modify = is_dio_modified(port, setup, id, mode, update)) < 0) {
		return -EIO;
	}
	if(modify == 0) {
		dbg_info("modify == 0\n");
		return 0;
	}

	memcpy(&tmp_path_info, &path_info, sizeof(MCDRV_PATH_INFO));
	if((dir == SNDRV_PCM_STREAM_PLAYBACK)
	|| (port_prm->stream == 0)) {
		port_block	= get_port_block(id);
		port_block_on	= get_port_block_on(id);
		for(i = 0; i < DIT0_PATH_CHANNELS; i++) {
			tmp_path_info.asDit0[i].abSrcOnOff[port_block]	&= ~(port_block_on);
			tmp_path_info.asDit0[i].abSrcOnOff[port_block]	|= (port_block_on<<1);
		}
		for(i = 0; i < DIT1_PATH_CHANNELS; i++) {
			tmp_path_info.asDit1[i].abSrcOnOff[port_block]	&= ~(port_block_on);
			tmp_path_info.asDit1[i].abSrcOnOff[port_block]	|= (port_block_on<<1);
		}
		for(i = 0; i < DIT2_PATH_CHANNELS; i++) {
			tmp_path_info.asDit2[i].abSrcOnOff[port_block]	&= ~(port_block_on);
			tmp_path_info.asDit2[i].abSrcOnOff[port_block]	|= (port_block_on<<1);
		}
		for(i = 0; i < DAC_PATH_CHANNELS; i++) {
			tmp_path_info.asDac[i].abSrcOnOff[port_block]	&= ~(port_block_on);
			tmp_path_info.asDac[i].abSrcOnOff[port_block]	|= (port_block_on<<1);
		}
		for(i = 0; i < AE_PATH_CHANNELS; i++) {
			tmp_path_info.asAe[i].abSrcOnOff[port_block]	&= ~(port_block_on);
			tmp_path_info.asAe[i].abSrcOnOff[port_block]	|= (port_block_on<<1);
		}
		for(i = 0; i < MIX_PATH_CHANNELS; i++) {
			tmp_path_info.asMix[i].abSrcOnOff[port_block]	&= ~(port_block_on);
			tmp_path_info.asMix[i].abSrcOnOff[port_block]	|= (port_block_on<<1);
		}
		for(i = 0; i < PEAK_PATH_CHANNELS; i++) {
			tmp_path_info.asPeak[i].abSrcOnOff[port_block]	&= ~(port_block_on);
			tmp_path_info.asPeak[i].abSrcOnOff[port_block]	|= (port_block_on<<1);
		}
		for(i = 0; i < CDSP_PATH_CHANNELS; i++) {
			tmp_path_info.asCdsp[i].abSrcOnOff[port_block]	&= ~(port_block_on);
			tmp_path_info.asCdsp[i].abSrcOnOff[port_block]	|= (port_block_on<<1);
		}
		for(i = 0; i < MIX2_PATH_CHANNELS; i++) {
			tmp_path_info.asMix2[i].abSrcOnOff[port_block]	&= ~(port_block_on);
			tmp_path_info.asMix2[i].abSrcOnOff[port_block]	|= (port_block_on<<1);
		}
	}
	if((dir == SNDRV_PCM_STREAM_CAPTURE)
	|| (port_prm->stream == 0)) {
		asDit	= (MCDRV_CHANNEL*)((void*)&tmp_path_info+get_port_srconoff_offset(id));
		if(id == 0) {
			channels	= DIT0_PATH_CHANNELS;
		}
		else if(id == 1) {
			channels	= DIT1_PATH_CHANNELS;
		}
		else if(id == 2) {
			channels	= DIT2_PATH_CHANNELS;
		}
		for(i = 0; i < channels; i++) {
			asDit[i].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]	&= ~MCDRV_SRC4_PDM_ON;
			asDit[i].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]	|= MCDRV_SRC4_PDM_OFF;
			asDit[i].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	&= ~MCDRV_SRC4_ADC0_ON;
			asDit[i].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_OFF;
			asDit[i].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK]	&= ~MCDRV_SRC4_ADC1_ON;
			asDit[i].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK]	|= MCDRV_SRC4_ADC1_OFF;
			asDit[i].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK]	&= ~MCDRV_SRC3_DIR0_ON;
			asDit[i].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK]	|= MCDRV_SRC3_DIR0_OFF;
			asDit[i].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK]	&= ~MCDRV_SRC3_DIR1_ON;
			asDit[i].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK]	|= MCDRV_SRC3_DIR1_OFF;
			asDit[i].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK]	&= ~MCDRV_SRC3_DIR2_ON;
			asDit[i].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK]	|= MCDRV_SRC3_DIR2_OFF;
			asDit[i].abSrcOnOff[MCDRV_SRC_AE_BLOCK]		&= ~MCDRV_SRC6_AE_ON;
			asDit[i].abSrcOnOff[MCDRV_SRC_AE_BLOCK]		|= MCDRV_SRC6_AE_OFF;
			asDit[i].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]	&= ~MCDRV_SRC6_MIX_ON;
			asDit[i].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]	|= MCDRV_SRC6_MIX_OFF;
			asDit[i].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK]	&= ~MCDRV_SRC4_DTMF_ON;
			asDit[i].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK]	|= MCDRV_SRC4_DTMF_OFF;
			asDit[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	&= ~MCDRV_SRC6_CDSP_ON;
			asDit[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_OFF;
		}
	}

	if(memcmp(&tmp_path_info, &path_info, sizeof(MCDRV_PATH_INFO)) == 0) {
		modify	= 0;
	}
	else {
		if((err = _McDrv_Ctrl(MCDRV_SET_PATH, &tmp_path_info, 0)) != MCDRV_SUCCESS) {
			return mc_asoc_map_drv_error(err);
		}
		if(mc_asoc_cdsp_ae_ex_flag == MC_ASOC_AE_EX_ON) {
			if((err = _McDrv_Ctrl(MCDRV_SET_AUDIOENGINE_EX, NULL, 0)) != MCDRV_SUCCESS) {
				return mc_asoc_map_drv_error(err);
			}
			mc_asoc->ae_ex_store.size	= 0;
			mc_asoc_cdsp_ae_ex_flag		= MC_ASOC_CDSP_AE_EX_OFF;
		}
	}

	if((err = _McDrv_Ctrl(MCDRV_SET_DIGITALIO, &dio, update)) != MCDRV_SUCCESS) {
		return mc_asoc_map_drv_error(err);
	}
	if(modify != 0) {
		return connect_path(codec);
	}
	return 0;
}

static int update_clock(struct mc_asoc_data *mc_asoc)
{
	MCDRV_CLOCK_INFO info;

	memset(&info, 0, sizeof(MCDRV_CLOCK_INFO));
	info.bCkSel	= mc_asoc->setup.init.bCkSel;
	info.bDivR0	= mc_asoc->setup.init.bDivR0;
	info.bDivF0	= mc_asoc->setup.init.bDivF0;
	info.bDivR1	= mc_asoc->setup.init.bDivR1;
	info.bDivF1	= mc_asoc->setup.init.bDivF1;

	return _McDrv_Ctrl(MCDRV_UPDATE_CLOCK, &info, 0);
}

static int set_clkdiv_common(
	struct mc_asoc_data *mc_asoc,
	int div_id,
	int div)
{
	struct mc_asoc_setup *setup	= &mc_asoc->setup;

	switch (div_id) {
	case MC_ASOC_CKSEL:
		switch (div) {
		case 0:
			setup->init.bCkSel	= MCDRV_CKSEL_CMOS;
			break;
		case 1:
			setup->init.bCkSel	= MCDRV_CKSEL_TCXO;
			break;
		case 2:
			setup->init.bCkSel	= MCDRV_CKSEL_CMOS_TCXO;
			break;
		case 3:
			setup->init.bCkSel	= MCDRV_CKSEL_TCXO_CMOS;
			break;
		default:
			return -EINVAL;
		}
		break;
	case MC_ASOC_DIVR0:
		if ((div < 1) || (div > 127)) {
			return -EINVAL;
		}
		setup->init.bDivR0	= div;
		break;
	case MC_ASOC_DIVF0:
		if ((div < 1) || (div > 255)) {
			return -EINVAL;
		}
		setup->init.bDivF0	= div;
		break;
	case MC_ASOC_DIVR1:
		if ((div < 1) || (div > 127)) {
			return -EINVAL;
		}
		setup->init.bDivR1	= div;
		break;
	case MC_ASOC_DIVF1:
		if ((div < 1) || (div > 255)) {
			return -EINVAL;
		}
		setup->init.bDivF1	= div;
		break;
	default:
		return -EINVAL;
	}

/*	mc_asoc->clk_update	= 1;*/

	return 0;
}

static int set_fmt_common(
	struct mc_asoc_port_params *port,
	unsigned int fmt)
{
	/* master */
	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBM_CFM:
		port->master	= MCDRV_DIO_MASTER;
		break;
	case SND_SOC_DAIFMT_CBS_CFS:
		port->master	= MCDRV_DIO_SLAVE;
		break;
	default:
		return -EINVAL;
	}

	/* inv */
	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		port->inv	= MCDRV_BCLK_NORMAL;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		port->inv	= MCDRV_BCLK_INVERT;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int mc_asoc_i2s_set_clkdiv(
	struct snd_soc_dai *dai,
	int div_id,
	int div)
{
	struct snd_soc_codec		*codec		= NULL;
	struct mc_asoc_data			*mc_asoc	= NULL;
	struct mc_asoc_port_params	*port		= NULL;

	if((dai == NULL)
	|| (dai->codec == NULL)
	|| (get_port_id(dai->id) < 0 || get_port_id(dai->id) >= IOPORT_NUM)) {
		return -EINVAL;
	}
	codec	= dai->codec;
	mc_asoc	= mc_asoc_get_mc_asoc(codec);
	if(mc_asoc == NULL) {
		return -EINVAL;
	}
	port		= &mc_asoc->port[get_port_id(dai->id)];

	switch (div_id) {
	case MC_ASOC_BCLK_MULT:
		switch (div) {
		case MC_ASOC_LRCK_X32:
			port->bckfs	= MCDRV_BCKFS_32;
			break;
		case MC_ASOC_LRCK_X48:
			port->bckfs	= MCDRV_BCKFS_48;
			break;
		case MC_ASOC_LRCK_X64:
			port->bckfs	= MCDRV_BCKFS_64;
			break;
		default:
			return -EINVAL;
		}
		break;

	default:
		return set_clkdiv_common(mc_asoc, div_id, div);
	}

	return 0;
}

static int mc_asoc_i2s_set_fmt(
	struct snd_soc_dai *dai,
	unsigned int fmt)
{
	struct snd_soc_codec		*codec		= NULL;
	struct mc_asoc_data			*mc_asoc	= NULL;
	struct mc_asoc_port_params	*port		= NULL;

	if((dai == NULL)
	|| (dai->codec == NULL)
	|| (get_port_id(dai->id) < 0 || get_port_id(dai->id) >= IOPORT_NUM)) {
		return -EINVAL;
	}
	codec	= dai->codec;
	mc_asoc	= mc_asoc_get_mc_asoc(codec);
	if(mc_asoc == NULL) {
		return -EINVAL;
	}
	port	= &mc_asoc->port[get_port_id(dai->id)];

	/* format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		port->format	= MCDRV_DAMODE_I2S;
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		port->format	= MCDRV_DAMODE_TAILALIGN;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		port->format	= MCDRV_DAMODE_HEADALIGN;
		break;
	default:
		return -EINVAL;
	}

	return set_fmt_common(port, fmt);
}

static int mc_asoc_i2s_hw_params(
	struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	struct snd_soc_pcm_runtime	*runtime	= snd_pcm_substream_chip(substream);
#endif
	struct snd_soc_codec		*codec		= NULL;
	struct mc_asoc_data			*mc_asoc	= NULL;
	struct mc_asoc_port_params	*port		= NULL;
	int		dir	= substream->stream;
	int		rate;
	int		err	= 0;
	int		id;

	TRACE_FUNC();

	if((substream == NULL)
	|| (dai == NULL)
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	|| (runtime == NULL)
#endif
	) {
		return -EINVAL;
	}

	id	= get_port_id(dai->id);
	if((id < 0) || (id >= IOPORT_NUM)) {
		dbg_info("dai->id=%d\n", get_port_id(dai->id));
		return -EINVAL;
	}

	dbg_info("hw_params: [%d] name=%s, dir=%d, rate=%d, bits=%d, ch=%d\n",
		 get_port_id(dai->id),
		 substream->name,
		 dir,
		 params_rate(params),
		 params_format(params),
		 params_channels(params));

#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	codec	= runtime->socdev->card->codec;
#else
	codec	= dai->codec;
#endif
	mc_asoc	= mc_asoc_get_mc_asoc(codec);
	if((codec == NULL)
	|| (mc_asoc == NULL)) {
		dbg_info("mc_asoc=NULL\n");
		return -EINVAL;
	}
	port	= &mc_asoc->port[id];

	/* format (bits) */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		port->bits[dir]	= MCDRV_BITSEL_16;
		break;
	case SNDRV_PCM_FORMAT_S20_3LE:
		port->bits[dir]	= MCDRV_BITSEL_20;
		break;
	case SNDRV_PCM_FORMAT_S24_3LE:
		port->bits[dir]	= MCDRV_BITSEL_24;
		break;
	default:
		return -EINVAL;
	}

	/* rate */
	switch (params_rate(params)) {
	case 8000:
		rate	= MCDRV_FS_8000;
		break;
	case 11025:
		rate	= MCDRV_FS_11025;
		break;
	case 16000:
		rate	= MCDRV_FS_16000;
		break;
	case 22050:
		rate	= MCDRV_FS_22050;
		break;
	case 32000:
		rate	= MCDRV_FS_32000;
		break;
	case 44100:
		rate	= MCDRV_FS_44100;
		break;
	case 48000:
		rate	= MCDRV_FS_48000;
		break;
	default:
		return -EINVAL;
	}

	mutex_lock(&mc_asoc->mutex);

	if ((port->stream & ~(1 << dir)) && (rate != port->rate)) {
		err	= -EBUSY;
		goto error;
	}

	port->rate	= rate;
	port->channels	= params_channels(params);

	if((mc_asoc_hwid_a != MCDRV_HWID_15H)
	&& (mc_asoc_hwid_a != MCDRV_HWID_16H)) {
		err	= update_clock(mc_asoc);
		if (err != MCDRV_SUCCESS) {
			dev_err(codec->dev, "%d: Error in update_clock\n", err);
			err	= -EIO;
			goto error;
		}
	}

	err	= setup_dai(codec, mc_asoc, id, MCDRV_DIO_DA, dir);
	if (err != MCDRV_SUCCESS) {
		dev_err(codec->dev, "%d: Error in setup_dai\n", err);
		err	= -EIO;
		goto error;
	}

	port->stream |= (1 << dir);

	if(dir == SNDRV_PCM_STREAM_CAPTURE) {
		mc_asoc_ap_cap_port_rate	= rate;
	}

error:
	mutex_unlock(&mc_asoc->mutex);

	return err;
}

static int mc_asoc_pcm_set_clkdiv(
	struct snd_soc_dai *dai,
	int div_id,
	int div)
{
	struct snd_soc_codec		*codec		= NULL;
	struct mc_asoc_data			*mc_asoc	= NULL;
	struct mc_asoc_port_params	*port		= NULL;

	if((dai == NULL)
	|| (dai->codec == NULL)
	|| (get_port_id(dai->id) < 0 || get_port_id(dai->id) >= IOPORT_NUM)) {
		return -EINVAL;
	}
	codec	= dai->codec;
	mc_asoc	= mc_asoc_get_mc_asoc(codec);
	if(mc_asoc == NULL) {
		return -EINVAL;
	}
	port	= &mc_asoc->port[get_port_id(dai->id)];

	switch (div_id) {
	case MC_ASOC_BCLK_MULT:
		switch (div) {
		case MC_ASOC_LRCK_X8:
			port->bckfs	= MCDRV_BCKFS_16;
			port->pcm_clkdown	= MCDRV_PCM_CLKDOWN_HALF;
			break;
		case MC_ASOC_LRCK_X16:
			port->bckfs	= MCDRV_BCKFS_16;
			port->pcm_clkdown	= MCDRV_PCM_CLKDOWN_OFF;
			break;
		case MC_ASOC_LRCK_X24:
			port->bckfs	= MCDRV_BCKFS_48;
			port->pcm_clkdown	= MCDRV_PCM_CLKDOWN_HALF;
			break;
		case MC_ASOC_LRCK_X32:
			port->bckfs	= MCDRV_BCKFS_32;
			port->pcm_clkdown	= MCDRV_PCM_CLKDOWN_OFF;
			break;
		case MC_ASOC_LRCK_X48:
			port->bckfs	= MCDRV_BCKFS_48;
			port->pcm_clkdown	= MCDRV_PCM_CLKDOWN_OFF;
			break;
		case MC_ASOC_LRCK_X64:
			port->bckfs	= MCDRV_BCKFS_64;
			port->pcm_clkdown	= MCDRV_PCM_CLKDOWN_OFF;
			break;
		case MC_ASOC_LRCK_X128:
			port->bckfs	= MCDRV_BCKFS_128;
			port->pcm_clkdown	= MCDRV_PCM_CLKDOWN_OFF;
			break;
		case MC_ASOC_LRCK_X256:
			port->bckfs	= MCDRV_BCKFS_256;
			port->pcm_clkdown	= MCDRV_PCM_CLKDOWN_OFF;
			break;
		case MC_ASOC_LRCK_X512:
			port->bckfs	= MCDRV_BCKFS_512;
			port->pcm_clkdown	= MCDRV_PCM_CLKDOWN_OFF;
			break;
		default:
			return -EINVAL;
		}
		break;

	default:
		return set_clkdiv_common(mc_asoc, div_id, div);
	}

	return 0;
}

static int mc_asoc_pcm_set_fmt(
	struct snd_soc_dai *dai,
	unsigned int fmt)
{
	struct snd_soc_codec		*codec		= NULL;
	struct mc_asoc_data			*mc_asoc	= NULL;
	struct mc_asoc_port_params	*port		= NULL;

	if((dai == NULL)
	|| (dai->codec == NULL)
	|| (get_port_id(dai->id) < 0 || get_port_id(dai->id) >= IOPORT_NUM)) {
		return -EINVAL;
	}
	codec	= dai->codec;
	mc_asoc	= mc_asoc_get_mc_asoc(codec);
	if(mc_asoc == NULL) {
		return -EINVAL;
	}
	port	= &mc_asoc->port[get_port_id(dai->id)];

	/* format */
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_DSP_A:
		port->format	= MCDRV_PCM_SHORTFRAME;
		break;
	case SND_SOC_DAIFMT_DSP_B:
		port->format	= MCDRV_PCM_LONGFRAME;
		break;
	default:
		return -EINVAL;
	}

	return set_fmt_common(port, fmt);
}

static int mc_asoc_pcm_hw_params(
	struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params,
	struct snd_soc_dai *dai)
{
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	struct snd_soc_pcm_runtime	*runtime	= snd_pcm_substream_chip(substream);
#endif
	struct snd_soc_codec		*codec		= NULL;
	struct mc_asoc_data			*mc_asoc	= NULL;
	struct mc_asoc_port_params	*port		= NULL;
	int		dir	= substream->stream;
	int		rate;
	int		err	= 0;

	TRACE_FUNC();

	if((substream == NULL)
	|| (dai == NULL)
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	|| (runtime == NULL)
#endif
	) {
		return -EINVAL;
	}

	if((get_port_id(dai->id) < 0) || (get_port_id(dai->id) >= IOPORT_NUM)) {
		dbg_info("dai->id=%d\n", get_port_id(dai->id));
		return -EINVAL;
	}

	dbg_info("hw_params: [%d] name=%s, dir=%d, rate=%d, bits=%d, ch=%d\n",
		 get_port_id(dai->id),
		 substream->name,
		 dir,
		 params_rate(params),
		 params_format(params),
		 params_channels(params));

#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	codec	= runtime->socdev->card->codec;
#else
	codec	= dai->codec;
#endif
	mc_asoc	= mc_asoc_get_mc_asoc(codec);
	if((codec == NULL)
	|| (mc_asoc == NULL)) {
		return -EINVAL;
	}
	port	= &mc_asoc->port[get_port_id(dai->id)];

	/* channels */
	switch (params_channels(params)) {
	case 1:
		port->pcm_mono[dir]	= MCDRV_PCM_MONO;
		break;
	case 2:
		port->pcm_mono[dir]	= MCDRV_PCM_STEREO;
		break;
	default:
		return -EINVAL;
	}

	/* format (bits) */
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S8:
		port->bits[dir]	= MCDRV_PCM_BITSEL_8;
		port->pcm_order[dir]	= MCDRV_PCM_MSB_FIRST;
		port->pcm_law[dir]	= MCDRV_PCM_LINEAR;
		break;
	case SNDRV_PCM_FORMAT_S16_LE:
		port->bits[dir]	= MCDRV_PCM_BITSEL_16;
		port->pcm_order[dir]	= MCDRV_PCM_LSB_FIRST;
		port->pcm_law[dir]	= MCDRV_PCM_LINEAR;
		break;
	case SNDRV_PCM_FORMAT_S16_BE:
		port->bits[dir]	= MCDRV_PCM_BITSEL_16;
		port->pcm_order[dir]	= MCDRV_PCM_MSB_FIRST;
		port->pcm_law[dir]	= MCDRV_PCM_LINEAR;
		break;
	case SNDRV_PCM_FORMAT_A_LAW:
		port->bits[dir]	= MCDRV_PCM_BITSEL_8;
		port->pcm_order[dir]	= MCDRV_PCM_MSB_FIRST;
		port->pcm_law[dir]	= MCDRV_PCM_ALAW;
		break;
	case SNDRV_PCM_FORMAT_MU_LAW:
		port->bits[dir]	= MCDRV_PCM_BITSEL_8;
		port->pcm_order[dir]	= MCDRV_PCM_MSB_FIRST;
		port->pcm_law[dir]	= MCDRV_PCM_MULAW;
		break;
	default:
		return -EINVAL;
	}

	/* rate */
	switch (params_rate(params)) {
	case 8000:
		rate	= MCDRV_FS_8000;
		break;
	case 16000:
		rate	= MCDRV_FS_16000;
		break;
	default:
		return -EINVAL;
	}

	mutex_lock(&mc_asoc->mutex);

	if ((port->stream & ~(1 << dir)) && (rate != port->rate)) {
		err	= -EBUSY;
		goto error;
	}

	port->rate	= rate;
	port->channels	= params_channels(params);

	if((mc_asoc_hwid_a != MCDRV_HWID_15H)
	&& (mc_asoc_hwid_a != MCDRV_HWID_16H)) {
		err	= update_clock(mc_asoc);
		if (err != MCDRV_SUCCESS) {
			dev_err(codec->dev, "%d: Error in update_clock\n", err);
			err	= -EIO;
			goto error;
		}
	}

	err	= setup_dai(codec, mc_asoc, get_port_id(dai->id), MCDRV_DIO_PCM, dir);
	if (err != MCDRV_SUCCESS) {
		dev_err(codec->dev, "%d: Error in setup_dai\n", err);
		err	= -EIO;
		goto error;
	}

	port->stream |= (1 << dir);

	if(dir == SNDRV_PCM_STREAM_CAPTURE) {
		mc_asoc_ap_cap_port_rate	= rate;
	}

error:
	mutex_unlock(&mc_asoc->mutex);

	return err;
}

static int mc_asoc_hw_free(
	struct snd_pcm_substream *substream,
	struct snd_soc_dai *dai)
{
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	struct snd_soc_pcm_runtime	*runtime	= snd_pcm_substream_chip(substream);
#endif
	struct snd_soc_codec		*codec		= NULL;
	struct mc_asoc_data			*mc_asoc	= NULL;
	struct mc_asoc_port_params	*port		= NULL;
	int		dir	= substream->stream;
	int		err	= 0;
	int		id	=0;
	UINT32	update	=0;
	MCDRV_PATH_INFO	path_info;
	MCDRV_CHANNEL	*asDit;
	int		i;
	int		port_block, port_block_on;
	int		channels	= 0;

	TRACE_FUNC();

	if((substream == NULL)
	|| (dai == NULL)
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	|| (runtime == NULL)
#endif
	)
	{
		return -EINVAL;
	}

#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	codec	= runtime->socdev->card->codec;
#else
	codec	= dai->codec;
#endif
	mc_asoc	= mc_asoc_get_mc_asoc(codec);
	if((codec == NULL)
	|| (mc_asoc == NULL)
	|| (get_port_id(dai->id) < 0 || get_port_id(dai->id) >= IOPORT_NUM)) {
		return -EINVAL;
	}

	id	= get_port_id(dai->id);
	port	= &mc_asoc->port[id];

	mutex_lock(&mc_asoc->mutex);

	if (!(port->stream & (1 << dir))) {
		err	= 0;
		goto error;
	}

	port->stream &= ~(1 << dir);

	if((dir == SNDRV_PCM_STREAM_CAPTURE)
	&& (mc_asoc_ap_cap_port_rate == MCDRV_FS_8000)) {
		switch(mc_asoc_cap_path) {
		case	DIO_PORT0:
			update	= MCDRV_DIO0_COM_UPDATE_FLAG | MCDRV_DIO0_DIT_UPDATE_FLAG;
			break;
		case	DIO_PORT1:
			update	= MCDRV_DIO1_COM_UPDATE_FLAG | MCDRV_DIO1_DIT_UPDATE_FLAG;
			break;
		case	DIO_PORT2:
			update	= MCDRV_DIO2_COM_UPDATE_FLAG | MCDRV_DIO2_DIT_UPDATE_FLAG;
			break;
		default:
			update	= 0;
			break;
		}
		if(update != 0) {
			port_block	= get_port_block(id);
			port_block_on	= get_port_block_on(id);
			if((err = _McDrv_Ctrl(MCDRV_GET_PATH, &path_info, 0)) != MCDRV_SUCCESS) {
				err	= mc_asoc_map_drv_error(err);
				goto error;
			}
			for(i = 0; i < DIT0_PATH_CHANNELS; i++) {
				path_info.asDit0[i].abSrcOnOff[port_block]	&= ~(port_block_on);
				path_info.asDit0[i].abSrcOnOff[port_block]	|= (port_block_on<<1);
			}
			for(i = 0; i < DIT1_PATH_CHANNELS; i++) {
				path_info.asDit1[i].abSrcOnOff[port_block]	&= ~(port_block_on);
				path_info.asDit1[i].abSrcOnOff[port_block]	|= (port_block_on<<1);
			}
			for(i = 0; i < DIT2_PATH_CHANNELS; i++) {
				path_info.asDit2[i].abSrcOnOff[port_block]	&= ~(port_block_on);
				path_info.asDit2[i].abSrcOnOff[port_block]	|= (port_block_on<<1);
			}
			for(i = 0; i < DAC_PATH_CHANNELS; i++) {
				path_info.asDac[i].abSrcOnOff[port_block]	&= ~(port_block_on);
				path_info.asDac[i].abSrcOnOff[port_block]	|= (port_block_on<<1);
			}
			for(i = 0; i < AE_PATH_CHANNELS; i++) {
				path_info.asAe[i].abSrcOnOff[port_block]	&= ~(port_block_on);
				path_info.asAe[i].abSrcOnOff[port_block]	|= (port_block_on<<1);
			}
			for(i = 0; i < MIX_PATH_CHANNELS; i++) {
				path_info.asMix[i].abSrcOnOff[port_block]	&= ~(port_block_on);
				path_info.asMix[i].abSrcOnOff[port_block]	|= (port_block_on<<1);
			}
			for(i = 0; i < PEAK_PATH_CHANNELS; i++) {
				path_info.asPeak[i].abSrcOnOff[port_block]	&= ~(port_block_on);
				path_info.asPeak[i].abSrcOnOff[port_block]	|= (port_block_on<<1);
			}
			for(i = 0; i < CDSP_PATH_CHANNELS; i++) {
				path_info.asCdsp[i].abSrcOnOff[port_block]	&= ~(port_block_on);
				path_info.asCdsp[i].abSrcOnOff[port_block]	|= (port_block_on<<1);
			}
			for(i = 0; i < MIX2_PATH_CHANNELS; i++) {
				path_info.asMix2[i].abSrcOnOff[port_block]	&= ~(port_block_on);
				path_info.asMix2[i].abSrcOnOff[port_block]	|= (port_block_on<<1);
			}
			asDit	= (MCDRV_CHANNEL*)((void*)&path_info+get_port_srconoff_offset(id));
			if(id == 0) {
				channels	= DIT0_PATH_CHANNELS;
			}
			else if(id == 1) {
				channels	= DIT1_PATH_CHANNELS;
			}
			else if(id == 2) {
				channels	= DIT2_PATH_CHANNELS;
			}
			for(i = 0; i < channels; i++) {
				asDit[i].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]	&= ~MCDRV_SRC4_PDM_ON;
				asDit[i].abSrcOnOff[MCDRV_SRC_PDM_BLOCK]	|= MCDRV_SRC4_PDM_OFF;
				asDit[i].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	&= ~MCDRV_SRC4_ADC0_ON;
				asDit[i].abSrcOnOff[MCDRV_SRC_ADC0_BLOCK]	|= MCDRV_SRC4_ADC0_OFF;
				asDit[i].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK]	&= ~MCDRV_SRC4_ADC1_ON;
				asDit[i].abSrcOnOff[MCDRV_SRC_ADC1_BLOCK]	|= MCDRV_SRC4_ADC1_OFF;
				asDit[i].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK]	&= ~MCDRV_SRC3_DIR0_ON;
				asDit[i].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK]	|= MCDRV_SRC3_DIR0_OFF;
				asDit[i].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK]	&= ~MCDRV_SRC3_DIR1_ON;
				asDit[i].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK]	|= MCDRV_SRC3_DIR1_OFF;
				asDit[i].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK]	&= ~MCDRV_SRC3_DIR2_ON;
				asDit[i].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK]	|= MCDRV_SRC3_DIR2_OFF;
				asDit[i].abSrcOnOff[MCDRV_SRC_AE_BLOCK]		&= ~MCDRV_SRC6_AE_ON;
				asDit[i].abSrcOnOff[MCDRV_SRC_AE_BLOCK]		|= MCDRV_SRC6_AE_OFF;
				asDit[i].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]	&= ~MCDRV_SRC6_MIX_ON;
				asDit[i].abSrcOnOff[MCDRV_SRC_MIX_BLOCK]	|= MCDRV_SRC6_MIX_OFF;
				asDit[i].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK]	&= ~MCDRV_SRC4_DTMF_ON;
				asDit[i].abSrcOnOff[MCDRV_SRC_DTMF_BLOCK]	|= MCDRV_SRC4_DTMF_OFF;
				asDit[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	&= ~MCDRV_SRC6_CDSP_ON;
				asDit[i].abSrcOnOff[MCDRV_SRC_CDSP_BLOCK]	|= MCDRV_SRC6_CDSP_OFF;
			}
			if((err = _McDrv_Ctrl(MCDRV_SET_PATH, &path_info, 0)) != MCDRV_SUCCESS) {
				err	= mc_asoc_map_drv_error(err);
				goto error;
			}
			if(mc_asoc_cdsp_ae_ex_flag == MC_ASOC_AE_EX_ON) {
				if((err = _McDrv_Ctrl(MCDRV_SET_AUDIOENGINE_EX, NULL, 0)) != MCDRV_SUCCESS) {
					err	= mc_asoc_map_drv_error(err);
					goto error;
				}
				mc_asoc->ae_ex_store.size	= 0;
				mc_asoc_cdsp_ae_ex_flag		= MC_ASOC_CDSP_AE_EX_OFF;
			}
			err	= _McDrv_Ctrl(MCDRV_SET_DIGITALIO, (void *)&stDioInfo_Default, update);
			if (err < MCDRV_SUCCESS) {
				dev_err(codec->dev, "%d: Error in MCDRV_SET_DIGITALIO\n", err);
				goto error;
			}
			err	= connect_path(codec);
		}
	}

error:
	mutex_unlock(&mc_asoc->mutex);

	return err;
}

static struct snd_soc_dai_ops	mc_asoc_dai_ops[]	= {
	{
		.set_clkdiv	= mc_asoc_i2s_set_clkdiv,
		.set_fmt	= mc_asoc_i2s_set_fmt,
		.hw_params	= mc_asoc_i2s_hw_params,
		.hw_free	= mc_asoc_hw_free,
	},
	{
		.set_clkdiv	= mc_asoc_pcm_set_clkdiv,
		.set_fmt	= mc_asoc_pcm_set_fmt,
		.hw_params	= mc_asoc_pcm_hw_params,
		.hw_free	= mc_asoc_hw_free,
	},
	{
		.set_clkdiv	= mc_asoc_i2s_set_clkdiv,
		.set_fmt	= mc_asoc_i2s_set_fmt,
		.hw_params	= mc_asoc_i2s_hw_params,
		.hw_free	= mc_asoc_hw_free,
	},
	{
		.set_clkdiv	= mc_asoc_pcm_set_clkdiv,
		.set_fmt	= mc_asoc_pcm_set_fmt,
		.hw_params	= mc_asoc_pcm_hw_params,
		.hw_free	= mc_asoc_hw_free,
	},
	{
		.set_clkdiv	= mc_asoc_i2s_set_clkdiv,
		.set_fmt	= mc_asoc_i2s_set_fmt,
		.hw_params	= mc_asoc_i2s_hw_params,
		.hw_free	= mc_asoc_hw_free,
	},
	{
		.set_clkdiv	= mc_asoc_pcm_set_clkdiv,
		.set_fmt	= mc_asoc_pcm_set_fmt,
		.hw_params	= mc_asoc_pcm_hw_params,
		.hw_free	= mc_asoc_hw_free,
	}
};

#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
struct snd_soc_dai			mc_asoc_dai[]	= 
#else
struct snd_soc_dai_driver	mc_asoc_dai[]	= 
#endif
{
	{
		.name	= MC_ASOC_NAME "-da0i",
		.id	= 1,
		.playback	= {
			.stream_name	= "Playback",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates	= MC_ASOC_I2S_RATE,
			.formats	= MC_ASOC_I2S_FORMATS,
		},
		.capture	= {
			.stream_name	= "Capture",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates	= MC_ASOC_I2S_RATE,
			.formats	= MC_ASOC_I2S_FORMATS,
		},
		.ops	= &mc_asoc_dai_ops[0]
	},
	{
		.name	= MC_ASOC_NAME "-da0p",
		.id	= 1,
		.playback	= {
			.stream_name	= "Playback",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates	= MC_ASOC_PCM_RATE,
			.formats	= MC_ASOC_PCM_FORMATS,
		},
		.capture	= {
			.stream_name	= "Capture",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates	= MC_ASOC_PCM_RATE,
			.formats	= MC_ASOC_PCM_FORMATS,
		},
		.ops	= &mc_asoc_dai_ops[1]
	},
	{
		.name	= MC_ASOC_NAME "-da1i",
		.id	= 2,
		.playback	= {
			.stream_name	= "Playback",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates	= MC_ASOC_I2S_RATE,
			.formats	= MC_ASOC_I2S_FORMATS,
		},
		.capture	= {
			.stream_name	= "Capture",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates	= MC_ASOC_I2S_RATE,
			.formats	= MC_ASOC_I2S_FORMATS,
		},
		.ops	= &mc_asoc_dai_ops[2]
	},
	{
		.name	= MC_ASOC_NAME "-da1p",
		.id	= 2,
		.playback	= {
			.stream_name	= "Playback",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates	= MC_ASOC_PCM_RATE,
			.formats	= MC_ASOC_PCM_FORMATS,
		},
		.capture	= {
			.stream_name	= "Capture",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates	= MC_ASOC_PCM_RATE,
			.formats	= MC_ASOC_PCM_FORMATS,
		},
		.ops	= &mc_asoc_dai_ops[3]
	},
	{
		.name	= MC_ASOC_NAME "-da2i",
		.id	= 3,
		.playback	= {
			.stream_name	= "Playback",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates	= MC_ASOC_I2S_RATE,
			.formats	= MC_ASOC_I2S_FORMATS,
		},
		.capture	= {
			.stream_name	= "Capture",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates	= MC_ASOC_I2S_RATE,
			.formats	= MC_ASOC_I2S_FORMATS,
		},
		.ops	= &mc_asoc_dai_ops[4]
	},
	{
		.name	= MC_ASOC_NAME "-da2p",
		.id	= 3,
		.playback	= {
			.stream_name	= "Playback",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates	= MC_ASOC_PCM_RATE,
			.formats	= MC_ASOC_PCM_FORMATS,
		},
		.capture	= {
			.stream_name	= "Capture",
			.channels_min	= 1,
			.channels_max	= 2,
			.rates	= MC_ASOC_PCM_RATE,
			.formats	= MC_ASOC_PCM_FORMATS,
		},
		.ops	= &mc_asoc_dai_ops[5]
	},
};
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
EXPORT_SYMBOL_GPL(mc_asoc_dai);
#endif

/*
 * Control interface
 */
/*
 * Virtual register
 *
 * 16bit software registers are implemented for volumes and mute
 * switches (as an exception, no mute switches for MIC and HP gain).
 * Register contents are stored in codec's register cache.
 *
 *	15	14							8	7	6						0
 *	+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 *	|swR|			volume-R		|swL|			volume-L		|
 *	+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 */

static unsigned int mc_asoc_read_reg(
	struct snd_soc_codec *codec,
	unsigned int reg)
{
	int	ret;

	if(codec == NULL) {
		return -EINVAL;
	}
	ret	= read_cache(codec, reg);
	if (ret < 0) {
		return -EIO;
	}
	return (unsigned int)ret;
}

static int write_reg_vol(
	struct snd_soc_codec *codec,
	  unsigned int reg,
	  unsigned int value)
{
	MCDRV_VOL_INFO	update;
	SINT16	*vp;
	SINT16	db;
	int		err, i;
	int		cache;
	unsigned int	reg_DIR;

	if(reg == MC_ASOC_VOICE_RECORDING) {
		switch(get_bt_port()) {
		case	DIO_0:
			reg_DIR	= MC_ASOC_DVOL_DIR0;
			break;
		case	DIO_1:
			reg_DIR	= MC_ASOC_DVOL_DIR1;
			break;
		case	DIO_2:
			reg_DIR	= MC_ASOC_DVOL_DIR2;
			break;
		default:
			return -EIO;
		}
		memset(&update, 0, sizeof(MCDRV_VOL_INFO));
		if(value == 0) {/*	mute	*/
			vp	= (SINT16 *)((void *)&update + mc_asoc_vreg_map[MC_ASOC_DVOL_AENG6].offset);
			db	= mc_asoc_vreg_map[MC_ASOC_DVOL_AENG6].volmap[0];
			*vp	= db | MCDRV_VOL_UPDATE;
			vp++;
			*vp	= db | MCDRV_VOL_UPDATE;
			vp	= (SINT16 *)((void *)&update + mc_asoc_vreg_map[reg_DIR].offset);
			db	= mc_asoc_vreg_map[reg_DIR].volmap[0];
			*vp	= db | MCDRV_VOL_UPDATE;
			vp++;
			*vp	= db | MCDRV_VOL_UPDATE;
		} else {/*	unmute	*/
			vp		= (SINT16 *)((void *)&update + mc_asoc_vreg_map[MC_ASOC_DVOL_AENG6].offset);
			cache	= read_cache(codec, MC_ASOC_DVOL_AENG6);
			if(cache < 0) {
				return -EIO;
			}

			for (i = 0; i < 2; i++, vp++) {
				unsigned int	v	= (cache >> (i*8)) & 0xff;
				int	sw, vol;

				sw	= (v & 0x80);
				vol	= sw ? (v & 0x7f) : 0;
				db	= mc_asoc_vreg_map[MC_ASOC_DVOL_AENG6].volmap[vol];
				*vp	= db | MCDRV_VOL_UPDATE;
			}

			vp		= (SINT16 *)((void *)&update + mc_asoc_vreg_map[reg_DIR].offset);
			cache	= read_cache(codec, reg_DIR);
			if(cache < 0) {
				return -EIO;
			}

			for (i = 0; i < 2; i++, vp++) {
				unsigned int	v	= (cache >> (i*8)) & 0xff;
				int	sw, vol;

				sw	= (v & 0x80);
				vol	= sw ? (v & 0x7f) : 0;
				db	= mc_asoc_vreg_map[reg_DIR].volmap[vol];
				*vp	= db | MCDRV_VOL_UPDATE;
			}
		}

		err	= _McDrv_Ctrl(MCDRV_SET_VOLUME, &update, 0);
		if (err != MCDRV_SUCCESS) {
			dev_err(codec->dev, "%d: Error in MCDRV_SET_VOLUME\n", err);
			return -EIO;
		}
	}
	else if(mc_asoc_vreg_map[reg].offset != (size_t)-1) {
		memset(&update, 0, sizeof(MCDRV_VOL_INFO));
		vp		= (SINT16 *)((void *)&update + mc_asoc_vreg_map[reg].offset);
		cache	= read_cache(codec, reg);
		if(cache < 0) {
			return -EIO;
		}

		for (i = 0; i < 2; i++, vp++) {
			unsigned int	v	= (value >> (i*8)) & 0xff;
			unsigned int	c	= (cache >> (i*8)) & 0xff;

			if (v != c) {
				int		sw, vol;
				SINT16	db;
				sw	= (reg < MC_ASOC_AVOL_MIC1_GAIN) ? (v & 0x80) : 1;
				if(reg == MC_ASOC_DVOL_AENG6) {
					if(sw != 0) {
						sw	= read_cache(codec, MC_ASOC_VOICE_RECORDING);
					}
				}
				switch(get_bt_port()) {
				case	DIO_0:
					if(reg == MC_ASOC_DVOL_DIR0) {
						if(sw != 0) {
							sw	= read_cache(codec, MC_ASOC_VOICE_RECORDING);
						}
					}
					break;
				case	DIO_1:
					if(reg == MC_ASOC_DVOL_DIR1) {
						if(sw != 0) {
							sw	= read_cache(codec, MC_ASOC_VOICE_RECORDING);
						}
					}
					break;
				case	DIO_2:
					if(reg == MC_ASOC_DVOL_DIR2) {
						if(sw != 0) {
							sw	= read_cache(codec, MC_ASOC_VOICE_RECORDING);
						}
					}
					break;
				default:
					break;
				}
				vol	= sw ? (v & 0x7f) : 0;
				db	= mc_asoc_vreg_map[reg].volmap[vol];
				*vp	= db | MCDRV_VOL_UPDATE;
			}
		}

		err	= _McDrv_Ctrl(MCDRV_SET_VOLUME, &update, 0);
		if (err != MCDRV_SUCCESS) {
			dev_err(codec->dev, "%d: Error in MCDRV_SET_VOLUME\n", err);
			return -EIO;
		}
	}

	err	= write_cache(codec, reg, value);
	if (err != 0) {
		dev_err(codec->dev, "Cache write to %x failed: %d\n", reg, err);
	}

	return 0;
}


static int set_audio_mode_play(
	struct snd_soc_codec *codec,
	unsigned int value
)
{
	int		ret;
	struct mc_asoc_data	*mc_asoc	= mc_asoc_get_mc_asoc(codec);

	TRACE_FUNC();
	printk("audio_mode=%d\n", value);

	if(mc_asoc == NULL) {
		return -EINVAL;
	}

	if((ret = write_cache(codec, MC_ASOC_AUDIO_MODE_PLAY, value)) < 0) {
		return ret;
	}

	if(mc_asoc_hold == YMC_NOTITY_HOLD_ON) {
		return 0;
	}

	return connect_path(codec);
}

static int set_audio_mode_cap(
	struct snd_soc_codec *codec,
	unsigned int value
)
{
	int		ret;
	struct mc_asoc_data	*mc_asoc	= mc_asoc_get_mc_asoc(codec);

	TRACE_FUNC();
	printk("audio_mode=%d\n", value);

	if(mc_asoc == NULL) {
		return -EINVAL;
	}

	if((ret = write_cache(codec, MC_ASOC_AUDIO_MODE_CAP, value)) < 0) {
		return ret;
	}

	if(mc_asoc_hold == YMC_NOTITY_HOLD_ON) {
		return 0;
	}
	return connect_path(codec);
}

static int set_incall_mic(
	struct snd_soc_codec *codec,
	unsigned int reg,
	unsigned int value
)
{
	int		ret;

	TRACE_FUNC();

	if((ret = write_cache(codec, reg, value)) < 0) {
		return ret;
	}

	if(mc_asoc_hold == YMC_NOTITY_HOLD_ON) {
		return 0;
	}
	return connect_path(codec);
}

static int set_ain_playback(
	struct snd_soc_codec *codec,
	unsigned int reg,
	unsigned int value
)
{
	int		ret;
	int		audio_mode;
	int		audio_mode_cap;
	struct mc_asoc_data	*mc_asoc	= mc_asoc_get_mc_asoc(codec);

	TRACE_FUNC();

	dbg_info("ain_playback=%d\n", value);

	if(mc_asoc == NULL) {
		return -EINVAL;
	}

	if((audio_mode_cap = read_cache(codec, MC_ASOC_AUDIO_MODE_CAP)) < 0) {
		return -EIO;
	}
	if((audio_mode = read_cache(codec, MC_ASOC_AUDIO_MODE_PLAY)) < 0) {
		return -EIO;
	}

	if((ret = write_cache(codec, reg, value)) < 0) {
		return ret;
	}

	if((audio_mode == MC_ASOC_AUDIO_MODE_INCALL)
	|| (audio_mode == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)) {
		if((audio_mode_cap == MC_ASOC_AUDIO_MODE_INCALL)
		|| (audio_mode_cap == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)) {
			return 0;
		}
	}
	if((audio_mode == MC_ASOC_AUDIO_MODE_INCOMM)
	&& (audio_mode_cap == MC_ASOC_AUDIO_MODE_INCOMM)) {
		return 0;
	}

	if(mc_asoc_hold == YMC_NOTITY_HOLD_ON) {
		return 0;
	}

	return connect_path(codec);
}

static int set_dtmf_control(
	struct snd_soc_codec *codec,
	unsigned int reg,
	unsigned int value
)
{
	int		ret;
	struct mc_asoc_data	*mc_asoc	= mc_asoc_get_mc_asoc(codec);

	TRACE_FUNC();

	if(mc_asoc == NULL) {
		return -EINVAL;
	}

	if(value == 1) {
		/*	On	*/
		mc_asoc->dtmf_store.bOnOff	= MCDRV_DTMF_ON;
		if((ret = _McDrv_Ctrl(MCDRV_SET_DTMF, (void *)&mc_asoc->dtmf_store, (UINT32)-1)) != MCDRV_SUCCESS) {
			return mc_asoc_map_drv_error(ret);
		}
	}

	if((ret = write_cache(codec, reg, value)) < 0) {
		return ret;
	}

	if(mc_asoc_hold == YMC_NOTITY_HOLD_ON) {
		return 0;
	}

	ret	= connect_path(codec);
	if(value == 1) {
		/*	On	*/
	}
	else {
		if(ret == 0) {
			mc_asoc->dtmf_store.bOnOff	= MCDRV_DTMF_OFF;
			ret	= mc_asoc_map_drv_error(_McDrv_Ctrl(MCDRV_SET_DTMF, (void *)&mc_asoc->dtmf_store, MCDRV_DTMFONOFF_UPDATE_FLAG));
		}
	}

	return ret;
}

static int set_dtmf_output(
	struct snd_soc_codec *codec,
	unsigned int reg,
	unsigned int value
)
{
	int		ret;

	TRACE_FUNC();

	if((ret = write_cache(codec, reg, value)) < 0) {
		return ret;
	}

	if(mc_asoc_hold == YMC_NOTITY_HOLD_ON) {
		return 0;
	}

	return connect_path(codec);
}

static int mc_asoc_write_reg(
	struct snd_soc_codec *codec,
	unsigned int reg,
	unsigned int value)
{
	int		err	= 0;

	if(codec == NULL) {
		return -EINVAL;
	}
	if (reg < MC_ASOC_N_VOL_REG) {
		err	= write_reg_vol(codec, reg, value);
	}
	else {
		switch(reg) {
		case	MC_ASOC_AUDIO_MODE_PLAY:
			err	= set_audio_mode_play(codec, value);
			break;
		case	MC_ASOC_AUDIO_MODE_CAP:
			err	= set_audio_mode_cap(codec, value);
			break;
		case	MC_ASOC_OUTPUT_PATH:
		case	MC_ASOC_INPUT_PATH:
			err	= write_cache(codec, reg, value);
			break;
		case	MC_ASOC_INCALL_MIC_SP:
		case	MC_ASOC_INCALL_MIC_RC:
		case	MC_ASOC_INCALL_MIC_HP:
		case	MC_ASOC_INCALL_MIC_LO1:
		case	MC_ASOC_INCALL_MIC_LO2:
			err	= set_incall_mic(codec, reg, value);
			break;
		case	MC_ASOC_MAINMIC_PLAYBACK_PATH:
		case	MC_ASOC_SUBMIC_PLAYBACK_PATH:
		case	MC_ASOC_2MIC_PLAYBACK_PATH:
		case	MC_ASOC_HSMIC_PLAYBACK_PATH:
		case	MC_ASOC_BTMIC_PLAYBACK_PATH:
		case	MC_ASOC_LIN1_PLAYBACK_PATH:
		case	MC_ASOC_LIN2_PLAYBACK_PATH:
			err	= set_ain_playback(codec, reg, value);
			break;
		case	MC_ASOC_PARAMETER_SETTING:
			break;
		case	MC_ASOC_DTMF_CONTROL:
			err	= set_dtmf_control(codec, reg, value);
			break;
		case	MC_ASOC_DTMF_OUTPUT:
			err	= set_dtmf_output(codec, reg, value);
			break;

#ifdef MC_ASOC_TEST
		case	MC_ASOC_MAIN_MIC:
			mc_asoc_main_mic	= value;
			write_cache(codec, reg, value);
			break;
		case	MC_ASOC_SUB_MIC:
			mc_asoc_sub_mic	= value;
			write_cache(codec, reg, value);
			break;
		case	MC_ASOC_HS_MIC:
			mc_asoc_hs_mic	= value;
			write_cache(codec, reg, value);
			break;
		case	MC_ASOC_MIC1_BIAS:
			mc_asoc_mic1_bias	= value;
			write_cache(codec, reg, value);
			err	= connect_path(codec);
			break;
		case	MC_ASOC_MIC2_BIAS:
			mc_asoc_mic2_bias	= value;
			write_cache(codec, reg, value);
			err	= connect_path(codec);
			break;
		case	MC_ASOC_MIC3_BIAS:
			mc_asoc_mic3_bias	= value;
			write_cache(codec, reg, value);
			err	= connect_path(codec);
			break;
		case	MC_ASOC_CAP_PATH_8KHZ:
			mc_asoc_cap_path	= value;
			write_cache(codec, reg, value);
			break;
#endif

		default:
			err	= -EINVAL;
			break;
		}
	}

	if(err < 0) {
		dbg_info("err=%d\n", err);
	}
	return err;
}

static const DECLARE_TLV_DB_SCALE(mc_asoc_tlv_digital, -7500, 100, 1);
static const DECLARE_TLV_DB_SCALE(mc_asoc_tlv_adc, -2850, 150, 1);
static const DECLARE_TLV_DB_SCALE(mc_asoc_tlv_ain, -3150, 150, 1);
static const DECLARE_TLV_DB_SCALE(mc_asoc_tlv_aout, -3100, 100, 1);
static const DECLARE_TLV_DB_SCALE(mc_asoc_tlv_micgain, 1500, 500, 0);

static unsigned int	mc_asoc_tlv_hpsp[]	= {
	TLV_DB_RANGE_HEAD(5),
	0, 2, TLV_DB_SCALE_ITEM(-4400, 800, 1),
	2, 3, TLV_DB_SCALE_ITEM(-2800, 400, 0),
	3, 7, TLV_DB_SCALE_ITEM(-2400, 200, 0),
	7, 15, TLV_DB_SCALE_ITEM(-1600, 100, 0),
	15, 31, TLV_DB_SCALE_ITEM(-800, 50, 0),
};

static unsigned int	mc_asoc_tlv_hpgain[]	= {
	TLV_DB_RANGE_HEAD(2),
	0, 2, TLV_DB_SCALE_ITEM(0, 150, 0),
	2, 3, TLV_DB_SCALE_ITEM(300, 300, 0),
};

static const struct snd_kcontrol_new	mc_asoc_snd_controls[]	= {
	/*
	 * digital volumes and mute switches
	 */
	SOC_DOUBLE_TLV("AD#0 Digital Volume",
				MC_ASOC_DVOL_AD0, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("AD#0 Digital Switch",
				MC_ASOC_DVOL_AD0, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("AD#1 Digital Volume",
				MC_ASOC_DVOL_AD1, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("AD#1 Digital Switch",
				MC_ASOC_DVOL_AD1, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("AENG6 Volume",
				MC_ASOC_DVOL_AENG6, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("AENG6 Switch",
				MC_ASOC_DVOL_AENG6, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("PDM Volume",
				MC_ASOC_DVOL_PDM, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("PDM Switch",
				MC_ASOC_DVOL_PDM, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DTMF_B Volume",
				MC_ASOC_DVOL_DTMF_B, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DTMF_B Switch",
				MC_ASOC_DVOL_DTMF_B, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DIR#0 Volume",
				MC_ASOC_DVOL_DIR0, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DIR#0 Switch",
				MC_ASOC_DVOL_DIR0, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DIR#1 Volume",
				MC_ASOC_DVOL_DIR1, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DIR#1 Switch",
				MC_ASOC_DVOL_DIR1, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DIR#2 Volume",
				MC_ASOC_DVOL_DIR2, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DIR#2 Switch",
				MC_ASOC_DVOL_DIR2, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("AD#0 ATT Volume",
				MC_ASOC_DVOL_AD0_ATT, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("AD#0 ATT Switch",
				MC_ASOC_DVOL_AD0_ATT, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("AD#1 ATT Volume",
				MC_ASOC_DVOL_AD1_ATT, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("AD#1 ATT Switch",
				MC_ASOC_DVOL_AD1_ATT, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DTMF Volume",
				MC_ASOC_DVOL_DTMF, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DTMF Switch",
				MC_ASOC_DVOL_DTMF, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DIR#0 ATT Volume",
				MC_ASOC_DVOL_DIR0_ATT, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DIR#0 ATT Switch",
				MC_ASOC_DVOL_DIR0_ATT, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DIR#1 ATT Volume",
				MC_ASOC_DVOL_DIR1_ATT, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DIR#1 ATT Switch",
				MC_ASOC_DVOL_DIR1_ATT, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DIR#2 ATT Volume",
				MC_ASOC_DVOL_DIR2_ATT, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DIR#2 ATT Switch",
				MC_ASOC_DVOL_DIR2_ATT, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("Side Tone Playback Volume",
				MC_ASOC_DVOL_SIDETONE, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("Side Tone Playback Switch",
				MC_ASOC_DVOL_SIDETONE, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DAC Master Playback Volume",
				MC_ASOC_DVOL_DAC_MASTER, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DAC Master Playback Switch",
				MC_ASOC_DVOL_DAC_MASTER, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DAC Voice Playback Volume",
				MC_ASOC_DVOL_DAC_VOICE, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DAC Voice Playback Switch",
				MC_ASOC_DVOL_DAC_VOICE, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DAC Playback Volume",
				MC_ASOC_DVOL_DAC_ATT, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DAC Playback Switch",
				MC_ASOC_DVOL_DAC_ATT, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DIT#0 Capture Volume",
				MC_ASOC_DVOL_DIT0, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DIT#0 Capture Switch",
				MC_ASOC_DVOL_DIT0, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DIT#1 Capture Volume",
				MC_ASOC_DVOL_DIT1, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DIT#1 Capture Switch",
				MC_ASOC_DVOL_DIT1, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DIT#2 Capture Volume",
				MC_ASOC_DVOL_DIT2, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DIT#2 Capture Switch",
				MC_ASOC_DVOL_DIT2, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DIT#2_1 Capture Volume",
				MC_ASOC_DVOL_DIT2_1, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DIT#2_1 Capture Switch",
				MC_ASOC_DVOL_DIT2_1, 7, 15, 1, 0),
	
	SOC_DOUBLE_TLV("AD#0 MIX2 Volume",
				MC_ASOC_DVOL_AD0_MIX2, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("AD#0 MIX2 Switch",
				MC_ASOC_DVOL_AD0_MIX2, 7, 15, 1, 0),
	
	SOC_DOUBLE_TLV("AD#1 MIX2 Volume",
				MC_ASOC_DVOL_AD1_MIX2, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("AD#1 MIX2 Switch",
				MC_ASOC_DVOL_AD1_MIX2, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DIR#0 MIX2 Volume",
				MC_ASOC_DVOL_DIR0_MIX2, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DIR#0 MIX2 Switch",
				MC_ASOC_DVOL_DIR0_MIX2, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DIR#1 MIX2 Volume",
				MC_ASOC_DVOL_DIR1_MIX2, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DIR#1 MIX2 Switch",
				MC_ASOC_DVOL_DIR1_MIX2, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("DIR#2 MIX2 Volume",
				MC_ASOC_DVOL_DIR2_MIX2, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("DIR#2 MIX2 Switch",
				MC_ASOC_DVOL_DIR2_MIX2, 7, 15, 1, 0),


	SOC_DOUBLE_TLV("Master Playback Volume",
				MC_ASOC_DVOL_MASTER, 0, 8, 75, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("Master Playback Switch",
				MC_ASOC_DVOL_MASTER, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("Voice Playback Volume",
				MC_ASOC_DVOL_VOICE, 0, 8, 75, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("Voice Playback Switch",
				MC_ASOC_DVOL_VOICE, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("AnalogIn Playback Digital Volume",
				MC_ASOC_DVOL_AIN, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("AnalogIn Playback Digital Switch",
				MC_ASOC_DVOL_AIN, 7, 15, 1, 0),
	SOC_DOUBLE_TLV("AnalogIn Playback Volume",
				MC_ASOC_DVOL_AINPLAYBACK, 0, 8, 93, 0, mc_asoc_tlv_digital),
	SOC_DOUBLE("AnalogIn Playback Switch",
				MC_ASOC_DVOL_AINPLAYBACK, 7, 15, 1, 0),

	/*
	 * analog volumes and mute switches
	 */
	SOC_DOUBLE_TLV("AD#0 Analog Volume",
				MC_ASOC_AVOL_AD0, 0, 8, 31, 0, mc_asoc_tlv_adc),
	SOC_DOUBLE("AD#0 Analog Switch",
				MC_ASOC_AVOL_AD0, 7, 15, 1, 0),

	SOC_SINGLE_TLV("AD#1 Analog Volume",
				MC_ASOC_AVOL_AD1, 0, 31, 0, mc_asoc_tlv_adc),
	SOC_SINGLE("AD#1 Analog Switch",
				MC_ASOC_AVOL_AD1, 7, 1, 0),

	SOC_DOUBLE_TLV("Line 1 Bypass Playback Volume",
				MC_ASOC_AVOL_LIN1_BYPASS, 0, 8, 31, 0, mc_asoc_tlv_ain),
	SOC_DOUBLE("Line 1 Bypass Playback Switch",
				MC_ASOC_AVOL_LIN1_BYPASS, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("Line 2 Bypass Playback Volume",
				MC_ASOC_AVOL_LIN2_BYPASS, 0, 8, 31, 0, mc_asoc_tlv_ain),
	SOC_DOUBLE("Line 2 Bypass Playback Switch",
				MC_ASOC_AVOL_LIN2_BYPASS, 7, 15, 1, 0),

	SOC_SINGLE_TLV("Mic 1 Bypass Playback Volume",
				MC_ASOC_AVOL_MIC1, 0, 31, 0, mc_asoc_tlv_ain),
	SOC_SINGLE("Mic 1 Bypass Playback Switch",
				MC_ASOC_AVOL_MIC1, 7, 1, 0),

	SOC_SINGLE_TLV("Mic 2 Bypass Playback Volume",
				MC_ASOC_AVOL_MIC2, 0, 31, 0, mc_asoc_tlv_ain),
	SOC_SINGLE("Mic 2 Bypass Playback Switch",
				MC_ASOC_AVOL_MIC2, 7, 1, 0),

	SOC_SINGLE_TLV("Mic 3 Bypass Playback Volume",
				MC_ASOC_AVOL_MIC3, 0, 31, 0, mc_asoc_tlv_ain),
	SOC_SINGLE("Mic 3 Bypass Playback Switch",
				MC_ASOC_AVOL_MIC3, 7, 1, 0),

	SOC_DOUBLE_TLV("Headphone Playback Volume",
				MC_ASOC_AVOL_HP, 0, 8, 31, 0, mc_asoc_tlv_hpsp),
	SOC_DOUBLE("Headphone Playback Switch",
				MC_ASOC_AVOL_HP, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("Speaker Playback Volume",
				MC_ASOC_AVOL_SP, 0, 8, 31, 0, mc_asoc_tlv_hpsp),
	SOC_DOUBLE("Speaker Playback Switch",
				MC_ASOC_AVOL_SP, 7, 15, 1, 0),

	SOC_SINGLE_TLV("Receiver Playback Volume",
				MC_ASOC_AVOL_RC, 0, 31, 0, mc_asoc_tlv_hpsp),
	SOC_SINGLE("Receiver Playback Switch",
				MC_ASOC_AVOL_RC, 7, 1, 0),

	SOC_DOUBLE_TLV("Line 1 Playback Volume",
				MC_ASOC_AVOL_LOUT1, 0, 8, 31, 0, mc_asoc_tlv_aout),
	SOC_DOUBLE("Line 1 Playback Switch",
				MC_ASOC_AVOL_LOUT1, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("Line 2 Playback Volume",
				MC_ASOC_AVOL_LOUT2, 0, 8, 31, 0, mc_asoc_tlv_aout),
	SOC_DOUBLE("Line 2 Playback Switch",
				MC_ASOC_AVOL_LOUT2, 7, 15, 1, 0),

	SOC_DOUBLE_TLV("AnalogIn Playback Analog Volume",
				MC_ASOC_AVOL_AIN, 0, 8, 31, 0, mc_asoc_tlv_adc),
	SOC_DOUBLE("AnalogIn Playback Analog Switch",
				MC_ASOC_AVOL_AIN, 7, 15, 1, 0),


	SOC_SINGLE_TLV("Mic 1 Gain Volume",
				MC_ASOC_AVOL_MIC1_GAIN, 0, 3, 0, mc_asoc_tlv_micgain),

	SOC_SINGLE_TLV("Mic 2 Gain Volume",
				MC_ASOC_AVOL_MIC2_GAIN, 0, 3, 0, mc_asoc_tlv_micgain),

	SOC_SINGLE_TLV("Mic 3 Gain Volume",
				MC_ASOC_AVOL_MIC3_GAIN, 0, 3, 0, mc_asoc_tlv_micgain),

	SOC_SINGLE_TLV("HP Gain Playback Volume",
				MC_ASOC_AVOL_HP_GAIN, 0, 3, 0, mc_asoc_tlv_hpgain),

	SOC_SINGLE("Voice Recording Switch",
				MC_ASOC_VOICE_RECORDING, 0, 1, 0),
};

/*
 * Same as snd_soc_add_controls supported in alsa-driver 1.0.19 or later.
 * This function is implimented for compatibility with linux 2.6.29.
 */
static int mc_asoc_add_controls(
	struct snd_soc_codec *codec,
	const struct snd_kcontrol_new *controls,
	int n)
{
	int		err, i;
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
#else
	struct snd_soc_card			*soc_card		= NULL;
#endif
	struct snd_card				*card			= NULL;
	struct soc_mixer_control	*mixer_control	= NULL;

	if(codec != NULL) {
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
		card	= codec->card;
#else
		soc_card	= codec->card;
		if(soc_card != NULL) {
			card	= soc_card->snd_card;
		}
#endif
	}
	if(controls != NULL) {
		mixer_control	=  (struct soc_mixer_control *)controls->private_value;
	}
	if((card == NULL) || (mixer_control == NULL)) {
		return -EINVAL;
	}

	/* mc_asoc not use mixer control */
	if (mc_asoc_hwid == MC_ASOC_MC3_HW_ID) {
		for (i	= 0; i < n; i++, controls++, mixer_control++) {
			if ((mixer_control->reg != MC_ASOC_DVOL_AD1) &&
				(mixer_control->reg != MC_ASOC_DVOL_AD1_ATT) &&
				(mixer_control->reg != MC_ASOC_AVOL_AD1) &&
				(mixer_control->reg != MC_ASOC_AVOL_LIN1_BYPASS) &&
				(mixer_control->reg != MC_ASOC_DVOL_AD1_MIX2)
				) {
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON) || defined(KERNEL_2_6_35_OMAP)
				if ((err = snd_ctl_add(card, snd_soc_cnew(controls, codec, NULL))) < 0) {
					return err;
				}
#else
				if ((err = snd_ctl_add((struct snd_card *)codec->card->snd_card,
							snd_soc_cnew(controls, codec, NULL, NULL))) < 0) {
					return err;
				}
#endif
			}
		}
	}
	else if (mc_asoc_hwid == MC_ASOC_MC1_HW_ID || mc_asoc_hwid == MC_ASOC_MA1_HW_ID) {
		for (i	= 0; i < n; i++, controls++, mixer_control++) {
			if ((mixer_control->reg != MC_ASOC_DVOL_AD1) &&
				(mixer_control->reg != MC_ASOC_DVOL_AD1_ATT) &&
				(mixer_control->reg != MC_ASOC_DVOL_DTMF_B) &&
				(mixer_control->reg != MC_ASOC_DVOL_DTMF) &&
				(mixer_control->reg != MC_ASOC_AVOL_AD1) &&
				(mixer_control->reg != MC_ASOC_AVOL_LIN2_BYPASS) &&
				(mixer_control->reg != MC_ASOC_DVOL_DIT2_1) &&
				(mixer_control->reg != MC_ASOC_DVOL_AD0_MIX2) &&
				(mixer_control->reg != MC_ASOC_DVOL_AD1_MIX2) &&
				(mixer_control->reg != MC_ASOC_DVOL_DIR0_MIX2) &&
				(mixer_control->reg != MC_ASOC_DVOL_DIR1_MIX2) &&
				(mixer_control->reg != MC_ASOC_DVOL_DIR2_MIX2)
				) {
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON) || defined(KERNEL_2_6_35_OMAP)
				if ((err = snd_ctl_add(card, snd_soc_cnew(controls, codec, NULL))) < 0) {
					return err;
				}
#else
				if ((err = snd_ctl_add((struct snd_card *)codec->card->snd_card,
							snd_soc_cnew(controls, codec, NULL, NULL))) < 0) {
					return err;
				}
#endif
			}
		}
	}
	else {
		for (i	= 0; i < n; i++, controls++) {
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON) || defined(KERNEL_2_6_35_OMAP)
			if ((err = snd_ctl_add(card, snd_soc_cnew(controls, codec, NULL))) < 0) {
				return err;
			}
#else
			if ((err = snd_ctl_add((struct snd_card *)codec->card->snd_card,
						snd_soc_cnew(controls, codec, NULL, NULL))) < 0) {
				return err;
			}
#endif
		}
	}
	return 0;
}

/*
 * dapm_mixer_set
 */
/* Audio Mode */
static const char	*audio_mode_param_text[] = {
	"off", "audio", "incall", "audio+incall", "incommunication"
};
static const struct soc_enum	audio_mode_play_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_AUDIO_MODE_PLAY, 0,
		ARRAY_SIZE(audio_mode_param_text), audio_mode_param_text);
static const struct snd_kcontrol_new	audio_mode_play_param_mux = 
	SOC_DAPM_ENUM("Audio Mode Playback", audio_mode_play_param_enum);
static const struct soc_enum	audio_mode_cap_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_AUDIO_MODE_CAP, 0,
		ARRAY_SIZE(audio_mode_param_text), audio_mode_param_text);
static const struct snd_kcontrol_new	audio_mode_cap_param_mux = 
	SOC_DAPM_ENUM("Audio Mode Capture", audio_mode_cap_param_enum);

/* Output Path */
static const char	*output_path_param_text[] = {
	"SP", "RC", "HP", "HS", "LO1", "LO2", "BT",
	"SP+RC", "SP+HP", "SP+LO1", "SP+LO2", "SP+BT"
};
static const struct soc_enum	output_path_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_OUTPUT_PATH, 0,
		ARRAY_SIZE(output_path_param_text), output_path_param_text);
static const struct snd_kcontrol_new	output_path_param_mux = 
	SOC_DAPM_ENUM("Output Path", output_path_param_enum);

/* Input Path */
static const char	*input_path_param_text[] = {
	"MainMIC", "SubMIC", "2MIC", "Headset", "Bluetooth",
	"VoiceCall", "VoiceUplink", "VoiceDownlink", "Linein1", "Linein2"
};
static const struct soc_enum	input_path_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_INPUT_PATH, 0,
		ARRAY_SIZE(input_path_param_text), input_path_param_text);
static const struct snd_kcontrol_new	input_path_param_mux = 
	SOC_DAPM_ENUM("Input Path", input_path_param_enum);

/* Incall Mic */
static const char	*incall_mic_param_text[] = {
	"MainMIC", "SubMIC", "2MIC"
};
static const struct soc_enum	incall_mic_sp_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_INCALL_MIC_SP, 0, ARRAY_SIZE(incall_mic_param_text), incall_mic_param_text);
static const struct snd_kcontrol_new	incall_mic_sp_param_mux = 
	SOC_DAPM_ENUM("Incall Mic Speaker", incall_mic_sp_param_enum);
static const struct soc_enum	incall_mic_rc_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_INCALL_MIC_RC, 0, ARRAY_SIZE(incall_mic_param_text), incall_mic_param_text);
static const struct snd_kcontrol_new	incall_mic_rc_param_mux = 
	SOC_DAPM_ENUM("Incall Mic Receiver", incall_mic_rc_param_enum);
static const struct soc_enum	incall_mic_hp_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_INCALL_MIC_HP, 0, ARRAY_SIZE(incall_mic_param_text), incall_mic_param_text);
static const struct snd_kcontrol_new	incall_mic_hp_param_mux = 
	SOC_DAPM_ENUM("Incall Mic Headphone", incall_mic_hp_param_enum);
static const struct soc_enum	incall_mic_lo1_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_INCALL_MIC_LO1, 0, ARRAY_SIZE(incall_mic_param_text), incall_mic_param_text);
static const struct snd_kcontrol_new	incall_mic_lo1_param_mux = 
	SOC_DAPM_ENUM("Incall Mic LineOut1", incall_mic_lo1_param_enum);
static const struct soc_enum	incall_mic_lo2_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_INCALL_MIC_LO2, 0, ARRAY_SIZE(incall_mic_param_text), incall_mic_param_text);
static const struct snd_kcontrol_new	incall_mic_lo2_param_mux = 
	SOC_DAPM_ENUM("Incall Mic LineOut2", incall_mic_lo2_param_enum);

/* Playback Path */
static const struct snd_kcontrol_new	mainmic_playback_path_sw = 
	SOC_DAPM_SINGLE("Switch", MC_ASOC_MAINMIC_PLAYBACK_PATH, 0, 1, 0);
static const struct snd_kcontrol_new	submic_playback_path_sw = 
	SOC_DAPM_SINGLE("Switch", MC_ASOC_SUBMIC_PLAYBACK_PATH, 0, 1, 0);
static const struct snd_kcontrol_new	msmic_playback_path_sw = 
	SOC_DAPM_SINGLE("Switch", MC_ASOC_2MIC_PLAYBACK_PATH, 0, 1, 0);
static const struct snd_kcontrol_new	hsmic_playback_path_sw = 
	SOC_DAPM_SINGLE("Switch", MC_ASOC_HSMIC_PLAYBACK_PATH, 0, 1, 0);
static const struct snd_kcontrol_new	btmic_playback_path_sw = 
	SOC_DAPM_SINGLE("Switch", MC_ASOC_BTMIC_PLAYBACK_PATH, 0, 1, 0);
static const struct snd_kcontrol_new	lin1_playback_path_sw = 
	SOC_DAPM_SINGLE("Switch", MC_ASOC_LIN1_PLAYBACK_PATH, 0, 1, 0);
static const struct snd_kcontrol_new	lin2_playback_path_sw = 
	SOC_DAPM_SINGLE("Switch", MC_ASOC_LIN2_PLAYBACK_PATH, 0, 1, 0);

/* Parameter Setting */
static const char	*parameter_setting_param_text[] = {
	"DUMMY"
};
static const struct soc_enum	parameter_setting_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_PARAMETER_SETTING, 0,
		ARRAY_SIZE(parameter_setting_param_text), parameter_setting_param_text);
static const struct snd_kcontrol_new	parameter_setting_param_mux = 
	SOC_DAPM_ENUM("Parameter Setting", parameter_setting_param_enum);

/* DTMF Control */
static const struct snd_kcontrol_new	dtmf_control_sw = 
	SOC_DAPM_SINGLE("Switch", MC_ASOC_DTMF_CONTROL, 0, 1, 0);

/* DTMF Output */
static const char	*dtmf_output_param_text[] = {
	"SP", "NORMAL"
};
static const struct soc_enum	dtmf_output_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_DTMF_OUTPUT, 0,
		ARRAY_SIZE(dtmf_output_param_text), dtmf_output_param_text);
static const struct snd_kcontrol_new	dtmf_output_param_mux = 
	SOC_DAPM_ENUM("DTMF Output", dtmf_output_param_enum);

#ifdef MC_ASOC_TEST
static const char	*mic_param_text[] = {
	"NONE", "MIC1", "MIC2", "MIC3", "PDM"
};

/*	Main Mic	*/
static const struct soc_enum	main_mic_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_MAIN_MIC, 0, ARRAY_SIZE(mic_param_text), mic_param_text);
static const struct snd_kcontrol_new	main_mic_param_mux = 
	SOC_DAPM_ENUM("Main Mic", main_mic_param_enum);
/*	Sub Mic	*/
static const struct soc_enum	sub_mic_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_SUB_MIC, 0, ARRAY_SIZE(mic_param_text), mic_param_text);
static const struct snd_kcontrol_new	sub_mic_param_mux = 
	SOC_DAPM_ENUM("Sub Mic", sub_mic_param_enum);
/*	Headset Mic	*/
static const struct soc_enum	hs_mic_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_HS_MIC, 0, ARRAY_SIZE(mic_param_text)-1, mic_param_text);
static const struct snd_kcontrol_new	hs_mic_param_mux = 
	SOC_DAPM_ENUM("Headset Mic", hs_mic_param_enum);

/*	MICx_BIAS	*/
static const char	*mic_bias_param_text[] = {
	"OFF", "ALWAYS_ON", "SYNC_MIC"
};
static const struct soc_enum	mic1_bias_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_MIC1_BIAS, 0, ARRAY_SIZE(mic_bias_param_text), mic_bias_param_text);
static const struct snd_kcontrol_new	mic1_bias_param_mux = 
	SOC_DAPM_ENUM("MIC1 BIAS", mic1_bias_param_enum);
static const struct soc_enum	mic2_bias_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_MIC2_BIAS, 0, ARRAY_SIZE(mic_bias_param_text), mic_bias_param_text);
static const struct snd_kcontrol_new	mic2_bias_param_mux = 
	SOC_DAPM_ENUM("MIC2 BIAS", mic2_bias_param_enum);
static const struct soc_enum	mic3_bias_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_MIC3_BIAS, 0, ARRAY_SIZE(mic_bias_param_text), mic_bias_param_text);
static const struct snd_kcontrol_new	mic3_bias_param_mux = 
	SOC_DAPM_ENUM("MIC3 BIAS", mic3_bias_param_enum);

/*	CAP_PATH_8KHZ	*/
static const char	*cap_path_8khz_param_text[] = {
	"DIO_DEF", "DIO_PORT0", "DIO_PORT1", "DIO_PORT2"
};
static const struct soc_enum	cap_path_8khz_param_enum =
	SOC_ENUM_SINGLE(MC_ASOC_CAP_PATH_8KHZ, 0, ARRAY_SIZE(cap_path_8khz_param_text), cap_path_8khz_param_text);
static const struct snd_kcontrol_new	cap_path_8khz_param_mux = 
	SOC_DAPM_ENUM("CAP PATH 8KHZ", cap_path_8khz_param_enum);
#endif

static const struct snd_soc_dapm_widget	mc_asoc_widgets_common[] = {
	SND_SOC_DAPM_INPUT("DTMF"),

	/* DACs */
	SND_SOC_DAPM_DAC("DAC DUMMY", "DAC Playback", SND_SOC_NOPM, 0, 0),
	/* ADCs */
	SND_SOC_DAPM_ADC("ADC DUMMY", "ADC Capture", SND_SOC_NOPM, 0, 0),

	SND_SOC_DAPM_INPUT("INPUT DUMMY"),
	SND_SOC_DAPM_OUTPUT("OUTPUT DUMMY"),

	/* Audio Mode */
	SND_SOC_DAPM_INPUT("OFF"),
	SND_SOC_DAPM_INPUT("AUDIO"),
	SND_SOC_DAPM_INPUT("INCALL"),
	SND_SOC_DAPM_INPUT("AUDIO+INCALL"),
	SND_SOC_DAPM_INPUT("INCOMMUNICATION"),
	SND_SOC_DAPM_MUX("Audio Mode Playback", SND_SOC_NOPM, 0, 0, &audio_mode_play_param_mux),
	SND_SOC_DAPM_MUX("Audio Mode Capture", SND_SOC_NOPM, 0, 0, &audio_mode_cap_param_mux),

	/* Output Path */
	SND_SOC_DAPM_INPUT("SP"),
	SND_SOC_DAPM_INPUT("RC"),
	SND_SOC_DAPM_INPUT("HP"),
	SND_SOC_DAPM_INPUT("HS"),
	SND_SOC_DAPM_INPUT("LO1"),
	SND_SOC_DAPM_INPUT("LO2"),
	SND_SOC_DAPM_INPUT("BT"),
	SND_SOC_DAPM_INPUT("SP+RC"),
	SND_SOC_DAPM_INPUT("SP+HP"),
	SND_SOC_DAPM_INPUT("SP+LO1"),
	SND_SOC_DAPM_INPUT("SP+LO2"),
	SND_SOC_DAPM_INPUT("SP+BT"),
	SND_SOC_DAPM_MUX("Output Path", SND_SOC_NOPM, 0, 0, &output_path_param_mux),

	/* Input Path */
	SND_SOC_DAPM_INPUT("MainMIC"),
	SND_SOC_DAPM_INPUT("SubMIC"),
	SND_SOC_DAPM_INPUT("2MIC"),
	SND_SOC_DAPM_INPUT("Headset"),
	SND_SOC_DAPM_INPUT("Bluetooth"),
	SND_SOC_DAPM_INPUT("VoiceCall"),
	SND_SOC_DAPM_INPUT("VoiceUplink"),
	SND_SOC_DAPM_INPUT("VoiceDownlink"),
	SND_SOC_DAPM_INPUT("Linein1"),
	SND_SOC_DAPM_INPUT("Linein2"),
	SND_SOC_DAPM_MUX("Input Path", SND_SOC_NOPM, 0, 0, &input_path_param_mux),

	/* Incall Mic */
	SND_SOC_DAPM_INPUT("MainMIC"),
	SND_SOC_DAPM_INPUT("SubMIC"),
	SND_SOC_DAPM_INPUT("2MIC"),
	SND_SOC_DAPM_MUX("Incall Mic Speaker", SND_SOC_NOPM, 0, 0, &incall_mic_sp_param_mux),
	SND_SOC_DAPM_MUX("Incall Mic Receiver", SND_SOC_NOPM, 0, 0, &incall_mic_rc_param_mux),
	SND_SOC_DAPM_MUX("Incall Mic Headphone", SND_SOC_NOPM, 0, 0, &incall_mic_hp_param_mux),
	SND_SOC_DAPM_MUX("Incall Mic LineOut1", SND_SOC_NOPM, 0, 0, &incall_mic_lo1_param_mux),
	SND_SOC_DAPM_MUX("Incall Mic LineOut2", SND_SOC_NOPM, 0, 0, &incall_mic_lo2_param_mux),

	/* Playback Path */
	SND_SOC_DAPM_INPUT("ONOFF"),
	SND_SOC_DAPM_INPUT("MainMIC Playback Switch"),
	SND_SOC_DAPM_SWITCH("MainMIC Playback Path", SND_SOC_NOPM, 0, 0, &mainmic_playback_path_sw),
	SND_SOC_DAPM_INPUT("SubMIC Playback Switch"),
	SND_SOC_DAPM_SWITCH("SubMIC Playback Path", SND_SOC_NOPM, 0, 0, &submic_playback_path_sw),
	SND_SOC_DAPM_INPUT("2MIC Playback Switch"),
	SND_SOC_DAPM_SWITCH("2MIC Playback Path", SND_SOC_NOPM, 0, 0, &msmic_playback_path_sw),
	SND_SOC_DAPM_INPUT("HeadsetMIC Playback Switch"),
	SND_SOC_DAPM_SWITCH("HeadsetMIC Playback Path", SND_SOC_NOPM, 0, 0, &hsmic_playback_path_sw),
	SND_SOC_DAPM_INPUT("BluetoothMIC Playback Switch"),
	SND_SOC_DAPM_SWITCH("BluetoothMIC Playback Path", SND_SOC_NOPM, 0, 0, &btmic_playback_path_sw),

	/* Parameter Setting */
	SND_SOC_DAPM_INPUT("DUMMY"),
	SND_SOC_DAPM_MUX("Parameter Setting",
		SND_SOC_NOPM, 0, 0, &parameter_setting_param_mux),
};

static const struct snd_soc_dapm_widget	mc_asoc_widgets_LIN1[] = {
	/* LIN 1 Playback Path */
	SND_SOC_DAPM_SWITCH("LIN 1 Playback Path", SND_SOC_NOPM, 0, 0, &lin1_playback_path_sw)
};

static const struct snd_soc_dapm_widget	mc_asoc_widgets_LIN2[] = {
	/* LIN 2 Playback Path */
	SND_SOC_DAPM_SWITCH("LIN 2 Playback Path", SND_SOC_NOPM, 0, 0, &lin2_playback_path_sw)
};

static const struct snd_soc_dapm_widget	mc_asoc_widgets_DTMF[] = {
	/* DTMF Control */
	SND_SOC_DAPM_INPUT("DTMF Control Switch"),
	SND_SOC_DAPM_SWITCH("DTMF Control", SND_SOC_NOPM, 0, 0, &dtmf_control_sw),

	/* DTMF Output */
	SND_SOC_DAPM_INPUT("SP"),
	SND_SOC_DAPM_INPUT("NORMAL"),
	SND_SOC_DAPM_MUX("DTMF Output", SND_SOC_NOPM, 0, 0, &dtmf_output_param_mux)
};

static const struct snd_soc_dapm_widget	mc_asoc_widgets_Headset[] = {
	SND_SOC_DAPM_OUTPUT("HPOUTL"),
	SND_SOC_DAPM_OUTPUT("HPOUTR"),
	SND_SOC_DAPM_INPUT("AMIC1"),
	/* Headset Control */
	SND_SOC_DAPM_MIC("Mic Jack", NULL),
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
};

#ifdef MC_ASOC_TEST
static const struct snd_soc_dapm_widget	mc_asoc_widgets_test[] = {
	SND_SOC_DAPM_INPUT("NONE"),
	SND_SOC_DAPM_INPUT("MIC1"),
	SND_SOC_DAPM_INPUT("MIC2"),
	SND_SOC_DAPM_INPUT("MIC3"),
	SND_SOC_DAPM_INPUT("PDM"),
	/*	Main Mic	*/
	SND_SOC_DAPM_MUX("Main Mic", SND_SOC_NOPM, 0, 0, &main_mic_param_mux),
	/*	Sub Mic	*/
	SND_SOC_DAPM_MUX("Sub Mic", SND_SOC_NOPM, 0, 0, &sub_mic_param_mux),
	/*	Headset Mic	*/
	SND_SOC_DAPM_MUX("Headset Mic", SND_SOC_NOPM, 0, 0, &hs_mic_param_mux),

	/*	MICx BIAS	*/
	SND_SOC_DAPM_INPUT("OFF"),
	SND_SOC_DAPM_INPUT("ALWAYS_ON"),
	SND_SOC_DAPM_INPUT("SYNC_MIC"),
	SND_SOC_DAPM_MUX("MIC1 BIAS", SND_SOC_NOPM, 0, 0, &mic1_bias_param_mux),
	SND_SOC_DAPM_MUX("MIC2 BIAS", SND_SOC_NOPM, 0, 0, &mic2_bias_param_mux),
	SND_SOC_DAPM_MUX("MIC3 BIAS", SND_SOC_NOPM, 0, 0, &mic3_bias_param_mux),

	/*	CAP PATH 8KHZ	*/
	SND_SOC_DAPM_INPUT("DIO_DEF"),
	SND_SOC_DAPM_INPUT("DIO_PORT0"),
	SND_SOC_DAPM_INPUT("DIO_PORT1"),
	SND_SOC_DAPM_INPUT("DIO_PORT2"),
	SND_SOC_DAPM_MUX("CAP PATH 8KHZ", SND_SOC_NOPM, 0, 0, &cap_path_8khz_param_mux),
};
#endif

static const struct snd_soc_dapm_route	mc_asoc_intercon_common[] = {
	{"OUTPUT DUMMY",		NULL,				"DAC DUMMY"},
	{"ADC DUMMY",			NULL,				"INPUT DUMMY"},

	{"Audio Mode Playback",	"off",				"OFF"},
	{"Audio Mode Playback",	"audio",			"AUDIO"},
	{"Audio Mode Playback",	"incall",			"INCALL"},
	{"Audio Mode Playback",	"audio+incall",		"AUDIO+INCALL"},
	{"Audio Mode Playback",	"incommunication",	"INCOMMUNICATION"},
	{"Audio Mode Capture",	"off",				"OFF"},
	{"Audio Mode Capture",	"audio",			"AUDIO"},
	{"Audio Mode Capture",	"incall",			"INCALL"},
	{"Audio Mode Capture",	"audio+incall",		"AUDIO+INCALL"},
	{"Audio Mode Capture",	"incommunication",	"INCOMMUNICATION"},

	{"Output Path",			"SP",				"SP"},
	{"Output Path",			"RC",				"RC"},
	{"Output Path",			"HP",				"HP"},
	{"Output Path",			"HS",				"HS"},
	{"Output Path",			"LO1",				"LO1"},
	{"Output Path",			"LO2",				"LO2"},
	{"Output Path",			"BT",				"BT"},
	{"Output Path",			"SP+RC",			"SP+RC"},
	{"Output Path",			"SP+HP",			"SP+HP"},
	{"Output Path",			"SP+LO1",			"SP+LO1"},
	{"Output Path",			"SP+LO2",			"SP+LO2"},
	{"Output Path",			"SP+BT",			"SP+BT"},

	{"Input Path",			"MainMIC",			"MainMIC"},
	{"Input Path",			"SubMIC",			"SubMIC"},
	{"Input Path",			"2MIC",				"2MIC"},
	{"Input Path",			"Headset",			"Headset"},
	{"Input Path",			"Bluetooth",		"Bluetooth"},
	{"Input Path",			"VoiceCall",		"VoiceCall"},
	{"Input Path",			"VoiceUplink",		"VoiceUplink"},
	{"Input Path",			"VoiceDownlink",	"VoiceDownlink"},
	{"Input Path",			"Linein1",			"Linein1"},
	{"Input Path",			"Linein2",			"Linein2"},

	{"Incall Mic Speaker",	"MainMIC",			"MainMIC"},
	{"Incall Mic Speaker",	"SubMIC",			"SubMIC"},
	{"Incall Mic Speaker",	"2MIC",				"2MIC"},
	{"Incall Mic Receiver",	"MainMIC",			"MainMIC"},
	{"Incall Mic Receiver",	"SubMIC",			"SubMIC"},
	{"Incall Mic Receiver",	"2MIC",				"2MIC"},
	{"Incall Mic Headphone","MainMIC",			"MainMIC"},
	{"Incall Mic Headphone","SubMIC",			"SubMIC"},
	{"Incall Mic Headphone","2MIC",				"2MIC"},
	{"Incall Mic LineOut1",	"MainMIC",			"MainMIC"},
	{"Incall Mic LineOut1",	"SubMIC",			"SubMIC"},
	{"Incall Mic LineOut1",	"2MIC",				"2MIC"},
	{"Incall Mic LineOut2",	"MainMIC",			"MainMIC"},
	{"Incall Mic LineOut2",	"SubMIC",			"SubMIC"},
	{"Incall Mic LineOut2",	"2MIC",				"2MIC"},

	{"MainMIC Playback Path",		"Switch",	"ONOFF"},
	{"MainMIC Playback Switch",		NULL,		"MainMIC Playback Path"},
	{"SubMIC Playback Path",		"Switch",	"ONOFF"},
	{"SubMIC Playback Switch",		NULL,		"SubMIC Playback Path"},
	{"2MIC Playback Path",			"Switch",	"ONOFF"},
	{"2MIC Playback Switch",		NULL,		"2MIC Playback Path"},
	{"HeadsetMIC Playback Path",	"Switch",	"ONOFF"},
	{"HeadsetMIC Playback Switch",	NULL,		"HeadsetMIC Playback Path"},
	{"BluetoothMIC Playback Path",	"Switch",	"ONOFF"},
	{"BluetoothMIC Playback Switch",NULL,		"BluetoothMIC Playback Path"},

	{"Parameter Setting",	"DUMMY",			"DUMMY"},
};
static const struct snd_soc_dapm_route	mc_asoc_intercon_LIN1[] = {
	{"LIN 1 Playback Path",	"Switch",			"Linein1"},
};
static const struct snd_soc_dapm_route	mc_asoc_intercon_LIN2[] = {
	{"LIN 2 Playback Path",	"Switch",			"Linein2"},
};
static const struct snd_soc_dapm_route	mc_asoc_intercon_DTMF[] = {
	{"DTMF Control",		"Switch",			"DTMF"},

	{"DTMF Output",			"SP",				"SP"},
	{"DTMF Output",			"NORMAL",			"NORMAL"}
};

static const struct snd_soc_dapm_route	mc_asoc_intercon_Headset[] = {
	{"Headphone Jack",		NULL,				"HPOUTL"},
	{"Headphone Jack",		NULL,				"HPOUTR"},
	{"Mic Jack",			NULL,				"AMIC1"},
};

#ifdef MC_ASOC_TEST
static const struct snd_soc_dapm_route	mc_asoc_intercon_test[] = {
	{"Main Mic",			"NONE",				"NONE"},
	{"Main Mic",			"MIC1",				"MIC1"},
	{"Main Mic",			"MIC2",				"MIC2"},
	{"Main Mic",			"MIC3",				"MIC3"},
	{"Main Mic",			"PDM",				"PDM"},
	{"Sub Mic",				"NONE",				"NONE"},
	{"Sub Mic",				"MIC1",				"MIC1"},
	{"Sub Mic",				"MIC2",				"MIC2"},
	{"Sub Mic",				"MIC3",				"MIC3"},
	{"Sub Mic",				"PDM",				"PDM"},
	{"Headset Mic",			"NONE",				"NONE"},
	{"Headset Mic",			"MIC1",				"MIC1"},
	{"Headset Mic",			"MIC2",				"MIC2"},
	{"Headset Mic",			"MIC3",				"MIC3"},
	{"MIC1 BIAS",			"OFF",				"OFF"},
	{"MIC1 BIAS",			"ALWAYS_ON",		"ALWAYS_ON"},
	{"MIC1 BIAS",			"SYNC_MIC",			"SYNC_MIC"},
	{"MIC2 BIAS",			"OFF",				"OFF"},
	{"MIC2 BIAS",			"ALWAYS_ON",		"ALWAYS_ON"},
	{"MIC2 BIAS",			"SYNC_MIC",			"SYNC_MIC"},
	{"MIC3 BIAS",			"OFF",				"OFF"},
	{"MIC3 BIAS",			"ALWAYS_ON",		"ALWAYS_ON"},
	{"MIC3 BIAS",			"SYNC_MIC",			"SYNC_MIC"},
	{"CAP PATH 8KHZ",		"DIO_DEF",			"DIO_DEF"},
	{"CAP PATH 8KHZ",		"DIO_PORT0",		"DIO_PORT0"},
	{"CAP PATH 8KHZ",		"DIO_PORT1",		"DIO_PORT1"},
	{"CAP PATH 8KHZ",		"DIO_PORT2",		"DIO_PORT2"},
};
#endif

static int mc_asoc_add_widgets(struct snd_soc_codec *codec)
{
	int		err;

	if(codec == NULL) {
		return -EINVAL;
	}

#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	if((err = snd_soc_dapm_new_controls(codec, mc_asoc_widgets_common, ARRAY_SIZE(mc_asoc_widgets_common))) < 0) {
		return err;
	}
	if((err = snd_soc_dapm_add_routes(codec, mc_asoc_intercon_common, ARRAY_SIZE(mc_asoc_intercon_common))) < 0) {
		return err;
	}

	if (mc_asoc_hwid == MC_ASOC_MC3_HW_ID) {
		if((err = snd_soc_dapm_new_controls(codec, mc_asoc_widgets_LIN2, ARRAY_SIZE(mc_asoc_widgets_LIN2))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_add_routes(codec, mc_asoc_intercon_LIN2, ARRAY_SIZE(mc_asoc_intercon_LIN2))) < 0) {
			return err;
		}

		if((err = snd_soc_dapm_new_controls(codec, mc_asoc_widgets_DTMF, ARRAY_SIZE(mc_asoc_widgets_DTMF))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_add_routes(codec, mc_asoc_intercon_DTMF, ARRAY_SIZE(mc_asoc_intercon_DTMF))) < 0) {
			return err;
		}
	}
	else if (mc_asoc_hwid == MC_ASOC_MC1_HW_ID || mc_asoc_hwid == MC_ASOC_MA1_HW_ID) {
		if((err = snd_soc_dapm_new_controls(codec, mc_asoc_widgets_LIN1, ARRAY_SIZE(mc_asoc_widgets_LIN1))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_add_routes(codec, mc_asoc_intercon_LIN1, ARRAY_SIZE(mc_asoc_intercon_LIN1))) < 0) {
			return err;
		}
	}
	else {
		if((err = snd_soc_dapm_new_controls(codec, mc_asoc_widgets_LIN1, ARRAY_SIZE(mc_asoc_widgets_LIN1))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_add_routes(codec, mc_asoc_intercon_LIN1, ARRAY_SIZE(mc_asoc_intercon_LIN1))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_new_controls(codec, mc_asoc_widgets_LIN2, ARRAY_SIZE(mc_asoc_widgets_LIN2))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_add_routes(codec, mc_asoc_intercon_LIN2, ARRAY_SIZE(mc_asoc_intercon_LIN2))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_new_controls(codec, mc_asoc_widgets_DTMF, ARRAY_SIZE(mc_asoc_widgets_DTMF))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_add_routes(codec, mc_asoc_intercon_DTMF, ARRAY_SIZE(mc_asoc_intercon_DTMF))) < 0) {
			return err;
		}
	}
	if((mc_asoc_hwid_a == MCDRV_HWID_15H)
	|| (mc_asoc_hwid_a == MCDRV_HWID_16H)) {
		if((err = snd_soc_dapm_new_controls(codec, mc_asoc_widgets_Headset, ARRAY_SIZE(mc_asoc_widgets_Headset))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_add_routes(codec, mc_asoc_intercon_Headset, ARRAY_SIZE(mc_asoc_intercon_Headset))) < 0) {
			return err;
		}
	}


  #ifdef MC_ASOC_TEST
	if((err = snd_soc_dapm_new_controls(codec, mc_asoc_widgets_test, ARRAY_SIZE(mc_asoc_widgets_test))) < 0) {
		return err;
	}
	if((err = snd_soc_dapm_add_routes(codec, mc_asoc_intercon_test, ARRAY_SIZE(mc_asoc_intercon_test))) < 0) {
		return err;
	}
  #endif

	if((err = snd_soc_dapm_new_widgets(codec)) < 0) {
		return err;
	}

#elif defined(KERNEL_2_6_35_OMAP)
	if((err = snd_soc_dapm_new_controls(codec->dapm, mc_asoc_widgets_common, ARRAY_SIZE(mc_asoc_widgets_common))) < 0) {
		return err;
	}
	if((err = snd_soc_dapm_add_routes(codec->dapm, mc_asoc_intercon_common, ARRAY_SIZE(mc_asoc_intercon_common))) < 0) {
		return err;
	}
	if (mc_asoc_hwid == MC_ASOC_MC3_HW_ID) {
		if((err = snd_soc_dapm_new_controls(codec->dapm, mc_asoc_widgets_LIN2, ARRAY_SIZE(mc_asoc_widgets_LIN2))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_add_routes(codec->dapm, mc_asoc_intercon_LIN2, ARRAY_SIZE(mc_asoc_intercon_LIN2))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_new_controls(codec->dapm, mc_asoc_widgets_DTMF, ARRAY_SIZE(mc_asoc_widgets_DTMF))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_add_routes(codec->dapm, mc_asoc_intercon_DTMF, ARRAY_SIZE(mc_asoc_intercon_DTMF))) < 0) {
			return err;
		}
	}
	else if (mc_asoc_hwid == MC_ASOC_MC1_HW_ID || mc_asoc_hwid == MC_ASOC_MA1_HW_ID) {
		if((err = snd_soc_dapm_new_controls(codec->dapm, mc_asoc_widgets_LIN1, ARRAY_SIZE(mc_asoc_widgets_LIN1))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_add_routes(codec->dapm, mc_asoc_intercon_LIN1, ARRAY_SIZE(mc_asoc_intercon_LIN1))) < 0) {
			return err;
		}
	}
	else {
		if((err = snd_soc_dapm_new_controls(codec->dapm, mc_asoc_widgets_LIN1, ARRAY_SIZE(mc_asoc_widgets_LIN1))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_add_routes(codec->dapm, mc_asoc_intercon_LIN1, ARRAY_SIZE(mc_asoc_intercon_LIN1))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_new_controls(codec->dapm, mc_asoc_widgets_LIN2, ARRAY_SIZE(mc_asoc_widgets_LIN2))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_add_routes(codec->dapm, mc_asoc_intercon_LIN2, ARRAY_SIZE(mc_asoc_intercon_LIN2))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_new_controls(codec->dapm, mc_asoc_widgets_DTMF, ARRAY_SIZE(mc_asoc_widgets_DTMF))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_add_routes(codec->dapm, mc_asoc_intercon_DTMF, ARRAY_SIZE(mc_asoc_intercon_DTMF))) < 0) {
			return err;
		}
	}
	if((mc_asoc_hwid_a == MCDRV_HWID_15H)
	|| (mc_asoc_hwid_a == MCDRV_HWID_16H)) {
		if((err = snd_soc_dapm_new_controls(codec->dapm, mc_asoc_widgets_Headset, ARRAY_SIZE(mc_asoc_widgets_Headset))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_add_routes(codec->dapm, mc_asoc_intercon_Headset, ARRAY_SIZE(mc_asoc_intercon_Headset))) < 0) {
			return err;
		}
	}

  #ifdef MC_ASOC_TEST
	if((err = snd_soc_dapm_new_controls(codec->dapm, mc_asoc_widgets_test, ARRAY_SIZE(mc_asoc_widgets_test))) < 0) {
		return err;
	}
	if((err = snd_soc_dapm_add_routes(codec->dapm, mc_asoc_intercon_test, ARRAY_SIZE(mc_asoc_intercon_test))) < 0) {
		return err;
	}
  #endif

	if((err = snd_soc_dapm_new_widgets(codec->dapm)) < 0) {
		return err;
	}
#else
	if((err = snd_soc_dapm_new_controls(&codec->dapm, mc_asoc_widgets_common, ARRAY_SIZE(mc_asoc_widgets_common))) < 0) {
		return err;
	}
	if((err = snd_soc_dapm_add_routes(&codec->dapm, mc_asoc_intercon_common, ARRAY_SIZE(mc_asoc_intercon_common))) < 0) {
		return err;
	}
	if (mc_asoc_hwid == MC_ASOC_MC3_HW_ID) {
		if((err = snd_soc_dapm_new_controls(&codec->dapm, mc_asoc_widgets_LIN2, ARRAY_SIZE(mc_asoc_widgets_LIN2))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_add_routes(&codec->dapm, mc_asoc_intercon_LIN2, ARRAY_SIZE(mc_asoc_intercon_LIN2))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_new_controls(&codec->dapm, mc_asoc_widgets_DTMF, ARRAY_SIZE(mc_asoc_widgets_DTMF))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_add_routes(&codec->dapm, mc_asoc_intercon_DTMF, ARRAY_SIZE(mc_asoc_intercon_DTMF))) < 0) {
			return err;
		}
	}
	else if (mc_asoc_hwid == MC_ASOC_MC1_HW_ID || mc_asoc_hwid == MC_ASOC_MA1_HW_ID) {
		if((err = snd_soc_dapm_new_controls(&codec->dapm, mc_asoc_widgets_LIN1, ARRAY_SIZE(mc_asoc_widgets_LIN1))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_add_routes(&codec->dapm, mc_asoc_intercon_LIN1, ARRAY_SIZE(mc_asoc_intercon_LIN1))) < 0) {
			return err;
		}
	}
	else {
		if((err = snd_soc_dapm_new_controls(&codec->dapm, mc_asoc_widgets_LIN1, ARRAY_SIZE(mc_asoc_widgets_LIN1))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_add_routes(&codec->dapm, mc_asoc_intercon_LIN1, ARRAY_SIZE(mc_asoc_intercon_LIN1))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_new_controls(&codec->dapm, mc_asoc_widgets_LIN2, ARRAY_SIZE(mc_asoc_widgets_LIN2))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_add_routes(&codec->dapm, mc_asoc_intercon_LIN2, ARRAY_SIZE(mc_asoc_intercon_LIN2))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_new_controls(&codec->dapm, mc_asoc_widgets_DTMF, ARRAY_SIZE(mc_asoc_widgets_DTMF))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_add_routes(&codec->dapm, mc_asoc_intercon_DTMF, ARRAY_SIZE(mc_asoc_intercon_DTMF))) < 0) {
			return err;
		}
	}
	if((mc_asoc_hwid_a == MCDRV_HWID_15H)
	|| (mc_asoc_hwid_a == MCDRV_HWID_16H)) {
		if((err = snd_soc_dapm_new_controls(&codec->dapm, mc_asoc_widgets_Headset, ARRAY_SIZE(mc_asoc_widgets_Headset))) < 0) {
			return err;
		}
		if((err = snd_soc_dapm_add_routes(&codec->dapm, mc_asoc_intercon_Headset, ARRAY_SIZE(mc_asoc_intercon_Headset))) < 0) {
			return err;
		}
	}

  #ifdef MC_ASOC_TEST
	if((err = snd_soc_dapm_new_controls(&codec->dapm, mc_asoc_widgets_test, ARRAY_SIZE(mc_asoc_widgets_test))) < 0) {
		return err;
	}
	if((err = snd_soc_dapm_add_routes(&codec->dapm, mc_asoc_intercon_test, ARRAY_SIZE(mc_asoc_intercon_test))) < 0) {
		return err;
	}
  #endif

	if((err = snd_soc_dapm_new_widgets(&codec->dapm)) < 0) {
		return err;
	}
#endif
	return 0;
}

/*
 * Hwdep interface
 */
static int mc_asoc_hwdep_open(struct snd_hwdep * hw, struct file *file)
{
	/* Nothing to do */
	return 0;
}

static int mc_asoc_hwdep_release(struct snd_hwdep *hw, struct file *file)
{
	/* Nothing to do */
	return 0;
}

static int hwdep_ioctl_read_reg(MCDRV_REG_INFO *args)
{
	int		err;
	MCDRV_REG_INFO	reg_info;

	if (!access_ok(VERIFY_WRITE, args, sizeof(MCDRV_REG_INFO))) {
		return -EFAULT;
	}
	if (copy_from_user(&reg_info, args, sizeof(MCDRV_REG_INFO)) != 0) {
		return -EFAULT;
	}

	err	= _McDrv_Ctrl(MCDRV_READ_REG, &reg_info, 0);
	if (err != MCDRV_SUCCESS) {
		return mc_asoc_map_drv_error(err);
	}
	else {
		if (copy_to_user(args, &reg_info, sizeof(MCDRV_REG_INFO)) != 0) {
			err	= -EFAULT;
		}
	}

	return 0;
}

static int hwdep_ioctl_write_reg(const MCDRV_REG_INFO *args)
{
	int		err;
	MCDRV_REG_INFO	reg_info;

	if (!access_ok(VERIFY_READ, args, sizeof(MCDRV_REG_INFO))) {
		return -EFAULT;
	}

	if (copy_from_user(&reg_info, args, sizeof(MCDRV_REG_INFO)) != 0) {
		return -EFAULT;
	}

	err	= _McDrv_Ctrl(MCDRV_WRITE_REG, &reg_info, 0);
	if (err != MCDRV_SUCCESS) {
		return mc_asoc_map_drv_error(err);
	}
	return 0;
}

static int mc_asoc_hwdep_ioctl(
	struct snd_hwdep *hw,
	struct file *file,
	unsigned int cmd,
	unsigned long arg)
{
	int		err	= 0;
	int		output_path;
	void	*param	= NULL;
	struct snd_soc_codec	*codec		= NULL;
	struct mc_asoc_data		*mc_asoc	= NULL;
	ymc_ctrl_args_t	ymc_ctrl_arg;
	UINT32	hold;
	UINT8	audio_mode;

	if(hw != NULL) {
		codec	= hw->private_data;
	}
	mc_asoc	= mc_asoc_get_mc_asoc(codec);
	if(mc_asoc == NULL) {
		return -EINVAL;
	}

	if((output_path = read_cache(codec, MC_ASOC_OUTPUT_PATH)) < 0) {
		return -EIO;
	}

	switch (cmd) {
	case YMC_IOCTL_SET_CTRL:
		if (!access_ok(VERIFY_READ, (ymc_ctrl_args_t *)arg, sizeof(ymc_ctrl_args_t))) {
			return -EFAULT;
		}
		if (copy_from_user(&ymc_ctrl_arg, (ymc_ctrl_args_t *)arg, sizeof(ymc_ctrl_args_t)) != 0) {
			err	= -EFAULT;
			break;
		}
		if(MAX_YMS_CTRL_PARAM_SIZE < ymc_ctrl_arg.size) {
			return -ENOMEM;
		}
		if (!(param	= kzalloc(ymc_ctrl_arg.size, GFP_KERNEL))) {
			return -ENOMEM;
		}
		if (copy_from_user(param, ymc_ctrl_arg.param, ymc_ctrl_arg.size) != 0) {
			err	= -EFAULT;
			break;
		}
		err	= mc_asoc_parser(mc_asoc,
							 mc_asoc_hwid,
							 param,
							 ymc_ctrl_arg.size,
							 ymc_ctrl_arg.option);
		if(err == 0) {
			if(((ymc_ctrl_arg.option&0xFF00) == YMC_DSP_VOICECALL_BASE_1MIC)
			|| ((ymc_ctrl_arg.option&0xFF00) == YMC_DSP_VOICECALL_BASE_2MIC)) {
				if(mc_asoc_hold != YMC_NOTITY_HOLD_ON) {
					if((err = connect_path(codec)) < 0) {
						break;
					}
				}
			}
			else if((ymc_ctrl_arg.option&0xFF00) != 0) {
				if(mc_asoc_cdsp_ae_ex_flag == MC_ASOC_CDSP_ON) {
					if((err = set_cdsp_param(codec)) < 0) {
						break;
					}
				}
			}
			if(mc_asoc_ae_dir == MC_ASOC_AE_PLAY) {
				if(ae_onoff(mc_asoc->ae_onoff, output_path) == MC_ASOC_AENG_OFF) {
					break;
				}
			}
			else if(mc_asoc_ae_dir == MC_ASOC_AE_CAP) {
				if((mc_asoc->ae_onoff[MC_ASOC_DSP_INPUT_EXT] == MC_ASOC_AENG_OFF)
				|| ((mc_asoc->ae_onoff[MC_ASOC_DSP_INPUT_EXT] == 0x80)
					&& (mc_asoc->ae_onoff[MC_ASOC_DSP_INPUT_BASE] == MC_ASOC_AENG_OFF))) {
					break;
				}
			}
			if((ymc_ctrl_arg.option == 0)
			|| ((ymc_ctrl_arg.option&0xFF) != 0)) {
				if((err = set_audioengine(codec)) < 0) {
					break;
				}
				if((mc_asoc_hwid == MC_ASOC_MC3_HW_ID) || (mc_asoc_hwid == MC_ASOC_MA2_HW_ID)) {
					if(mc_asoc_cdsp_ae_ex_flag != MC_ASOC_CDSP_ON) {
						if((err = set_audioengine_ex(codec)) < 0) {
							break;
						}
					}
				}
			}
		}
		break;

	case YMC_IOCTL_READ_REG:
		err	= hwdep_ioctl_read_reg((MCDRV_REG_INFO*)arg);
		dbg_info("err=%d, RegType=%d, Addr=%d, Data=0x%02X\n",
				err,
				((MCDRV_REG_INFO*)arg)->bRegType,
				((MCDRV_REG_INFO*)arg)->bAddress,
				((MCDRV_REG_INFO*)arg)->bData);
		break;

	case YMC_IOCTL_WRITE_REG:
		err	= hwdep_ioctl_write_reg((MCDRV_REG_INFO*)arg);
		dbg_info("err=%d, RegType=%d, Addr=%d, Data=0x%02X\n",
				err,
				((MCDRV_REG_INFO*)arg)->bRegType,
				((MCDRV_REG_INFO*)arg)->bAddress,
				((MCDRV_REG_INFO*)arg)->bData);
		break;

	case YMC_IOCTL_NOTIFY_HOLD:
		if (!access_ok(VERIFY_READ, (UINT32 *)arg, sizeof(UINT32))) {
			return -EFAULT;
		}
		if (copy_from_user(&hold, (UINT32 *)arg, sizeof(UINT32)) != 0) {
			err	= -EFAULT;
			break;
		}
		dbg_info("hold=%ld\n", hold);
		switch(hold) {
		case	YMC_NOTITY_HOLD_OFF:
			err	= connect_path(codec);
		case	YMC_NOTITY_HOLD_ON:
			mc_asoc_hold	= (UINT8)hold;
			break;
		default:
			err	= -EINVAL;
			break;
		}
		break;

	case YMC_IOCTL_NOTIFY_MODE:
		if (!access_ok(VERIFY_READ, (UINT8 *)arg, sizeof(UINT8))) {
			return -EFAULT;
		}
		if (copy_from_user(&audio_mode, (UINT8 *)arg, sizeof(UINT8)) != 0) {
			err	= -EFAULT;
			break;
		}
		mc_asoc->audio_mode = audio_mode;
            break;
	default:
		err	= -EINVAL;
	}

	if(param != NULL) {
		kfree(param);
	}
	return err;
}

static int mc_asoc_add_hwdep(struct snd_soc_codec *codec)
{
	struct snd_hwdep	*hw;
	struct mc_asoc_data	*mc_asoc	= NULL;
	int		err;

	mc_asoc	= mc_asoc_get_mc_asoc(codec);
	if(mc_asoc == NULL) {
		return -EINVAL;
	}

#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	err	= snd_hwdep_new(codec->card, MC_ASOC_HWDEP_ID, 0, &hw);
#else
	err	= snd_hwdep_new((struct snd_card *)codec->card->snd_card, 
						MC_ASOC_HWDEP_ID, 0, &hw);
#endif
	if (err < 0) {
		return err;
	}

	hw->iface			= SNDRV_HWDEP_IFACE_MC_YAMAHA;
	hw->private_data	= codec;
	hw->ops.open		= mc_asoc_hwdep_open;
	hw->ops.release		= mc_asoc_hwdep_release;
	hw->ops.ioctl		= mc_asoc_hwdep_ioctl;
	hw->exclusive		= 1;
	strcpy(hw->name, MC_ASOC_HWDEP_ID);
	mc_asoc->hwdep		= hw;

	return 0;
}

/*
 * HW_ID
 */
static int mc_asoc_i2c_detect(
	struct i2c_client *client_a,
	struct i2c_client *client_d,
	UINT8	*hwid_a,
	UINT8	*hwid_d)
{
	UINT8	bHwid	= mc_asoc_i2c_read_byte(client_a, 8);

	*hwid_a	= bHwid;

	if (bHwid == MCDRV_HWID_14H || bHwid == MCDRV_HWID_15H || bHwid == MCDRV_HWID_16H) {
		bHwid	= mc_asoc_i2c_read_byte(client_d, 8);
		if (bHwid != MC_ASOC_MC3_HW_ID && bHwid != MC_ASOC_MA2_HW_ID) {
			return -ENODEV;
		}
	}
	else {
		if (bHwid != MC_ASOC_MC1_HW_ID && bHwid != MC_ASOC_MA1_HW_ID) {
			return -ENODEV;
		}
	}
	*hwid_d	= bHwid;

	return 0;
}


static struct input_dev		*inp_dev;
#if defined(ALSA_VER_1_0_23)
#else
#define	SW_DRV
#endif
#ifdef SW_DRV
static struct switch_dev	*h2w_sdev=NULL;
static ssize_t headset_print_name(struct switch_dev *sdev, char *buf)
{
	switch (switch_get_state(sdev)) {
	case 0:
		return sprintf(buf, "No Device\n");
	case 1:
		return sprintf(buf, "Headset\n");
	case 2:
		return sprintf(buf, "Headphone\n");
	}
	return -EINVAL;
}
#endif
static struct snd_soc_jack	hs_jack;
static struct snd_soc_jack_pin hs_jack_pins[] = {
	{
		.pin	= "Mic Jack",
		.mask	= SND_JACK_MICROPHONE,
	},
	{
		.pin	= "Headphone Jack",
		.mask	= SND_JACK_HEADPHONE,
	},
};

#ifdef CONFIG_MACH_GOLDFISH
#include <mach/hardware.h>
#include <mach/extint.h>
#endif
//#define	DLY_KEY_ON_OFF_FIXED
static void hsdet_cb( UINT32 dFlags, UINT8 bKeyCnt0, UINT8 bKeyCnt1, UINT8 bKeyCnt2 )
{
	MCDRV_HSDET_INFO	stHSDetInfo;
	int	err;
	static UINT8	bMicDet	= 0;
    struct snd_soc_codec	*codec		= mc_asoc_get_codec_data();
	struct mc_asoc_data		*mc_asoc	= NULL;
	int i;

	TRACE_FUNC();

printk("dFlags=0x%06lX, bKeyCnt0=%d, bKeyCnt1=%d, bKeyCnt2=%d\n", dFlags, bKeyCnt0, bKeyCnt1, bKeyCnt2);
	mc_asoc    = mc_asoc_get_mc_asoc(codec);

	if(mc_asoc_audio_mode_incall()){
		if(has_wake_lock(WAKE_LOCK_SUSPEND)){
			printk("There is a wake_lock locked now, By Headset.\n");
			printk("To unlock the wake_lock\n");
			wake_unlock(&mc_asoc->switch_wake_lock);
		}
		wake_lock_timeout(&mc_asoc->switch_wake_lock, msecs_to_jiffies(1000));
    }

	if(dFlags & MCDRV_HSDET_EVT_PLUGDET_DB_FLAG) {
		dbg_info("PLUGDETDB\n");
		snd_soc_jack_report(&hs_jack, SND_JACK_HEADSET, SND_JACK_HEADSET);
		input_report_switch(inp_dev, SW_HEADPHONE_INSERT, 1);
		if(dFlags & MCDRV_HSDET_EVT_MICDET_FLAG) {
			dbg_info("MICDET\n");
#ifdef SW_DRV
            if(1 == mc_asoc->switch_state){
				printk("MICROPHONE ALREADY INSERT\n");
            } else {
				printk("MICROPHONE INSERT\n");
				switch_set_state(h2w_sdev, 1);
				mc_asoc->switch_state = 1;
				input_report_switch(inp_dev, SW_MICROPHONE_INSERT, 1);
				bMicDet	= 1;
            }
#endif
		} else {
#ifdef SW_DRV
            if((1 == mc_asoc->switch_state) ||(2 == mc_asoc->switch_state)){
				printk("HEADPHONE ALREADY INSERT\n");
			} else {
				printk("HEADPHONE INSERT\n");
				switch_set_state(h2w_sdev, 2);
				mc_asoc->switch_state = 2;
				input_report_switch(inp_dev, SW_HEADPHONE_INSERT, 1);
			}
#endif
		}
	}

	if(dFlags & MCDRV_HSDET_EVT_PLUGUNDET_DB_FLAG) {
dbg_info("PLUGUNDETDB\n");
		snd_soc_jack_report(&hs_jack, 0, SND_JACK_HEADSET);
#ifdef SW_DRV
		printk("HEADPHONE OUT\n");
dbg_info("switch_set_state 0\n");
		switch_set_state(h2w_sdev, 0);
		mc_asoc->switch_state = 0;
#endif
		input_report_switch(inp_dev, SW_HEADPHONE_INSERT, 0);
		input_report_switch(inp_dev, SW_MICROPHONE_INSERT, 0);
		bMicDet	= 0;
	}

	if(dFlags & MCDRV_HSDET_EVT_KEYOFF0_FLAG) {
		dbg_info("KEYOFF_0\n");
		if(stHSDetInfo_Default.bKey0OnDlyTim2 == 0) {
			_McDrv_Ctrl(MCDRV_GET_HSDET, (void *)&stHSDetInfo, 0);
			if((stHSDetInfo.bEnKeyOff & 1) != 0) {
				stHSDetInfo.bEnKeyOff	&= ~1;
				stHSDetInfo.bKey0OnDlyTim	= stHSDetInfo_Default.bKey0OnDlyTim;
				err	= _McDrv_Ctrl(MCDRV_SET_HSDET, (void *)&stHSDetInfo, 0x2020);
				if (err < MCDRV_SUCCESS)
					dbg_info("%d: Error in MCDRV_SET_HSDET\n", err);
			}
		}
	}
	if(dFlags & MCDRV_HSDET_EVT_KEYOFF1_FLAG) {
		dbg_info("KEYOFF_1\n");
		if(stHSDetInfo_Default.bKey1OnDlyTim2 == 0) {
			_McDrv_Ctrl(MCDRV_GET_HSDET, (void *)&stHSDetInfo, 0);
			if((stHSDetInfo.bEnKeyOff & 2) != 0) {
				stHSDetInfo.bEnKeyOff	&= ~2;
				stHSDetInfo.bKey1OnDlyTim	= stHSDetInfo_Default.bKey1OnDlyTim;
				err	= _McDrv_Ctrl(MCDRV_SET_HSDET, (void *)&stHSDetInfo, 0x4020);
				if (err < MCDRV_SUCCESS)
					dbg_info("%d: Error in MCDRV_SET_HSDET\n", err);
			}
		}
	}
	if(dFlags & MCDRV_HSDET_EVT_KEYOFF2_FLAG) {
		dbg_info("KEYOFF_2\n");
		if(stHSDetInfo_Default.bKey2OnDlyTim2 == 0) {
			_McDrv_Ctrl(MCDRV_GET_HSDET, (void *)&stHSDetInfo, 0);
			if((stHSDetInfo.bEnKeyOff & 4) != 0) {
				stHSDetInfo.bEnKeyOff	&= ~4;
				stHSDetInfo.bKey2OnDlyTim	= stHSDetInfo_Default.bKey2OnDlyTim;
				err	= _McDrv_Ctrl(MCDRV_SET_HSDET, (void *)&stHSDetInfo, 0x8020);
				if (err < MCDRV_SUCCESS)
					dbg_info("%d: Error in MCDRV_SET_HSDET\n", err);
			}
		}
	}

	if(dFlags & MCDRV_HSDET_EVT_DLYKEYON0_FLAG) {
		if(stHSDetInfo_Default.bKey0OnDlyTim2 == 0) {
			_McDrv_Ctrl(MCDRV_GET_HSDET, (void *)&stHSDetInfo, 0);
			stHSDetInfo.bEnKeyOff	|= 1;
			stHSDetInfo.bKey0OnDlyTim	= 0;
			err	= _McDrv_Ctrl(MCDRV_SET_HSDET, (void *)&stHSDetInfo, 0x2020);
			if (err < MCDRV_SUCCESS)
				dbg_info("%d: Error in MCDRV_SET_HSDET\n", err);
		}
		if(bMicDet != 0) {
			printk("Long Press DLYKEYON_0\n");
			printk("mc_asoc->audio_mode %d\n",mc_asoc->audio_mode);
			if((mc_asoc->audio_mode == MC_ASOC_AUDIO_MODE_RINGTONG)
				|| (mc_asoc->audio_mode == MC_ASOC_AUDIO_MODE_INCALL)){
				printk("MC_ASOC_EV_KEY_DELAYKEYON0_ENDCALL %d\n",MC_ASOC_EV_KEY_DELAYKEYON0_ENDCALL);
				input_report_key(inp_dev, MC_ASOC_EV_KEY_DELAYKEYON0_ENDCALL, 1);
				input_sync(inp_dev);
				msleep(2000);
				input_report_key(inp_dev, MC_ASOC_EV_KEY_DELAYKEYON0_ENDCALL, 0);
				input_sync(inp_dev);
				printk("MC_ASOC_EV_KEY_DELAYKEYON0_ENDCALL END\n");

			}else{
				printk("MC_ASOC_EV_KEY_DELAYKEYON0 %d\n",MC_ASOC_EV_KEY_DELAYKEYON0);
				input_report_key(inp_dev, MC_ASOC_EV_KEY_DELAYKEYON0, 1);
				input_sync(inp_dev);
				msleep(2000);
				input_report_key(inp_dev, MC_ASOC_EV_KEY_DELAYKEYON0, 0);
				input_sync(inp_dev);
			}
		}
	}
	else if(dFlags & MCDRV_HSDET_EVT_DLYKEYON1_FLAG) {
		if(bMicDet != 0) {
			dbg_info("DLYKEYON_1\n");
			input_report_key(inp_dev, MC_ASOC_EV_KEY_DELAYKEYON1, 1);
			input_sync(inp_dev);
			input_report_key(inp_dev, MC_ASOC_EV_KEY_DELAYKEYON1, 0);
			input_sync(inp_dev);
		}

		if(stHSDetInfo_Default.bKey1OnDlyTim2 == 0) {
			_McDrv_Ctrl(MCDRV_GET_HSDET, (void *)&stHSDetInfo, 0);
			stHSDetInfo.bEnKeyOff	|= 2;
			stHSDetInfo.bKey1OnDlyTim	= 0;
			err	= _McDrv_Ctrl(MCDRV_SET_HSDET, (void *)&stHSDetInfo, 0x4020);
			if (err < MCDRV_SUCCESS)
				dbg_info("%d: Error in MCDRV_SET_HSDET\n", err);
		}
	}
	else if(dFlags & MCDRV_HSDET_EVT_DLYKEYON2_FLAG) {
		if(bMicDet != 0) {
			dbg_info("DLYKEYON_2\n");
			input_report_key(inp_dev, MC_ASOC_EV_KEY_DELAYKEYON2, 1);
			input_sync(inp_dev);
			input_report_key(inp_dev, MC_ASOC_EV_KEY_DELAYKEYON2, 0);
			input_sync(inp_dev);
		}

		if(stHSDetInfo_Default.bKey2OnDlyTim2 == 0) {
			_McDrv_Ctrl(MCDRV_GET_HSDET, (void *)&stHSDetInfo, 0);
			stHSDetInfo.bEnKeyOff	|= 4;
			stHSDetInfo.bKey2OnDlyTim	= 0;
			err	= _McDrv_Ctrl(MCDRV_SET_HSDET, (void *)&stHSDetInfo, 0x8020);
			if (err < MCDRV_SUCCESS)
				dbg_info("%d: Error in MCDRV_SET_HSDET\n", err);
		}
	}

	if(dFlags & MCDRV_HSDET_EVT_DLYKEYOFF0_FLAG) {
		if(mc_asoc_hwid_a == MCDRV_HWID_15H){
			if(dFlags & MCDRV_HSDET_EVT_KEYON0_FLAG) {
				dbg_info("Short press DLYKEYOFF_0\n");
				for(i=0; i<=bKeyCnt0; i++){
					printk("Short press %d time\n", i+1);
					input_report_key(inp_dev, mc_asoc_ev_key_delaykeyoff0[bKeyCnt0], 1);
					input_sync(inp_dev);
					input_report_key(inp_dev, mc_asoc_ev_key_delaykeyoff0[bKeyCnt0], 0);
					input_sync(inp_dev);
				}
			}
        } else if(mc_asoc_hwid_a == MCDRV_HWID_16H){
        	dbg_info("Short press DLYKEYOFF_0\n");
			for(i=0; i<=bKeyCnt0; i++){
				printk("Short press %d time\n", i+1);
				input_report_key(inp_dev, mc_asoc_ev_key_delaykeyoff0[bKeyCnt0], 1);
				input_sync(inp_dev);
				input_report_key(inp_dev, mc_asoc_ev_key_delaykeyoff0[bKeyCnt0], 0);
				input_sync(inp_dev);
			}
        }
	}
	else if(dFlags & MCDRV_HSDET_EVT_DLYKEYOFF1_FLAG) {
		if(mc_asoc_hwid_a == MCDRV_HWID_15H){
			if(dFlags & MCDRV_HSDET_EVT_KEYON1_FLAG) {
 				dbg_info("DLYKEYOFF_1\n");
				input_report_key(inp_dev, mc_asoc_ev_key_delaykeyoff1[bKeyCnt1], 1);
				input_sync(inp_dev);
				input_report_key(inp_dev, mc_asoc_ev_key_delaykeyoff1[bKeyCnt1], 0);
				input_sync(inp_dev);
			}
		} else if(mc_asoc_hwid_a == MCDRV_HWID_16H){
			dbg_info("DLYKEYOFF_1\n");
			input_report_key(inp_dev, mc_asoc_ev_key_delaykeyoff1[bKeyCnt1], 1);
			input_sync(inp_dev);
			input_report_key(inp_dev, mc_asoc_ev_key_delaykeyoff1[bKeyCnt1], 0);
			input_sync(inp_dev);
		}
	}
	else if(dFlags & MCDRV_HSDET_EVT_DLYKEYOFF2_FLAG) {
		if(mc_asoc_hwid_a == MCDRV_HWID_15H){
			if(dFlags & MCDRV_HSDET_EVT_KEYON2_FLAG) {
				dbg_info("DLYKEYOFF_2\n");
				input_report_key(inp_dev, mc_asoc_ev_key_delaykeyoff2[bKeyCnt2], 1);
				input_sync(inp_dev);
				input_report_key(inp_dev, mc_asoc_ev_key_delaykeyoff2[bKeyCnt2], 0);
				input_sync(inp_dev);
			}
		} else if(mc_asoc_hwid_a == MCDRV_HWID_16H){
			dbg_info("DLYKEYOFF_2\n");
			input_report_key(inp_dev, mc_asoc_ev_key_delaykeyoff2[bKeyCnt2], 1);
			input_sync(inp_dev);
			input_report_key(inp_dev, mc_asoc_ev_key_delaykeyoff2[bKeyCnt2], 0);
			input_sync(inp_dev);
		}
	}

#ifdef CONFIG_MACH_GOLDFISH
  #ifdef EXTINT_NUM
	goldfish_clear_extint(mc_asoc_i2c_a->irq);
  #else
	goldfish_clear_extint();
  #endif
#endif
}

static struct workqueue_struct	*my_wq;
extern int	set_irq_type(unsigned int irq, unsigned int type);

static void irq_func(struct work_struct *work)
{
	int err;

	TRACE_FUNC();

	if ((err = _McDrv_Ctrl(MCDRV_IRQ, NULL, 0)) < 0) {
		pr_info("irq_func %d\n", mc_asoc_map_drv_error(err));
	}

	if (IRQ_TYPE == IRQ_TYPE_LEVEL_LOW)
		enable_irq(mc_asoc_i2c_a->irq);

	kfree( (void *)work );
}
irqreturn_t irq_handler(int irq, void *dev_id, struct pt_regs *regs)
{
	int ret;
	struct work_struct *work;

#ifdef CONFIG_SND_SOC_MC_YAMAHA_DEBUG
	TRACE_FUNC();
#endif
	work = (struct work_struct *)kmalloc(sizeof(struct work_struct), GFP_KERNEL);
	if (work) {
		if (IRQ_TYPE == IRQ_TYPE_LEVEL_LOW)
			disable_irq_nosync(mc_asoc_i2c_a->irq);

		INIT_WORK( (struct work_struct *)work, irq_func );
		ret = queue_work( my_wq, (struct work_struct *)work );
	}
	return IRQ_HANDLED;
}

static int init_irq(struct snd_soc_codec *codec) 
{
	int err;

	TRACE_FUNC();

//jeff,  
#ifdef CONFIG_MACH_STUTTGART
	//if GPF2_7_SW_DBG/HS is 1, now HS is debug function, so not init_irq, otherwise it will wakeup AP in LPA and AFTR.
	if(gpio_get_value(EXYNOS4_GPF2(7))){ 
		//dev_info(codec->dev, "Stuttgart, now HS port is debug console, so don't init IRQ\n");
		return 0;
	}
#endif

	my_wq = create_workqueue("irq_queue");

#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON) || defined(KERNEL_2_6_35_OMAP)
	err = set_irq_type(mc_asoc_i2c_a->irq, IRQ_TYPE);
#else
	err = irq_set_irq_type(mc_asoc_i2c_a->irq, IRQ_TYPE);
#endif
	if (err < 0) {
		dev_err(codec->dev, "Failed to set_irq_type: %d\n", err);
		return -EIO;
	}

	err = gpio_request(EXYNOS4_GPX1(5), "XEINIT(13)GPX1_5" );
	if (err < 0)
		goto err_request_gpio;

	s3c_gpio_cfgpin(EXYNOS4_GPX1(5),S3C_GPIO_SFN(0xf));
	s3c_gpio_setpull(EXYNOS4_GPX1(5), S3C_GPIO_PULL_UP);

	err = request_irq(mc_asoc_i2c_a->irq, (void *)irq_handler, IRQF_DISABLED, "MC_YAMAHA IRQ", NULL);
	if (err < 0) {
		dev_err(codec->dev, "Failed to request_irq: %d\n", err);
		return -EIO;
	}
	enable_irq_wake(mc_asoc_i2c_a->irq);
	return 0;

err_request_gpio:
	gpio_free(EXYNOS4_GPX1(5));

    return -EIO;
}
static int term_irq(void)
{
//jeff,  
#ifdef CONFIG_MACH_STUTTGART
	if(gpio_get_value(EXYNOS4_GPF2(7))) 
		return 0;
#endif

	free_irq(mc_asoc_i2c_a->irq, NULL);
	gpio_free(EXYNOS4_GPX1(5));
	destroy_workqueue( my_wq );

	return 0;
}

static int mc_asoc_power_set(struct snd_soc_codec *codec, unsigned char stat)
{
	struct mc_asoc_data		*mc_asoc	= NULL;
	SINT16	*vol	= NULL;
	int		err, i;

    mc_asoc	= mc_asoc_get_mc_asoc(codec);
    if(mc_asoc == NULL) {
        return -EINVAL;
    }

    mutex_lock(&mc_asoc->mutex);
    if(stat){
        vol	= (SINT16 *)&mc_asoc->vol_store;


        if ((mc_asoc_hwid_a == MCDRV_HWID_15H) 
        || (mc_asoc_hwid_a == MCDRV_HWID_16H)){
            err	= _McDrv_Ctrl(MCDRV_SET_PLL, (void *)mc_asoc + mc_asoc_info_store_tbl[0/*PLL*/].offset, 0);
            if (err != MCDRV_SUCCESS) {
                dev_err(codec->dev, "%d: Error in MCDRV_SET_PLL\n", err);
                err	= -EIO;
                goto error;
            }
        }
        err	= _McDrv_Ctrl(MCDRV_INIT, &mc_asoc->setup.init, 0);
        if (err != MCDRV_SUCCESS) {
            dev_err(codec->dev, "%d: Error in MCDRV_INIT\n", err);
            err	= -EIO;
            goto error;
        }
        else {
            err	= 0;
        }

        if((mc_asoc_hwid_a == MCDRV_HWID_15H) 
        || (mc_asoc_hwid_a == MCDRV_HWID_16H)){
            // IRQ Initialize
            err = init_irq(codec);
            if (err < 0) {
                dev_err(codec->dev, "%d: Error in init_irq\n", err);
                goto error;
            }
        }

        /* restore parameters */
        for (i	= 0; i < sizeof(MCDRV_VOL_INFO)/sizeof(SINT16); i++, vol++) {
            *vol |= 0x0001;
        }

        /* When pvPrm is "NULL" ,dPrm is "0" */
        if (mc_asoc_cdsp_ae_ex_flag == MC_ASOC_CDSP_ON) {
            err	= _McDrv_Ctrl(MCDRV_SET_CDSP, mc_asoc->cdsp_store.prog,
                    mc_asoc->cdsp_store.size);
            if (err != MCDRV_SUCCESS) {
                dev_err(codec->dev, "%d: Error in mc_asoc_resume\n", err);
                err	= -EIO;
                goto error;
            }
            for (i	= 0; i < mc_asoc->cdsp_store.param_count; i++) {
                err	= _McDrv_Ctrl(MCDRV_SET_CDSP_PARAM,
                        &(mc_asoc->cdsp_store.cdspparam[i]), 0);
                if (err != MCDRV_SUCCESS) {
                    dev_err(codec->dev, "%d: Error in mc_asoc_resume\n", err);
                    err	= -EIO;
                    goto error;
                }
            }
        }
        else if (mc_asoc_cdsp_ae_ex_flag == MC_ASOC_AE_EX_ON) {
            err	= _McDrv_Ctrl(MCDRV_SET_AUDIOENGINE_EX, mc_asoc->ae_ex_store.prog,
                    mc_asoc->ae_ex_store.size);
            if (err != MCDRV_SUCCESS) {
                dev_err(codec->dev, "%d: Error in mc_asoc_resume\n", err);
                err	= -EIO;
                goto error;
            }
            for (i	= 0; i < mc_asoc->ae_ex_store.param_count; i++) {
                err	= _McDrv_Ctrl(MCDRV_SET_EX_PARAM,
                        &(mc_asoc->ae_ex_store.exparam[i]), 0);
                if (err != MCDRV_SUCCESS) {
                    dev_err(codec->dev, "%d: Error in mc_asoc_resume\n", err);
                    err	= -EIO;
                    goto error;
                }
            }
        }

        for (i	= 1/*DigitalIO*/; i < MC_ASOC_N_INFO_STORE; i++) {
            struct mc_asoc_info_store	*store	= &mc_asoc_info_store_tbl[i];
            if (store->set) {
                err	= _McDrv_Ctrl(store->set, (void *)mc_asoc + store->offset,
                        store->flags);
                if (err != MCDRV_SUCCESS) {
                    dev_err(codec->dev, "%d: Error in mc_asoc_resume\n", err);
                    err	= -EIO;
                    goto error;
                }
                else {
                    err	= 0;
                }
            }
        }

        mc_asoc_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
        mc_asoc->mc_asoc_power_status = 1;
    } else{
        mc_asoc_set_bias_level(codec, SND_SOC_BIAS_OFF);


        if (mc_asoc_hwid == MC_ASOC_MC1_HW_ID || mc_asoc_hwid == MC_ASOC_MA1_HW_ID) {
            for (i	= 0; i < MC_ASOC_N_INFO_STORE; i++) {
                if((mc_asoc_info_store_tbl[i].set == MCDRV_SET_DTMF)
                        || (mc_asoc_info_store_tbl[i].set == MCDRV_SET_DITSWAP)
                        || (mc_asoc_info_store_tbl[i].set == MCDRV_SET_ADC_EX)
                        || (mc_asoc_info_store_tbl[i].set == MCDRV_SET_PDM_EX)
                        || (mc_asoc_info_store_tbl[i].set == MCDRV_SET_PLL)
                        || (mc_asoc_info_store_tbl[i].set == MCDRV_SET_HSDET)) {
                    mc_asoc_info_store_tbl[i].get	= 0;
                    mc_asoc_info_store_tbl[i].set	= 0;
                }
            }
        }
        else if((mc_asoc_hwid_a != MCDRV_HWID_15H)
             && (mc_asoc_hwid_a != MCDRV_HWID_16H)){
            for (i	= 0; i < MC_ASOC_N_INFO_STORE; i++) {
                if((mc_asoc_info_store_tbl[i].set == MCDRV_SET_PLL)
                        || (mc_asoc_info_store_tbl[i].set == MCDRV_SET_HSDET)) {
                    mc_asoc_info_store_tbl[i].get	= 0;
                    mc_asoc_info_store_tbl[i].set	= 0;
                }
            }
        }

        /* store parameters */
        for (i	= 0; i < MC_ASOC_N_INFO_STORE; i++) {
            struct mc_asoc_info_store	*store	= &mc_asoc_info_store_tbl[i];
            if (store->get) {
                err	= _McDrv_Ctrl(store->get, (void *)mc_asoc + store->offset, 0);
                if (err != MCDRV_SUCCESS) {
                    dev_err(codec->dev, "%d: Error in mc_asoc_suspend\n", err);
                    err	= -EIO;
                    goto error;
                }
                else {
                    err	= 0;
                }
            }
        }

        if((mc_asoc_hwid_a == MCDRV_HWID_15H) 
        || (mc_asoc_hwid_a == MCDRV_HWID_16H)){
            // IRQ terminate
            term_irq();
        }

        err	= _McDrv_Ctrl(MCDRV_TERM, NULL, 0);
        if (err != MCDRV_SUCCESS) {
            dev_err(codec->dev, "%d: Error in MCDRV_TERM\n", err);
            err	= -EIO;
        }
        else {
            err	= 0;
        }
        mc_asoc->mc_asoc_power_status = 0;
		if(mc_asoc->switch_state != 0){
			dbg_info("switch_set_state 0\n");
			switch_set_state(h2w_sdev, 0);
			mc_asoc->switch_state = 0;
			printk("HEADPHONE OUT\n");
		}
    }
    mutex_unlock(&mc_asoc->mutex);
	return err;

error:
    mutex_unlock(&mc_asoc->mutex);
	return err;
}

static ssize_t mc_asoc_power_show(struct sysdev_class *class,
	struct sysdev_class_attribute * attr, char *buf)
{
	struct snd_soc_codec	*codec		= mc_asoc_get_codec_data();
	struct mc_asoc_data		*mc_asoc	= NULL;

	mc_asoc	= mc_asoc_get_mc_asoc(codec);
	return sprintf(buf, "%d\n", mc_asoc->mc_asoc_power_status);
}

static ssize_t mc_asoc_power_set_store(struct sysdev_class *class,
        struct sysdev_class_attribute * attr, const char *buf, size_t count)
{
    struct snd_soc_codec	*codec		= mc_asoc_get_codec_data();

    if(strncmp(buf, "1", 1)==0){
        mc_asoc_power_set(codec, 1);
    } else {
        mc_asoc_power_set(codec, 0);
    }

    return count;
}

static ssize_t mc_asoc_id_show(struct sysdev_class *class,
	struct sysdev_class_attribute * attr, char *buf)
{
    struct i2c_client * mc_asoc_i2c_a = mc_asoc_get_i2c_client(MCDRV_SLAVEADDR_3AH);
    struct i2c_client * mc_asoc_i2c_d = mc_asoc_get_i2c_client(MCDRV_SLAVEADDR_11H);
    /* check HW_ID */
    mc_asoc_i2c_detect(mc_asoc_i2c_a, mc_asoc_i2c_d, &mc_asoc_hwid_a, &mc_asoc_hwid);
    if(mc_asoc_hwid_a == MCDRV_HWID_15H) {
        return sprintf(buf, "%#x\n", MCDRV_HWID_15H);
    } else if(mc_asoc_hwid_a == MCDRV_HWID_16H) {
        return sprintf(buf, "%#x\n", MCDRV_HWID_16H);
    } else {
        return -ENODEV;
    }

}

static ssize_t mc_asoc_id_store(struct sysdev_class *class,
	struct sysdev_class_attribute * attr, const char *buf, size_t count)
{
	return count;
}

static SYSDEV_CLASS_ATTR(mc_asoc_id, 0644, mc_asoc_id_show, mc_asoc_id_store);
static SYSDEV_CLASS_ATTR(mc_asoc_power, 0644, mc_asoc_power_show, mc_asoc_power_set_store);
static struct sysdev_class_attribute *mc_asoc_attributes[] = {
	&attr_mc_asoc_id,
	&attr_mc_asoc_power,
	NULL
};

static struct sysdev_class module_mc_asoc_class = {
	.name = "mc_asoc",
};

static void headset_delay_workqueue(struct work_struct *work)
{
	struct snd_soc_codec	*codec		= mc_asoc_get_codec_data();
	struct mc_asoc_data		*mc_asoc	= NULL;

	mc_asoc	= mc_asoc_get_mc_asoc(codec);
	switch_set_state(h2w_sdev, 1);
	mc_asoc->switch_state = 1;
	input_report_switch(inp_dev, SW_MICROPHONE_INSERT, 1);
}
static void headphone_delay_workqueue(struct work_struct *work)
{
	struct snd_soc_codec	*codec		= mc_asoc_get_codec_data();
	struct mc_asoc_data		*mc_asoc	= NULL;

	mc_asoc	= mc_asoc_get_mc_asoc(codec);
	switch_set_state(h2w_sdev, 2);
	mc_asoc->switch_state = 2;
	input_report_switch(inp_dev, SW_HEADPHONE_INSERT, 1);
}

/*
 * Codec device
 */
static int mc_asoc_probe(
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	struct platform_device *pdev
#else
	struct snd_soc_codec *codec
#endif
)
{
	int		i;
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	struct snd_soc_device	*socdev		= platform_get_drvdata(pdev);
	struct snd_soc_codec	*codec		= mc_asoc_get_codec_data();
#endif
	struct mc_asoc_data		*mc_asoc	= NULL;
	struct device			*dev		= NULL;
	struct sysdev_class_attribute **attr = NULL;
	struct mc_asoc_setup	*setup		= &mc_asoc_cfg_setup;
	int		err;
	UINT32	update	= 0;

	TRACE_FUNC();

	if (codec == NULL) {
/*		printk(KERN_ERR "I2C bus is not probed successfully\n");*/
		err	= -ENODEV;
		goto error_codec_data;
	}
#if defined(MC_ASOC_POWER_SET)
	mc_asoc_set_codec_data(codec);
#endif
	mc_asoc	= mc_asoc_get_mc_asoc(codec);
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	if(socdev != NULL) {
		dev	= socdev->dev;
	}
#else
	dev	= codec->dev;
#endif
	if (mc_asoc == NULL || dev == NULL) {
		err	= -ENODEV;
		goto error_codec_data;
	}

#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	socdev->codec_data	= setup;
	socdev->card->codec	= codec;
	socdev->card->pmdown_time	= 500;
#endif

	/* init hardware */
	memcpy(&mc_asoc->setup, setup, sizeof(struct mc_asoc_setup));
	err	= _McDrv_Ctrl(MCDRV_SET_PLL, &mc_asoc->setup.pll, 0);
	if (err != MCDRV_SUCCESS) {
		dev_err(dev, "%d: Error in MCDRV_SET_PLL\n", err);
		err	= -EIO;
		goto error_init_hw;
	}
	err	= _McDrv_Ctrl(MCDRV_INIT, &mc_asoc->setup.init, 0);
	if (err != MCDRV_SUCCESS) {
		dev_err(dev, "%d: Error in MCDRV_INIT\n", err);
		err	= -EIO;
		goto error_init_hw;
	}

	/* HW_ID */
	if ((err = mc_asoc_i2c_detect(mc_asoc_i2c_a, mc_asoc_i2c_d, &mc_asoc_hwid_a, &mc_asoc_hwid)) < 0) {
		dev_err(dev, "err=%d: failed to MC_ASOC HW_ID\n", err);
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
		kfree(mc_asoc);
		kfree(codec);
#endif
		return err;
	}

#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	/* pcm */
	err	= snd_soc_new_pcms(socdev, SNDRV_DEFAULT_IDX1, SNDRV_DEFAULT_STR1);
	if (err < 0) {
		dev_err(dev, "%d: Error in snd_soc_new_pcms\n", err);
		goto error_new_pcm;
	}
#endif

	/* controls */
	err	= mc_asoc_add_controls(codec, mc_asoc_snd_controls,
				ARRAY_SIZE(mc_asoc_snd_controls));
	if (err < 0) {
		dev_err(dev, "%d: Error in mc_asoc_add_controls\n", err);
		goto error_add_ctl;
	}

	err	= mc_asoc_add_widgets(codec);
	if (err < 0) {
		dev_err(dev, "%d: Error in mc_asoc_add_widgets\n", err);
		goto error_add_ctl;
	}

	/* hwdep */
	err	= mc_asoc_add_hwdep(codec);
	if (err < 0) {
		dev_err(dev, "%d: Error in mc_asoc_add_hwdep\n", err);
		goto error_add_hwdep;
	}

	write_cache(codec, MC_ASOC_VOICE_RECORDING, VOICE_RECORDING_UNMUTE);
	write_cache(codec, MC_ASOC_INCALL_MIC_SP, INCALL_MIC_SP);
	write_cache(codec, MC_ASOC_INCALL_MIC_RC, INCALL_MIC_RC);
	write_cache(codec, MC_ASOC_INCALL_MIC_HP, INCALL_MIC_HP);
	write_cache(codec, MC_ASOC_INCALL_MIC_LO1, INCALL_MIC_LO1);
	write_cache(codec, MC_ASOC_INCALL_MIC_LO2, INCALL_MIC_LO2);

#ifdef MC_ASOC_TEST
	write_cache(codec, MC_ASOC_MAIN_MIC, mc_asoc_main_mic);
	write_cache(codec, MC_ASOC_SUB_MIC, mc_asoc_sub_mic);
	write_cache(codec, MC_ASOC_HS_MIC, mc_asoc_hs_mic);
	write_cache(codec, MC_ASOC_MIC1_BIAS, mc_asoc_mic1_bias);
	write_cache(codec, MC_ASOC_MIC2_BIAS, mc_asoc_mic2_bias);
	write_cache(codec, MC_ASOC_MIC3_BIAS, mc_asoc_mic3_bias);
	write_cache(codec, MC_ASOC_CAP_PATH_8KHZ, mc_asoc_cap_path);
#endif

	if((mc_asoc_hwid_a == MCDRV_HWID_15H)
	|| (mc_asoc_hwid_a == MCDRV_HWID_16H)) {
		/* Headset jack detection */
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
		snd_soc_jack_new(socdev->card, "Headset", SND_JACK_HEADSET, &hs_jack);
#else
		snd_soc_jack_new(codec, "Headset", SND_JACK_HEADSET, &hs_jack);
#endif
		snd_soc_jack_add_pins(&hs_jack, ARRAY_SIZE(hs_jack_pins), hs_jack_pins);

		inp_dev	= input_allocate_device();
		input_set_capability(inp_dev, EV_SW, SW_HEADPHONE_INSERT);
		input_set_capability(inp_dev, EV_SW, SW_MICROPHONE_INSERT);
		input_set_capability(inp_dev, EV_KEY, MC_ASOC_EV_KEY_DELAYKEYON0);
		input_set_capability(inp_dev, EV_KEY, MC_ASOC_EV_KEY_DELAYKEYON0_ENDCALL);
		input_set_capability(inp_dev, EV_KEY, MC_ASOC_EV_KEY_DELAYKEYON1);
		input_set_capability(inp_dev, EV_KEY, MC_ASOC_EV_KEY_DELAYKEYON2);
        inp_dev->name = "Headset Hook Key";
		for(i = 0; i < 8; i++) {
			input_set_capability(inp_dev, EV_KEY, mc_asoc_ev_key_delaykeyoff0[i]);
			input_set_capability(inp_dev, EV_KEY, mc_asoc_ev_key_delaykeyoff1[i]);
			input_set_capability(inp_dev, EV_KEY, mc_asoc_ev_key_delaykeyoff2[i]);
		}
		err	= input_register_device(inp_dev);
		if (err != 0) {
			dev_err(dev, "%d: Error in input_register_device\n", err);
			goto error_set_mode;
		}

#ifdef SW_DRV
		h2w_sdev	= kzalloc(sizeof(struct switch_dev), GFP_KERNEL);
		h2w_sdev->name			= "h2w";
		h2w_sdev->print_name	= headset_print_name;
		err	= switch_dev_register(h2w_sdev);
		if (err < 0) {
			dev_err(dev, "%d: Error in switch_dev_register\n", err);
			goto error_set_mode;
		}
#endif
	}

	update	= MCDRV_DIO0_COM_UPDATE_FLAG | MCDRV_DIO0_DIR_UPDATE_FLAG | MCDRV_DIO0_DIT_UPDATE_FLAG |
				MCDRV_DIO1_COM_UPDATE_FLAG | MCDRV_DIO1_DIR_UPDATE_FLAG | MCDRV_DIO1_DIT_UPDATE_FLAG |
				MCDRV_DIO2_COM_UPDATE_FLAG | MCDRV_DIO2_DIR_UPDATE_FLAG | MCDRV_DIO2_DIT_UPDATE_FLAG;
	err	= _McDrv_Ctrl(MCDRV_SET_DIGITALIO, (void *)&stDioInfo_Default, update);
	if (err < MCDRV_SUCCESS) {
		dev_err(dev, "%d: Error in MCDRV_SET_DIGITALIO\n", err);
		goto error_set_mode;
	}

	err	= _McDrv_Ctrl(MCDRV_SET_DAC, (void *)&stDacInfo_Default, 0x7);
	if (err < MCDRV_SUCCESS) {
		dev_err(dev, "%d: Error in MCDRV_SET_DAC\n", err);
		goto error_set_mode;
	}

	err	= _McDrv_Ctrl(MCDRV_SET_ADC, (void *)&stAdcInfo_Default, 0x7);
	if (err < MCDRV_SUCCESS) {
		dev_err(dev, "%d: Error in MCDRV_SET_ADC\n", err);
		goto error_set_mode;
	}

	if (mc_asoc_hwid == MC_ASOC_MC3_HW_ID || mc_asoc_hwid == MC_ASOC_MA2_HW_ID) {
		err = _McDrv_Ctrl(MCDRV_SET_ADC_EX, (void *)&stAdcExInfo_Default, 0x3F);
		if (err < 0) {
			dev_err(dev, "%d: Error in MCDRV_SET_ADC_EX\n", err);
			goto error_set_mode;
		}
	}

	err	= _McDrv_Ctrl(MCDRV_SET_SP, (void *)&stSpInfo_Default, 0);
	if (err < MCDRV_SUCCESS) {
		dev_err(dev, "%d: Error in MCDRV_SET_SP\n", err);
		goto error_set_mode;
	}

	err	= _McDrv_Ctrl(MCDRV_SET_DNG, (void *)&stDngInfo_Default, 0x7F3F3F);
	if (err < MCDRV_SUCCESS) {
		dev_err(dev, "%d: Error in MCDRV_SET_DNG\n", err);
		goto error_set_mode;
	}

	err	= _McDrv_Ctrl(MCDRV_SET_SYSEQ, (void *)&stSyseqInfo_Default, 0x3);
	if (err < MCDRV_SUCCESS) {
		dev_err(dev, "%d: Error in MCDRV_SET_SYSEQ\n", err);
		goto error_set_mode;
	}

	if (mc_asoc_hwid == MC_ASOC_MC3_HW_ID || mc_asoc_hwid == MC_ASOC_MA2_HW_ID) {
		err	= _McDrv_Ctrl(MCDRV_SET_DTMF, (void *)&stDtmfInfo_Default, 0xff);
		if (err < MCDRV_SUCCESS) {
			dev_err(dev, "%d: Error in MCDRV_SET_DTMF\n", err);
			goto error_set_mode;
		}
		else {
			memcpy(&mc_asoc->dtmf_store, &stDtmfInfo_Default, sizeof(MCDRV_DTMF_PARAM));
		}

		err	= _McDrv_Ctrl(MCDRV_SET_DITSWAP, (void *)&stDitswapInfo_Default, 0x3);
		if (err < MCDRV_SUCCESS) {
			dev_err(dev, "%d: Error in MCDRV_SET_DITSWAP\n", err);
			goto error_set_mode;
		}

		if((mc_asoc_hwid_a == MCDRV_HWID_15H)
		|| (mc_asoc_hwid_a == MCDRV_HWID_16H)) {
			if(mc_asoc_hwid_a == MCDRV_HWID_15H)
				stHSDetInfo_Default.bIrqType = MCDRV_IRQTYPE_REF;
			if(mc_asoc_hwid_a == MCDRV_HWID_16H)
				stHSDetInfo_Default.bIrqType = MCDRV_IRQTYPE_NORMAL;
			stHSDetInfo_Default.cbfunc	= hsdet_cb;
			err	= _McDrv_Ctrl(MCDRV_SET_HSDET, (void *)&stHSDetInfo_Default, 0x1fffffff);
			if (err < MCDRV_SUCCESS) {
				dev_err(dev, "%d: Error in MCDRV_SET_HSDET\n", err);
				goto error_set_mode;
			}
		}
	}

	if((err = connect_path(codec)) < 0) {
		dev_err(dev, "%d: Error in coeenct_path\n", err);
		goto error_set_mode;
	}

	wake_lock_init(&mc_asoc->switch_wake_lock, WAKE_LOCK_SUSPEND,"headset_wake");
	if((mc_asoc_hwid_a == MCDRV_HWID_15H)
	|| (mc_asoc_hwid_a == MCDRV_HWID_16H)) {
		if ((err = _McDrv_Ctrl(MCDRV_IRQ, NULL, 0)) < 0) {
			dev_err(dev, "%d: Error in MCDRV_IRQ\n", err);
		}
		if ((err = _McDrv_Ctrl(MCDRV_IRQ, NULL, 0)) < 0) {
			dev_err(dev, "%d: Error in MCDRV_IRQ\n", err);
		}
		if ((err = _McDrv_Ctrl(MCDRV_IRQ, NULL, 0)) < 0) {
			dev_err(dev, "%d: Error in MCDRV_IRQ\n", err);
		}
		// IRQ Initialize
		err = init_irq(codec);
		if (err < 0) {
			dev_err(dev, "%d: Error in init_irq\n", err);
			goto error_set_mode;
		}
		device_init_wakeup(dev, 1);
	}

    mc_asoc_set_bias_level(codec, SND_SOC_BIAS_STANDBY);
    mc_asoc->mc_asoc_power_status = 1;

    err=sysdev_class_register(&module_mc_asoc_class);
    if (unlikely(err))
    {
        return err;
    }

    for (attr = mc_asoc_attributes; *attr; attr++)
    {
        err = sysdev_class_create_file(&module_mc_asoc_class, *attr);
        if (err)
        {
            goto out_unreg;
        }
    }

	return 0;

error_set_mode:
error_add_hwdep:
error_add_ctl:
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	snd_soc_free_pcms(socdev);
error_new_pcm:
#endif
	_McDrv_Ctrl(MCDRV_TERM, NULL, 0);
error_init_hw:
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	socdev->card->codec	= NULL;
#endif
error_codec_data:
out_unreg:
    for (; attr >= mc_asoc_attributes; attr--)
    {
        sysdev_class_remove_file(&module_mc_asoc_class, *attr);
    }

    sysdev_class_unregister(&module_mc_asoc_class);

    return err;
}

#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
static int mc_asoc_remove(struct platform_device *pdev)
{
	struct snd_soc_device *socdev	= platform_get_drvdata(pdev);
	int err;

	TRACE_FUNC();

	if((mc_asoc_hwid_a == MCDRV_HWID_15H)
	|| (mc_asoc_hwid_a == MCDRV_HWID_16H)) {
		// IRQ terminate
		term_irq();
		wake_lock_destroy(&mc_asoc->switch_wake_lock);
		input_unregister_device(inp_dev);
#ifdef SW_DRV
		if(h2w_sdev != NULL) {
			switch_dev_unregister(h2w_sdev);
			kfree(h2w_sdev);
			h2w_sdev	= NULL;
		}
#endif
	}

	mc_asoc_set_bias_level(socdev->card->codec, SND_SOC_BIAS_OFF);
	if (socdev->card->codec)
	{
		snd_soc_free_pcms(socdev);

		err	= _McDrv_Ctrl(MCDRV_TERM, NULL, 0);
		if (err != MCDRV_SUCCESS) {
			dev_err(socdev->dev, "%d: Error in MCDRV_TERM\n", err);
			return -EIO;
		}
	}

	return 0;
}
#else
static int mc_asoc_remove(struct snd_soc_codec *codec)
{
	int err;
	struct mc_asoc_data		*mc_asoc	= NULL;

	TRACE_FUNC();

	mc_asoc    = mc_asoc_get_mc_asoc(codec);
	if((mc_asoc_hwid_a == MCDRV_HWID_15H)
	|| (mc_asoc_hwid_a == MCDRV_HWID_16H)) {
		// IRQ terminate
		term_irq();
		wake_lock_destroy(&mc_asoc->switch_wake_lock);
		input_unregister_device(inp_dev);
#ifdef SW_DRV
		if(h2w_sdev != NULL) {
			switch_dev_unregister(h2w_sdev);
			kfree(h2w_sdev);
			h2w_sdev	= NULL;
		}
#endif
	}

	mc_asoc_set_bias_level(codec, SND_SOC_BIAS_OFF);
	if (codec) {
		err	= _McDrv_Ctrl(MCDRV_TERM, NULL, 0);
		if (err != MCDRV_SUCCESS) {
			dev_err(codec->dev, "%d: Error in MCDRV_TERM\n", err);
			return -EIO;
		}
	}
	return 0;
}
#endif

unsigned char mc_asoc_audio_mode_incall(void)
{
    struct snd_soc_codec *codec	= mc_asoc_get_codec_data();
    struct mc_asoc_data	*mc_asoc	= NULL;

    if(codec == NULL) {
        return -EINVAL;
    }
    mc_asoc	= mc_asoc_get_mc_asoc(codec);
    if(mc_asoc == NULL) {
        return -EINVAL;
    }

    if((mc_asoc->audio_mode == MC_ASOC_AUDIO_MODE_INCALL)
            || (mc_asoc->audio_mode == MC_ASOC_AUDIO_MODE_AUDIO_INCALL)) {
        return 1;
    } else {
        return 0;
    }
}
EXPORT_SYMBOL_GPL(mc_asoc_audio_mode_incall);

static int mc_asoc_suspend(
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	struct platform_device *pdev,
#else
	struct snd_soc_codec *codec,
#endif
	pm_message_t state)
{
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	struct snd_soc_device	*socdev		= platform_get_drvdata(pdev);
	struct snd_soc_codec	*codec		= NULL;
#endif
	struct mc_asoc_data	*mc_asoc	= NULL;
	int err;

	TRACE_FUNC();

#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	if(socdev != NULL) {
		codec	= socdev->card->codec;
	}
#endif
	printk("[%s][%d]\n",__func__, __LINE__);
	mc_asoc	= mc_asoc_get_mc_asoc(codec);
	if(mc_asoc_audio_mode_incall()){
		if(device_may_wakeup(codec->dev)){
			enable_irq_wake(mc_asoc_i2c_a->irq);
		}
		return 0;
	}

	printk("[%s][%d]\n",__func__, __LINE__);
	err = mc_asoc_power_set(codec, 0);

	return err;
}

static int mc_asoc_resume(
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	struct platform_device *pdev
#else
	struct snd_soc_codec *codec
#endif
)
{
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	struct snd_soc_device	*socdev		= platform_get_drvdata(pdev);
	struct snd_soc_codec	*codec		= NULL;
#endif
	struct mc_asoc_data	*mc_asoc	= NULL;
	int err;

	TRACE_FUNC();

#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	if(socdev != NULL) {
		codec	= socdev->card->codec;
	}
#endif
	mc_asoc	= mc_asoc_get_mc_asoc(codec);
	if(mc_asoc_audio_mode_incall()){
		if(device_may_wakeup(codec->dev)){
			disable_irq_wake(mc_asoc_i2c_a->irq);
		}
		return 0;
	}

	err = mc_asoc_power_set(codec, 1);

	return err;
}

static int mc_asoc_set_bias_level(
	struct snd_soc_codec	*codec,
	enum snd_soc_bias_level	level)
{
	printk("%s codec[%p] level[%d]\n", __FUNCTION__, codec, level);
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	codec->bias_level	= level;
#elif defined(KERNEL_2_6_35_OMAP)
	codec->dapm->bias_level	= level;
#else
	codec->dapm.bias_level	= level;
#endif

	return 0;
}

#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
struct snd_soc_codec_device	mc_asoc_codec_dev	= {
	.probe			= mc_asoc_probe,
	.remove			= mc_asoc_remove,
	.suspend		= mc_asoc_suspend,
	.resume			= mc_asoc_resume
};
EXPORT_SYMBOL_GPL(mc_asoc_codec_dev);
#else
struct snd_soc_codec_driver	mc_asoc_codec_dev	= {
	.probe			= mc_asoc_probe,
	.remove			= mc_asoc_remove,
	.suspend		= mc_asoc_suspend,
	.resume			= mc_asoc_resume,
	.read			= mc_asoc_read_reg,
	.write			= mc_asoc_write_reg,
	.reg_cache_size	= MC_ASOC_N_REG,
	.reg_word_size	= sizeof(u16),
	.reg_cache_step	= 1,
	.set_bias_level	= mc_asoc_set_bias_level
};
#endif

/*
 * I2C client
 */
static void init_ap_play_port(
	void
)
{
	UINT8	bCh;

	for(bCh = 0; bCh < DAC_PATH_CHANNELS; bCh++) {
		if((stPresetPathInfo[0][1].asDac[bCh].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK]&MCDRV_SRC3_DIR0_ON) != 0) {
			mc_asoc_ap_play_port	= DIO_0;
			return;
		}
		else if((stPresetPathInfo[0][1].asDac[bCh].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK]&MCDRV_SRC3_DIR1_ON) != 0) {
			mc_asoc_ap_play_port	= DIO_1;
			return;
		}
		else if((stPresetPathInfo[0][1].asDac[bCh].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK]&MCDRV_SRC3_DIR2_ON) != 0) {
			mc_asoc_ap_play_port	= DIO_2;
			return;
		}
	}
	for(bCh = 0; bCh < AE_PATH_CHANNELS; bCh++) {
		if((stPresetPathInfo[0][1].asAe[bCh].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK]&MCDRV_SRC3_DIR0_ON) != 0) {
			mc_asoc_ap_play_port	= DIO_0;
			return;
		}
		else if((stPresetPathInfo[0][1].asAe[bCh].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK]&MCDRV_SRC3_DIR1_ON) != 0) {
			mc_asoc_ap_play_port	= DIO_1;
			return;
		}
		else if((stPresetPathInfo[0][1].asAe[bCh].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK]&MCDRV_SRC3_DIR2_ON) != 0) {
			mc_asoc_ap_play_port	= DIO_2;
			return;
		}
	}
	for(bCh = 0; bCh < CDSP_PATH_CHANNELS; bCh++) {
		if((stPresetPathInfo[0][1].asCdsp[bCh].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK]&MCDRV_SRC3_DIR0_ON) != 0) {
			mc_asoc_ap_play_port	= DIO_0;
			return;
		}
		else if((stPresetPathInfo[0][1].asCdsp[bCh].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK]&MCDRV_SRC3_DIR1_ON) != 0) {
			mc_asoc_ap_play_port	= DIO_1;
			return;
		}
		else if((stPresetPathInfo[0][1].asCdsp[bCh].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK]&MCDRV_SRC3_DIR2_ON) != 0) {
			mc_asoc_ap_play_port	= DIO_2;
			return;
		}
	}
	for(bCh = 0; bCh < MIX_PATH_CHANNELS; bCh++) {
		if((stPresetPathInfo[0][1].asMix[bCh].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK]&MCDRV_SRC3_DIR0_ON) != 0) {
			mc_asoc_ap_play_port	= DIO_0;
			return;
		}
		else if((stPresetPathInfo[0][1].asMix[bCh].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK]&MCDRV_SRC3_DIR1_ON) != 0) {
			mc_asoc_ap_play_port	= DIO_1;
			return;
		}
		else if((stPresetPathInfo[0][1].asMix[bCh].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK]&MCDRV_SRC3_DIR2_ON) != 0) {
			mc_asoc_ap_play_port	= DIO_2;
			return;
		}
	}
	for(bCh = 0; bCh < MIX2_PATH_CHANNELS; bCh++) {
		if((stPresetPathInfo[0][1].asMix2[bCh].abSrcOnOff[MCDRV_SRC_DIR0_BLOCK]&MCDRV_SRC3_DIR0_ON) != 0) {
			mc_asoc_ap_play_port	= DIO_0;
			return;
		}
		else if((stPresetPathInfo[0][1].asMix2[bCh].abSrcOnOff[MCDRV_SRC_DIR1_BLOCK]&MCDRV_SRC3_DIR1_ON) != 0) {
			mc_asoc_ap_play_port	= DIO_1;
			return;
		}
		else if((stPresetPathInfo[0][1].asMix2[bCh].abSrcOnOff[MCDRV_SRC_DIR2_BLOCK]&MCDRV_SRC3_DIR2_ON) != 0) {
			mc_asoc_ap_play_port	= DIO_2;
			return;
		}
	}
	for(bCh = 0; bCh < ADC0_PATH_CHANNELS; bCh++) {
		if((stPresetPathInfo[0][1].asAdc0[bCh].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK]&MCDRV_SRC1_LINE1_L_ON) != 0) {
			mc_asoc_ap_play_port	= LIN1;
			return;
		}
		else if((stPresetPathInfo[0][1].asAdc0[bCh].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK]&MCDRV_SRC1_LINE1_M_ON) != 0) {
			mc_asoc_ap_play_port	= LIN1;
			return;
		}
		else if((stPresetPathInfo[0][1].asAdc0[bCh].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK]&MCDRV_SRC1_LINE1_R_ON) != 0) {
			mc_asoc_ap_play_port	= LIN1;
			return;
		}
		else if((stPresetPathInfo[0][1].asAdc0[bCh].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK]&MCDRV_SRC2_LINE2_L_ON) != 0) {
			mc_asoc_ap_play_port	= LIN2;
			return;
		}
		else if((stPresetPathInfo[0][1].asAdc0[bCh].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK]&MCDRV_SRC2_LINE2_M_ON) != 0) {
			mc_asoc_ap_play_port	= LIN2;
			return;
		}
		else if((stPresetPathInfo[0][1].asAdc0[bCh].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK]&MCDRV_SRC2_LINE2_R_ON) != 0) {
			mc_asoc_ap_play_port	= LIN2;
			return;
		}
	}
	for(bCh = 0; bCh < HP_PATH_CHANNELS; bCh++) {
		if((stPresetPathInfo[0][1].asHpOut[bCh].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK]&MCDRV_SRC1_LINE1_L_ON) != 0) {
			mc_asoc_ap_play_port	= LIN1;
			return;
		}
		else if((stPresetPathInfo[0][1].asHpOut[bCh].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK]&MCDRV_SRC1_LINE1_M_ON) != 0) {
			mc_asoc_ap_play_port	= LIN1;
			return;
		}
		else if((stPresetPathInfo[0][1].asHpOut[bCh].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK]&MCDRV_SRC1_LINE1_R_ON) != 0) {
			mc_asoc_ap_play_port	= LIN1;
			return;
		}
		else if((stPresetPathInfo[0][1].asHpOut[bCh].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK]&MCDRV_SRC2_LINE2_L_ON) != 0) {
			mc_asoc_ap_play_port	= LIN2;
			return;
		}
		else if((stPresetPathInfo[0][1].asHpOut[bCh].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK]&MCDRV_SRC2_LINE2_M_ON) != 0) {
			mc_asoc_ap_play_port	= LIN2;
			return;
		}
		else if((stPresetPathInfo[0][1].asHpOut[bCh].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK]&MCDRV_SRC2_LINE2_R_ON) != 0) {
			mc_asoc_ap_play_port	= LIN2;
			return;
		}
	}
	for(bCh = 0; bCh < SP_PATH_CHANNELS; bCh++) {
		if((stPresetPathInfo[0][1].asSpOut[bCh].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK]&MCDRV_SRC1_LINE1_L_ON) != 0) {
			mc_asoc_ap_play_port	= LIN1;
			return;
		}
		else if((stPresetPathInfo[0][1].asSpOut[bCh].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK]&MCDRV_SRC1_LINE1_M_ON) != 0) {
			mc_asoc_ap_play_port	= LIN1;
			return;
		}
		else if((stPresetPathInfo[0][1].asSpOut[bCh].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK]&MCDRV_SRC1_LINE1_R_ON) != 0) {
			mc_asoc_ap_play_port	= LIN1;
			return;
		}
		else if((stPresetPathInfo[0][1].asSpOut[bCh].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK]&MCDRV_SRC2_LINE2_L_ON) != 0) {
			mc_asoc_ap_play_port	= LIN2;
			return;
		}
		else if((stPresetPathInfo[0][1].asSpOut[bCh].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK]&MCDRV_SRC2_LINE2_M_ON) != 0) {
			mc_asoc_ap_play_port	= LIN2;
			return;
		}
		else if((stPresetPathInfo[0][1].asSpOut[bCh].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK]&MCDRV_SRC2_LINE2_R_ON) != 0) {
			mc_asoc_ap_play_port	= LIN2;
			return;
		}
	}
	for(bCh = 0; bCh < RC_PATH_CHANNELS; bCh++) {
		if((stPresetPathInfo[0][1].asRcOut[bCh].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK]&MCDRV_SRC1_LINE1_M_ON) != 0) {
			mc_asoc_ap_play_port	= LIN1;
			return;
		}
		else if((stPresetPathInfo[0][1].asRcOut[bCh].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK]&MCDRV_SRC2_LINE2_M_ON) != 0) {
			mc_asoc_ap_play_port	= LIN2;
			return;
		}
	}
	for(bCh = 0; bCh < LOUT1_PATH_CHANNELS; bCh++) {
		if((stPresetPathInfo[0][1].asLout1[bCh].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK]&MCDRV_SRC1_LINE1_L_ON) != 0) {
			mc_asoc_ap_play_port	= LIN1;
			return;
		}
		else if((stPresetPathInfo[0][1].asLout1[bCh].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK]&MCDRV_SRC1_LINE1_M_ON) != 0) {
			mc_asoc_ap_play_port	= LIN1;
			return;
		}
		else if((stPresetPathInfo[0][1].asLout1[bCh].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK]&MCDRV_SRC1_LINE1_R_ON) != 0) {
			mc_asoc_ap_play_port	= LIN1;
			return;
		}
		else if((stPresetPathInfo[0][1].asLout1[bCh].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK]&MCDRV_SRC2_LINE2_L_ON) != 0) {
			mc_asoc_ap_play_port	= LIN2;
			return;
		}
		else if((stPresetPathInfo[0][1].asLout1[bCh].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK]&MCDRV_SRC2_LINE2_M_ON) != 0) {
			mc_asoc_ap_play_port	= LIN2;
			return;
		}
		else if((stPresetPathInfo[0][1].asLout1[bCh].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK]&MCDRV_SRC2_LINE2_R_ON) != 0) {
			mc_asoc_ap_play_port	= LIN2;
			return;
		}
	}
	for(bCh = 0; bCh < LOUT2_PATH_CHANNELS; bCh++) {
		if((stPresetPathInfo[0][1].asLout2[bCh].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK]&MCDRV_SRC1_LINE1_L_ON) != 0) {
			mc_asoc_ap_play_port	= LIN1;
			return;
		}
		else if((stPresetPathInfo[0][1].asLout2[bCh].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK]&MCDRV_SRC1_LINE1_M_ON) != 0) {
			mc_asoc_ap_play_port	= LIN1;
			return;
		}
		else if((stPresetPathInfo[0][1].asLout2[bCh].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK]&MCDRV_SRC1_LINE1_R_ON) != 0) {
			mc_asoc_ap_play_port	= LIN1;
			return;
		}
		else if((stPresetPathInfo[0][1].asLout2[bCh].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK]&MCDRV_SRC2_LINE2_L_ON) != 0) {
			mc_asoc_ap_play_port	= LIN2;
			return;
		}
		else if((stPresetPathInfo[0][1].asLout2[bCh].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK]&MCDRV_SRC2_LINE2_M_ON) != 0) {
			mc_asoc_ap_play_port	= LIN2;
			return;
		}
		else if((stPresetPathInfo[0][1].asLout2[bCh].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK]&MCDRV_SRC2_LINE2_R_ON) != 0) {
			mc_asoc_ap_play_port	= LIN2;
			return;
		}
	}
}

static void init_ap_cap_port(
	void
)
{
	UINT8	bCh, bBlock;

	for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++) {
		for(bCh = 0; bCh < DIT0_PATH_CHANNELS; bCh++) {
			if((stPresetPathInfo[0][4].asDit0[bCh].abSrcOnOff[bBlock]&0x55) != 0) {
				mc_asoc_ap_cap_port	= DIO_0;
				return;
			}
		}
		for(bCh = 0; bCh < DIT1_PATH_CHANNELS; bCh++) {
			if((stPresetPathInfo[0][4].asDit1[bCh].abSrcOnOff[bBlock]&0x55) != 0) {
				mc_asoc_ap_cap_port	= DIO_1;
				return;
			}
		}
		for(bCh = 0; bCh < DIT2_PATH_CHANNELS; bCh++) {
			if((stPresetPathInfo[0][4].asDit2[bCh].abSrcOnOff[bBlock]&0x55) != 0) {
				mc_asoc_ap_cap_port	= DIO_2;
				return;
			}
		}
		for(bCh = 0; bCh < LOUT1_PATH_CHANNELS; bCh++) {
			if((stPresetPathInfo[0][4].asLout1[bCh].abSrcOnOff[bBlock]&0x55) != 0) {
				mc_asoc_ap_cap_port	= LOUT1;
				return;
			}
		}
		for(bCh = 0; bCh < LOUT2_PATH_CHANNELS; bCh++) {
			if((stPresetPathInfo[0][4].asLout2[bCh].abSrcOnOff[bBlock]&0x55) != 0) {
				mc_asoc_ap_cap_port	= LOUT2;
				return;
			}
		}
	}
}

static void init_cp_port(
	void
)
{
	UINT8	bCh, bBlock;

	for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++) {
		for(bCh = 0; bCh < DIT0_PATH_CHANNELS; bCh++) {
			if((stPresetPathInfo[0][12].asDit0[bCh].abSrcOnOff[bBlock]&0x55) != 0) {
				mc_asoc_cp_port	= DIO_0;
				return;
			}
		}
		for(bCh = 0; bCh < DIT1_PATH_CHANNELS; bCh++) {
			if((stPresetPathInfo[0][12].asDit1[bCh].abSrcOnOff[bBlock]&0x55) != 0) {
				mc_asoc_cp_port	= DIO_1;
				return;
			}
		}
		for(bCh = 0; bCh < DIT2_PATH_CHANNELS; bCh++) {
			if((stPresetPathInfo[0][12].asDit2[bCh].abSrcOnOff[bBlock]&0x55) != 0) {
				mc_asoc_cp_port	= DIO_2;
				return;
			}
		}
		for(bCh = 0; bCh < LOUT1_PATH_CHANNELS; bCh++) {
			if((stPresetPathInfo[0][13].asLout1[bCh].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]&0x55) != 0) {
				for(bCh = 0; bCh < ADC0_PATH_CHANNELS; bCh++) {
					if((stPresetPathInfo[0][12].asAdc0[bCh].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK]&MCDRV_SRC1_LINE1_L_ON) != 0) {
						mc_asoc_cp_port	= LIN1_LOUT1;
						return;
					}
					else if((stPresetPathInfo[0][12].asAdc0[bCh].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK]&MCDRV_SRC1_LINE1_M_ON) != 0) {
						mc_asoc_cp_port	= LIN1_LOUT1;
						return;
					}
					else if((stPresetPathInfo[0][12].asAdc0[bCh].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK]&MCDRV_SRC1_LINE1_R_ON) != 0) {
						mc_asoc_cp_port	= LIN1_LOUT1;
						return;
					}
					else if((stPresetPathInfo[0][12].asAdc0[bCh].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK]&MCDRV_SRC2_LINE2_L_ON) != 0) {
						mc_asoc_cp_port	= LIN2_LOUT1;
						return;
					}
					else if((stPresetPathInfo[0][12].asAdc0[bCh].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK]&MCDRV_SRC2_LINE2_M_ON) != 0) {
						mc_asoc_cp_port	= LIN2_LOUT1;
						return;
					}
					else if((stPresetPathInfo[0][12].asAdc0[bCh].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK]&MCDRV_SRC2_LINE2_R_ON) != 0) {
						mc_asoc_cp_port	= LIN2_LOUT1;
						return;
					}
				}
			}
		}
		for(bCh = 0; bCh < LOUT2_PATH_CHANNELS; bCh++) {
			if((stPresetPathInfo[0][13].asLout2[bCh].abSrcOnOff[MCDRV_SRC_DAC_L_BLOCK]&0x55) != 0) {
				for(bCh = 0; bCh < ADC0_PATH_CHANNELS; bCh++) {
					if((stPresetPathInfo[0][12].asAdc0[bCh].abSrcOnOff[MCDRV_SRC_LINE1_L_BLOCK]&MCDRV_SRC1_LINE1_L_ON) != 0) {
						mc_asoc_cp_port	= LIN1_LOUT2;
						return;
					}
					else if((stPresetPathInfo[0][12].asAdc0[bCh].abSrcOnOff[MCDRV_SRC_LINE1_M_BLOCK]&MCDRV_SRC1_LINE1_M_ON) != 0) {
						mc_asoc_cp_port	= LIN1_LOUT2;
						return;
					}
					else if((stPresetPathInfo[0][12].asAdc0[bCh].abSrcOnOff[MCDRV_SRC_LINE1_R_BLOCK]&MCDRV_SRC1_LINE1_R_ON) != 0) {
						mc_asoc_cp_port	= LIN1_LOUT2;
						return;
					}
					else if((stPresetPathInfo[0][12].asAdc0[bCh].abSrcOnOff[MCDRV_SRC_LINE2_L_BLOCK]&MCDRV_SRC2_LINE2_L_ON) != 0) {
						mc_asoc_cp_port	= LIN2_LOUT2;
						return;
					}
					else if((stPresetPathInfo[0][12].asAdc0[bCh].abSrcOnOff[MCDRV_SRC_LINE2_M_BLOCK]&MCDRV_SRC2_LINE2_M_ON) != 0) {
						mc_asoc_cp_port	= LIN2_LOUT2;
						return;
					}
					else if((stPresetPathInfo[0][12].asAdc0[bCh].abSrcOnOff[MCDRV_SRC_LINE2_R_BLOCK]&MCDRV_SRC2_LINE2_R_ON) != 0) {
						mc_asoc_cp_port	= LIN2_LOUT2;
						return;
					}
				}
			}
		}
	}
}

static void init_bt_port(
	void
)
{
	UINT8	bCh, bBlock;

	for(bBlock = 0; bBlock < SOURCE_BLOCK_NUM; bBlock++) {
		for(bCh = 0; bCh < DIT0_PATH_CHANNELS; bCh++) {
			if((stPresetPathInfo[0][2].asDit0[bCh].abSrcOnOff[bBlock]&0x55) != 0) {
				mc_asoc_bt_port	= DIO_0;
				return;
			}
		}
		for(bCh = 0; bCh < DIT1_PATH_CHANNELS; bCh++) {
			if((stPresetPathInfo[0][2].asDit1[bCh].abSrcOnOff[bBlock]&0x55) != 0) {
				mc_asoc_bt_port	= DIO_1;
				return;
			}
		}
		for(bCh = 0; bCh < DIT2_PATH_CHANNELS; bCh++) {
			if((stPresetPathInfo[0][2].asDit2[bCh].abSrcOnOff[bBlock]&0x55) != 0) {
				mc_asoc_bt_port	= DIO_2;
				return;
			}
		}
	}
}

static int mc_asoc_i2c_probe(
	struct i2c_client *client,
	const struct i2c_device_id *id)
{
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	struct snd_soc_codec	*codec;
#else
	struct mc_asoc_priv		*mc_asoc_priv;
#endif
	int		i, j;
	struct mc_asoc_data		*mc_asoc;
	int		err;

	TRACE_FUNC();

#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	/* setup codec data */
	if (!(codec	= kzalloc(sizeof(struct snd_soc_codec), GFP_KERNEL))) {
		err	= -ENOMEM;
		goto err_alloc_codec;
	}
	codec->name		= MC_ASOC_NAME;
/*	codec->owner	= THIS_MODULE;*/
	mutex_init(&codec->mutex);
	codec->dev	= &client->dev;

	if (!(mc_asoc	= kzalloc(sizeof(struct mc_asoc_data), GFP_KERNEL))) {
		err	= -ENOMEM;
		goto err_alloc_data;
	}
  #ifdef ALSA_VER_1_0_23
	codec->private_data	= mc_asoc;
  #else
	snd_soc_codec_set_drvdata(codec, mc_asoc);
  #endif
	mutex_init(&mc_asoc->mutex);

	/* setup i2c client data */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err	= -ENODEV;
		goto err_i2c;
	}
	i2c_set_clientdata(client, codec);

	codec->control_data	= client;
	codec->read			= mc_asoc_read_reg;
	codec->write		= mc_asoc_write_reg;
	codec->hw_write		= NULL;
	codec->hw_read		= NULL;
	codec->reg_cache	= kzalloc(sizeof(u16) * MC_ASOC_N_REG, GFP_KERNEL);
	if (codec->reg_cache == NULL) {
		err	= -ENOMEM;
		goto err_alloc_cache;
	}
	codec->reg_cache_size	= MC_ASOC_N_REG;
	codec->reg_cache_step	= 1;
	INIT_LIST_HEAD(&codec->dapm_widgets);
	INIT_LIST_HEAD(&codec->dapm_paths);
	codec->dai		= mc_asoc_dai;
	codec->num_dai	= ARRAY_SIZE(mc_asoc_dai);
	codec->set_bias_level	= mc_asoc_set_bias_level;
	mc_asoc_set_codec_data(codec);
	if ((err = snd_soc_register_codec(codec)) < 0) {
		goto err_reg_codec;
	}

	/* setup DAI data */
	for (i	= 0; i < ARRAY_SIZE(mc_asoc_dai); i++) {
		mc_asoc_dai[i].dev	= &client->dev;
	}
	if ((err = snd_soc_register_dais(mc_asoc_dai, ARRAY_SIZE(mc_asoc_dai))) < 0) {
		goto err_reg_dai;
	}
#else
	if (!(mc_asoc_priv	= kzalloc(sizeof(struct mc_asoc_priv), GFP_KERNEL))) {
		err	= -ENOMEM;
		goto err_alloc_priv;
	}
	mc_asoc	= &mc_asoc_priv->data;
	mutex_init(&mc_asoc->mutex);
	i2c_set_clientdata(client, mc_asoc_priv);
	if ((err = snd_soc_register_codec(&client->dev, &mc_asoc_codec_dev,
									mc_asoc_dai, ARRAY_SIZE(mc_asoc_dai))) < 0) {
		goto err_reg_codec;
	}
#endif

	mc_asoc_i2c_a	= client;

	for(i = 0; i < MC_ASOC_DSP_N_OUTPUT+MC_ASOC_DSP_N_INPUT; i++) {
		for(j = 0; j < MC_ASOC_N_DSP_BLOCK; j++) {
			mc_asoc->block_onoff[i][j]	= MC_ASOC_DSP_BLOCK_THRU;
		}
		mc_asoc->dsp_onoff[i]	= 0x8000;
		mc_asoc->ae_onoff[i]	= 0x80;
	}
	mc_asoc->dsp_onoff[0]	= 0xF;
	mc_asoc->dsp_onoff[MC_ASOC_DSP_INPUT_BASE]	= 0xF;
	mc_asoc->ae_onoff[0]	= 1;
	mc_asoc->ae_onoff[MC_ASOC_DSP_INPUT_BASE]	= 1;
	memset(&mc_asoc_vol_info_mute, 0, sizeof(MCDRV_VOL_INFO));

	init_ap_play_port();
	init_ap_cap_port();
dbg_info("mc_asoc_ap_play_port=%d, mc_asoc_ap_cap_port=%d\n", mc_asoc_ap_play_port, mc_asoc_ap_cap_port);
	init_cp_port();
dbg_info("mc_asoc_cp_port=%d", mc_asoc_cp_port);
	init_bt_port();
dbg_info("mc_asoc_bt_port=%d", mc_asoc_bt_port);

	return 0;

#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
err_reg_dai:
	snd_soc_unregister_codec(codec);
#else
	snd_soc_unregister_codec(&client->dev);
#endif

err_reg_codec:
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	kfree(codec->reg_cache);
err_alloc_cache:
#endif
	i2c_set_clientdata(client, NULL);

#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
err_i2c:
	kfree(mc_asoc);
err_alloc_data:
	kfree(codec);
err_alloc_codec:
#else
err_alloc_priv:
	kfree(mc_asoc_priv);
#endif
	dev_err(&client->dev, "err=%d: failed to probe MC_ASOC\n", err);
	return err;
}

static int mc_asoc_i2c_remove(struct i2c_client *client)
{
#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	struct snd_soc_codec *codec	= i2c_get_clientdata(client);
#endif
	struct mc_asoc_data *mc_asoc;

	TRACE_FUNC();

#if defined(ALSA_VER_1_0_23) || defined(KERNEL_2_6_35_36_COMMON)
	if (codec) {
		mc_asoc	= mc_asoc_get_mc_asoc(codec);
		snd_soc_unregister_dais(mc_asoc_dai, ARRAY_SIZE(mc_asoc_dai));
		snd_soc_unregister_codec(codec);
		mutex_destroy(&mc_asoc->mutex);
		kfree(mc_asoc);
		mutex_destroy(&codec->mutex);
		kfree(codec);
	}
#else
	mc_asoc	= &((struct mc_asoc_priv*)(i2c_get_clientdata(client)))->data;
	mutex_destroy(&mc_asoc->mutex);
	snd_soc_unregister_codec(&client->dev);
#endif

	return 0;
}

static int mc_asoc_i2c_probe_d(
	struct i2c_client *client,
	const struct i2c_device_id *id)
{
	mc_asoc_i2c_d	= client;
	return 0;
}

static int mc_asoc_i2c_remove_d(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id	mc_asoc_i2c_id_a[] = {
	{"mc_asoc_a", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, mc_asoc_i2c_id_a);

static const struct i2c_device_id	mc_asoc_i2c_id_d[] = {
	{"mc_asoc_d", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, mc_asoc_i2c_id_d);

static struct i2c_driver	mc_asoc_i2c_driver_a = {
	.driver	= {
		.name	= "mc_asoc_a",
		.owner	= THIS_MODULE,
	},
	.probe		= mc_asoc_i2c_probe,
	.remove		= mc_asoc_i2c_remove,
	.id_table	= mc_asoc_i2c_id_a,
};

static struct i2c_driver	mc_asoc_i2c_driver_d = {
	.driver	= {
		.name	= "mc_asoc_d",
		.owner	= THIS_MODULE,
	},
	.probe		= mc_asoc_i2c_probe_d,
	.remove		= mc_asoc_i2c_remove_d,
	.id_table	= mc_asoc_i2c_id_d,
};

/*
 * Module init and exit
 */
static int __init mc_asoc_init(void)
{
	int		err	= 0;

	err	= i2c_add_driver(&mc_asoc_i2c_driver_a);
	if (err >= 0) {
		err	= i2c_add_driver(&mc_asoc_i2c_driver_d);
	}

	return err;
}
module_init(mc_asoc_init);

static void __exit mc_asoc_exit(void)
{
	i2c_del_driver(&mc_asoc_i2c_driver_a);
	i2c_del_driver(&mc_asoc_i2c_driver_d);
}
module_exit(mc_asoc_exit);

MODULE_AUTHOR("Yamaha Corporation");
MODULE_DESCRIPTION("Yamaha MC ALSA SoC codec driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(MC_ASOC_DRIVER_VERSION);
