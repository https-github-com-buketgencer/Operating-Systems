// Buket Gencer 210104004298
/*
### Project Overview
- **Objective**: Design and implement a simulated virtual memory management system with various page replacement algorithms in C/C++.
- **Simulation Details**:
  - Use an integer C++ array as physical memory.
  - Two C++ threads simulate two separate processes sharing the same C++ array.
  - Virtual memory may be larger than physical memory, with excess data stored on a disk file.

### Program Steps
1. **Memory Initialization**:
   - Each thread fills its entire virtual memory with random integers.
   - Use `srand(1000)` for thread 1 and `srand(2000)` for thread 2 for consistent random number generation.
2. **Sorting and Searching**:
   - Each thread sorts its array using merge sort.
   - Perform binary searches on the sorted array for 5 numbers, 2 of which are not in the array.

### Memory Access Functions
- **Functions**:
  - `void set(unsigned int threadNum, unsigned int index, int value);`
  - `int get(unsigned int threadNum, unsigned int index);`
- **Purpose**: These functions access virtual memory. If the data isn't in physical memory, the page replacement algorithm is triggered.

### Global Page Replacement Policy
- Implement a global policy where page replacement statistics are calculated within the `get` and `set` functions.

### Part 1: Page Table Design
- **Task**: Design a page table structure supporting Clock (CL) and Least-Recently-Used (LRU) algorithms.
- **Report**:
  - Include figures and explanations of each field in the page table entries.
  - Reference implementation code (line numbers, C++ files, etc.).

### Part 2: Program Implementation
- **Program Name**: `sortArrays`
- **Command Line Structure**:

  sortArrays frameSize numPhysical numVirtual pageReplacement pageTablePrintInt diskFileName.dat

- **Parameters**:
  - `frameSize`: Size of page frames.
  - `numPhysical`: Total number of page frames in physical memory.
  - `numVirtual`: Total number of frames in virtual address space.
  - `pageReplacement`: Page replacement algorithm (CL, LRU).
  - `pageTablePrintInt`: Interval of memory accesses after which the page table is printed.
  - `diskFileName.dat`: Name of the file storing out-of-memory frames.

- **Example Command**:

  sortArrays 8 5 7 LRU 100 diskFileName.dat

  - Defines frame size, physical and virtual frames, page replacement algorithm, and disk file name.

### Statistics
- **Outputs for Each Thread**:
  - Number of reads
  - Number of writes
  - Number of page misses
  - Number of page replacements
  - Number of disk page writes
  - Number of disk page reads
  - Number of physical frames in memory

### Implementation Requirements
- Implement all necessary functions including `get/set`, page replacement, and all program phases.
- Virtual memory is used solely for array storage. Use C++ variables for temporary sorting data.
*/

#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <fstream>
#include <string>
#include <queue>
#include <cmath>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <climits>
#include <chrono>
#include <cstdio>

using namespace std;
#define MAX_PAGE_TABLE_SIZE 1000000      // maximum page table size in number of entries
#define MAX_PHYSICAL_MEMORY_SIZE 1000000 // maximum physical memory size in number of entries
int globalFrameSize = 0;                 // global frame size
int virtual_page_number = 0;             // number of virtual pages
int physical_memory_size = 0;            // size of physical memory
int disk_size = 0;                       // size of disk
string disk_file_name;                   // disk file name
int fd;                                  // file descriptor for disk file
string pageReplacement;                  // page replacement algorithm
static int clockHand = 0;                // clock hand for clock algorithm
unsigned int memoryAccessCounter = 0;    // memory access counter
unsigned int pageTablePrintInt;          // page table print interval

// declaraiton of functions
void printPageTable();                                            // print page table
void initializePageTable();                                       // initialize page table
void printPhysicalMemory();                                       // print physical memory
void initializePhysicalMemory();                                  // initialize physical memory
void fillVirtualMemory(uint32_t threadNum);                       // fill virtual memory with random integers
void set(unsigned int threadNum, unsigned int index, int value);  // set the value of the data at the given index in the virtual memory
void initializeDisk();                                            // initialize disk
void printDisk();                                                 // print disk
int get(unsigned int threadNum, unsigned int index);              // get the value of the data at the given index in the virtual memory
void merge(unsigned int threadNum, int left, int mid, int right); // merge function for merge sort
void mergeSort(unsigned int threadNum, int left, int right);      // merge sort function
int binarySearch(int threadNum, int l, int r, int x);             // binary search function
// declaraiton of struct
struct PageTableEntry;      // page table entry
struct physicalMemoryEntry; // physical memory entry

