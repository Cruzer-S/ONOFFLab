CC = gcc
CFLAGS = -lwiringPi

output: main.o debugger.o wifi_manager.o
	${CC} -o output $^ ${CFLAGS}

main.o: main.c wifi_manager.h debugger.h
	${CC} -c main.c -o main.o

debugger.o: debugger.c debugger.h
	${CC} -c debugger.c -o debugger.o

wifi_manager.o: wifi_manager.c wifi_manager.h
	${CC} -c wifi_manager.c -o wifi_manager.o

clean:
	rm *.o *.out

all: output
