TARGET ?= smart_home_hub
CC ?= gcc

USE_PAHO ?= 0
PAHO_DIR ?= ../6-4-3_MQTT设备程序编写/mqtt_device/paho.mqtt.c

SRCS := $(shell find src -name '*.c')
OBJS := $(SRCS:.c=.o)

CFLAGS ?= -O2 -Wall -Wextra -std=c11 -pthread
CFLAGS += -Iinclude

LDFLAGS ?=
LDLIBS ?= -pthread

ifeq ($(USE_PAHO),1)
CFLAGS += -DHUB_USE_PAHO -I$(PAHO_DIR)/build/src
LDLIBS += -L$(PAHO_DIR)/build/src -lpaho-mqtt3c
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) $(LDLIBS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

install: $(TARGET)
	mkdir -p $(DESTDIR)
	cp -f $(TARGET) $(DESTDIR)/
	cp -f config/hub_config.json $(DESTDIR)/
	cp -f scripts/run_hub.sh $(DESTDIR)/

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean install

