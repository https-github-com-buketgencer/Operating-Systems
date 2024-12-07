CC = g++
CFLAGS = -Wall -Wextra
OBJ = main.o

# Executables
MFS = makeFileSystem
FSO = fileSystemOper

# Define the target all
all: $(MFS) $(FSO)

# Link object files into the executables
$(MFS): main.o
	$(CC) $(CFLAGS) main.o -o $(MFS)

$(FSO): main.o
	$(CC) $(CFLAGS) main.o -o $(FSO)

# Compile source files into object files
main.o: main.cpp
	$(CC) $(CFLAGS) -c main.cpp -o main.o

# Run targets
make_file_system: $(MFS)
	./$(MFS) 4 mySystem.dat "/home/bktgncr/hw2/test"

file_system_operation: $(FSO)
	./$(FSO) mySystem.dat read "/d2/d4/os_quiz_notes.pdf" quiz_notes_copy.pdf
#./$(FSO) mySystem.dat read "/d2/d4/os_quiz_notes.pdf" quiz_notes_copy.pdf
#./$(FSO) mySystem.dat dir "/d2/d4"
#./$(FSO) mySystem.dat dir "/d1"
#./$(FSO) mySystem.dat dumpe2fs
#./$(FSO) mySystem.dat read "/d1/d3/deneme.txt" copy.txt
#./$(FSO) mySystem.dat read "/d2/d4/gtu_fotolar/gtu_cse_building.jpg" cse_gtu_bina_copy.jpg
# Clean target
clean:
	rm -f $(MFS) $(FSO) $(OBJ)

