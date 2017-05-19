#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

char* str = "Hello World\n";

int main()
{
	int fd = open("/dev/nrf24l01", O_NONBLOCK | O_WRONLY);
	ssize_t res = write(fd, str, strlen(str));
	if(res >= 0)
		printf("OK\n");
	else
		printf("Got status %d\n", errno);
	close(fd);
}