int applyLRUReplacement(vector<PageTableEntry> &pageTable, vector<physicalMemoryEntry> &physicalMemory, int fd, int globalFrameSize, int virtual_page_number);
int applyClockReplacement(vector<PageTableEntry> &pageTable, vector<physicalMemoryEntry> &physicalMemory, int fd, int globalFrameSize, int virtual_page_number, int &clockHand);

// page table entry
struct PageTableEntry
{
    int frameNumber;       // frame number in physical memory
    int valid;             // valid bit
    int modified;          // modified bit
    int referenced;        // referenced bit
    time_t lastAccessTime; // last access time in nano seconds for LRU
};

// statistics structure
struct Statistics
{
    unsigned int reads;
    unsigned int writes;
    unsigned int pageMisses;
    unsigned int pageReplacements;
    unsigned int diskPageWrites;
    unsigned int diskPageReads;
    unsigned int physicalFramesInMemory;
};

// global statistics array
Statistics statsOfProgram = {0, 0, 0, 0, 0, 0, 0};

// page table
vector<PageTableEntry> pageTable(MAX_PAGE_TABLE_SIZE); // page table

void printStatistics()
{
    // print the statistics in a table format
    printf("┌───────────────────────────────┬────────────┐\n");
    printf("│ Statistic                     │ Value      │\n");
    printf("├───────────────────────────────┼────────────┤\n");
    printf("│ Reads                         │ %10u │\n", statsOfProgram.reads);
    printf("│ Writes                        │ %10u │\n", statsOfProgram.writes);
    printf("│ Page Misses                   │ %10u │\n", statsOfProgram.pageMisses);
    printf("│ Page Replacements             │ %10u │\n", statsOfProgram.pageReplacements);
    printf("│ Disk Page Writes              │ %10u │\n", statsOfProgram.diskPageWrites);
    printf("│ Disk Page Reads               │ %10u │\n", statsOfProgram.diskPageReads);
    printf("│ Physical Frames In Memory     │ %10u │\n", statsOfProgram.physicalFramesInMemory);
    printf("└───────────────────────────────┴────────────┘\n");
}

// page table print function
void printPageTable()
{
    // print the page table entries
    printf("┌──────────────┬──────────────┬────────┬─────────┬───────────┬────────────────────┐\n");
    printf("│ Page Table   │ Frame Number │ Valid  │ Modified│ Referenced│ Last Access Time   │\n");
    printf("├──────────────┼──────────────┼────────┼─────────┼───────────┼────────────────────┤\n");

    // print the page table entries for each page
    for (int i = 0; i < virtual_page_number; i++)
    {
        printf("│ Entry %2d     │ %12d │ %6d │ %8d │ %9d │ %18ld │\n",
               i,
               pageTable[i].frameNumber,
               pageTable[i].valid,
               pageTable[i].modified,
               pageTable[i].referenced,
               static_cast<long>(pageTable[i].lastAccessTime));
    }

    // print the end of the page table
    printf("└──────────────┴──────────────┴────────┴─────────┴───────────┴────────────────────┘\n");
}

// initialize the page table
void initializePageTable()
{
    for (int i = 0; i < virtual_page_number; i++)
    {
        pageTable[i].frameNumber = -1; // if -1 then it is not in physical memory.
        pageTable[i].valid = 0;
        pageTable[i].modified = 0;
        pageTable[i].referenced = 0;
        pageTable[i].lastAccessTime = 0;
    }
}

// physical memory entry
struct physicalMemoryEntry
{
    int data;      // random integer
    int threadNum; // thread number that the data belongs to . 1 or 2
    int diskIndex; // if the data is not in physical memory, then it is in disk. diskIndex is the index of the data in the disk.
};

// physical memory entries vector
vector<physicalMemoryEntry> physicalMemory; // physical memory

void initializePhysicalMemory()
{
    // check if physical memory is null

    for (int i = 0; i < physical_memory_size; i++)
    {
        // use push back to add the physical memory entries
        physicalMemory.push_back({-1, -1, -1});
    }
}

