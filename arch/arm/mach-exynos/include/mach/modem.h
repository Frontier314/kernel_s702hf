/* linux/arch/arm/mach-s5pv310/include/mach/modem.h
 *
 * Copyright (c) 2011 Samsung Electronics Co.Ltd
 *		
 *
 *
 * Based on arch/arm/mach-s5p6442/include/mach/io.h
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __MODEM_H
#define __MODEM_H

#define GPIO_PHONE_ON		EXYNOS4_GPF3(1)	//power on cp
#define GPIO_PHONE_ACTIVE	S5PV310_GPX0(7)	// NC
#define GPIO_PDA_ACTIVE		S5PV310_GPK1(0)	// NC
#define GPIO_CP_DUMP_INT	S5PV310_GPL0(5)	// NC
#define GPIO_CP_RST			EXYNOS4_GPF2(2)	//reset cp
#define GPIO_CP_PMU_RST	EXYNOS4_GPF2(3)	//reset cp pmu
#define GPIO_CP_REQ_RST	S5PV310_GPL0(6)	// NC
#define GPIO_IPC_SLAVE_WAKEUP	EXYNOS4_GPC1(0)	//ap -> cp wakeup slave
#define GPIO_CP_SILENCE_RESET_INT	EXYNOS4_GPX2(0)	//cp->ap interrupt
#define GPIO_IPC_HOST_WAKEUP	EXYNOS4_GPX2(3)	//cp -> ap wakeup host
#define GPIO_SUSPEND_REQUEST	EXYNOS4_GPF0(5)	//cp -> ap request sleep
#define GPIO_ISP_INT		S5PV310_GPL0(7)	// NC
#define GPIO_ACTIVE_STATE	EXYNOS4_GPC1(1)		//ap active state
#define GPIO_USB_SWITCH_CTRL	EXYNOS4_GPF0(6)	//usb switch control
#define GPIO_CP_ABNORMAL_RESET_INT EXYNOS4_GPF3(0) //cp reset out

//haozz add
//#define GPIO_CP_COREDUMP_IND    EXYNOS4_GPX1(7)  //coredump ind

#define IRQ_PHONE_ACTIVE	IRQ_EINT7 //NC
#define IRQ_SUSPEND_REQUEST	IRQ_EINT12
#define IRQ_IPC_HOST_WAKEUP	IRQ_EINT14

#define AP_UART1_RXD	EXYNOS4_GPA0(4)
#define AP_UART1_TXD	EXYNOS4_GPA0(5)
#define AP_UART1_CTS	EXYNOS4_GPA0(6)
#define AP_UART1_RTS	EXYNOS4_GPA0(7)

#define HOST_WUP_LEVEL 0

struct modem_ops {
	void (*modem_on)(void);
	void (*modem_off)(void);
	void (*modem_reset)(void);
	void (*modem_boot)(void);
	void (*modem_suspend)(void);
	void (*modem_resume)(void);
	void (*modem_cfg)(void);
};

struct modem_platform_data {
	const char *name;
	unsigned gpio_phone_on;
	unsigned gpio_phone_active;
	unsigned gpio_pda_active;
	unsigned gpio_cp_reset;
	unsigned gpio_usim_boot;
	unsigned gpio_flm_sel;
	unsigned gpio_cp_req_reset;	/*HSIC*/
	unsigned gpio_ipc_slave_wakeup;
	unsigned gpio_ipc_host_wakeup;
	unsigned gpio_suspend_request;
	unsigned gpio_active_state;
	unsigned gpio_cp_dump_int;
	unsigned gpio_cp_reset_int;
	int wakeup;
	struct modem_ops ops;
};

extern struct platform_device smm6260_modem;
extern void smm6060_initial_poweron(void);
extern int smm6260_is_on(void);
extern int smm6260_is_cp_wakeup_ap(void);
extern int smm6260_set_active_state(int val);
extern int smm6260_set_slave_wakeup(int val);

#define MODEM_IPC_DEBUG
//#define MODEM_DATA_DEBUG
//#define MODEM_DATA_TRACE
#ifdef MODEM_IPC_DEBUG
#define modem_ipc_dbg dev_info
#else
#define modem_ipc_dbg
#endif
#ifdef MODEM_DATA_TRACE//trace for acm write/read
#define modem_data_trace dev_info
#else
#define modem_data_trace 
#endif
#ifdef MODEM_DATA_DEBUG//trace for acm write data printf
#define modem_data_debug dev_info
#else
#define modem_data_debug
#endif

#endif//__MODEM_H
