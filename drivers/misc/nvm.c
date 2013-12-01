//
// nvm.c
//

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/interrupt.h>
#include <linux/moduleparam.h>
#include <linux/stat.h>

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/list.h>

#include <linux/mmc/core.h>
#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/slab.h>

#include <linux/scatterlist.h>

#include <linux/nvm.h>

#define RESULT_OK		0
#define RESULT_FAIL		1
#define RESULT_UNSUP_HOST	2
#define RESULT_UNSUP_CARD	3

struct movi_data {
	char *buf;
	
	struct mmc_card *card;
};

static struct movi_data g_movi_data;

/*
 * Fill in the mmc_request structure given a set of transfer parameters.
 */
static void mmc_movi_prepare_mrq(struct movi_data *movi,
	struct mmc_request *mrq, struct scatterlist *sg, unsigned sg_len,
	unsigned dev_addr, unsigned blocks, unsigned blksz, int write)
{
	BUG_ON(!mrq || !mrq->cmd || !mrq->data || !mrq->stop);

	if (blocks > 1) {
		mrq->cmd->opcode = write ?
			MMC_WRITE_MULTIPLE_BLOCK : MMC_READ_MULTIPLE_BLOCK;
	} else {
		mrq->cmd->opcode = write ?
			MMC_WRITE_BLOCK : MMC_READ_SINGLE_BLOCK;
	}

	mrq->cmd->arg = dev_addr;
	if (!mmc_card_blockaddr(movi->card))
		mrq->cmd->arg <<= 9;

	mrq->cmd->flags = MMC_RSP_R1 | MMC_CMD_ADTC;

	if (blocks == 1)
		mrq->stop = NULL;
	else {
		mrq->stop->opcode = MMC_STOP_TRANSMISSION;
		mrq->stop->arg = 0;
		mrq->stop->flags = MMC_RSP_R1B | MMC_CMD_AC;
	}

	mrq->data->blksz = blksz;
	mrq->data->blocks = blocks;
	mrq->data->flags = write ? MMC_DATA_WRITE : MMC_DATA_READ;
	mrq->data->sg = sg;
	mrq->data->sg_len = sg_len;

	mmc_set_data_timeout(mrq->data, movi->card);
}

/*
 * Wait for the card to finish the busy state
 */
static int mmc_movi_wait_busy(struct movi_data *movi)
{
	int ret, busy;
	struct mmc_command cmd;

	busy = 0;
	do {
		memset(&cmd, 0, sizeof(struct mmc_command));

		cmd.opcode = MMC_SEND_STATUS;
		cmd.arg = movi->card->rca << 16;
		cmd.flags = MMC_RSP_R1 | MMC_CMD_AC;

		ret = mmc_wait_for_cmd(movi->card->host, &cmd, 0);
		if (ret)
			break;

		if (!busy && !(cmd.resp[0] & R1_READY_FOR_DATA)) {
			busy = 1;
			printk(KERN_INFO "%s: Warning: Host did not "
				"wait for busy state to end.\n",
				mmc_hostname(movi->card->host));
		}
	} while (!(cmd.resp[0] & R1_READY_FOR_DATA));

	return ret;
}

static int mmc_movi_check_result(struct movi_data *movi,
	struct mmc_request *mrq)
{
	int ret;

	BUG_ON(!mrq || !mrq->cmd || !mrq->data);

	ret = 0;

	if (!ret && mrq->cmd->error)
		ret = mrq->cmd->error;
	if (!ret && mrq->data->error)
		ret = mrq->data->error;
	if (!ret && mrq->stop && mrq->stop->error)
		ret = mrq->stop->error;
	if (!ret && mrq->data->bytes_xfered !=
		mrq->data->blocks * mrq->data->blksz)
		ret = RESULT_FAIL;

	if (ret == -EINVAL)
		ret = RESULT_UNSUP_HOST;

	return ret;
}

