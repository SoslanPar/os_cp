CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude
LDFLAGS = -lrt -lpthread

SRC_DIR = src
OBJ_DIR = obj
BIN_DIR = bin

SERVER_SRC = $(SRC_DIR)/server.c $(SRC_DIR)/game.c
CLIENT_SRC = $(SRC_DIR)/client.c

SERVER_OBJ = $(SERVER_SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
CLIENT_OBJ = $(CLIENT_SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

all: directories $(BIN_DIR)/server $(BIN_DIR)/client

directories:
	mkdir -p $(OBJ_DIR) $(BIN_DIR)

$(BIN_DIR)/server: $(SERVER_OBJ)
	$(CC) $(SERVER_OBJ) -o $@ $(LDFLAGS)

$(BIN_DIR)/client: $(CLIENT_OBJ)
	$(CC) $(CLIENT_OBJ) -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: all clean directories
