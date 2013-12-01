/*
 * MC ASoC codec driver - private header
 *
 * Copyright (c) 2011-2012 Yamaha Corporation
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#ifndef MC_ASOC_PRIV_H
#define MC_ASOC_PRIV_H

#include <sound/soc.h>
#include <linux/wakelock.h>
#include "mcdriver.h"

#undef	MC_ASOC_TEST

/*
 * Virtual registers
 */
enum {
	MC_ASOC_DVOL_AD0,
	MC_ASOC_DVOL_AD1,
	MC_ASOC_DVOL_AENG6,
	MC_ASOC_DVOL_PDM,
	MC_ASOC_DVOL_DTMF_B,
	MC_ASOC_DVOL_DIR0,
	MC_ASOC_DVOL_DIR1,
	MC_ASOC_DVOL_DIR2,
	MC_ASOC_DVOL_AD0_ATT,
	MC_ASOC_DVOL_AD1_ATT,
	MC_ASOC_DVOL_DTMF,
	MC_ASOC_DVOL_DIR0_ATT,
	MC_ASOC_DVOL_DIR1_ATT,
	MC_ASOC_DVOL_DIR2_ATT,
	MC_ASOC_DVOL_SIDETONE,
	MC_ASOC_DVOL_DAC_MASTER,
	MC_ASOC_DVOL_DAC_VOICE,
	MC_ASOC_DVOL_DAC_ATT,
	MC_ASOC_DVOL_DIT0,
	MC_ASOC_DVOL_DIT1,
	MC_ASOC_DVOL_DIT2,
	MC_ASOC_DVOL_DIT2_1,

	MC_ASOC_DVOL_MASTER,
	MC_ASOC_DVOL_VOICE,
	MC_ASOC_DVOL_AIN,
	MC_ASOC_DVOL_AINPLAYBACK,

	MC_ASOC_AVOL_AD0,
	MC_ASOC_AVOL_AD1,
	MC_ASOC_AVOL_LIN1_BYPASS,
	MC_ASOC_AVOL_LIN2_BYPASS,
	MC_ASOC_AVOL_MIC1,
	MC_ASOC_AVOL_MIC2,
	MC_ASOC_AVOL_MIC3,
	MC_ASOC_AVOL_HP,
	MC_ASOC_AVOL_SP,
	MC_ASOC_AVOL_RC,
	MC_ASOC_AVOL_LOUT1,
	MC_ASOC_AVOL_LOUT2,

	MC_ASOC_AVOL_AIN,

	MC_ASOC_AVOL_MIC1_GAIN,
	MC_ASOC_AVOL_MIC2_GAIN,
	MC_ASOC_AVOL_MIC3_GAIN,
	MC_ASOC_AVOL_HP_GAIN,

	MC_ASOC_VOICE_RECORDING,

	MC_ASOC_DVOL_AD0_MIX2,
	MC_ASOC_DVOL_AD1_MIX2,
	MC_ASOC_DVOL_DIR0_MIX2,
	MC_ASOC_DVOL_DIR1_MIX2,
	MC_ASOC_DVOL_DIR2_MIX2,
	MC_ASOC_DVOL_PDM_MIX2,
	MC_ASOC_DVOL_MIX_MIX2,

	MC_ASOC_AUDIO_MODE_PLAY,
	MC_ASOC_AUDIO_MODE_CAP,
	MC_ASOC_OUTPUT_PATH,
	MC_ASOC_INPUT_PATH,
	MC_ASOC_INCALL_MIC_SP,
	MC_ASOC_INCALL_MIC_RC,
	MC_ASOC_INCALL_MIC_HP,
	MC_ASOC_INCALL_MIC_LO1,
	MC_ASOC_INCALL_MIC_LO2,

	MC_ASOC_MAINMIC_PLAYBACK_PATH,
	MC_ASOC_SUBMIC_PLAYBACK_PATH,
	MC_ASOC_2MIC_PLAYBACK_PATH,
	MC_ASOC_HSMIC_PLAYBACK_PATH,
	MC_ASOC_BTMIC_PLAYBACK_PATH,
	MC_ASOC_LIN1_PLAYBACK_PATH,
	MC_ASOC_LIN2_PLAYBACK_PATH,

