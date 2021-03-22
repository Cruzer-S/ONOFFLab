CC = gcc
CFLAGS = -std=c11

UNION_DIR = ../union

TARGET = output

SRC_DIR = ./source
OBJ_DIR = ./build
INC_DIR = ./include $(UNION_DIR)

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst %.c, $(OBJ_DIR)/%.o, $(notdir $(SRCS)))
LIBS = $(shell find $(UNION_DIR)/*.o) -lwiringPi

all:
	@make -C $(UNION_DIR) all
	@make $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) -o $@ -c $< $(addprefix -I ,$(INC_DIR)) $(CFLAGS)

clean:
	@rm -f $(wildcard $(OBJ_DIR)/*.o)
	@make -C $(UNION_DIR) clean
	@rm -f $(TARGET)

union:
	@make -C $(UNION_DIR) all

debug: CFLAGS += -g -DDEBUG
debug: all
