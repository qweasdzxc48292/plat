TARGET ?= embedded_debug_toolkit
CC ?= gcc

CFLAGS ?= -O2 -Wall -Wextra -std=c11 -pthread
CFLAGS += -D_GNU_SOURCE -Iinclude

LDFLAGS ?=
LDLIBS ?= -pthread -ldl

SRCS := \
	src/main.c \
	src/core/edt_time.c \
	src/core/ring_buffer.c \
	src/logic/logic_analyzer.c \
	src/protocol/protocol_decoder.c \
	src/protocol/sump_server.c \
	src/tools/v4l2_debugger.c \
	src/tools/net_capture.c \
	src/tools/system_monitor.c \
	src/plugins/plugin_manager.c \
	src/service/debug_service.c

OBJS := $(SRCS:.c=.o)

TEST_BIN := tests/edt_unit_tests
TEST_SRCS := \
	tests/test_ring_buffer.c \
	tests/test_protocol_decoder.c \
	src/core/ring_buffer.c \
	src/core/edt_time.c \
	src/protocol/protocol_decoder.c
TEST_OBJS := $(TEST_SRCS:.c=.o)

PLUGIN_SO := plugins/libsample_backend_plugin.so
PLUGIN_SRC := plugins_src/sample_backend_plugin.c

all: $(TARGET) plugins

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) $(LDLIBS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

plugins: $(PLUGIN_SO)

$(PLUGIN_SO): $(PLUGIN_SRC) include/plugin_manager.h
	@mkdir -p plugins
	$(CC) $(CFLAGS) -fPIC -shared $(PLUGIN_SRC) -o $(PLUGIN_SO)

test: $(TEST_BIN)
	./$(TEST_BIN)

$(TEST_BIN): $(TEST_OBJS)
	$(CC) $(TEST_OBJS) $(LDFLAGS) $(LDLIBS) -o $(TEST_BIN)

clean:
	rm -f $(OBJS) $(TEST_OBJS) $(TARGET) $(TEST_BIN) $(PLUGIN_SO)

.PHONY: all clean test plugins
