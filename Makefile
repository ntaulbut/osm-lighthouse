# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -O3

# Target executable
TARGET = build/loader

# Object files (placed in the build directory)
OBJ = build/fixed_stack.o build/load.o build/sqlite3.o

# Default target
all: $(TARGET)

# Rule to link object files into the final executable
$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) -O3

# Rule to compile fixed_stack.o
build/fixed_stack.o: fixed_stack.c
	$(CC) $(CFLAGS) -c fixed_stack.c -o build/fixed_stack.o

# Rule to compile load.o
build/load.o: load.c
	$(CC) $(CFLAGS) -c load.c -o build/load.o

# Rule to compile sqlite3.o
build/sqlite3.o: sqlite3.c
	$(CC) $(CFLAGS) -c sqlite3.c -o build/sqlite3.o

# Clean up object files and the executable
clean:
	rm -f $(OBJ) $(TARGET)
