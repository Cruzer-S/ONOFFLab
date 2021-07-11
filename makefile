CC = gcc
CFLAGS = -std=c11 -lpthread -lrt
DEBUG = DEBUG

ifeq ($(DEBUG), 1)
	CFLAGS += -g -DDEBUG
endif

TARGET = output

SRC_DIR = ./source
OBJ_DIR = ./build
INC_DIR = ./include

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(patsubst %.c, $(OBJ_DIR)/%.o, $(notdir $(SRCS)))

all: $(OBJ_DIR) $(TARGET)
	@make $(TARGET)

$(OBJ_DIR):
	@mkdir $(OBJ_DIR)

$(TARGET): $(OBJS)
	$(CC) -o $(TARGET) $(OBJS) $(CFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) -o $@ -c $< $(addprefix -I ,$(INC_DIR)) $(CFLAGS)

clean:
	@rm -f $(wildcard $(OBJ_DIR)/*.o)
	@rm -f $(TARGET)
	@rm -f logg.txt
	@rm -f *.stl *.gcode
