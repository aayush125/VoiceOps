# Compiler and compiler flags
CC := g++
CFLAGS := `pkg-config --cflags --libs gtkmm-4.0`
LDADD = `pkg-config --libs gtkmm-4.0` -lsqlite3

# Directories
INCLUDES := -Iheaders
SRC_DIR := src
OBJ_DIR := obj
BIN_DIR := bin

# Fetch files
SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(SRCS:$(SRC_DIR)/%.cpp=$(OBJ_DIR)/%.o)

# Executable name
EXEC := $(BIN_DIR)/voiceops

all: $(EXEC)

# Generate executable from object files
$(EXEC): $(OBJS) | $(BIN_DIR)
	$(CC) $(OBJS) -o $(EXEC) $(LDADD)

# Generate object files from source files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

# Make directories if they don't already exist
$(OBJ_DIR) $(BIN_DIR):
	mkdir -p $@

.PHONY: all clean

clean:
	@echo 'Cleaning up...'
	rm -f $(OBJ_DIR)/*.o $(BIN_DIR)/voiceops.exe