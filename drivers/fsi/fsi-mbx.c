/*
 * Mailbox (MBX) FSI Client device driver
 *
 * Copyright (C) IBM Corporation 2016
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERGCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/fsi.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/list.h>

#define FSI_ENGID_P9MBX		0x20
#define MBX_REG_MAX		0x1FC
#define MBX_IOCTL_READ_REG	_IOR('f', 1, struct mbx_reg_data)
#define MBX_IOCTL_WRITE_REG	_IOWR('f', 2, struct mbx_reg_data)

struct mbx_device {
	struct list_head link;
	struct fsi_device *fsi_dev;
	struct miscdevice mdev;
	char	name[32];
};

#define to_mbx_dev(x)		container_of((x), struct mbx_device, mdev)

struct mbx_reg_data {
	uint32_t offset;	/* What address */
	uint32_t value;		/* value at address */
};

static struct list_head mbx_devices;
static atomic_t mbx_idx = ATOMIC_INIT(0);
static int mbx_probe(struct device *);

static bool mbx_offset_valid(uint32_t offset)
{
	return (offset >= 0 && offset <= MBX_REG_MAX);
}

static ssize_t mbx_read(struct file *filep, char __user *buf, size_t len,
			loff_t *offset)
{
	int rc;
	uint32_t value;
	uint32_t pos = *offset;
	struct miscdevice *mdev =
				(struct miscdevice *)filep->private_data;
	struct mbx_device *mbx = to_mbx_dev(mdev);

printk("mbx_read >> engine offset:%08x\n", pos);
//	if (!mbx_offset_valid(pos))
//		return -ERANGE;
pos = 0x000000E0;
	rc = fsi_device_read(mbx->fsi_dev, pos, &value, sizeof(value));
	if (rc) {
printk("fsi_device_read failed\n");
		return -EIO;
	}
printk("fsi_device_read success\n");

	rc = copy_to_user(buf, &value, sizeof(value));
	if (rc)
		return -EFAULT;

	return 0;
}

static ssize_t mbx_write(struct file *filep, const char __user *buf,
			size_t len, loff_t *offset)
{
	int rc = 0;
	uint32_t value;
	uint32_t pos = *offset;
	struct miscdevice *mdev =
				(struct miscdevice *)filep->private_data;
	struct mbx_device *mbx = to_mbx_dev(mdev);


printk("mbx_write >> offset %08x\n", pos);
//	if (!mbx_offset_valid(pos))
//		return -ERANGE;

	rc = copy_from_user(&value, buf, sizeof(value));
	if (rc)
		return -EFAULT;
pos = 0x000000E0;
printk("mbx_write: value to write:%08x\n", value);
	rc = fsi_device_write(mbx->fsi_dev, pos, &value, sizeof(value));
	if (rc) {
printk("fsi_device_write failed\n");
		return -EIO;
	}

	return 0;
}

static const struct file_operations mbx_fops = {
	.owner	= THIS_MODULE,
	.llseek	= no_seek_end_llseek,
	.read	= mbx_read,
	.write	= mbx_write,
};

static int mbx_probe(struct device *dev)
{
	struct fsi_device *fsi_dev = to_fsi_dev(dev);
	struct mbx_device *mbx;

printk("mbx_probe >>\n");
	mbx = devm_kzalloc(dev, sizeof(*mbx), GFP_KERNEL);
	if (!mbx)
		return -ENOMEM;

	snprintf(mbx->name, sizeof(mbx->name),
			"mbx%d", atomic_inc_return(&mbx_idx));
	mbx->fsi_dev = fsi_dev;
	mbx->mdev.minor = MISC_DYNAMIC_MINOR;
	mbx->mdev.fops = &mbx_fops;
	mbx->mdev.name = mbx->name;
	mbx->mdev.parent = dev;
	list_add(&mbx->link, &mbx_devices);

	return misc_register(&mbx->mdev);
}

static struct fsi_device_id mbx_ids[] = {
	{
		.engine_type = FSI_ENGID_P9MBX,
		.version = FSI_VERSION_ANY,
	},
	{ 0 }
};

static struct fsi_driver mbx_drv = {
	.id_table = mbx_ids,
	.drv = {
		.name = "mbx",
		.bus = &fsi_bus_type,
		.probe = mbx_probe,
	}
};

static int mbx_init(void)
{
	INIT_LIST_HEAD(&mbx_devices);
	return fsi_driver_register(&mbx_drv);
}

static void mbx_exit(void)
{
	struct list_head *pos;
	struct mbx_device *mbx;

	list_for_each(pos, &mbx_devices) {
		mbx = list_entry(pos, struct mbx_device, link);
		misc_deregister(&mbx->mdev);
		devm_kfree(&mbx->fsi_dev->dev, mbx);
	}
	fsi_driver_unregister(&mbx_drv);
}

module_init(mbx_init);
module_exit(mbx_exit);
MODULE_LICENSE("GPL");
