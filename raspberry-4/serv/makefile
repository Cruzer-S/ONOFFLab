CC = gcc
CFLAGS = 
OUTPUT = output

${OUTPUT}: main.o server_handler.o
	${CC} -o ${OUTPUT} $^ ${CFLAGS}

main.o: main.c server_handler.h
	${CC} -c main.c -o main.o

server_handler.o: server_handler.c server_handler.h
	${CC} -c server_handler.c -o server_handler.o

clean:
	rm ${wildcard *.o} ${OUTPUT}

all: ${wildcard *.o} ${OUTPUT} 

# placeholder
# $@ = target
# $^ = prerequisities
#