static int mmc_movi_simple_transfer(struct movi_data *movi,
	struct scatterlist *sg, unsigned sg_len, unsigned dev_addr,
	unsigned blocks, unsigned blksz, int write)
{
	struct mmc_request mrq;
	struct mmc_command cmd;
	struct mmc_command stop;
	struct mmc_data data;

	memset(&mrq, 0, sizeof(struct mmc_request));
	memset(&cmd, 0, sizeof(struct mmc_command));
	memset(&data, 0, sizeof(struct mmc_data));
	memset(&stop, 0, sizeof(struct mmc_command));

	mrq.cmd = &cmd;
	mrq.data = &data;
	mrq.stop = &stop;

	mmc_movi_prepare_mrq(movi, &mrq, sg, sg_len, dev_addr,
		blocks, blksz, write);

	mmc_wait_for_req(movi->card->host, &mrq);

	mmc_movi_wait_busy(movi);

	return mmc_movi_check_result(movi, &mrq);
}

static int movi_basic_read(struct movi_data *movi, unsigned int start)
{
	int ret;
	struct scatterlist sg;

	mmc_claim_host(movi->card->host);

//	ret = mmc_movi_set_blksize(movi, 512);
//	if (ret)
//		return ret;

	sg_init_one(&sg, movi->buf, 512);

//	pr_info("Movi: read block %d\n", start);
	ret = mmc_movi_simple_transfer(movi, &sg, 1, start, 1, 512, 0);
	if (ret)
		return ret;

	mmc_release_host(movi->card->host);
	return 0;
}

static int movi_basic_write(struct movi_data *movi, unsigned int start)
{
	int ret;
	struct scatterlist sg;

	mmc_claim_host(movi->card->host);

//	ret = mmc_movi_set_blksize(movi, 512);
//	if (ret)
//		return ret;

	sg_init_one(&sg, movi->buf, 512);

//	pr_info("Movi: write block %d\n", start);
	ret = mmc_movi_simple_transfer(movi, &sg, 1, start, 1, 512, 1);
	if (ret)
		return ret;

	mmc_release_host(movi->card->host);
	return 0;
}

#define MOVI_BLKSIZE 		(1<<9)
#define PARAM_BLK_START 	(1057)

int movi_mmc_init(struct mmc_card *card)
{
	g_movi_data.card = card;

	return 0;
}

int read_nvm(NVM_t part, char *buf, size_t len)
{
	char *rbuf;
	unsigned int start_blk;
		
	if (part >= NVM_MAX)
		return -EINVAL;

	if(g_movi_data.card == NULL)
		return -EINVAL;

	rbuf = kzalloc(MOVI_BLKSIZE, GFP_KERNEL);
	if(!rbuf)
	{
		pr_info("Fail to allocate read buffer\n");
		return -ENOMEM;
	}
	g_movi_data.buf = rbuf;

	start_blk = PARAM_BLK_START + part;
	movi_basic_read(&g_movi_data, start_blk);	
	
	memcpy(buf, rbuf, len);

	kfree(rbuf);
	g_movi_data.buf = NULL;
	return len;
}

int write_nvm(NVM_t part, char *buf, size_t len)
{
	char *wbuf;
	unsigned int start_blk;
		
	if (part >= NVM_MAX)
		return -EINVAL;

	if(g_movi_data.card == NULL)
		return -EINVAL;

	wbuf = kzalloc(MOVI_BLKSIZE, GFP_KERNEL);
	if(!wbuf)
	{
		pr_info("Fail to allocate read buffer\n");
		return -ENOMEM;
	}
	g_movi_data.buf = wbuf;

	memcpy(wbuf, buf, len);
	start_blk = PARAM_BLK_START + part;
	
	movi_basic_write(&g_movi_data, start_blk);	
	
	kfree(wbuf);
	g_movi_data.buf = NULL;
	
	return len;
}


