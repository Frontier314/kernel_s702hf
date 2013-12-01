/* linux/arch/arm/plat-samsung/dev-hsmmc3.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Copyright (c) 2008 Simtec Electronics
 *	Ben Dooks <ben@simtec.co.uk>
 *	http://armlinux.simtec.co.uk/
 *
 * Based on arch/arm/plat-samsung/dev-hsmmc1.c
 *
 * Samsung device definition for hsmmc device 3
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/mmc/host.h>

#include <mach/map.h>
#include <plat/sdhci.h>
#include <plat/devs.h>

#ifdef CONFIG_BCM4330_DRIVER
#include <linux/delay.h>
#include <linux/skbuff.h>
#include <mach/gpio.h>
#include <plat/gpio-cfg.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h> 
#include <asm/mach/arch.h>
#include <asm/mach/irq.h>
#include <linux/wlan_plat.h>
#include <mach/regs-gpio.h>
#include <linux/regulator/machine.h>
#endif
#define S3C_SZ_HSMMC	(0x1000)

static struct resource s3c_hsmmc3_resource[] = {
	[0] = {
		.start	= S3C_PA_HSMMC3,
		.end	= S3C_PA_HSMMC3 + S3C_SZ_HSMMC - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= IRQ_HSMMC3,
		.end	= IRQ_HSMMC3,
		.flags	= IORESOURCE_IRQ,
	}
};
#ifdef CONFIG_BCM4330_DRIVER
bool wifi_on= 0;
static int bcm4329_wifi_cd = 0;
static struct platform_device *wifi_status_cb_devid;
static void (*wifi_status_cb) (struct platform_device *dev, int state);
int bcm4329_wifi_set_carddetect(int val)
{
	printk("%s: %d\n", __func__, val);
	wifi_on=val;
	bcm4329_wifi_cd = val;
	if (wifi_status_cb) {
		wifi_status_cb(wifi_status_cb_devid, val);
	} else
		printk("%s: Nobody to notify\n", __func__);
	return 0;
}
EXPORT_SYMBOL(wifi_on);
static int bcm4329_wifi_power_state;

#define WL_REG_ON (EXYNOS4_GPF3(3))
#define WL_REG_ON_DES "GPF3"

int bcm4329_wifi_power(int on)
{
	int ret;
	struct regulator *wlan32k_regulator;
	static int regulator_flag = 0;

	printk("%s: %d\n", __func__, on);

	/*stuttgart, enable max77676 32khz for wlan*/
	wlan32k_regulator = regulator_get(NULL, "wlan_32khz");
	if(IS_ERR(wlan32k_regulator)){
		pr_err("%s: failed to get %s\n", __func__, "wlan_32khz");
		return -1;	
	};
	/*end*/

	mdelay(100);
		
	if(on)
	{
		/*stuttgart, enable max77676 32khz for wlan*/
		if(!regulator_flag) {
			regulator_enable(wlan32k_regulator);
			regulator_flag = 1;
		}
		/*end*/
/*		ret = gpio_request(WL_REG_ON, WL_REG_ON_DES);
		if (ret)
			printk(KERN_ERR "#### failed to request GPK3-2\n ");
*/              s3c_gpio_cfgpin(WL_REG_ON, S3C_GPIO_OUTPUT);
		s3c_gpio_setpull(WL_REG_ON, S3C_GPIO_PULL_NONE);
		gpio_set_value(WL_REG_ON, 1);
		s5p_gpio_set_pd_cfg(WL_REG_ON, S5P_GPIO_PD_OUTPUT1);
		s5p_gpio_set_pd_pull(WL_REG_ON, S5P_GPIO_PD_UPDOWN_DISABLE);

/*
		ret = gpio_request(EXYNOS4_GPX2(7), "GPX2");
		if (ret)
			printk(KERN_ERR "#### failed to request GPX2-7\n ");			
		s3c_gpio_cfgpin(EXYNOS4_GPX2(7), S3C_GPIO_SFN(0x2));
		s3c_gpio_setpull	(EXYNOS4_GPX2(7),S3C_GPIO_PULL_NONE);
*/
		//irq_set_irq_wake(gpio_to_irq(EXYNOS4_GPX2(7)), 1);
	}
	else
	{
		s3c_gpio_cfgpin(WL_REG_ON, S3C_GPIO_INPUT);
		s3c_gpio_setpull(WL_REG_ON, S3C_GPIO_PULL_DOWN);
		s5p_gpio_set_pd_cfg(WL_REG_ON, S5P_GPIO_PD_INPUT);
		s5p_gpio_set_pd_pull(WL_REG_ON, S5P_GPIO_PD_DOWN_ENABLE);
		
		//irq_set_irq_wake(gpio_to_irq(EXYNOS4_GPX2(7)), 0);
/*
		s3c_gpio_cfgpin(EXYNOS4_GPX2(7), S3C_GPIO_INPUT);             
		s3c_gpio_setpull	(EXYNOS4_GPX2(7),S3C_GPIO_PULL_DOWN);	
*/

		/*gpio_free(WL_REG_ON);*/
		/*gpio_free(EXYNOS4_GPX2(7));*/
		/*stuttgart, disable max77676 32khz for wlan*/
		if(regulator_flag) {
			regulator_disable(wlan32k_regulator);
			regulator_flag = 0;
		}
		/*end*/
		
	}

	/*stuttgart, put regulator*/
	regulator_put(wlan32k_regulator);
	/*end*/

	mdelay(200);
	bcm4329_wifi_power_state = on;
	return 0;
}

