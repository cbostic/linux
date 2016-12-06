#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>

//#include <linux/device.h>
//#include <linux/fsi.h>
//#include <linux/module.h>
//#include <linux/slab.h>

//#include "fsi-master.h"


#define MBX_IOCTL_READ	_IOR('f', 1, struct mbx_reg_data)
#define MBX_IOCTL_WRITE	_IOWR('f', 2, struct mbx_reg_data)

struct mbx_reg_data {
	uint32_t offset;
	uint32_t value;
};

int main(int argc, char **argv)
{
	int fd;
	ssize_t rc;
	struct mbx_reg_data data;

	fd = open("/dev/mbx1", O_RDWR);
	if (fd < 0)
		err(errno, "/dev/mbx1");

	if (argc == 3) {
		data.offset = strtoul(argv[1], NULL, 0);
		data.value = strtoul(argv[2], NULL, 0);
		printf("Writing %08x to %08x\n", data.value, data.offset);

		rc = ioctl(fd, MBX_IOCTL_WRITE, &data);
		if (rc)
			err(errno, "ioctl failed");

	} if (argc == 2) {
		data.offset = strtoul(argv[1], NULL, 0);
		printf("Reading from %08x\n", data.offset);

		rc = ioctl(fd, MBX_IOCTL_READ, &data);
		if (rc)
			err(errno, "read failed");
		else
			printf("Got %08x\n", data.value);
	}
}
