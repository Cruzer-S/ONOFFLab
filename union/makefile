CC = gcc
CFLAGS = -std=c11
DEBUG = DEBUG

ifeq ($(DEBUG), 1)
	CFLAGS += -g -DDEBUG
endif

SRC_DIR = ./source
OBJ_DIR = ./build
INC_DIR = ./include

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst %.c, $(OBJ_DIR)/%.o, $(notdir $(SRCS)))

all: $(OBJS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) -o $@ -c $< $(addprefix -I ,$(INC_DIR)) $(CFLAGS)

clean:
	@rm -f $(wildcard $(OBJ_DIR)/*.o)
	@rm -f $(TARGET)