// print physical memory
void printPhysicalMemory()
{
    // print physical memory entries
    for (int i = 0; i < physical_memory_size; i++)
    {
        cout << "Physical Memory Entry " << i << ": ";                    // entry means every integer in the physical memory
        cout << "Data: " << physicalMemory[i].data << ", ";               // data is the random integer
        cout << "Thread Number: " << physicalMemory[i].threadNum << ", "; // thread number that the data belongs to
        cout << "Disk Index: " << physicalMemory[i].diskIndex << endl;    // if the data is not in physical memory, then it is in disk. diskIndex is the index of the data in the disk.
    }

    /* print every begin of the frame
    for (int i = 0; i < physical_memory_size; i += globalFrameSize)
    {
        cout << "Frame " << i / globalFrameSize << ": "; // frame number
        for (int j = 0; j < globalFrameSize; j++)        // print the data in the frame
        {
            cout << physicalMemory[i + j].data << " "; // data is the random integer
        }
        cout << endl;
    }
    */
}

// fill the virtual memory with tasks: fill the entire virtual memory with random integers. use the same random numbers for all runs of the program.
void fillVirtualMemory(uint32_t threadNum)
{
    // fillVirtualMemory function is used to fill the entire virtual memory with random integers.
    // before filling the virtual memory, call srand(1000) for thread 1 and call srand(2000) for thread 2

    // srand(1000) for thread 1 and srand(2000) for thread 2
    srand(1000);

    // size of the virtual memory
    size_t size = virtual_page_number * globalFrameSize;

    // according to thread nume fill the virtual memory with random integers

    for (size_t i = 0; i < size; i++)
    {
        // generate random integer
        int randomValue = rand() % 1000; // random integer
        // set the value of the data in the virtual memory
        set(threadNum, i, randomValue);
    }
}

// Function to apply the Clock Replacement Algorithm
int applyClockReplacement(vector<PageTableEntry> &pageTable, vector<physicalMemoryEntry> &physicalMemory, int fd, int globalFrameSize, int virtual_page_number, int &clockHand)
{
    while (true)
    {
        // If Referenced bit is 0 and Valid bit is 1, then evict the page. Otherwise, set the Referenced bit to 0.
        if (pageTable[clockHand].valid && pageTable[clockHand].referenced == 0)
        {
            int frameNumber = pageTable[clockHand].frameNumber;

            // ıf page is modified, write it to the disk
            if (pageTable[clockHand].modified)
            {
                vector<int> data(globalFrameSize);
                for (uint32_t j = 0; j < globalFrameSize; ++j)
                {
                    data[j] = physicalMemory[frameNumber * globalFrameSize + j].data;
                }

                int pos = clockHand * globalFrameSize; // calculate the position with using clock hand and global frame size
                // increade the number of disk page writes
                statsOfProgram.diskPageWrites++;
                lseek(fd, pos * sizeof(int), SEEK_SET);
                write(fd, &data[0], globalFrameSize * sizeof(int)); // write the data to the disk
            }

            // update the page table entry for the evicted page
            pageTable[clockHand].valid = 0;
            pageTable[clockHand].modified = 0;
            pageTable[clockHand].referenced = 0;
            pageTable[clockHand].lastAccessTime = 0;
            pageTable[clockHand].frameNumber = -1;

            int evictedFrame = frameNumber;

            // update the clock hand for the next page
            clockHand = (clockHand + 1) % virtual_page_number;

            return evictedFrame;
        }

        // If referenced bit is 1, set it to 0 and move to the next page
        pageTable[clockHand].referenced = 0;
        clockHand = (clockHand + 1) % virtual_page_number;
    }
}

// Function to apply the LRU Replacement Algorithm
int applyLRUReplacement(vector<PageTableEntry> &pageTable, vector<physicalMemoryEntry> &physicalMemory, int fd, int globalFrameSize, int virtual_page_number)
{
    long long minTime = LLONG_MAX;
    int lruPage = -1;

    // Find the least recently used page
    for (int i = 0; i < virtual_page_number; ++i)
    {
        if (pageTable[i].valid && pageTable[i].lastAccessTime < minTime)
        {
            minTime = pageTable[i].lastAccessTime;
            lruPage = i;
        }
    }

    int frameNumber = pageTable[lruPage].frameNumber;

    // Page replacement and disk index update
    if (pageTable[lruPage].modified)
    {
        vector<int> data;
        for (uint32_t j = 0; j < globalFrameSize; ++j)
        {
            data.push_back(physicalMemory[frameNumber * globalFrameSize + j].data);
        }

        // find the position of the page in the disk
        int pos = lruPage * globalFrameSize;
        // increase the number of disk page writes
        statsOfProgram.diskPageWrites++;
        lseek(fd, pos * sizeof(int), SEEK_SET);
        write(fd, &data[0], globalFrameSize * sizeof(int));

        // update the disk index
        for (uint32_t j = 0; j < globalFrameSize; ++j)
        {
            physicalMemory[frameNumber * globalFrameSize + j].diskIndex = pos + j;
        }
    }

    pageTable[lruPage].valid = 0;
    pageTable[lruPage].modified = 0; //???
    pageTable[lruPage].referenced = 0;
    pageTable[lruPage].lastAccessTime = 0;
    pageTable[lruPage].frameNumber = -1;
    return frameNumber; // LRU return the frame number that will be replaced
}

