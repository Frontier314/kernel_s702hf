/*
 *  smdk_wm8994.c
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 */

#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/io.h>

#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>

#include <mach/regs-clock.h>

#include <mach/regs-pmu-4212.h>

#include "i2s.h"
#include "s3c-i2s-v2.h"

#include "../codecs/mc_yamaha/mc_asoc.h"
//#include "../codecs/mc1n/mc1n.h"

 /*
  * Default CFG switch settings to use this driver:
  *	SMDKV310: CFG5-1000, CFG7-111111
  */

 /*
  * Configure audio route as :-
  * $ amixer sset 'DAC1' on,on
  * $ amixer sset 'Right Headphone Mux' 'DAC'
  * $ amixer sset 'Left Headphone Mux' 'DAC'
  * $ amixer sset 'DAC1R Mixer AIF1.1' on
  * $ amixer sset 'DAC1L Mixer AIF1.1' on
  * $ amixer sset 'IN2L' on
  * $ amixer sset 'IN2L PGA IN2LN' on
  * $ amixer sset 'MIXINL IN2L' on
  * $ amixer sset 'AIF1ADC1L Mixer ADC/DMIC' on
  * $ amixer sset 'IN2R' on
  * $ amixer sset 'IN2R PGA IN2RN' on
  * $ amixer sset 'MIXINR IN2R' on
  * $ amixer sset 'AIF1ADC1R Mixer ADC/DMIC' on
  */

/* SMDK has a 16.934MHZ crystal attached to WM8994 */
#define SMDK_WM8994_FREQ 16934000

static int set_epll_rate(unsigned long rate)
{
	struct clk *fout_epll;

	fout_epll = clk_get(NULL, "fout_epll");
	if (IS_ERR(fout_epll)) {
		printk(KERN_ERR "%s: failed to get fout_epll\n", __func__);
		return -ENOENT;
	}

	if (rate == clk_get_rate(fout_epll))
		goto out;

	clk_set_rate(fout_epll, rate);
out:
	clk_put(fout_epll);

	return 0;
}



/* XXX BLC(bits-per-channel) --> BFS(bit clock shud be >= FS*(Bit-per-channel)*2) XXX */
/* XXX BFS --> RFS(must be a multiple of BFS)                                 XXX */
/* XXX RFS & SRC_CLK --> Prescalar Value(SRC_CLK / RFS_VAL / fs - 1)          XXX */
//static
int stuttgart_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
//	struct snd_soc_dai *cpu_dai = rtd->dai->cpu_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	unsigned int rclk, psr=1;
	int bfs, rfs, ret;
	unsigned long epll_out_rate;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	u32 format;
	
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U24:
	case SNDRV_PCM_FORMAT_S24:
		bfs = 48;
		break;
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_S16_LE:
		bfs = 32;
		break;
	default:
		return -EINVAL;
	}

	/* The Fvco for WM8580 PLLs must fall within [90,100]MHz.
	 * This criterion can't be met if we request PLL output
	 * as {8000x256, 64000x256, 11025x256}Hz.
	 * As a wayout, we rather change rfs to a minimum value that
	 * results in (params_rate(params) * rfs), and itself, acceptable
	 * to both - the CODEC and the CPU.
	 */
	switch (params_rate(params)) {
	case 16000:
	case 22050:
	case 24000:
	case 32000:
	case 44100:
	case 48000:
	case 88200:
	case 96000:
		if (bfs == 48)
			rfs = 384;
		else
			rfs = 256;
		break;
	case 64000:
		rfs = 384;
		break;
	case 8000:
	case 11025:
	case 12000:
		if (bfs == 48)
			rfs = 768;
		else
			rfs = 512;
		break;
	default:
		return -EINVAL;
	}

