ifneq ($(KERNELRELEASE),)
obj-m += hub_led_drv.o
else
KDIR ?= /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f led_test

led_test: led_test.c
	$(CC) -O2 -Wall -Wextra -o $@ $<
endif

