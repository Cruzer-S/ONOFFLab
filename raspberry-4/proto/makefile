CC = gcc
CFLAGS = -lwiringPi -std=c11
OUTPUT = output
DEBUG = 

${OUTPUT}: main.o debugger.o wifi_manager.o server_ipc.o
	${CC} -o ${OUTPUT} $^ ${CFLAGS} ${DEBUG}

main.o: main.c wifi_manager.h debugger.h server_ipc.h
	${CC} -c main.c -o main.o ${CFLAGS} ${DEBUG}

debugger.o: debugger.c debugger.h
	${CC} -c debugger.c -o debugger.o ${CFLAGS} ${DEBUG}

wifi_manager.o: wifi_manager.c wifi_manager.h
	${CC} -c wifi_manager.c -o wifi_manager.o ${CLFAGS} ${DEBUG}

server_ipc.o: server_ipc.c server_ipc.h
	${CC} -c server_ipc.c -o server_ipc.o ${CFLAGS} ${DEBUG}

clean:
	rm ${wildcard *.o} ${OUTPUT}

all: ${wildcard *.o} ${OUTPUT} 

debug: DEBUG += -g -DDEBUG
debug: ${OUTPUT}

# placeholder
# $@ = target
# $^ = prerequisities
#