#ifndef SND_SAMSUNG_I2S_MASTER	/* MC-1N is I2S master */	
	rclk = params_rate(params) * rfs;

	switch (rclk) {
	case 4096000:
	case 5644800:
	case 6144000:
	case 8467200:
	case 9216000:
		psr = 8;
		break;
	case 8192000:
	case 11289600:
	case 12288000:
	case 16934400:
	case 18432000:
		psr = 4;
		break;
	case 22579200:
	case 24576000:
	case 33868800:
	case 36864000:
		psr = 2;
		break;
	case 67737600:
	case 73728000:
		psr = 1;
		break;
	default:
		printk("Not yet supported!\n");
		return -EINVAL;
	}

	epll_out_rate = rclk * psr;

	/* Set EPLL clock rate */
	ret = set_epll_rate(epll_out_rate);
	if (ret < 0)
		return ret;
#endif

	#ifdef SND_SAMSUNG_I2S_MASTER
//		ret = snd_soc_dai_set_sysclk(cpu_dai, S3C64XX_CLKSRC_PCLK,
//					0, SND_SOC_CLOCK_OUT);
		ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_RCLKSRC_0,
					0, SND_SOC_CLOCK_OUT);
	#else
//	ret = snd_soc_dai_set_sysclk(cpu_dai, S3C64XX_CLKSRC_CDCLK,
//					0, SND_SOC_CLOCK_OUT);
	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
					0, SND_SOC_CLOCK_OUT);
	#endif
	if (ret < 0)
		return ret;

	/* We use MUX for basic ops in SoC-Master mode */
//	#ifdef SND_SAMSUNG_I2S_MASTER	/* MC-1N is I2S master */	
//	ret = snd_soc_dai_set_sysclk(cpu_dai, S3C64XX_CLKSRC_MUX,
//					0, SND_SOC_CLOCK_OUT);
//	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_RCLKSRC_1,
//					0, SND_SOC_CLOCK_OUT);

//	#else
//	ret = snd_soc_dai_set_sysclk(cpu_dai, S3C64XX_CLKSRC_MUX,
//					0, SND_SOC_CLOCK_IN);
//	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_RCLKSRC_1,
//					0, SND_SOC_CLOCK_IN);
//	#endif
#ifndef SND_SAMSUNG_I2S_MASTER	/* MC-1N is I2S master */	
//	if (ret < 0)
//		return ret;

//	ret = snd_soc_dai_set_clkdiv(cpu_dai, S3C_I2SV2_DIV_PRESCALER, psr-1);
//	if (ret < 0)
//		return ret;

	ret = snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_BCLK, bfs);
	if (ret < 0)
		return ret;

//	ret = snd_soc_dai_set_clkdiv(cpu_dai, S3C_I2SV2_DIV_RCLK, rfs);
//	if (ret < 0)
//		return ret;
#endif

#ifdef SND_SAMSUNG_I2S_MASTER	/* MC-1N is I2S master */	
	format = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBM_CFM |		SND_SOC_DAIFMT_NB_NF;
#else	/* MC-1N is slave */	
	format = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS |		SND_SOC_DAIFMT_NB_NF;
#endif
	
	ret = snd_soc_dai_set_fmt(codec_dai, format);	
	if (ret < 0) {
		pr_info("a500: codec set_fmt %d\n", ret);	
		return ret;
	}	
	ret = snd_soc_dai_set_fmt(cpu_dai, format);
	if (ret < 0) {	
		pr_info("a500: cpu set_fmt %d\n", ret);	
		return ret;	
	}


	if (ret < 0) {		
		pr_info("a500: cpu set_sysclk %d\n", ret);	
		return ret;	
	}	/* set codec BCLK */	
//	ret = snd_soc_dai_set_clkdiv(		codec_dai,		
//								MC1N_BCLK_MULT,	
//								MCDRV_BCKFS_32/*MC1N_LRCK_X64*/);	
	ret = snd_soc_dai_set_clkdiv(		codec_dai,		
								MC_ASOC_BCLK_MULT,	
								MC_ASOC_LRCK_X32);	

	if (ret < 0) {	
		pr_info("a500: codec set_clkdiv %d\n", ret);	
		return ret;	
	}
	return 0;
}
#if 0
/* XXX BLC(bits-per-channel) --> BFS(bit clock shud be >= FS*(Bit-per-channel)*2) XXX */
/* XXX BFS --> RFS(must be a multiple of BFS)                                 XXX */
/* XXX RFS & SRC_CLK --> Prescalar Value(SRC_CLK / RFS_VAL / fs - 1)          XXX */
//static

