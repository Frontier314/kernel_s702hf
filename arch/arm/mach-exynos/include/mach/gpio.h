/* linux/arch/arm/mach-exynos/include/mach/gpio.h
 *
 * Copyright (c) 2010-2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS - gpio map definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __ASM_ARCH_GPIO_H
#define __ASM_ARCH_GPIO_H __FILE__

#include "gpio-exynos4.h"
#include "gpio-exynos5.h"

#define S5P_GPIO_OUTPUT0    S5P_GPIO_PD_OUTPUT0
#define S5P_GPIO_OUTPUT1    S5P_GPIO_PD_OUTPUT1
#define S5P_GPIO_INPUT      S5P_GPIO_PD_INPUT
#define S5P_GPIO_PREVIOUS_STATE  S5P_GPIO_PD_PREV_STATE
#define S5P_GPIO_PULL_UP_DOWN_DISABLE S5P_GPIO_PD_UPDOWN_DISABLE
#define S5P_GPIO_PULL_DOWN_ENABLE S5P_GPIO_PD_DOWN_ENABLE
#define S5P_GPIO_PULL_UP_ENABLE S5P_GPIO_PD_UP_ENABLE

#define s5p_gpio_set_conpdn  s5p_gpio_set_pd_cfg
#define s5p_gpio_set_pudpdn  s5p_gpio_set_pd_pull

#if defined(CONFIG_ARCH_EXYNOS4)
#define S3C_GPIO_END		EXYNOS4_GPIO_END
#define ARCH_NR_GPIOS		(EXYNOS4XXX_GPIO_END +	\
				CONFIG_SAMSUNG_GPIO_EXTRA)
#elif defined(CONFIG_ARCH_EXYNOS5)
#define S3C_GPIO_END		EXYNOS5_GPIO_END
#define ARCH_NR_GPIOS		(EXYNOS5_GPIO_END +	\
				CONFIG_SAMSUNG_GPIO_EXTRA)
#else
#error "ARCH_EXYNOS* is not defined"
#endif

#include <asm-generic/gpio.h>

#endif /* __ASM_ARCH_GPIO_H */