// set function to set the value of the data at the given index in the virtual memory
void set(unsigned int threadNum, unsigned int index, int value)
{

    // increase the write counter
    statsOfProgram.writes++;
    int pageIndex = index / globalFrameSize; // which page the index belongs to
    int offset = index % globalFrameSize;    // offset of the index in the page

    // current time in microseconds
    auto now = std::chrono::steady_clock::now();
    auto now_us = std::chrono::time_point_cast<std::chrono::microseconds>(now);
    long long time_in_us = now_us.time_since_epoch().count();

    if (pageTable[pageIndex].valid) // check if the page is in the physical memory
    {
        int frameNumber = pageTable[pageIndex].frameNumber;
        physicalMemory[frameNumber * globalFrameSize + offset].data = value;
        physicalMemory[frameNumber * globalFrameSize + offset].threadNum = threadNum;
        physicalMemory[frameNumber * globalFrameSize + offset].diskIndex = index;

        // update the page table entry
        pageTable[pageIndex].modified = 1;
        pageTable[pageIndex].referenced = 1;
        pageTable[pageIndex].lastAccessTime = time_in_us;
    }
    else
    {
        int allocated = 0;
        int frameNumber = -1;

        // Find an empty frame in the physical memory
        for (uint32_t i = 0; i < physical_memory_size; i += globalFrameSize)
        {
            bool frameEmpty = true;
            for (uint32_t j = 0; j < globalFrameSize; ++j)
            {
                if (physicalMemory[i + j].threadNum != -1)
                {
                    frameEmpty = false;
                    break;
                }
            }
            if (frameEmpty) // if the frame is empty
            {
                frameNumber = i / globalFrameSize;
                physicalMemory[frameNumber * globalFrameSize + offset].data = value;
                physicalMemory[frameNumber * globalFrameSize + offset].threadNum = threadNum;
                physicalMemory[frameNumber * globalFrameSize + offset].diskIndex = -1;

                pageTable[pageIndex].frameNumber = frameNumber;
                pageTable[pageIndex].valid = 1;
                pageTable[pageIndex].modified = 1;
                pageTable[pageIndex].referenced = 1;
                pageTable[pageIndex].lastAccessTime = time_in_us;
                allocated = 1;
                break;
            }
        }
        // when the page fault occurs
        if (!allocated)
        {
            if (pageReplacement == "LRU")
            {
                frameNumber = applyLRUReplacement(pageTable, physicalMemory, fd, globalFrameSize, virtual_page_number);
                // increase the number of page replacements
                statsOfProgram.pageReplacements++;
                // increase the number of page misses
                statsOfProgram.pageMisses++;
            }
            else if (pageReplacement == "CL")
            {

                frameNumber = applyClockReplacement(pageTable, physicalMemory, fd, globalFrameSize, virtual_page_number, clockHand);
                // increase the number of page replacements
                statsOfProgram.pageReplacements++;
                // increase the number of page misses
                statsOfProgram.pageMisses++;
            }

            // increase the number of disk page reads
            statsOfProgram.diskPageReads++;
            lseek(fd, pageIndex * globalFrameSize * sizeof(int), SEEK_SET);
            read(fd, &physicalMemory[frameNumber * globalFrameSize], globalFrameSize * sizeof(int));

            pageTable[pageIndex].frameNumber = frameNumber;
            pageTable[pageIndex].valid = 1;
            pageTable[pageIndex].modified = 1;
            pageTable[pageIndex].referenced = 1;
            pageTable[pageIndex].lastAccessTime = time_in_us;

            physicalMemory[frameNumber * globalFrameSize + offset].data = value;
            physicalMemory[frameNumber * globalFrameSize + offset].threadNum = threadNum;

            // update the thread number and disk index
            for (uint32_t j = 0; j < globalFrameSize; ++j)
            {
                physicalMemory[frameNumber * globalFrameSize + j].diskIndex = pageIndex * globalFrameSize + j;
            }
        }
    }

    // print the page table at every pageTablePrintInt memory accesses
    memoryAccessCounter++;
    if (memoryAccessCounter % pageTablePrintInt == 0)
    {
        printPageTable();
    }
}

