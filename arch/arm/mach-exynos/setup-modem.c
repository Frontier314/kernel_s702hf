#include <linux/gpio.h>
#include <plat/gpio-cfg.h>
#include <plat/devs.h>
#include <mach/regs-gpio.h>
#include <mach/gpio.h>
#include <mach/irqs.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/regulator/consumer.h>
#ifdef CONFIG_HAS_WAKELOCK
#include <linux/wakelock.h>
#endif
#include <mach/modem.h>
#include <linux/io.h>
/*diog.zhao 120716
**add for solve reset_out_int works abnormal while power off*/
static void __iomem *map_reg_gpio_int;
static int gpio_int16_con;//hzz
extern void usb_host_phy_init(void);
extern void usb_host_phy_off(void);
extern void usb_host_phy_suspend(void);
extern int usb_host_phy_resume(void);
extern void mc_hostwakeup_enable(bool enable);
int smm6260_is_on(void)
{
	return gpio_get_value(GPIO_PHONE_ON);
}
EXPORT_SYMBOL_GPL(smm6260_is_on);

int smm6260_set_active_state(int val)
{
	mc_hostwakeup_enable(val);
	gpio_set_value(GPIO_ACTIVE_STATE, val ? 1 : 0);
#ifdef MODEM_IPC_DEBUG
	printk("%s: AP>>CP:   ACTIVE_STATE:%d\n", __func__, val ? 1 : 0);
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(smm6260_set_active_state);

int smm6260_set_slave_wakeup(int val)
{
	gpio_set_value(GPIO_IPC_SLAVE_WAKEUP, val ? 1 : 0);
#ifdef MODEM_IPC_DEBUG
	printk("%s: AP>>CP:   SLAV_WUP:%d,%d\n", __func__, val ? 1 : 0,	gpio_get_value(GPIO_IPC_SLAVE_WAKEUP));
#endif
	return 0;
}
EXPORT_SYMBOL_GPL(smm6260_set_slave_wakeup);

int smm6260_is_cp_wakeup_ap(void)
{
	return ((gpio_get_value(GPIO_IPC_HOST_WAKEUP)) ==HOST_WUP_LEVEL)? 1:0;
	//return ((gpio_get_value(GPIO_IPC_SLAVE_WAKEUP)) ==1)? 1 : 0;
}
EXPORT_SYMBOL_GPL(smm6260_is_cp_wakeup_ap);


void smm6260_set_uart_gpio(void)
{
	s3c_gpio_cfgpin(AP_UART1_RXD, S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(AP_UART1_TXD, S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(AP_UART1_CTS, S3C_GPIO_SFN(2));
	s3c_gpio_cfgpin(AP_UART1_RTS, S3C_GPIO_SFN(2));

	s3c_gpio_setpull(AP_UART1_RXD, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(AP_UART1_TXD, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(AP_UART1_CTS, S3C_GPIO_PULL_NONE);
	s3c_gpio_setpull(AP_UART1_RTS, S3C_GPIO_PULL_NONE);

	printk("AP Uart1 ctrl val= %8x\n", __raw_readl(S5P_VA_GPIO + 0x00));
	printk("AP uart1 setpull val= %4x\n", __raw_readl(S5P_VA_GPIO + 0x08));
}

void smm6260_cfg(void)
{
	static int smm6260_initialed=0;
	int err = 0;

	if(smm6260_initialed)
		return;
	/*TODO: check uart init func AP FLM BOOT RX -- */
	
 //lisw debug configed and set value in uboot	
	err = gpio_request(GPIO_CP_PMU_RST, "CP_PMU_RST");
	if (err) {
		printk(KERN_ERR "fail to request gpio %s\n", "CP_PMU_RST");
	} else {
		gpio_direction_output(GPIO_CP_PMU_RST, 0);
		s3c_gpio_setpull(GPIO_CP_PMU_RST, S3C_GPIO_PULL_NONE);
	}	
	err = gpio_request(GPIO_PHONE_ON, "PHONE_ON");
	if (err) {
		printk(KERN_ERR "fail to request gpio %s\n", "PHONE_ON");
	} else {
		gpio_direction_output(GPIO_PHONE_ON, 0);
		s3c_gpio_setpull(GPIO_PHONE_ON, S3C_GPIO_PULL_NONE);
	}
	err = gpio_request(GPIO_CP_RST, "CP_RST");
	if (err) {
		printk(KERN_ERR "fail to request gpio %s\n", "CP_RST");
	} else {
		gpio_direction_output(GPIO_CP_RST, 0);
		s3c_gpio_setpull(GPIO_CP_RST, S3C_GPIO_PULL_NONE);
	}

	err = gpio_request(GPIO_IPC_SLAVE_WAKEUP, "IPC_SLAVE_WAKEUP");
	if (err) {
		printk(KERN_ERR "fail to request gpio %s\n",
			"IPC_SLAVE_WAKEUP");
	} else {
		gpio_direction_output(GPIO_IPC_SLAVE_WAKEUP, 0);
		s3c_gpio_setpull(GPIO_IPC_SLAVE_WAKEUP, S3C_GPIO_PULL_NONE);
	}
//haozz add
 /*       err = gpio_request(GPIO_CP_COREDUMP_IND,"CP_COREDUMP_IND");
	if (err) {
		printk(KERN_ERR "fail to request gpio %s\n", "CP_COREDUMP_IND");
	} else {
		gpio_direction_output(GPIO_CP_COREDUMP_IND, 0);
		s3c_gpio_setpull(GPIO_CP_COREDUMP_IND, S3C_GPIO_PULL_NONE);
		printk("haozz coredump output 1");
	}*/

	err = gpio_request(GPIO_IPC_HOST_WAKEUP, "IPC_HOST_WAKEUP");
	if (err) {
		printk(KERN_ERR "fail to request gpio %s\n", "IPC_HOST_WAKEUP");
	} else {
//		gpio_direction_output(GPIO_IPC_HOST_WAKEUP, 0);
		s3c_gpio_cfgpin(GPIO_IPC_HOST_WAKEUP, S3C_GPIO_INPUT);
		s3c_gpio_setpull(GPIO_IPC_HOST_WAKEUP, S3C_GPIO_PULL_NONE);
	}
	err = gpio_request(GPIO_SUSPEND_REQUEST, "IPC_SUSPEND_REQUEST");
	if (err) {
		printk(KERN_ERR "fail to request gpio %s\n", "IPC_SUSPEND_REQUEST");
	} else {
		gpio_direction_output(GPIO_SUSPEND_REQUEST, 0);
		s3c_gpio_cfgpin(GPIO_SUSPEND_REQUEST, S3C_GPIO_INPUT);
		s3c_gpio_setpull(GPIO_SUSPEND_REQUEST, S3C_GPIO_PULL_NONE);
	}	

	err = gpio_request(GPIO_CP_ABNORMAL_RESET_INT, "IPC_CP_ABNORMAL_RESET");
	if (err) {
		printk(KERN_ERR "fail to request gpio %s\n", "IPC_CP_ABNORMAL_RESET");
	} else {
		s3c_gpio_cfgpin(GPIO_CP_ABNORMAL_RESET_INT, S3C_GPIO_INPUT);
		s3c_gpio_setpull(GPIO_CP_ABNORMAL_RESET_INT, S3C_GPIO_PULL_NONE);
	}

	err = gpio_request(GPIO_CP_SILENCE_RESET_INT, "CP_SILENCE_RESET_INT");
	if (err) {
		printk(KERN_ERR "fail to request gpio %s\n", "GPIO_CP_SILENCE_RESET_INT");
	} else {
		s3c_gpio_cfgpin(GPIO_CP_SILENCE_RESET_INT, S3C_GPIO_INPUT);
		s3c_gpio_setpull(GPIO_CP_SILENCE_RESET_INT, S3C_GPIO_PULL_NONE);
	}
/*	*/
	err = gpio_request(GPIO_ACTIVE_STATE, "ACTIVE_STATE");
	if (err) {
		printk(KERN_ERR "fail to request gpio %s\n", "ACTIVE_STATE");
	} else {
//		gpio_direction_input(GPIO_ACTIVE_STATE);
		gpio_direction_output(GPIO_ACTIVE_STATE, 0);
		s3c_gpio_setpull(GPIO_ACTIVE_STATE, S3C_GPIO_PULL_NONE);
	}

	err = gpio_request(GPIO_USB_SWITCH_CTRL, "USB_SWITCH_CTRL");
	if (err) {
		printk(KERN_ERR "fail to request gpio %s\n", "USB_SWITCH_CTRL");
	} else {
#if 1
		gpio_direction_output(GPIO_USB_SWITCH_CTRL, 0);
#else
		gpio_direction_output(GPIO_USB_SWITCH_CTRL, 1);
#endif
		s3c_gpio_setpull(GPIO_USB_SWITCH_CTRL, S3C_GPIO_PULL_NONE);
	}
	/*power down gpio set*/
	s5p_gpio_set_pd_cfg(GPIO_CP_RST,S5P_GPIO_PD_PREV_STATE);
	s5p_gpio_set_pd_pull(GPIO_CP_RST, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_pd_cfg(GPIO_CP_PMU_RST,S5P_GPIO_PD_PREV_STATE);
	s5p_gpio_set_pd_pull(GPIO_CP_PMU_RST, S3C_GPIO_PULL_NONE);
	s5p_gpio_set_pd_cfg(GPIO_PHONE_ON,S5P_GPIO_PD_PREV_STATE);
	s5p_gpio_set_pd_pull(GPIO_PHONE_ON, S3C_GPIO_PULL_NONE);
	
	//s5p_gpio_set_pd_cfg(GPIO_IPC_HOST_WAKEUP,S5P_GPIO_PD_PREV_STATE);
	//s5p_gpio_set_pd_cfg(GPIO_IPC_SLAVE_WAKEUP,S5P_GPIO_PD_PREV_STATE);
	s5p_gpio_set_pd_cfg(GPIO_IPC_SLAVE_WAKEUP,S5P_GPIO_PD_INPUT);
	s5p_gpio_set_pd_pull(GPIO_IPC_SLAVE_WAKEUP, S3C_GPIO_PULL_DOWN);
	s5p_gpio_set_pd_cfg(GPIO_SUSPEND_REQUEST,S5P_GPIO_PD_INPUT);
	s5p_gpio_set_pd_pull(GPIO_SUSPEND_REQUEST, S3C_GPIO_PULL_NONE);	
	s5p_gpio_set_pd_cfg(GPIO_ACTIVE_STATE,S5P_GPIO_PD_PREV_STATE);	
	s5p_gpio_set_pd_cfg(GPIO_ACTIVE_STATE,S5P_GPIO_PD_INPUT);
	s5p_gpio_set_pd_pull(GPIO_ACTIVE_STATE, S3C_GPIO_PULL_NONE);	
	
	s5p_gpio_set_pd_cfg(GPIO_CP_ABNORMAL_RESET_INT,S5P_GPIO_PD_INPUT);
	s5p_gpio_set_pd_pull(GPIO_CP_ABNORMAL_RESET_INT, S3C_GPIO_PULL_DOWN);	

	smm6260_set_uart_gpio();
/*diog.zhao 120716
**add for solve reset_out_int works abnormal while power off*/
	map_reg_gpio_int =  ioremap(0x1140073c,0x20);
	smm6260_initialed = 1;
}
static void smm6260_vcc_init(void)
{

}

static void smm6260_vcc_off(void)
{

}

static void smm6260_on(void)
{
	int count =1000;
	printk("%s: start\n", __func__);

	
	smm6260_vcc_init();
	/*assert reset single*/
	gpio_set_value(GPIO_PHONE_ON, 0);
	gpio_set_value(GPIO_CP_PMU_RST, 0);	
	gpio_set_value(GPIO_CP_RST, 0);
	msleep(100);
	/*release reset single*/
	gpio_set_value(GPIO_CP_RST, 1);	
	mdelay(1);
	gpio_set_value(GPIO_CP_PMU_RST, 1);
	mdelay(2);
	/*triger reset single*/
	gpio_set_value(GPIO_PHONE_ON, 1);
//	msleep(100);
	printk("%s: finish\n", __func__);
}

static void smm6260_off(void)
{
	printk("%s\n", __func__);

	gpio_set_value(GPIO_CP_PMU_RST, 0);
	gpio_set_value(GPIO_CP_RST, 0);
	gpio_set_value(GPIO_PHONE_ON, 0);
	smm6260_vcc_off();
}

static void smm6260_reset(void)
{
	printk("%s\n", __func__);

	gpio_set_value(GPIO_CP_RST, 0);
	msleep(20);

	gpio_set_value(GPIO_CP_RST, 1);
	udelay(160);
}

/* move the PDA_ACTIVE Pin control to sleep_gpio_table */
static void smm6260_suspend(void)
{	
	/*keep gpio configuration*/
	smm6260_vcc_off();
	/*save gpio int register*/
/*diog.zhao 120716
**add for solve reset_out_int works abnormal while power off*/
	if(map_reg_gpio_int)
		gpio_int16_con = readl(map_reg_gpio_int);
	
}

static void smm6260_resume(void)
{
	smm6260_vcc_init();	
	if(!smm6260_is_cp_wakeup_ap())//haozz
		smm6260_set_slave_wakeup(1);
/*diog.zhao 120716
**add for solve reset_out_int works abnormal while power off*/
	s5p_gpio_set_pd_cfg(GPIO_CP_ABNORMAL_RESET_INT,S5P_GPIO_PD_INPUT);
	s5p_gpio_set_pd_pull(GPIO_CP_ABNORMAL_RESET_INT, S3C_GPIO_PULL_NONE);	
	/*restore gpio int register*/
	if(map_reg_gpio_int)
		writel(gpio_int16_con,map_reg_gpio_int);

}
void smm6060_initial_poweron(void)
{
//	usb_host_phy_off();
//	xmm6260_cfg();
//	xmm6260_on();
}
EXPORT_SYMBOL(smm6060_initial_poweron);

static struct modem_platform_data smm6260_data = {
	.name = "smm6260",
	.gpio_phone_on = GPIO_PHONE_ON,
//	.gpio_phone_active = GPIO_PHONE_ACTIVE,
//	.gpio_pda_active = GPIO_PDA_ACTIVE,
	.gpio_cp_reset = GPIO_CP_RST,
//	.gpio_cp_req_reset = GPIO_CP_REQ_RST,
	.gpio_ipc_slave_wakeup = GPIO_IPC_SLAVE_WAKEUP,
	.gpio_ipc_host_wakeup = GPIO_IPC_HOST_WAKEUP,
	.gpio_suspend_request = GPIO_SUSPEND_REQUEST,
	.gpio_active_state = GPIO_ACTIVE_STATE,
	.wakeup = 1, //diog.zhao, support wakeup ap
//	.gpio_cp_dump_int = GPIO_CP_DUMP_INT,
	.gpio_cp_reset_int = GPIO_CP_SILENCE_RESET_INT,
	.ops = {
		.modem_on = smm6260_on,
		.modem_off = smm6260_off,
		.modem_reset = smm6260_reset,
		.modem_suspend = smm6260_suspend,
		.modem_resume = smm6260_resume,
		.modem_cfg = smm6260_cfg,
	}
};

struct platform_device smm6260_modem = {
	.name = "smm_modem",
	.id = -1,

	.dev = {
		.platform_data = &smm6260_data,
	},
};
EXPORT_SYMBOL(smm6260_modem);

