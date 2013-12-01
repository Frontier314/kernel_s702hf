/* smm6260_tty.c
 *
 *  Base on smd-tty.c
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/tty.h>
#include <linux/tty_driver.h>
#include <linux/tty_flip.h>

#include "smm6260_smd.h"
#include "smm6260_tty.h"
#ifdef CONFIG_SMM6260_MODEM_NET
#define MAX_SMD_TTYS (4-CONFIG_SMM6260_MODEM_MAX_NUM)
#else
#define MAX_SMD_TTYS 4
#endif

#define TTY_PACKET_SIZE 4096

static DEFINE_MUTEX(smm6260_tty_lock);

struct smm6260_tty_info {
	smd_channel_t *ch;
	struct tty_struct *tty;
	struct wake_lock wake_lock;
	int open_count;
	unsigned char buffer[TTY_PACKET_SIZE];
};

static struct smm6260_tty_info smm6260_tty[MAX_SMD_TTYS];

static const struct smd_tty_channel_desc smm6260_default_tty_channels[] = {
	{ .id = 0, .name = "SMD_TTY0" },
#ifdef  CONFIG_SMM6260_MODEM_NET
#if (CONFIG_SMM6260_MODEM_MAX_NUM == 2)
	{ .id = 1, .name = "SMD_TTY1" },
#elif (CONFIG_SMM6260_MODEM_MAX_NUM == 1)
	{ .id = 1, .name = "SMD_TTY1" },
	{ .id = 2, .name = "SMD_TTY2" },
#endif
#else
	{ .id = 1, .name = "SMD_TTY1" },
	{ .id = 2, .name = "SMD_TTY2" },
	{ .id = 3, .name = "SMD_TTY3" },
#endif

};

static const struct smd_tty_channel_desc *smm6260_tty_channels =
		smm6260_default_tty_channels;
static int smm6260_tty_channels_len = ARRAY_SIZE(smm6260_default_tty_channels);

int smm6260_set_channel_list(const struct smd_tty_channel_desc *channels, int len)
{
	smm6260_tty_channels = channels;
	smm6260_tty_channels_len = len;
	return 0;
}

static void smm6260_tty_notify(void *priv, unsigned event)
{
	unsigned char *ptr;
	int avail;
	struct smm6260_tty_info *info = priv;
	struct tty_struct *tty = info->tty;
	int count;
	
	//printk("%s: - event =%d\n", __func__, event);
	if (!tty)
		return;

	if (event != SMD_EVENT_DATA)
	{	
		switch(event)
		{
		case SMD_EVENT_CONNECT:
			//tty_wakeup(tty);
			break;
		case SMD_EVENT_DISCONNECT:
			//tty_hangup(tty);
			break;
		}
		return;
	}

	for (;;) {
#if 1
		if (test_bit(TTY_THROTTLED, &tty->flags))
		{
			//printk("%s:  tty TTY_THROTTLED\n", __func__);
			//wake_lock_timeout(&info->wake_lock, HZ / 2);
			break;
		}
#endif
#if 0
		avail = smd_read_avail(info->ch);
		if (avail == 0)
		{
			printk("%s: smd buffer avail size=%d\n", __func__, avail);
			break;
		}
		if(avail>TTY_PACKET_SIZE)
			avail = TTY_PACKET_SIZE;
		if (smd_read(info->ch, info->buffer, avail) != avail) {
			/* shouldn't be possible since we're in interrupt
			** context here and nobody else could 'steal' our
			** characters.
			*/
			printk(KERN_ERR "OOPS - smm6260_tty_buffer mismatch?!");
		}
		/*push tty buffer size under 256 byte*/
		tty_insert_flip_string(tty,  info->buffer, avail);
#else	
		avail = smd_read_avail(info->ch);
		if (avail == 0)
		{
			//printk("%s: smd buffer avail size=%d\n", __func__, avail);
			break;
		}
		else
			{
				setstamp(8);
				printstamp();
			}
		if(avail>TTY_PACKET_SIZE)
			avail = TTY_PACKET_SIZE;
		avail = tty_prepare_flip_string(tty, &ptr, avail);
#if 0		
		if(avail ==0)
		{
			tty_throttle(tty);
			printk("%s: tty buffer avail size=%d\n", __func__, avail);
			break;
		}
#endif		
		if (smd_read(info->ch, ptr, avail) != avail) {
			/* shouldn't be possible since we're in interrupt
			** context here and nobody else could 'steal' our
			** characters.
			*/
			printk(KERN_ERR "OOPS - smm6260_tty_buffer mismatch?!");
		}
#endif
		//printk("%s: push tty size=%d\n", __func__, avail);
		wake_lock_timeout(&info->wake_lock, HZ * 2);
		tty_flip_buffer_push(tty);
		//printk("%s:  tty push data %d\n", __func__, avail);
		if(avail ==0)
			{
			printk("&");
			msleep(10);
			}
	}

	/* XXX only when writable and necessary */
