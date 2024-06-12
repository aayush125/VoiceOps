# Compiler and compiler flags
CC := g++
COMMON_CFLAGS := `pkg-config --cflags --libs glib-2.0` -O0 -g # -O2
COMMON_LDADD := `pkg-config --libs glib-2.0` -Llib -lopus -lws2_32 -lgdi32 -lsqlite3 -g -O0 # -O2

# Client flags
CFLAGS := `pkg-config --cflags --libs gtkmm-4.0` $(COMMON_CFLAGS)
LDADD := `pkg-config --libs gtkmm-4.0` $(COMMON_LDADD)

# Server flags
SERVER_CFLAGS := $(COMMON_CFLAGS)
SERVER_LDADD := $(COMMON_LDADD) -lwinmm -lrenamenoise

# Directories
INCLUDES := -Iheaders
BIN_DIR := bin

SRC_DIR := src
SERVER_SRC_DIR := $(SRC_DIR)/server
COMMON_SRC_DIR := $(SRC_DIR)/common

OBJ_DIR := obj
SERVER_OBJ_DIR := $(OBJ_DIR)/server
COMMON_OBJ_DIR := $(OBJ_DIR)/common

# Fetch files
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
SERVER_SRCS := $(wildcard $(SERVER_SRC_DIR)/*.cpp)
COMMON_SRCS := $(wildcard $(COMMON_SRC_DIR)/*.cpp)

OBJS := $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)
SERVER_OBJS := $(SERVER_SRCS:$(SERVER_SRC_DIR)/%.cpp=$(SERVER_OBJ_DIR)/%.o)
COMMON_OBJS := $(COMMON_SRCS:$(COMMON_SRC_DIR)/%.cpp=$(COMMON_OBJ_DIR)/%.o)

# Executable name
EXEC := $(BIN_DIR)/voiceops
SERVER_EXEC := $(BIN_DIR)/server

all: $(EXEC) $(SERVER_EXEC)

# Generate executable from object files
$(EXEC): $(OBJS) $(COMMON_OBJS) | $(BIN_DIR)
	$(CC) $(OBJS) $(COMMON_OBJS) -o $(EXEC) $(LDADD)

$(SERVER_EXEC): $(SERVER_OBJS) $(COMMON_OBJS) | $(BIN_DIR)
	$(CC) $(SERVER_OBJS) $(COMMON_OBJS) -o $(SERVER_EXEC) $(SERVER_LDADD)

# Generate object files from source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(SERVER_OBJ_DIR)/%.o: $(SERVER_SRC_DIR)/%.cpp | $(SERVER_OBJ_DIR)
	$(CC) $(SERVER_CFLAGS) $(INCLUDES) -c $< -o $@

$(COMMON_OBJ_DIR)/%.o: $(COMMON_SRC_DIR)/%.cpp | $(COMMON_OBJ_DIR)
	$(CC) $(COMMON_CFLAGS) $(INCLUDES) -c $< -o $@

# Make directories if they don't already exist
$(OBJ_DIR) $(BIN_DIR) $(SERVER_OBJ_DIR) $(COMMON_OBJ_DIR):
	mkdir -p $@

.PHONY: all clean

clean:
	@echo 'Cleaning up...'
	rm -f $(OBJ_DIR)/*.o $(SERVER_OBJ_DIR)/*.o $(COMMON_OBJ_DIR)/*.o $(BIN_DIR)/voiceops.exe $(BIN_DIR)/server.exe
