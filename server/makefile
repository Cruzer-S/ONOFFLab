CC = gcc
FLAGS = -std=c11

OBJ_DIR = ./build/
SRC_DIR = ./src/

OBJECT := $(patsubst $(SRC_DIR)%.c,$(OBJ_DIR)/%.o,$(target))
TARGET := $(wildcard *.c)
OUTPUT := output

$(OUTPUT): $(objects)
	$(CC) -o $(OUTPUT) $(objects $(FLAGS)

clean:
	rm $(OBJECT_DIR)/%.o $(OUTPUT)
