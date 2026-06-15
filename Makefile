CC = gcc
VERSION ?= 0.1.1
CFLAGS = -std=c11 -Wall -Wextra -Os -ffunction-sections -fdata-sections -fno-ident \
         -Isrc/cli -Isrc/cli/parser -Isrc/cli/runner -Isrc/cli/commands \
         -Isrc/engine -Isrc/engine/net -Isrc/engine/tls -Isrc/engine/http -Isrc/engine/utils \
         -D_GNU_SOURCE -DNURL_VERSION=\"$(VERSION)\"
LDFLAGS = -Wl,-Bstatic -lssl -lcrypto -Wl,-Bdynamic -lpthread -ldl -lz -Wl,--gc-sections -s

SRCS = src/engine/net/nurl_net.c \
       src/engine/tls/nurl_tls.c \
       src/engine/http/nurl_http.c \
       src/engine/http/nurl_decompress.c \
       src/engine/http/nurl_redirect.c \
       src/engine/utils/nurl_utils.c \
       src/engine/utils/nurl_cookies.c \
       src/engine/utils/nurl_config.c \
       src/engine/nurl_engine.c \
       src/cli/parser/nurl_cli.c \
       src/cli/runner/nurl_runner.c \
       src/cli/runner/nurl_request.c \
       src/cli/commands/get.c \
       src/cli/commands/post.c \
       src/cli/commands/put.c \
       src/cli/commands/delete.c \
       src/cli/commands/head.c \
       src/cli/commands/patch.c \
       src/cli/commands/options.c \
       src/cli/commands/resolve.c \
       src/cli/commands/ping.c \
       src/cli/commands/download.c \
       src/cli/commands/upload.c \
       src/cli/commands/inspect.c \
       src/main.c
TARGET = nurl

all: $(TARGET)

$(TARGET):
	$(CC) $(CFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

clean:
	rm -f $(TARGET)
