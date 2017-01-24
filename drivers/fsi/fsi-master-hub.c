/*
 * Hub Master FSI Client device driver
 *
 * Copyright (C) IBM Corporation 2017
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

#include "fsi-master.h"

#define FSI_HUB_MAX_LINKS	8

#define FSI_ENGID_HUB_MASTER	0x1C
#define FSI_ENGID_HUB_LINK	0x1D

static void hub_master_handle_err(struct fsi_master *master)
{
	int rc;
	uint32_t mesrb;

	rc = fsi_device_read(master->engine, FSI_MESRB0, &mesrb, sizeof(mesrb));
	if (rc) {
printk("read of MESRB afer access failed with %d\n", rc);
	} else {
		printk("HMP8 MESRB: %08x\n", mesrb);
	}
printk("Reset HMP8 bridge\n");
	mesrb = FSI_MRESB_RST_GEN;
	rc = fsi_device_write(master->engine, FSI_MRESB0, &mesrb, sizeof(mesrb));
	if (rc) {
printk("General reset of HMP8 bridge failed with :%d\n", rc);
	}
	mesrb = FSI_MRESB_RST_ERR;
	rc = fsi_device_write(master->engine, FSI_MRESB0, &mesrb, sizeof(mesrb));
	if (rc) {
printk("Error reset of HMP8 bridge failed with :%d\n", rc);
	}
}

int hub_master_read(struct fsi_master *master, int linkno, uint8_t slave,
			uint32_t addr, void *val, size_t size)
{
	int rc = 0;

	addr += (linkno * 0x80000);
	rc =  fsi_device_read(master->link, addr, val, size);
	if (rc) {
printk("hub_master_read of %08x failed with %d\n", addr, rc);
		hub_master_handle_err(master);
	}
	return rc;
}

int hub_master_write(struct fsi_master *master, int linkno, uint8_t slave,
			uint32_t addr, const void *val, size_t size)
{
	int rc = 0;

	addr += (linkno * 0x80000);
	rc = fsi_device_write(master->link, addr, val, size);
	if (rc) {
printk("hub_master_write to %08x failed with %d\n", addr, rc);
		hub_master_handle_err(master);
	}
	return rc;
}

int hub_master_break(struct fsi_master *master, int linkno)
{
	uint32_t command;
	uint32_t break_addr = (linkno * 0x80000) + 4; // hw workaround

printk("hub_master_break >> to %08x\n", break_addr);
	command = FSI_BREAK;

	/* TODO break id should be 0, quirks for some links */
	return fsi_device_write(master->link, break_addr,  &command,
				sizeof(command));
}

int hub_master_link_enable(struct fsi_master *master, int link)
{
	int rc;
	uint32_t menp = L_MSB_MASK(link);
printk("hub_master_link_enable >> master id:%d, link:%d, menp:%08x\n", master->idx, link, menp);

	rc = fsi_device_write(master->engine, FSI_MSENP0, &menp, sizeof(menp));
	if (rc) {
printk("hub_master_link_enable: failed to write MSENP: rc:%d\n", rc);
		return rc;
	}
	mdelay(10); // necessary?

// DEBUG ONLY vvv
	rc = fsi_device_read(master->engine, FSI_MLEVP0, &menp, sizeof(menp));
printk("hub_master_link_enable: MLEVP after link enable: %08x\n", menp);
	return rc;



#if 0
	rc = hub_master_break(master, link);
	if (rc) {
printk("hub_master_link_enable: failed to issue break, rc:%d\n", rc);
		return rc;
	}
	mdelay(10); // necessary?

	rc = fsi_device_write(master->engine, FSI_MCENP0, &menp, sizeof(menp));
	if (rc) {
printk("hub_master_link_enable: failed to disable link after break, rc:%d\n", rc);
		return rc;
	}
	mdelay(10); // necessary?

	rc = fsi_device_read(master->engine, FSI_MLEVP0, &menp, sizeof(menp));
	if (rc) {
printk("hub_master_link_enable: failed to read MLEVP after link disable, rc:%d\n", rc);
		return rc;
	}

printk("hub_master_link_enable: mlevp after enable/break/disable: %08x\n", menp);
	if ((L_MSB_MASK(link) & menp) == 0) {
		/* Nothing plugged into this link slot */
//		return -ENODEV;
printk("hub_master_link_enable: no indication this link has anything plugged, try anyway\n");
	}
	mdelay(10); // necessary?

	rc = fsi_device_read(master->engine, FSI_MENP0, &menp, sizeof(menp));
	if (rc) {
printk("hub_master_link_enable: failed to read back menp state\n");
	} else {
printk("hub_master_link_enable; menp state read back as %08x\n", menp);
	}

printk("hub_master_link_enable: enable link second and final time\n");
	return fsi_device_write(master->engine, FSI_MSENP0, &menp, sizeof(menp));
#endif

}