int stuttgart_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	unsigned int rclk;
	int bfs, psr, rfs, ret;
	unsigned long epll_out_rate;
	u32 format;
	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_U24:
	case SNDRV_PCM_FORMAT_S24:
		bfs = 48;
		break;
	case SNDRV_PCM_FORMAT_U16_LE:
	case SNDRV_PCM_FORMAT_S16_LE:
		bfs = 32;
		break;
	default:
		return -EINVAL;
	}

	switch (params_rate(params)) {
	case 16000:
	case 22050:
	case 24000:
	case 32000:
	case 44100:
	case 48000:
	case 88200:
	case 96000:
		if (bfs == 48)
			rfs = 384;
		else
			rfs = 256;
		break;
	case 64000:
		rfs = 384;
		break;
	case 8000:
	case 11025:
	case 12000:
		if (bfs == 48)
			rfs = 768;
		else
			rfs = 512;
		break;
	default:
		return -EINVAL;
	}

	rclk = params_rate(params) * rfs;

	switch (rclk) {
	case 4096000:
	case 5644800:
	case 6144000:
	case 8467200:
	case 9216000:
		psr = 8;
		break;
	case 8192000:
	case 11289600:
	case 12288000:
	case 16934400:
	case 18432000:
		psr = 4;
		break;
	case 22579200:
	case 24576000:
	case 33868800:
	case 36864000:
		psr = 2;
		break;
	case 67737600:
	case 73728000:
		psr = 1;
		break;
	default:
		printk("Not yet supported!\n");
		return -EINVAL;
	}

	set_epll_rate(rclk * psr);

	ret = snd_soc_dai_set_fmt(codec_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_fmt(cpu_dai, SND_SOC_DAIFMT_I2S
					| SND_SOC_DAIFMT_NB_NF
					| SND_SOC_DAIFMT_CBS_CFS);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_sysclk(cpu_dai, SAMSUNG_I2S_CDCLK,
					0, SND_SOC_CLOCK_OUT);
	if (ret < 0)
		return ret;

	ret = snd_soc_dai_set_clkdiv(cpu_dai, SAMSUNG_I2S_DIV_BCLK, bfs);
	if (ret < 0)
		return ret;


#if 0	/* MC-1N is I2S master */	
	format = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBM_CFM |		SND_SOC_DAIFMT_NB_NF;
#else	/* MC-1N is slave */	
	format = SND_SOC_DAIFMT_I2S | SND_SOC_DAIFMT_CBS_CFS |		SND_SOC_DAIFMT_NB_NF;
#endif
	
	ret = snd_soc_dai_set_fmt(codec_dai, format);	
	if (ret < 0) {
		pr_info("a500: codec set_fmt %d\n", ret);	
		return ret;
	}	
	ret = snd_soc_dai_set_fmt(cpu_dai, format);
	if (ret < 0) {	
		pr_info("a500: cpu set_fmt %d\n", ret);	
		return ret;	
	}


	if (ret < 0) {		
		pr_info("a500: cpu set_sysclk %d\n", ret);	
		return ret;	
	}	/* set codec BCLK */	
	ret = snd_soc_dai_set_clkdiv(		codec_dai,		
								MC_ASOC_BCLK_MULT,	
								MC_ASOC_LRCK_X32);	
	if (ret < 0) {	
		pr_info("a500: codec set_clkdiv %d\n", ret);	
		return ret;	
	}
	return 0;
}
#endif
/*
 * SMDK WM8994 DAI operations.
 */
static struct snd_soc_ops stuttgart_ops = {
	.hw_params = stuttgart_hw_params,
};


/* SMDK Playback widgets */
static const struct snd_soc_dapm_widget mc_yamaha_dapm_widgets_pbk[] = {
	SND_SOC_DAPM_HP("Front", NULL),
	SND_SOC_DAPM_HP("Center+Sub", NULL),
	SND_SOC_DAPM_HP("Rear", NULL),
};