#define nvm_attr(_name) \
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = __stringify(_name),	\
		.mode = 0644,			\
	},					\
	.show	= _name##_show,			\
	.store	= _name##_store,		\
}

static ssize_t cpflag_show(struct kobject *kobj, struct kobj_attribute *attr,
		      char *buf)
{
	int ret, flag;
	int var = 0;

	ret = read_nvm(NVM_CPFLAG, (char *)&flag, 4);
	if(ret<0)
	{
		pr_info("Fail to read nvm\n");
		return ret;
	}
	
	if(flag == 0x11223344)
		var = 1;
	
	return sprintf(buf, "%d\n", var);
}

static ssize_t cpflag_store(struct kobject *kobj, struct kobj_attribute *attr,
		       const char *buf, size_t count)
{
	int ret, val;
	int flag = 0;

	sscanf(buf, "%d", &val);
	if(val)
		flag = 0x11223344;
	
	ret = write_nvm(NVM_CPFLAG, (char *)&flag, 4);
	if(ret<0)
	{
		pr_info("Fail to write nvm\n");
		return ret;
	}
	
	return count;
}

nvm_attr(cpflag);

static ssize_t hsflag_show(struct kobject *kobj, struct kobj_attribute *attr,
		      char *buf)
{
    int ret, flag;
    int var = 0;

    ret = read_nvm(NVM_HSFLAG, (char *)&flag, sizeof(int));
    if(ret<0)
    {
        pr_info("Fail to read nvm\n");
        return ret;
    }

    if(flag == 0xAABBCCDD)
        var = 1;

    return sprintf(buf, "%d\n", var);
}

static ssize_t hsflag_store(struct kobject *kobj, struct kobj_attribute *attr,
		       const char *buf, size_t count)
{
	pr_info("%s\n", buf);
	
	return count;
}

nvm_attr(hsflag);

static ssize_t sdflag_show(struct kobject *kobj, struct kobj_attribute *attr,
		      char *buf)
{
	int var;

	if (strcmp(attr->attr.name, "sdflag") == 0)
		var = 1;
	else
		var = 0;
	return sprintf(buf, "%d\n", var);
}

static ssize_t sdflag_store(struct kobject *kobj, struct kobj_attribute *attr,
		       const char *buf, size_t count)
{
	pr_info("%s\n", buf);
	
	return count;
}

nvm_attr(sdflag);

static ssize_t pscal_show(struct kobject *kobj, struct kobj_attribute *attr,
		      char *buf)
{
	char low_threshold[20]={'0'};
	char high_threshold[20]={'0'};
	char rbuf[42] = {0};
	char *p = NULL;
	char *q = NULL;
	char *led = NULL;
	char *s = NULL;

	read_nvm(NVM_PSCAL, rbuf, 40);
	pr_info("=====rbuf=====:%s\n", rbuf);

	p = strstr(rbuf, "s980");
	if (NULL != p)
	{
		led = strstr(rbuf, "led");
		if (NULL == led)
		{
			q = strchr(rbuf, ',');
			if (NULL == q)
				return -1;

			strncpy(low_threshold, rbuf, q-rbuf);
		}
		else
		{
			s = strstr(rbuf, ";");
			if (NULL == s)
				return -1;

			s = s + 1;
			q = strchr(s, ',');
			if (NULL == q)
				return -1;

			strncpy(low_threshold, s, q - s);
		}

		q = q + 1;
		p = p - 1;
		strncpy(high_threshold, q, p-q);
	}

	return sprintf(buf, "%s,%s\n", low_threshold, high_threshold);
}

static ssize_t pscal_store(struct kobject *kobj, struct kobj_attribute *attr,
		       const char *buf, size_t count)
{
	char cal_buf[40] = {0};
	char *p = NULL;
	pr_info("%s", buf);

	read_nvm(NVM_PSCAL, cal_buf, 7);
	p = strstr(cal_buf, "led");
	if (NULL != p)
	{
		p = p + 4;
		*(p - 1) = ';';
		strcpy(p, buf);
	}

	write_nvm(NVM_PSCAL, cal_buf, strlen(cal_buf));
	
	return count;
}

