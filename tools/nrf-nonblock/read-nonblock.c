#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

char str[33];

int main()
{
	int fd = open("/dev/nrf24l01", O_NONBLOCK | O_RDONLY);
	printf("Got fd %d\n", fd);
	if(fd < 0)
		printf("Got open error: %d\n", errno);
	ssize_t res = read(fd, str, 32);
	str[sizeof(str) - 1] = 0;
	if(res >= 0)
		printf("OK, %ld bytes: %s\n", res, str);
	else
		printf("Got status %d\n", errno);
	close(fd);
}