/* SMDK Capture widgets */
static const struct snd_soc_dapm_widget mc_yamaha_dapm_widgets_cpt[] = {
	SND_SOC_DAPM_MIC("MicIn", NULL),
	SND_SOC_DAPM_LINE("LineIn", NULL),
};

/* SMDK-PAIFTX connections */
static const struct snd_soc_dapm_route audio_map_tx[] = {
	/* MicIn feeds AINL */
	{"AINL", NULL, "MicIn"},

	/* LineIn feeds AINL/R */
	{"AINL", NULL, "LineIn"},
	{"AINR", NULL, "LineIn"},
};

/* SMDK-PAIFRX connections */
static const struct snd_soc_dapm_route audio_map_rx[] = {
	/* Front Left/Right are fed VOUT1L/R */
	{"Front", NULL, "VOUT1L"},
	{"Front", NULL, "VOUT1R"},

	/* Center/Sub are fed VOUT2L/R */
	{"Center+Sub", NULL, "VOUT2L"},
	{"Center+Sub", NULL, "VOUT2R"},

	/* Rear Left/Right are fed VOUT3L/R */
	{"Rear", NULL, "VOUT3L"},
	{"Rear", NULL, "VOUT3R"},
};

static int stuttgart_mc_yamaha_init(struct snd_soc_pcm_runtime *rtd)
{
	printk("stuttgart_mc_yamaha_init\n");
	struct snd_soc_codec *codec = rtd->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;

#if 0
	/* Add smdk specific Capture widgets */
	snd_soc_dapm_new_controls(dapm, mc_yamaha_dapm_widgets_cpt,
				  ARRAY_SIZE(mc_yamaha_dapm_widgets_cpt));

	/* Set up PAIFTX audio path */
	snd_soc_dapm_add_routes(dapm, audio_map_tx, ARRAY_SIZE(audio_map_tx));

	/* Enabling the microphone requires the fitting of a 0R
	 * resistor to connect the line from the microphone jack.
	 */
	snd_soc_dapm_disable_pin(dapm, "MicIn");

	/* signal a DAPM event */
	snd_soc_dapm_sync(dapm);
#endif
	return 0;
}
 
static struct snd_soc_dai_link stuttgart_dai[] = {
	{ 
		.name = "mc_asoc AIF1",
		.stream_name = "Pri_Dai",
		.cpu_dai_name = "samsung-i2s.0",
		.codec_dai_name = "mc_asoc-da0i",
		.platform_name = "samsung-audio",
		.codec_name = "mc_asoc_a.5-003a",
		.init = stuttgart_mc_yamaha_init,
		.ops = &stuttgart_ops,
	},
        {
                .name = "Sec_FIFO TX",
                .stream_name = "Sec_Dai",
                .cpu_dai_name = "samsung-i2s.4",
                .codec_dai_name = "mc_asoc-da0i",
        #if defined(CONFIG_SND_SAMSUNG_NORMAL) || defined(CONFIG_SND_SAMSUNG_RP)
                .platform_name = "samsung-audio",
	#else
                .platform_name = "samsung-audio-idma",
	#endif

                .codec_name = "mc_asoc_a.5-003a",
                .init = stuttgart_mc_yamaha_init,
                .ops = &stuttgart_ops,
        },
 
};

#if 0		
static struct snd_soc_dai_link panther_dai[] = {
	[0] = { /* Primary DAI i/f */
		.name = "MC_YAMAHA TX",
		.stream_name = "Playback",
		.cpu_dai_name = "samsung-i2s.0",
//		.codec_dai_name = "mc1n-aif1",
		.codec_dai_name = "mc_asoc-da0i",
		.platform_name = "samsung-audio",
//		.codec_name = "mc1n.0-003a",
		.codec_name = "mc_asoc_a",
		.init = panther_mc1n_init,
		.ops = &panther_ops,
	}, {
		.name = "MC_YAMAHA RX",
		.stream_name = "Capture",
		.cpu_dai_name = "samsung-i2s.0",
//		.codec_dai_name = "mc1n-aif1",
		.codec_dai_name = "mc_asoc-da0i",		
		.platform_name = "samsung-audio",
//		.codec_name = "mc1n.0-003a",
		.codec_name = "mc_asoc_a",		
		.init = panther_mc1n_init,
		.ops = &panther_ops,
	},
};
#endif

