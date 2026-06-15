CC      = gcc
VERSION ?= 0.2.1
CFLAGS  = -std=c11 -Wall -Wextra -Os -ffunction-sections -fdata-sections \
          -fno-ident -D_GNU_SOURCE -DNURL_VERSION=\"$(VERSION)\" \
          -Isrc -Isrc/cli -Isrc/cli/parser -Isrc/cli/runner -Isrc/cli/commands \
          -Isrc/engine -Isrc/engine/net -Isrc/engine/tls -Isrc/engine/http -Isrc/engine/utils -Isrc/compat
LDFLAGS = -Wl,-Bstatic -lssl -lcrypto -Wl,-Bdynamic -lpthread -ldl -lz \
          -Wl,--gc-sections -s

SRCS    := $(shell find src -name '*.c')
OBJS    := $(SRCS:src/%.c=build/%.o)
TARGET  = nurl

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

build/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build $(TARGET)

.PHONY: all clean
