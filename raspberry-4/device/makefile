# ============================================================================= #
CC = gcc
FLAGS = -lwiringPi -std=c11
OUTPUT = output

# ============================================================================= #
${OUTPUT}: main.o debugger.o wifi_manager.o server_ipc.o
	${CC} -o ${OUTPUT} $^ ${FLAGS}

main.o: main.c wifi_manager.h debugger.h server_ipc.h
	${CC} -c main.c -o main.o ${FLAGS}

# ============================================================================= #
debugger.o: debugger.c debugger.h
	${CC} -c debugger.c -o debugger.o ${FLAGS}

wifi_manager.o: wifi_manager.c wifi_manager.h
	${CC} -c wifi_manager.c -o wifi_manager.o ${CLFAGS}

server_ipc.o: server_ipc.c server_ipc.h
	${CC} -c server_ipc.c -o server_ipc.o ${FLAGS}

# ============================================================================= #
clean:
	rm ${wildcard *.o} ${OUTPUT}

all: ${wildcard *.o} ${OUTPUT}

debug: FLAGS += -g -DDEBUG
debug: ${OUTPUT}

# ============================================================================= #


# placeholder
# $@ = target
# $^ = prerequisities
#