static struct snd_soc_card stuttgart = {
	.name = "mc_yamaha",
	.dai_link = stuttgart_dai,
	.num_links = ARRAY_SIZE(stuttgart_dai),
};


static struct platform_device *stuttgart_snd_device;

static int __init stuttgart_audio_init(void)
{
	int ret;
	u32 val;
	u32 reg;

	printk("%s\n", __func__);


#if 1


//	val = __raw_readl(EXYNOS4_CLKOUT_CMU_CPU);
//	val &= ~0xffff;
//	val |= 0x1904;
//	__raw_writel(val, EXYNOS4_CLKOUT_CMU_CPU);

	val = __raw_readl(S5P_PMU_DEBUG);
	val &= ~0xf00;
	val |= 0x900;
	__raw_writel(val, S5P_PMU_DEBUG);

#endif
	
#if 0
#include <mach/map.h>
#define S3C_VA_AUDSS	S3C_ADDR(0x01600000)	/* Audio SubSystem */
#include <mach/regs-audss.h>
	/* We use I2SCLK for rate generation, so set EPLLout as
	 * the parent of I2SCLK.
	 */
	val = readl(S5P_CLKSRC_AUDSS);
	val &= ~(0x3<<2);
	val |= (1<<0);
	writel(val, S5P_CLKSRC_AUDSS);

	val = readl(S5P_CLKGATE_AUDSS);
	val |= (0x7f<<0);
	writel(val, S5P_CLKGATE_AUDSS);

#if 0 /* For Low Power Audio*/
	/* yman.seo CLKOUT is prior to CLK_OUT of SYSCON. XXTI & XUSBXTI work in sleep mode */
	reg = __raw_readl(S5P_OTHERS);
	reg &= ~(0x0003 << 8);
	reg |= 0x0003 << 8;	/* XUSBXTI */
	__raw_writel(reg, S5P_OTHERS);
#else
	/* yman.seo Set XCLK_OUT as 24MHz (XUSBXTI -> 24MHz) */
	reg = __raw_readl(S5P_CLK_OUT);
	reg &= ~S5P_CLKOUT_CLKSEL_MASK;
	reg |= S5P_CLKOUT_CLKSEL_XUSBXTI;
	reg &= ~S5P_CLKOUT_DIV_MASK;
	reg |= 0x0001 << S5P_CLKOUT_DIV_SHIFT;	/* DIVVAL = 1, Ratio = 2 = DIVVAL + 1 */
	__raw_writel(reg, S5P_CLK_OUT);

	reg = __raw_readl(S5P_OTHERS);
	reg &= ~(0x0003 << 8);
	reg |= 0x0000 << 8;	/* Clock from SYSCON */
	__raw_writel(reg, S5P_OTHERS);
#endif
#endif
	//printk("+++dai_link:%p\n", panther_dai[0].init);
	stuttgart_snd_device = platform_device_alloc("soc-audio", -1);
	if (!stuttgart_snd_device)
		return -ENOMEM;

	platform_set_drvdata(stuttgart_snd_device, &stuttgart);
	ret = platform_device_add(stuttgart_snd_device);

	if (ret)
		platform_device_put(stuttgart_snd_device);

       printk("---%s---\n", __func__);
	printk("+++dai_link:%p\n", stuttgart_dai[0].init);
	return ret;
}
module_init(stuttgart_audio_init);

static void __exit stuttgart_audio_exit(void)
{
	printk("%s\n", __func__);

	platform_device_unregister(stuttgart_snd_device);
}
module_exit(stuttgart_audio_exit);

MODULE_DESCRIPTION("ALSA SoC Stuttgart MC_YAMAHA");
MODULE_LICENSE("GPL");
