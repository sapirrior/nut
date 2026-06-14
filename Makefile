CC = gcc
VERSION ?= 0.1.1
CFLAGS = -std=c11 -Wall -Wextra -O2 -Isrc/cli -Isrc/cli/commands -Isrc/client -Isrc/utils -D_GNU_SOURCE -DNURL_VERSION=\"$(VERSION)\"
LDFLAGS = /data/data/com.termux/files/usr/lib/libssl.a /data/data/com.termux/files/usr/lib/libcrypto.a -lpthread -ldl

SRCS = src/client/nurl_net.c src/client/nurl_tls.c src/client/nurl_http.c \
       src/cli/nurl_cli.c src/cli/nurl_runner.c src/cli/nurl_request.c \
       src/cli/commands/get.c src/cli/commands/post.c src/cli/commands/put.c \
       src/cli/commands/delete.c src/cli/commands/head.c src/cli/commands/patch.c \
       src/cli/commands/options.c src/cli/commands/resolve.c src/cli/commands/ping.c \
       src/cli/commands/download.c src/cli/commands/upload.c src/cli/commands/inspect.c \
       src/utils/nurl_utils.c src/utils/nurl_cookies.c src/utils/nurl_config.c src/main.c
OBJS = $(SRCS:.c=.o)
TARGET = nurl

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
