SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := .
CC=mpicc

EXE := $(BIN_DIR)/word_count.out

SRC := $(wildcard $(SRC_DIR)/*.c)

OBJ := $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

CPPFLAGS:= -Iinclude -MMD -MP
CFLAGS:= -Wall -Wextra -Wpedantic
LDFLAGS:=
LDLIBS:= 

.PHONY: all clean

all: $(EXE)

$(EXE): $(OBJ) | $(BIN_DIR)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(BIN_DIR) $(OBJ_DIR):
	mkdir -p $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	@$(RM) -rv $(OBJ_DIR)

-include $(OBJ:.o=.d)