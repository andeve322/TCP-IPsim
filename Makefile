CC      ?= clang
CFLAGS  ?= -Wall -Wextra -pthread -I. -O2

# All C sources live in the project root
SRCS    = queue.c event.c protocols.c nodes.c http_server.c main.c
OBJS    = $(SRCS:.c=.o)
TARGET  = bin/tcpsim

all: $(TARGET)

$(TARGET): $(OBJS) | bin
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

bin:
	mkdir -p bin

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
