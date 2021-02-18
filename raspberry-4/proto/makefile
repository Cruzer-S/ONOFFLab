CC = gcc
CFLAGS = -lwiringPi
OUTPUT = output

${OUTPUT}: main.o debugger.o wifi_manager.o
	${CC} -o ${OUTPUT} $^ ${CFLAGS}

main.o: main.c wifi_manager.h debugger.h
	${CC} -c main.c -o main.o

debugger.o: debugger.c debugger.h
	${CC} -c debugger.c -o debugger.o

wifi_manager.o: wifi_manager.c wifi_manager.h
	${CC} -c wifi_manager.c -o wifi_manager.o

clean:
	rm ${wildcard *.o} ${OUTPUT}

all: ${wildcard *.o} ${OUTPUT} 

# placeholder
# $@ = target
# $^ = prerequisities
#
