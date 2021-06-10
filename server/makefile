CC = gcc
CFLAGS = -std=c11 -lpthread -lrt
DEBUG = DEBUG

ifeq ($(DEBUG), 1)
	CFLAGS += -g -DDEBUG
	UNION_FLAGS = debug
else
	UNION_FLAGS = all
endif

UNION_DIR = ../union

TARGET = output

SRC_DIR = ./source
OBJ_DIR = ./build
INC_DIR = ./include $(UNION_DIR)/include

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst %.c, $(OBJ_DIR)/%.o, $(notdir $(SRCS)))
LIBS = $(shell find $(UNION_DIR)/build/*.o)

all: $(OBJ_DIR) $(TARGET)
	@make $(TARGET)

$(OBJ_DIR):
	@mkdir $(OBJ_DIR)

$(TARGET): union
$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(LIBS) $(CFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) -o $@ -c $< $(addprefix -I ,$(INC_DIR)) $(CFLAGS)

clean:
	@rm -f $(wildcard $(OBJ_DIR)/*.o)
	@make -C $(UNION_DIR) clean
	@rm -f $(TARGET)
	@rm -f logg.txt

union:
ifeq ($(DEBUG), 1)
	@make -C $(UNION_DIR) DEBUG=1
else
	@make -C $(UNION_DIR) all
endif
