#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    int fd;
    unsigned char buf[2] = {0, 1};

    if (argc != 2) {
        fprintf(stderr, "Usage: %s <0|1>\n", argv[0]);
        return 1;
    }

    buf[1] = (unsigned char)(atoi(argv[1]) ? 0 : 1);

    fd = open("/dev/hub_led", O_RDWR);
    if (fd < 0) {
        perror("open /dev/hub_led");
        return 1;
    }

    if (write(fd, buf, sizeof(buf)) != (ssize_t)sizeof(buf)) {
        perror("write");
        close(fd);
        return 1;
    }

    printf("set LED %s\n", atoi(argv[1]) ? "ON" : "OFF");
    close(fd);
    return 0;
}