static int hub_master_probe(struct device *dev)
{
	struct fsi_device *fsi_dev = to_fsi_dev(dev);
	struct fsi_master *master;
	int rc = 0;
	uint32_t mver;

printk("hub_master_probe >> 0123.1044\n");

	if (master && (master->n_links >= FSI_HUB_MAX_LINKS * 2))
		return 0;

	if (fsi_dev->id->engine_type == FSI_ENGID_HUB_MASTER) {

printk("hub_master_probe : Master engine type\n");
mdelay(100);
		master = devm_kzalloc(dev, sizeof(*master), GFP_KERNEL);
		if (!master)
			return -ENOMEM;

		fsi_dev->master = master;

		/* TODO: can use master->dev to get at the engine possibly */
		master->engine = fsi_dev;
		master->read = hub_master_read;
		master->write = hub_master_write;
		master->send_break = hub_master_break;
		master->link_enable = hub_master_link_enable;
		rc = fsi_set_next_master(fsi_dev, master);

		/* Initialize the HMP8 engine */
		rc = fsi_device_read(master->engine, FSI_MVER, &mver, sizeof(mver));
		if (rc) {
printk("hub_master_probe: could not read MVER\n");
			return rc;
		}
printk("hub_master_probe: MVER: %08x\n", mver);
		mver = FSI_MRESP_RST_ALL_MASTER | FSI_MRESP_RST_ALL_LINK
			| FSI_MRESP_RST_MCR | FSI_MRESP_RST_PYE;
		rc = fsi_device_write(master->engine, FSI_MRESP0, &mver, sizeof(mver));
		if (rc) {
printk("hub_master_probe: could not write MRESP0\n");
			return rc;
		}
		mver = FSI_MECTRL_EOAE | FSI_MECTRL_P8_AUTO_TERM;
		rc = fsi_device_write(master->engine, FSI_MECTRL, &mver, sizeof(mver));
		if (rc) {
printk("hub_master_probe: could not write MECTRL\n");
			return rc;
		}

		mver = FSI_MMODE_EIP | FSI_MMODE_ECRC | FSI_MMODE_EPC
			| fsi_mmode_crs0(1) | fsi_mmode_crs1(1)
			| FSI_MMODE_P8_TO_LSB;
		rc = fsi_device_write(master->engine, FSI_MMODE, &mver, sizeof(mver));
		if (rc) {
printk("hub_master_probe: could not write MMMODE\n");
			return rc;
		}

		mver = 0xffff0000;
		rc = fsi_device_write(master->engine, FSI_MDLYR, &mver, sizeof(mver));
		if (rc) {
printk("hub_master_probe: could not write MDLYR\n");
			return rc;
		}

		mver = ~0;
		rc = fsi_device_write(master->engine, FSI_MSENP0, &mver, sizeof(mver));
		if (rc) {
printk("hub_master_probe: could not write MSENP0\n");
			return rc;
		}

		udelay(1000);

		rc = fsi_device_write(master->engine, FSI_MCENP0, &mver, sizeof(mver));
		if (rc) {
printk("hub_master_probe: could not write MCENP0\n");
			return rc;
		}

		rc = fsi_device_read(master->engine, FSI_MAEB, &mver, sizeof(mver));
		if (rc) {
printk("hub_master_probe: could not read MAEB\n");
			return rc;
		}

		mver = FSI_MRESP_RST_ALL_MASTER | FSI_MRESP_RST_ALL_LINK;
		rc = fsi_device_write(master->engine, FSI_MRESP0, &mver, sizeof(mver));
		if (rc) {
printk("hub_master_probe: could not write MRESP0 #2\n");
			return rc;
                }

		rc = fsi_device_read(master->engine, FSI_MLEVP0, &mver, sizeof(mver));
		if (rc) {
printk("hub_master_probe: could not read MLEVP\n");
			return rc;
		}
printk("hub_master_probe: MLEVP after init: %08x\n", mver);

printk("hub_master_probe: Reset bridge\n");
		mver = FSI_MRESB_RST_GEN;
		rc = fsi_device_write(master->engine, FSI_MRESB0, &mver, sizeof(mver));
		if (rc) {
printk("hub_master_probe: could not write to MRESB GEN\n");
		}
		mver = FSI_MRESB_RST_ERR;
		rc = fsi_device_write(master->engine, FSI_MRESB0, &mver, sizeof(mver));
		if (rc) {
printk("hub_master_probe: could not write to MRESB ERR\n");
		}


	} else {
		/* A hub link device */
		/* Treat all hub link space as one 'link' device */
		master = fsi_get_link_master(fsi_dev);

printk("hub_master_probe: Hub link type\n");
mdelay(100);
		if (!master) {
printk("hub_master_probe:  !master, exiting\n");
			return -ENODEV;
		}
printk("hub_master_probe: 20\n");
		if (master->n_links == 0) {
printk("hub_master_probe: 21\n");

			/*
			 * CFAM Config space doesn't list correct size
			 * for hub links, fix this.
			 */
			fsi_dev->size = fsi_dev->addr * FSI_HUB_MAX_LINKS;
			master->link = fsi_dev;
		}
		master->n_links++;
printk("hub_master_probe: number links:%d\n", master->n_links);

		if (master->n_links == (FSI_HUB_MAX_LINKS * 2)) {
printk("hub_master_probe: register hub master\n");
			return fsi_master_register(master);
		}
	}

printk("hub_master_probe <<\n");

	return 0;
}

static struct fsi_device_id hub_master_ids[] = {
	{
		.engine_type = FSI_ENGID_HUB_MASTER,
		.version = FSI_VERSION_ANY,
	},
	{
		.engine_type = FSI_ENGID_HUB_LINK,
		.version = FSI_VERSION_ANY,
	},
	{ 0 }
};

static struct fsi_driver hub_master_drv = {
	.id_table = hub_master_ids,
	.drv = {
		.name = "hub_master",
		.bus = &fsi_bus_type,
		.probe = hub_master_probe,
	}
};

static int hub_master_init(void)
{
	return fsi_driver_register(&hub_master_drv);
}

static void hub_master_exit(void)
{
	fsi_driver_unregister(&hub_master_drv);
}

module_init(hub_master_init);
module_exit(hub_master_exit);
MODULE_LICENSE("GPL");
