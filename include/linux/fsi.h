/*
 * FSI device & driver interfaces
 *
 * Copyright (C) IBM Corporation 2016
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef LINUX_FSI_H
#define LINUX_FSI_H

#include <linux/device.h>

struct fsi_device {
	struct list_head	link;	/* for slave's list */
	struct device		dev;
	const struct fsi_device_id *id;
	struct fsi_master	*master;
	u8			engine_type;
	u8			version;
	u8			unit;
	u8			si1s_bit;
	struct fsi_slave	*slave;
	uint32_t		addr;
	uint32_t		size;
	int (*irq_handler)(int, void *);
};

extern int fsi_device_read(struct fsi_device *dev, uint32_t addr,
		void *val, size_t size);
extern int fsi_device_write(struct fsi_device *dev, uint32_t addr,
		const void *val, size_t size);
extern int fsi_device_peek(struct fsi_device *dev, void *val);

struct fsi_device_id {
	u8	engine_type;
	u8	version;
};

#define FSI_VERSION_ANY		0

#define FSI_DEVICE(t) \
	.engine_type = (t), .version = FSI_VERSION_ANY,

#define FSI_DEVICE_VERSIONED(t, v) \
	.engine_type = (t), .version = (v),

struct fsi_driver {
	struct device_driver		drv;
	const struct fsi_device_id	*id_table;
};

#define to_fsi_dev(devp) container_of(devp, struct fsi_device, dev)
#define to_fsi_drv(drvp) container_of(drvp, struct fsi_driver, drv)

extern int fsi_driver_register(struct fsi_driver *);
extern void fsi_driver_unregister(struct fsi_driver *);

/* module_fsi_driver() - Helper macro for drivers that don't do
 * anything special in module init/exit.  This eliminates a lot of
 * boilerplate.  Each module may only use this macro once, and
 * calling it replaces module_init() and module_exit()
 */
#define module_fsi_driver(__fsi_driver) \
		module_driver(__fsi_driver, fsi_driver_register, \
				fsi_driver_unregister)

extern struct bus_type fsi_bus_type;

extern struct fsi_master *fsi_get_link_master(struct fsi_device *dev);
extern int fsi_set_next_master(struct fsi_device *dev,
		struct fsi_master *master);
extern int fsi_enable_irq(struct fsi_device *dev);
extern void fsi_disable_irq(struct fsi_device *dev);

#define FSI_MAX_LINKS	64

#endif /* LINUX_FSI_H */
