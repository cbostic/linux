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

int main(int argc, char **argv)
{
	int fd;
	ssize_t rc;
	uint32_t offset, value;

	fd = open("/dev/mbx1", O_RDWR);
	if (fd < 0)
		err(errno, "/dev/mbx1");

	if (argc == 3) {
		offset = strtoul(argv[1], NULL, 0);
		value = strtoul(argv[2], NULL, 0);
		printf("Writing %08x to %08x\n", value, offset);

		lseek(fd, offset, SEEK_SET);
		rc = write(fd, &value, sizeof(value));
		if (rc) {
			printf("write failed with rc:%d\n", rc);
		}
		return rc;

	} if (argc == 2) {
		offset = strtoul(argv[1], NULL, 0);
		printf("Reading from %08x\n", offset);

		lseek(fd, offset, SEEK_SET);
		rc = read(fd, &value, sizeof(value));
		if (rc) {
			printf("read failed with rc:%d\n");
		} else {
			printf("read success:%08x\n", value);
		}
		return rc;
	}
}
