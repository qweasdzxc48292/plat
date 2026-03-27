CC ?= gcc
ROOT := ../..

CFLAGS ?= -O2 -Wall -Wextra -std=c11 -pthread
CFLAGS += -I$(ROOT)/include

SRCS := \
	$(ROOT)/src/core/hub_config.c \
	$(ROOT)/src/device/device_hal.c \
	$(ROOT)/src/utils/hub_log.c \
	hal_demo.c

OBJS := $(SRCS:.c=.o)
TARGET := hal_demo

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -pthread -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean

