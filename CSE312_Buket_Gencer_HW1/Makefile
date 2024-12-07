# Makefile

# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -std=c++11

# Target executable
TARGET = main.o

# Source file
SRC = main.cpp

# Default rule for compiling the program
all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

# Rule for running the program with specific arguments
run: $(TARGET)
	./$(TARGET) 4 5 7 LRU 100 diskFileNamedat

# Clean rule for removing the compiled executable
clean:
	rm -f $(TARGET)
