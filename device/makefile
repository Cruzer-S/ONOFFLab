# ============================================================================= #
CC = gcc
FLAGS = -lwiringPi -std=c11
OUTPUT = output
LIBDIR = ../union/_

# ============================================================================= #
${OUTPUT}: main.o debugger.o ipc_manager.o wifi_manager.o task_manager.o
	${CC} -o ${OUTPUT} $^ ${FLAGS}

main.o: main.c wifi_manager.h debugger.h $(LIBDIR)ipc_manager.h task_manager.h
	${CC} -c main.c -o main.o ${FLAGS}

# ============================================================================= #
debugger.o: debugger.c debugger.h
	${CC} -c debugger.c -o debugger.o ${FLAGS}

wifi_manager.o: wifi_manager.c wifi_manager.h debugger.h
	${CC} -c wifi_manager.c -o wifi_manager.o ${CLFAGS}

ipc_manager.o: $(LIBDIR)ipc_manager.c $(LIBDIR)ipc_manager.h
	${CC} -c $(LIBDIR)ipc_manager.c -o ipc_manager.o ${CFLAGS}

task_manager.o: task_manager.c task_manager.h
	${CC} -c task_manager.c -o task_manager.o ${FLAGS}

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