nvm_attr(pscal);

static ssize_t psctrl_show(struct kobject *kobj, struct kobj_attribute *attr,
		      char *buf)
{
	char ctrl[20]={'0'};
	char rbuf[60] = {0};
	char *p = NULL;

	read_nvm(NVM_PSCAL, rbuf, sizeof(rbuf) - 1);
	pr_info("=====rbuf=====:%s\n", rbuf);

	p = strstr(rbuf, "led");
	if (NULL != p)
	{
		strncpy(ctrl, rbuf, p-rbuf-1);
	}

	return sprintf(buf, "%s\n", ctrl);
}

static ssize_t psctrl_store(struct kobject *kobj, struct kobj_attribute *attr,
		       const char *buf, size_t count)
{
	pr_info("%s\n", buf);

	write_nvm(NVM_PSCAL, buf, count);

	return count;
}

nvm_attr(psctrl);

static ssize_t wsinfo_show(struct kobject *kobj, struct kobj_attribute *attr,
		      char *buf)
{
	read_nvm(NVM_WSINFO, buf, 32);
	
	return 32;
}

static ssize_t wsinfo_store(struct kobject *kobj, struct kobj_attribute *attr,
		       const char *buf, size_t count)
{
	int size;

	if(count > 32)
		size = 32;
	else
		size = count;
	
//	pr_info("%d,%d\n", data[0], data[1]);
	write_nvm(NVM_WSINFO, buf, size);
	
	return count;
}

nvm_attr(wsinfo);

int logo_blk_cnt = 0;
static ssize_t logo_show(struct kobject *kobj, struct kobj_attribute *attr,
		      char *buf)
{
	unsigned char rbuf[12];
	unsigned char rbuf1[12];
	unsigned short startx, starty;
	unsigned short width, height;

	read_nvm(NVM_LOGO, rbuf, 12);
	startx = rbuf[4] | (rbuf[5]<<8);
	starty = rbuf[6] | (rbuf[7]<<8);
	width = rbuf[8] | (rbuf[9]<<8);
	height = rbuf[10] | (rbuf[11]<<8);
	rbuf[4] = '\0';
	read_nvm(NVM_LOGO+78, rbuf1, 7);
	rbuf1[7] = '\0';
	logo_blk_cnt = 0;

	return sprintf(buf, "TAG: %s,%s, (x, y) = (%u, %u), (w,h) = (%u, %u)\n", \
		rbuf, rbuf1, startx, starty, width, height);
}

static ssize_t logo_store(struct kobject *kobj, struct kobj_attribute *attr,
		       const char *buf, size_t count)
{
	int size;

	if(count > 512)
		size = 512;
	else
		size = count;

//	pr_info("Write logo data to block: %d\n", logo_blk_cnt);
	write_nvm(NVM_LOGO+logo_blk_cnt, buf, size);
	logo_blk_cnt++;

	return count;
}

nvm_attr(logo);

static struct attribute *attrs[] = {
	&cpflag_attr.attr,
	&hsflag_attr.attr,
	&sdflag_attr.attr,
	&pscal_attr.attr,
	&psctrl_attr.attr,
	&wsinfo_attr.attr,
	&logo_attr.attr,
	NULL,	/* need to NULL terminate the list of attributes */
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static struct kobject *nvm_kobj = NULL;

static int __init nvm_init(void)
{
	int ret;

	nvm_kobj = kobject_create_and_add("nvm", kernel_kobj);
	if (!nvm_kobj)
		return -ENOMEM;

	ret = sysfs_create_group(nvm_kobj, &attr_group);
	if (ret)
		kobject_put(nvm_kobj);

	return 0;
}

core_initcall(nvm_init);
// end of file