static int bcm4329_wifi_reset_state;
int bcm4329_wifi_reset(int on)
{
	printk("%s: do nothing\n", __func__);
	bcm4329_wifi_reset_state = on;
	return 0;
}

static void *bcm4329_wifi_mem_prealloc(int section, unsigned long size);
int bcm4329_wifi_status_register(
		void (*cb)(struct platform_device *dev, int state))
{
	if (wifi_status_cb)
		return -EBUSY;
	wifi_status_cb = cb;
	wifi_status_cb_devid = &s3c_device_hsmmc3;
	return 0;
}
#endif
static u64 s3c_device_hsmmc3_dmamask = 0xffffffffUL;

struct s3c_sdhci_platdata s3c_hsmmc3_def_platdata = {
	.max_width	= 4,
	.host_caps	= (MMC_CAP_4_BIT_DATA |
#if defined(CONFIG_MMC_CH3_CLOCK_GATING)
			MMC_CAP_CLOCK_GATING |
#endif
			   MMC_CAP_MMC_HIGHSPEED | MMC_CAP_SD_HIGHSPEED),
	.clk_type	= S3C_SDHCI_CLK_DIV_INTERNAL,
};

struct platform_device s3c_device_hsmmc3 = {
	.name		= "s3c-sdhci",
	.id		= 3,
	.num_resources	= ARRAY_SIZE(s3c_hsmmc3_resource),
	.resource	= s3c_hsmmc3_resource,
	.dev		= {
		.dma_mask		= &s3c_device_hsmmc3_dmamask,
		.coherent_dma_mask	= 0xffffffffUL,
		.platform_data		= &s3c_hsmmc3_def_platdata,
	},
};
#ifdef CONFIG_BCM4330_DRIVER
static struct wifi_platform_data bcm4329_wifi_control = {
	.set_power      = bcm4329_wifi_power,
	.set_reset      = bcm4329_wifi_reset,
	.set_carddetect = bcm4329_wifi_set_carddetect,
	.mem_prealloc	= bcm4329_wifi_mem_prealloc,
};

#define PREALLOC_WLAN_NUMBER_OF_SECTIONS	4
#define PREALLOC_WLAN_NUMBER_OF_BUFFERS		160
#define PREALLOC_WLAN_SECTION_HEADER		24

#define WLAN_SECTION_SIZE_0	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 128)
#define WLAN_SECTION_SIZE_1	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 128)
#define WLAN_SECTION_SIZE_2	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 512)
#define WLAN_SECTION_SIZE_3	(PREALLOC_WLAN_NUMBER_OF_BUFFERS * 1024)

#define WLAN_SKB_BUF_NUM	16