//	tty_wakeup(tty);
}

static int smm6260_tty_open(struct tty_struct *tty, struct file *f)
{
	int res = 0;
	int n = tty->index;
	struct smm6260_tty_info *info;
	const char *name = NULL;
	int i;
	for (i = 0; i < smm6260_tty_channels_len; i++) {
		if (smm6260_tty_channels[i].id == n) {
			name = smm6260_tty_channels[i].name;
			break;
		}
	}
	if (!name)
		return -ENODEV;

	info = smm6260_tty + n;

	mutex_lock(&smm6260_tty_lock);
	wake_lock_init(&info->wake_lock, WAKE_LOCK_SUSPEND, name);
	tty->driver_data = info;

	if (info->open_count++ == 0) {
		info->tty = tty;
		tty->low_latency=1;//set to low latency tty
		if (info->ch) {
			smd_kick(info->ch);
		} else {
			res = smd_open(name, &info->ch, info, smm6260_tty_notify);
		}
	}
	mutex_unlock(&smm6260_tty_lock);
	return res;
}

static void smm6260_tty_close(struct tty_struct *tty, struct file *f)
{
	struct smm6260_tty_info *info = tty->driver_data;

	if (info == 0)
		return;

	mutex_lock(&smm6260_tty_lock);
	if (--info->open_count == 0) {
		info->tty = 0;
		tty->driver_data = 0;
		wake_lock_destroy(&info->wake_lock);
		if (info->ch) {
			smd_close(info->ch);
			info->ch = 0;
		}
	}
	mutex_unlock(&smm6260_tty_lock);
}

static int smm6260_tty_write(struct tty_struct *tty, const unsigned char *buf, int len)
{
	struct smm6260_tty_info *info = tty->driver_data;
	int avail;

	/* if we're writing to a packet channel we will
	** never be able to write more data than there
	** is currently space for
	*/
	avail = smd_write_avail(info->ch);
	if (len > avail)
		len = avail;

	return smd_write(info->ch, buf, len);
}

static int smm6260_tty_write_room(struct tty_struct *tty)
{
	struct smm6260_tty_info *info = tty->driver_data;
	return smd_write_avail(info->ch);
}

static int smm6260_tty_chars_in_buffer(struct tty_struct *tty)
{
	struct smm6260_tty_info *info = tty->driver_data;
	return smd_read_avail(info->ch);
}

static void smm6260_tty_unthrottle(struct tty_struct *tty)
{
	struct smm6260_tty_info *info = tty->driver_data;
	//printk("%s:  TTY_THROTTL Clean\n", __func__);
	smd_kick(info->ch);
}
static void smm6260_tty_throttle(struct tty_struct *tty)
{
	struct smm6260_tty_info *info = tty->driver_data;
	//printk("%s:  TTY_THROTTLED Set\n", __func__);
}

static struct tty_operations smm6260_tty_ops = {
	.open = smm6260_tty_open,
	.close = smm6260_tty_close,
	.write = smm6260_tty_write,
	.write_room = smm6260_tty_write_room,
//	.chars_in_buffer = smm6260_tty_chars_in_buffer,
	.unthrottle = smm6260_tty_unthrottle,
	.throttle =smm6260_tty_throttle,
};

static struct tty_driver *smm6260_tty_driver;

static int __init smm6260_tty_init(void)
{
	int ret, i;

	smm6260_tty_driver = alloc_tty_driver(MAX_SMD_TTYS);
	if (smm6260_tty_driver == 0)
		return -ENOMEM;

	smm6260_tty_driver->owner = THIS_MODULE;
	smm6260_tty_driver->driver_name = "smm6260_tty_driver";
	smm6260_tty_driver->name = "ttyACM";
	smm6260_tty_driver->major = 0;
	smm6260_tty_driver->minor_start = 0;
	smm6260_tty_driver->type = TTY_DRIVER_TYPE_SERIAL;
	smm6260_tty_driver->subtype = SERIAL_TYPE_NORMAL;
	smm6260_tty_driver->init_termios = tty_std_termios;
	smm6260_tty_driver->init_termios.c_iflag = 0;
	smm6260_tty_driver->init_termios.c_oflag = 0;
	smm6260_tty_driver->init_termios.c_cflag = B4000000 | CS8 | CREAD;
	smm6260_tty_driver->init_termios.c_lflag = 0;
	smm6260_tty_driver->flags = TTY_DRIVER_RESET_TERMIOS |
		TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;
	tty_set_operations(smm6260_tty_driver, &smm6260_tty_ops);

	ret = tty_register_driver(smm6260_tty_driver);
	if (ret) return ret;

	for (i = 0; i < smm6260_tty_channels_len; i++)
	{
		tty_register_device(smm6260_tty_driver, smm6260_tty_channels[i].id, 0);
		printk("%s: %s\n", __func__, smm6260_tty_channels[i].name);
	}

	return 0;
}

module_init(smm6260_tty_init);