	MC_ASOC_PARAMETER_SETTING,

	MC_ASOC_DTMF_CONTROL,
	MC_ASOC_DTMF_OUTPUT,

#ifdef MC_ASOC_TEST
	MC_ASOC_MAIN_MIC,
	MC_ASOC_SUB_MIC,
	MC_ASOC_HS_MIC,
	MC_ASOC_MIC1_BIAS,
	MC_ASOC_MIC2_BIAS,
	MC_ASOC_MIC3_BIAS,
	MC_ASOC_CAP_PATH_8KHZ,
#endif

	MC_ASOC_N_REG
};
#define MC_ASOC_N_VOL_REG			(MC_ASOC_DVOL_MIX_MIX2+1)


#define MC_ASOC_SWITCH_OFF			0
#define MC_ASOC_SWITCH_ON			1

#define MC_ASOC_CDSP_AE_EX_OFF		0
#define MC_ASOC_CDSP_ON				1
#define MC_ASOC_AE_EX_ON			2

#define MC_ASOC_AE_NONE				(0)
#define MC_ASOC_AE_PLAY				(1)
#define MC_ASOC_AE_CAP				(2)

#define MC_ASOC_CDSP_NONE			(0)
#define MC_ASOC_CDSP_PLAY			(1)
#define MC_ASOC_CDSP_CAP			(2)

#define MC_ASOC_AUDIO_MODE_OFF			(0)
#define MC_ASOC_AUDIO_MODE_AUDIO		(1)
#define MC_ASOC_AUDIO_MODE_RINGTONG		(1)
#define MC_ASOC_AUDIO_MODE_INCALL		(2)
#define MC_ASOC_AUDIO_MODE_AUDIO_INCALL	(3)
#define MC_ASOC_AUDIO_MODE_INCOMM		(4)

#define MC_ASOC_OUTPUT_PATH_SP		(0)
#define MC_ASOC_OUTPUT_PATH_RC		(1)
#define MC_ASOC_OUTPUT_PATH_HP		(2)
#define MC_ASOC_OUTPUT_PATH_HS		(3)
#define MC_ASOC_OUTPUT_PATH_LO1		(4)
#define MC_ASOC_OUTPUT_PATH_LO2		(5)
#define MC_ASOC_OUTPUT_PATH_BT		(6)
#define MC_ASOC_OUTPUT_PATH_SP_RC	(7)
#define MC_ASOC_OUTPUT_PATH_SP_HP	(8)
#define MC_ASOC_OUTPUT_PATH_SP_LO1	(9)
#define MC_ASOC_OUTPUT_PATH_SP_LO2	(10)
#define MC_ASOC_OUTPUT_PATH_SP_BT	(11)
#define MC_ASOC_OUTPUT_PATH_LO1_RC	(12)
#define MC_ASOC_OUTPUT_PATH_LO1_HP	(13)
#define MC_ASOC_OUTPUT_PATH_LO1_BT	(14)
#define MC_ASOC_OUTPUT_PATH_LO2_RC	(15)
#define MC_ASOC_OUTPUT_PATH_LO2_HP	(16)
#define MC_ASOC_OUTPUT_PATH_LO2_BT	(17)
#define MC_ASOC_OUTPUT_PATH_LO1_LO2	(18)
#define MC_ASOC_OUTPUT_PATH_LO2_LO1	(19)

#define MC_ASOC_INPUT_PATH_MAINMIC	(0)
#define MC_ASOC_INPUT_PATH_SUBMIC	(1)
#define MC_ASOC_INPUT_PATH_2MIC		(2)
#define MC_ASOC_INPUT_PATH_HS		(3)
#define MC_ASOC_INPUT_PATH_BT		(4)
#define MC_ASOC_INPUT_PATH_VOICECALL	(5)
#define MC_ASOC_INPUT_PATH_VOICEUPLINK	(6)
#define MC_ASOC_INPUT_PATH_VOICEDOWNLINK	(7)
#define MC_ASOC_INPUT_PATH_LIN1		(8)
#define MC_ASOC_INPUT_PATH_LIN2		(9)

