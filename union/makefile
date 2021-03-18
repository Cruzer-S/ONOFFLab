CC = gcc
CFLAGS = -std=c11

SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c,%.o,$(SRCS))

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

all: $(OBJS)

clean:
	rm -f $(wildcard *.o)