static struct sk_buff *wlan_static_skb[WLAN_SKB_BUF_NUM];

typedef struct wifi_mem_prealloc_struct {
	void *mem_ptr;
	unsigned long size;
} wifi_mem_prealloc_t;

static wifi_mem_prealloc_t wifi_mem_array[PREALLOC_WLAN_NUMBER_OF_SECTIONS] = {
	{ NULL, (WLAN_SECTION_SIZE_0 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_1 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_2 + PREALLOC_WLAN_SECTION_HEADER) },
	{ NULL, (WLAN_SECTION_SIZE_3 + PREALLOC_WLAN_SECTION_HEADER) }
};

static void *bcm4329_wifi_mem_prealloc(int section, unsigned long size)
{
	if (section == PREALLOC_WLAN_NUMBER_OF_SECTIONS)
		return wlan_static_skb;
	if ((section < 0) || (section > PREALLOC_WLAN_NUMBER_OF_SECTIONS))
		return NULL;
	if (wifi_mem_array[section].size < size)
		return NULL;
	return wifi_mem_array[section].mem_ptr;
}
int __init bcm4329_init_wifi_mem(void)
{
	int i;
	for(i=0;( i < WLAN_SKB_BUF_NUM );i++) {
		if (i < (WLAN_SKB_BUF_NUM/2))
			wlan_static_skb[i] = dev_alloc_skb(4096);
		else
			wlan_static_skb[i] = dev_alloc_skb(8192);
	}
	for(i=0;( i < PREALLOC_WLAN_NUMBER_OF_SECTIONS );i++) {
		wifi_mem_array[i].mem_ptr = kmalloc(wifi_mem_array[i].size,
							GFP_KERNEL);
		if (wifi_mem_array[i].mem_ptr == NULL)
			return -ENOMEM;
	}
	return 0;
}
#endif
void s3c_sdhci3_set_platdata(struct s3c_sdhci_platdata *pd)
{
	struct s3c_sdhci_platdata *set = &s3c_hsmmc3_def_platdata;

	set->cd_type = pd->cd_type;
#ifndef CONFIG_BCM4330_DRIVER
	set->ext_cd_init = pd->ext_cd_init;
#else
	set->ext_cd_init = bcm4329_wifi_status_register;
#endif
	set->ext_cd_cleanup = pd->ext_cd_cleanup;
	set->ext_cd_gpio = pd->ext_cd_gpio;
	set->ext_cd_gpio_invert = pd->ext_cd_gpio_invert;

	if (pd->max_width)
		set->max_width = pd->max_width;
	if (pd->cfg_gpio)
		set->cfg_gpio = pd->cfg_gpio;
	if (pd->cfg_card)
		set->cfg_card = pd->cfg_card;
	if (pd->host_caps)
		set->host_caps |= pd->host_caps;
	if (pd->clk_type)
		set->clk_type = pd->clk_type;
}
#ifdef CONFIG_BCM4330_DRIVER
static struct resource bcm4329_wifi_resources[] = {
	[0] = {
		.name		= "bcm4329_wlan_irq",
		.flags          = IORESOURCE_IRQ | IORESOURCE_IRQ_HIGHLEVEL | IORESOURCE_IRQ_SHAREABLE ,
	},
};

static struct platform_device bcm4329_wifi_device = {
        .name           = "bcm4329_wlan",
        .id             = 1,
        .num_resources  = ARRAY_SIZE(bcm4329_wifi_resources),
        .resource       = bcm4329_wifi_resources,
        .dev            = {
                .platform_data = &bcm4329_wifi_control,
        },
};

static int __init bcm4329_wifi_init(void)
{
	int ret;

 	printk("%s: start\n", __func__);
  	bcm4329_init_wifi_mem();
	bcm4329_wifi_device.resource->start = gpio_to_irq(EXYNOS4_GPX2(7));
	bcm4329_wifi_device.resource->end = gpio_to_irq(EXYNOS4_GPX2(7));
	ret = platform_device_register(&bcm4329_wifi_device);
        return ret;
}

module_init(bcm4329_wifi_init);
#endif
