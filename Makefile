# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -O3

# Target executable
TARGET = loader

# Object files
OBJ = fixed_stack.o load.o sqlite3.o

# Default target
all: $(TARGET)

# Rule to link object files into the final executable
$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET) -O3

fixed_stack.o: fixed_stack.c
	$(CC) $(CFLAGS) -c fixed_stack.c

load.o: load.c
	$(CC) $(CFLAGS) -c load.c

sqlite3.o:
	$(CC) $(CFLAGS) -c sqlite3.c

# Clean up object files and the executable
clean:
	rm -f $(OBJ) $(TARGET)