#define MC_ASOC_INCALL_MIC_MAINMIC	(0)
#define MC_ASOC_INCALL_MIC_SUBMIC	(1)
#define MC_ASOC_INCALL_MIC_2MIC		(2)

#define MC_ASOC_DTMF_OUTPUT_SP		(0)
#define MC_ASOC_DTMF_OUTPUT_NORMAL	(1)


#define MC_ASOC_MAX_EXPARAM			(50)
#define MC_ASOC_MAX_CDSPPARAM		(128)

#define mc_asoc_i2c_read_byte(c,r)	i2c_smbus_read_byte_data((c), (r)<<1)

#define MC_ASOC_MC1_HW_ID			0x79
#define MC_ASOC_MC3_HW_ID			0x46
#define MC_ASOC_MA1_HW_ID			0x71
#define MC_ASOC_MA2_HW_ID			0x44

#define MC_ASOC_DSP_BLOCK_BEX		(0)
#define MC_ASOC_DSP_BLOCK_WIDE		(1)
#define MC_ASOC_DSP_BLOCK_DRC		(2)
#define MC_ASOC_DSP_BLOCK_EQ5		(3)
#define MC_ASOC_DSP_BLOCK_EQ3		(4)
#define MC_ASOC_DSP_BLOCK_EXTENSION	(5)
#define MC_ASOC_N_DSP_BLOCK			(6)

#define MC_ASOC_DSP_BLOCK_OFF		(0)
#define MC_ASOC_DSP_BLOCK_ON		(1)
#define MC_ASOC_DSP_BLOCK_THRU		(2)

#define MC_ASOC_AENG_OFF			(0)
#define MC_ASOC_AENG_ON				(1)

#define MC_ASOC_DSP_N_OUTPUT		(7)
#define MC_ASOC_DSP_INPUT_BASE		(MC_ASOC_DSP_N_OUTPUT)
#define MC_ASOC_DSP_INPUT_EXT		(MC_ASOC_DSP_INPUT_BASE+1)
#define MC_ASOC_DSP_N_INPUT			(2)
#define MC_ASOC_DSP_N_VOICECALL		(14)

/*
 * Driver private data structure
 */
struct mc_asoc_setup {
	MCDRV_INIT_INFO	init;
	MCDRV_PLL_INFO	pll;
	unsigned char	pcm_extend[IOPORT_NUM];
	unsigned char	pcm_hiz_redge[IOPORT_NUM];
	unsigned char	pcm_hperiod[IOPORT_NUM];
	unsigned char	slot[IOPORT_NUM][SNDRV_PCM_STREAM_LAST+1][DIO_CHANNELS];
};

struct mc_asoc_port_params {
	UINT8	rate;
	UINT8	bits[SNDRV_PCM_STREAM_LAST+1];
	UINT8	pcm_mono[SNDRV_PCM_STREAM_LAST+1];
	UINT8	pcm_order[SNDRV_PCM_STREAM_LAST+1];
	UINT8	pcm_law[SNDRV_PCM_STREAM_LAST+1];
	UINT8	master;
	UINT8	inv;
	UINT8	format;
	UINT8	bckfs;
	UINT8	pcm_clkdown;
	UINT8	channels;
	UINT8	stream;							/* bit0: Playback, bit1: Capture */
};

#define AE_EX_PROGRAM_SIZE	1024
typedef struct {
	UINT8			prog[AE_EX_PROGRAM_SIZE];
	UINT32			size;
	MCDRV_EXPARAM	exparam[MC_ASOC_MAX_EXPARAM];
	UINT8			param_count;
} MC_AE_EX_STORE;