int get(unsigned int threadNum, unsigned int index)
{
    // increase the read counter
    statsOfProgram.reads++;

    int pageIndex = index / globalFrameSize; // calculate the page index
    int offset = index % globalFrameSize;    // calculate the offset in the page

    // current time in microseconds
    auto now = std::chrono::steady_clock::now();
    auto now_us = std::chrono::time_point_cast<std::chrono::microseconds>(now);
    long long time_in_us = now_us.time_since_epoch().count();

    if (pageTable[pageIndex].valid)
    {
        // page is in the physical memory
        int frameNumber = pageTable[pageIndex].frameNumber;

        // update the page table entry
        pageTable[pageIndex].referenced = 1;
        pageTable[pageIndex].lastAccessTime = time_in_us;

        // return the value in the physical memory
        return physicalMemory[frameNumber * globalFrameSize + offset].data;
    }
    else
    {
        // page is not in the physical memory. Page fault occurs
        int frameNumber = -1;

        // find an empty frame in the physical memory
        for (uint32_t i = 0; i < physical_memory_size / globalFrameSize; i++)
        {
            bool frameEmpty = true;
            for (uint32_t j = 0; j < globalFrameSize; j++)
            {
                if (physicalMemory[i * globalFrameSize + j].threadNum != -1)
                {
                    frameEmpty = false;
                    break;
                }
            }
            if (frameEmpty)
            {
                frameNumber = i;
                break;
            }
        }

        if (frameNumber == -1)
        {
            // no empty frame in the physical memory. Page replacement is needed
            if (pageReplacement == "LRU")
            {
                frameNumber = applyLRUReplacement(pageTable, physicalMemory, fd, globalFrameSize, virtual_page_number);
                // increase the number of page replacements
                statsOfProgram.pageReplacements++;
                // increase the number of page misses
                statsOfProgram.pageMisses++;
            }
            else if (pageReplacement == "CL")
            {
                frameNumber = applyClockReplacement(pageTable, physicalMemory, fd, globalFrameSize, virtual_page_number, clockHand);
                // increase the number of page replacements
                statsOfProgram.pageReplacements++;
                // increase the number of page misses
                statsOfProgram.pageMisses++;
            }
        }

        // read the page from the disk to the physical memory
        // increase the number of disk page reads
        statsOfProgram.diskPageReads++;
        lseek(fd, pageIndex * globalFrameSize * sizeof(int), SEEK_SET);
        read(fd, &physicalMemory[frameNumber * globalFrameSize], globalFrameSize * sizeof(int));

        // update the page table entry
        pageTable[pageIndex].frameNumber = frameNumber;
        pageTable[pageIndex].valid = 1;
        pageTable[pageIndex].referenced = 1;
        pageTable[pageIndex].lastAccessTime = time_in_us;

        // update the thread number and disk index
        for (uint32_t j = 0; j < globalFrameSize; j++)
        {
            physicalMemory[frameNumber * globalFrameSize + j].threadNum = threadNum;
            physicalMemory[frameNumber * globalFrameSize + j].diskIndex = pageIndex * globalFrameSize + j;
        }

        // print the page table at every pageTablePrintInt memory accesses
        memoryAccessCounter++;
        if (memoryAccessCounter % pageTablePrintInt == 0)
        {
            printPageTable();
        }

        // return the value in the physical memory
        return physicalMemory[frameNumber * globalFrameSize + offset].data;
    }
}

void merge(unsigned int threadNum, int left, int mid, int right)
{
    int n1 = mid - left + 1;
    int n2 = right - mid;

    // create temporary arrays
    std::vector<int> L(n1);
    std::vector<int> R(n2);

    // Copy data to temporary arrays L[] and R[]
    for (int i = 0; i < n1; i++)
        L[i] = get(threadNum, left + i);
    for (int j = 0; j < n2; j++)
        R[j] = get(threadNum, mid + 1 + j);

    // Merge the temporary arrays back into arr[left..right]
    int i = 0, j = 0;
    int k = left;

    while (i < n1 && j < n2)
    {
        if (L[i] <= R[j])
        {
            set(threadNum, k, L[i]);
            i++;
        }
        else
        {
            set(threadNum, k, R[j]);
            j++;
        }
        k++;
    }

    // Copy the remaining elements of L[], if there are any
    while (i < n1)
    {
        set(threadNum, k, L[i]);
        i++;
        k++;
    }

    while (j < n2)
    {
        set(threadNum, k, R[j]);
        j++;
        k++;
    }
}

void mergeSort(unsigned int threadNum, int left, int right)
{
    if (left < right)
    {
        int mid = left + (right - left) / 2;

        // sort the left half
        mergeSort(threadNum, left, mid);

        // sort the right half
        mergeSort(threadNum, mid + 1, right);

        // merge the sorted halves
        merge(threadNum, left, mid, right);
    }
}

void initializeDisk()
{
    // disk is a file. you can use file operations to initialize the disk.
    // open the file and write random integers to the file.

    fd = open(disk_file_name.c_str(), O_RDWR | O_CREAT, 0666);
    if (fd == -1)
    {
        cout << "Error: Cannot open disk file" << endl;
        exit(1);
    }

    // fill the -1 to the disk for the first time
    int randomInt = -1;
    for (int i = 0; i < disk_size; i++)
    {
        write(fd, &randomInt, sizeof(int));
    }
}

void printDisk()
{
    // print disk entries
    int randomInt;
    lseek(fd, 0, SEEK_SET);
    for (int i = 0; i < disk_size; i++)
    {
        read(fd, &randomInt, sizeof(int));

        printf("Disk Entry %3d: %10d\n", i, randomInt);
    }
}

int binarySearch(int threadNum, int l, int r, int x)
{
    while (l <= r)
    {
        int m = l + (r - l) / 2;

        int mid = get(threadNum, m); // get the value of the data at the given index in the virtual memory

        if (mid == x)
            return m;

        if (mid < x)
            l = m + 1;
        else
            r = m - 1;
    }

    return -1;
}
// physical memory.  threads share the same physical memory.

int main(int argc, char *argv[])
{

    if (argc != 7)
    {
        cout << "Usage: sortArrays frameSize numPhysical numVirtual pageReplacement pageTablePrintInt diskFileName.dat" << endl;
        return 1;
    }
    // command line arguments
    int frameSize = stoi(argv[1]);
    int numPhysical = stoi(argv[2]);
    int numVirtual = stoi(argv[3]);
    pageReplacement = argv[4];
    pageTablePrintInt = stoi(argv[5]);
    string diskFileName = argv[6];

    // check  max and argumants

    // numVirtual = (2^numVirtual)
    numVirtual = pow(2, numVirtual);
    frameSize = pow(2, frameSize);
    numPhysical = pow(2, numPhysical);

    // physical_memory_size = (2^frameSize)* (2^numPhysical)
    physical_memory_size = frameSize * numPhysical;
    // print physical memory size
    virtual_page_number = numVirtual;
    disk_size = numVirtual * frameSize * 2;
    disk_file_name = diskFileName;
    globalFrameSize = frameSize;
    statsOfProgram.physicalFramesInMemory = numPhysical;

    // init page table
    initializePageTable();
    // init physical memory
    initializePhysicalMemory();

    // print physical memory
    // printPhysicalMemory();

    // init disk
    initializeDisk();

    fillVirtualMemory(1);

    // print physical memory
    // printPhysicalMemory();

    // print disk
    // printDisk();

    // merge sort
    mergeSort(1, 0, (virtual_page_number * globalFrameSize) - 1);

    printf("-----------------------------------------\n");
    printf("After merge sort\n");
    // print: --------------------\n
    printf("-----------------------------------------\n");
    // print physical memory
    // printPhysicalMemory();

    // print disk
    // printDisk();

    // search 5 numbers (2 of them not in the array)
    int searchNumbers[] = {994, 966, 899, 110, 290}; // 110 ve 290 diskte kesin yok
    int searchResults[5];

    for (int i = 0; i < 5; i++)
    {
        searchResults[i] = binarySearch(1, 0, virtual_page_number * globalFrameSize - 1, searchNumbers[i]);
    }

    // print the found and not found numbers
    cout << "Search Results:" << endl;

    printf("┌───────────────┬───────────────┐\n");
    printf("│ Search Number │    Status     │\n");
    printf("├───────────────┼───────────────┤\n");

    for (int i = 0; i < 5; i++)
    {
        if (searchResults[i] == -1)
        {
            printf("│ %13d │ %-13s │\n", searchNumbers[i], "not found");
        }
        else
        {
            printf("│ %13d │ %-13s │\n", searchNumbers[i], "found");
        }
    }

    printf("└───────────────┴───────────────┘\n");

    // print the statistics
    printStatistics();

    return 0;
}