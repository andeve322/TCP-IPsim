CC ?= clang
CFLAGS ?= -Wall -Wextra -pthread -Iinclude -O2
SRCS = src/queue.c src/event.c src/protocols.c src/nodes.c src/http_server.c src/main.c
OBJS = $(SRCS:.c=.o)
TARGET = bin/tcpsim

all: $(TARGET)

$(TARGET): $(OBJS) | bin
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

bin:
	mkdir -p bin

clean:
	rm -f src/*.o $(TARGET)

.PHONY: all clean
