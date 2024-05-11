# Compiler and compiler flags
CC := g++
CFLAGS := `pkg-config --cflags --libs gtkmm-4.0`
LDADD = `pkg-config --libs gtkmm-4.0` -lws2_32 -lsqlite3

SERVER_CFLAGS := -O2
SERVER_LDADD := -lws2_32 -lsqlite3

# Directories
INCLUDES := -Iheaders
BIN_DIR := bin

SRC_DIR := src
SERVER_SRC_DIR := $(SRC_DIR)/server

OBJ_DIR := obj
SERVER_OBJ_DIR := $(OBJ_DIR)/server

# Fetch files
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
SERVER_SRCS := $(wildcard $(SERVER_SRC_DIR)/*.cpp)

OBJS := $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
SERVER_OBJS := $(SERVER_SRCS:$(SERVER_SRC_DIR)/%.cpp=$(SERVER_OBJ_DIR)/%.o)

# Executable name
EXEC := $(BIN_DIR)/voiceops
SERVER_EXEC := $(BIN_DIR)/server

all: $(EXEC) $(SERVER_EXEC)

# Generate executable from object files
$(EXEC): $(OBJS) | $(BIN_DIR)
	$(CC) $(OBJS) -o $(EXEC) $(LDADD)

$(SERVER_EXEC): $(SERVER_OBJS) | $(BIN_DIR)
	$(CC) $(SERVER_OBJS) -o $(SERVER_EXEC) $(SERVER_LDADD)

# Generate object files from source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(SERVER_OBJ_DIR)/%.o: $(SERVER_SRC_DIR)/%.cpp | $(SERVER_OBJ_DIR)
	$(CC) $(SERVER_CFLAGS) $(INCLUDES) -c $< -o $@

# Make directories if they don't already exist
$(OBJ_DIR) $(BIN_DIR) $(SERVER_OBJ_DIR):
	mkdir -p $@

.PHONY: all clean

clean:
	@echo 'Cleaning up...'
	rm -f $(OBJ_DIR)/*.o $(SERVER_OBJ_DIR)/*.o $(BIN_DIR)/voiceops.exe $(BIN_DIR)/server.exe
