CC = gcc
CFLAGS = -std=c11
OUTPUT = output.out

${OUTPUT}: main.o server_handler.o device_manager.o
	${CC} -o ${OUTPUT} $^ ${CFLAGS}

main.o: main.c server_handler.h device_manager.h
	${CC} -c main.c -o main.o ${CFLAGS}

server_handler.o: server_handler.c server_handler.h
	${CC} -c server_handler.c -o server_handler.o ${CFLAGS}

device_manager.o: device_manager.c device_manager.h
	${CC} -c device_manager.c -o device_manager.o ${CFLAGS}

clean:
	rm ${wildcard *.o} ${OUTPUT}

all: ${wildcard *.o} ${OUTPUT}

debug: CFLAGS += -DDEBUG -g
debug: all

# placeholder
# $@ = target
# $^ = prerequisities