typedef struct {
	UINT8			fw_ver_major[MC_ASOC_DSP_N_VOICECALL];
	UINT8			fw_ver_minor[MC_ASOC_DSP_N_VOICECALL];
	UINT8			onoff[MC_ASOC_DSP_N_VOICECALL];
	MCDRV_CDSPPARAM	cdspparam[MC_ASOC_DSP_N_VOICECALL][MC_ASOC_MAX_CDSPPARAM];
	UINT8			param_count[MC_ASOC_DSP_N_VOICECALL];
} MC_DSP_VOICECALL;

struct mc_asoc_data {
	struct mutex				mutex;
	struct mc_asoc_setup		setup;
	struct mc_asoc_port_params	port[IOPORT_NUM];
	struct snd_hwdep			*hwdep;
	struct delayed_work         headset_delay;
	struct delayed_work         headphone_delay;
	struct wake_lock            switch_wake_lock;
	MCDRV_PLL_INFO				pll_store;
	MCDRV_PATH_INFO				path_store;
	MCDRV_VOL_INFO				vol_store;
	MCDRV_DIO_INFO				dio_store;
	MCDRV_DAC_INFO				dac_store;
	MCDRV_ADC_INFO				adc_store;
	MCDRV_ADC_EX_INFO			adc_ex_store;
	MCDRV_SP_INFO				sp_store;
	MCDRV_DNG_INFO				dng_store;
	MCDRV_SYSEQ_INFO			syseq_store;
	MCDRV_AE_INFO				ae_store;
	MCDRV_PDM_INFO				pdm_store;
	MCDRV_PDM_EX_INFO			pdm_ex_store;
	MCDRV_DTMF_INFO				dtmf_store;
	MCDRV_DITSWAP_INFO			ditswap_store;
	MCDRV_HSDET_INFO			hsdet_store;
	MC_AE_EX_STORE				ae_ex_store;
	struct {
		UINT8			*prog;
		UINT32			size;
		MCDRV_CDSPPARAM	cdspparam[MC_ASOC_MAX_CDSPPARAM];
		UINT8			param_count;
	}							cdsp_store;

	UINT16						dsp_onoff[MC_ASOC_DSP_N_OUTPUT+MC_ASOC_DSP_N_INPUT];
	UINT16						vendordata[MC_ASOC_DSP_N_OUTPUT+MC_ASOC_DSP_N_INPUT];
	UINT8						block_onoff[MC_ASOC_DSP_N_OUTPUT+MC_ASOC_DSP_N_INPUT][MC_ASOC_N_DSP_BLOCK];
	UINT8						ae_onoff[MC_ASOC_DSP_N_OUTPUT+MC_ASOC_DSP_N_INPUT];
	MCDRV_AE_INFO				ae_info[MC_ASOC_DSP_N_OUTPUT+MC_ASOC_DSP_N_INPUT];
	MC_AE_EX_STORE				aeex_prm[MC_ASOC_DSP_N_OUTPUT+MC_ASOC_DSP_N_INPUT];
	MC_DSP_VOICECALL			dsp_voicecall;
	UINT8						mc_asoc_power_status;
	UINT8						audio_mode;
	UINT8						switch_state;
	UINT8						switch_pre_state;
};

extern struct i2c_client	*mc_asoc_get_i2c_client(int slave);
extern int	mc_asoc_map_drv_error(int err);

/*
 * For debugging
 */
#ifdef CONFIG_SND_SOC_MC_YAMAHA_DEBUG

#define dbg_info(format, arg...)	snd_printd(KERN_INFO format, ## arg)
#define TRACE_FUNC()				snd_printd(KERN_INFO "<trace> %s()\n", __FUNCTION__)
#define _McDrv_Ctrl					McDrv_Ctrl_dbg

extern SINT32	McDrv_Ctrl_dbg(UINT32 dCmd, void *pvPrm, UINT32 dPrm);

#else

#define dbg_info(format, arg...)
#define TRACE_FUNC()
#define _McDrv_Ctrl McDrv_Ctrl

#endif /* CONFIG_SND_SOC_MC_YAMAHA_DEBUG */

#endif /* MC_ASOC_PRIV_H */
