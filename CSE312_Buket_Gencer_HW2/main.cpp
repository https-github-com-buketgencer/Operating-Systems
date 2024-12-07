#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <vector>
#include <ctime>
#include <filesystem>
#include <map>
#include <algorithm>

using namespace std;
namespace fs = std::filesystem;

struct superBlock
{
    int blockSize;
    int totalBlocks;
    int rootDirPos;   // it keeps the block number where the root directory starts
    int freeBlockPos; // for writing sequentially to the file system ı keep free block number
    int fileCount;
    int dirCount;
    int rootDirSize; // root directory size
};

struct directoryEntry
{
    char fileName[32];
    int blockLocationOfEntry; // it keeps the block number where the file starts
    int fileSize;
    char date[12];
    char time[10];
    bool isDirectory; // flag for directory or file
};

struct Block
{
    int blockNumber;
    vector<directoryEntry> entries;
};

superBlock sb;                   // global super block. global because it is used in many functions and it is updated in many functions
vector<Block> blocks;            // it keeps all blocks and their entries
map<string, fs::path> filePaths; // it keeps file name and full path

// printf map content function for debugging
void printMap(map<string, fs::path> &m)
{
    for (auto it = m.begin(); it != m.end(); ++it)
    {
        cout << "Key: " << it->first << " Value: " << it->second << endl;
    }
}

void writeSuperBlockToFile(const string &fileName)
{
    ofstream fs(fileName, ios::binary | ios::in | ios::out);
    if (!fs)
    {
        cerr << "Error opening file to write super block!" << endl;
        return;
    }
    fs.seekp(0, ios::beg);                                             // I set the pointer to the beginning of the file.
    fs.write(reinterpret_cast<const char *>(&sb), sizeof(superBlock)); // ı write super block to file
    fs.close();
}

// it creates an empty file system with given block size and file name
void makeFileSystem(int blockSize, const string &fileName)
{
    int totalSize = 16 * 1024 * 1024;                 // 16 MB
    int totalBlocks = totalSize / (blockSize * 1024); // total block number

    ofstream fs(fileName, ios::binary | ios::trunc);
    if (!fs)
    {
        cerr << "Error creating file system file!" << endl;
        return;
    }

    char *buffer = new char[totalSize]; // I create a buffer with the size of the file system
    memset(buffer, 0, totalSize);       // I fill the buffer with 0
    fs.write(buffer, totalSize);        // I write the buffer to the file
    delete[] buffer;                    // I delete the buffer because I don't need it anymore

    // I fill the super block with the necessary information
    sb.blockSize = blockSize;
    sb.totalBlocks = totalBlocks;
    sb.rootDirPos = 1;
    sb.freeBlockPos = 2;
    sb.fileCount = 0;
    sb.dirCount = 1;
    sb.rootDirSize = blockSize;

    writeSuperBlockToFile(fileName); // I write the super block to the file after filling it with the necessary information
    cout << "File system created successfully with a size of " << totalSize << " bytes." << endl;
}

// it finds the next free block number
int findNextFreeBlock()
{
    return sb.freeBlockPos++;
}

// it gets the creation date and time of the test file
void getCreationDateAndTime(const string &path, char *date, char *time)
{
    struct stat attr;
    if (stat(path.c_str(), &attr) == 0)
    {
        std::tm *tm = std::localtime(&attr.st_mtime);
        strftime(date, 12, "%Y-%m-%d", tm);
        strftime(time, 10, "%H:%M:%S", tm);
    }
    else
    {
        cerr << "Error getting file attributes: " << strerror(errno) << endl;
    }
}

int calculateDirectorySize(const fs::path &directoryPath)
{
    int totalSize = 0;
    // for (const auto &entry : fs::directory_iterator(directoryPath))
    // {
    //     if (entry.is_directory())
    //     {
    //         totalSize += calculateDirectorySize(entry.path());
    //     }
    //     else
    //     {
    //         totalSize += static_cast<int>(fs::file_size(entry));
    //     }
    // }

    // all files and directories in the directory
    for (const auto &entry : fs::directory_iterator(directoryPath))
    {

        totalSize += sizeof(directoryEntry);
    }
    return totalSize;
}

// it creates directory blocks recursively
void createDirectoryBlocks(const fs::path &directoryPath, int &startBlock, const string &fileName)
{
    Block currentBlockData;                    // current block data to be written to the file
    currentBlockData.blockNumber = startBlock; // start block number. it is updated in the function
    int entrySize = sizeof(directoryEntry);    // each directory entry size

    vector<directoryEntry> directoryEntries; // All directory entries saved in this vector

    // root directory entry is created and added to the vector if the start block is equal to the root directory position
    if (startBlock == sb.rootDirPos)
    {
        directoryEntry rootEntry;
        memset(&rootEntry, 0, sizeof(rootEntry));
        strncpy(rootEntry.fileName, "/", sizeof(rootEntry.fileName) - 1);
        rootEntry.isDirectory = true;
        rootEntry.blockLocationOfEntry = startBlock;
        rootEntry.fileSize = calculateDirectorySize(directoryPath);
        getCreationDateAndTime(directoryPath.string(), rootEntry.date, rootEntry.time);

        directoryEntries.push_back(rootEntry);
    }

    // loop through all files and directories in the directory
    for (const auto &entry : fs::directory_iterator(directoryPath))
    {
        // in this code, we dont write entries to the file. we just keep them in the vector.
        //  we write them to the file in another function.
        directoryEntry dirEntry;
        memset(&dirEntry, 0, sizeof(dirEntry));
        strncpy(dirEntry.fileName, entry.path().filename().string().c_str(), sizeof(dirEntry.fileName) - 1);

        if (entry.is_directory())
        {
            dirEntry.isDirectory = true;
            dirEntry.blockLocationOfEntry = findNextFreeBlock();
            sb.dirCount++;
            createDirectoryBlocks(entry.path(), dirEntry.blockLocationOfEntry, fileName);
            dirEntry.fileSize = calculateDirectorySize(entry.path());
            getCreationDateAndTime(entry.path().string(), dirEntry.date, dirEntry.time);
        }
        else
        {
            dirEntry.isDirectory = false;
            dirEntry.fileSize = static_cast<int>(fs::file_size(entry));
            dirEntry.blockLocationOfEntry = -1;
            sb.fileCount++;
            getCreationDateAndTime(entry.path().string(), dirEntry.date, dirEntry.time);

            // file name and full path are kept in the map
            filePaths[dirEntry.fileName] = entry.path();
        }
        directoryEntries.push_back(dirEntry); // entry is added to the vector
    }

    // add the directory entries vector to the block data
    for (auto &dirEntry : directoryEntries)
    {
        currentBlockData.entries.push_back(dirEntry);
        if (currentBlockData.entries.size() * entrySize >= sb.blockSize * 1024)
        {
            blocks.push_back(currentBlockData);
            currentBlockData.blockNumber = findNextFreeBlock();
            currentBlockData.entries.clear();
        }
    }

    // if blocks are not empty, add the last block to the blocks vector.
    if (!currentBlockData.entries.empty())
    {
        blocks.push_back(currentBlockData);
    }

    writeSuperBlockToFile(fileName); // write super block to the file
}

// it writes file data to the file system
void writeFileData(const string &fileName, const fs::path &filePath, int &startBlock)
{
    ifstream infile(filePath, ios::binary);
    if (!infile)
    {
        cerr << "Error: Unable to open file " << filePath << " for reading!" << endl;
        return;
    }

    int fileSize = static_cast<int>(fs::file_size(filePath));
    char *buffer = new char[fileSize];
    infile.read(buffer, fileSize);

    ofstream fs(fileName, ios::binary | ios::in | ios::out);
    if (!fs)
    {
        cerr << "Error: Unable to open file system for writing!" << endl;
        delete[] buffer;
        return;
    }

    // calculate how many blocks are needed for the file
    int blocksNeeded = (fileSize / (sb.blockSize * 1024)) + 1;

    // loop through the blocks needed and write to the block each time
    for (int i = 0; i < blocksNeeded; ++i)
    {
        fs.seekp(startBlock * sb.blockSize * 1024, ios::beg); // startBlock * block_size * 1024.
        int bytesToWrite = min(fileSize - i * sb.blockSize * 1024, sb.blockSize * 1024);
        // cout << "Şuan yazdiğim block numarasi: " << startBlock << endl;
        fs.write(buffer + i * sb.blockSize * 1024, bytesToWrite);
        startBlock++;
        sb.freeBlockPos++;
    }

    delete[] buffer;
    fs.close();
    writeSuperBlockToFile(fileName); // write super block to the file

    // cout << "cikmadan önceki free blok numarasi: " << sb.freeBlockPos << endl;
}

void finalizeFileEntries(const string &fileName)
{
    for (auto &block : blocks)
    {
        for (auto &entry : block.entries)
        {
            if (!entry.isDirectory && entry.blockLocationOfEntry == -1)
            {
                // we save the free block number as a starting block number
                int entryStartBlock = sb.freeBlockPos;
                entry.blockLocationOfEntry = entryStartBlock;

                // write the file data to the file system
                writeFileData(fileName, filePaths[entry.fileName], entryStartBlock);

                // update the block entry with the new block number
                for (auto &blockEntry : block.entries)
                {
                    if (strcmp(blockEntry.fileName, entry.fileName) == 0)
                    {
                        blockEntry.blockLocationOfEntry = entry.blockLocationOfEntry;
                        break;
                    }
                }
            }
        }
    }

    // write all blocks and entries to the file
    int fd = open(fileName.c_str(), O_WRONLY);
    if (fd < 0)
    {
        cerr << "Error: Unable to open file for writing!" << endl;
        return;
    }

    // write all blocks and entries to the file
    for (const auto &block : blocks)
    {
        lseek(fd, block.blockNumber * sb.blockSize * 1024, SEEK_SET);
        write(fd, block.entries.data(), sizeof(directoryEntry) * block.entries.size());
    }

    close(fd);

    // we write super block to the file after filling it with the necessary information
    writeSuperBlockToFile(fileName);
}

// i don't use this function in the main function. it is just for debugging
// it prints all directory blocks not the file data blocks
void printDirectoryBlocks(const string &fileName)
{
    ifstream fs(fileName, ios::binary);
    if (!fs)
    {
        cerr << "Error: Unable to open file system for reading!" << endl;
        return;
    }

    fs.seekg(0, ios::beg);
    fs.read(reinterpret_cast<char *>(&sb), sizeof(superBlock));

    sort(blocks.begin(), blocks.end(), [](const Block &a, const Block &b)
         { return a.blockNumber < b.blockNumber; });

    for (const auto &block : blocks)
    {
        cout << "Block " << block.blockNumber << " (" << (block.blockNumber * sb.blockSize * 1024) << " - "
             << ((block.blockNumber + 1) * sb.blockSize * 1024 - 1) << " bytes):" << endl;

        int entryCount = 1;
        for (const auto &de : block.entries)
        {
            cout << "  Entry " << entryCount++ << ": " << endl;
            cout << "    File Name: " << de.fileName << endl;
            cout << "    Block Location of Entry: " << de.blockLocationOfEntry << endl;
            cout << "    File Size: " << de.fileSize << " bytes" << endl;
            cout << "    Date: " << de.date << endl;
            cout << "    Time: " << de.time << endl;
            cout << "    Is Directory: " << (de.isDirectory ? "Yes" : "No") << endl;
        }
    }

    fs.close();
}

// it prints the super block information
// i don't use this function in the main function. it is just for debugging
void printSuperBlockInfo()
{
    cout << "Super Block Information:" << endl;
    cout << "  Block Size: " << sb.blockSize << " KB" << endl;
    cout << "  Total Blocks: " << sb.totalBlocks << endl;
    cout << "  Root Directory Position: " << sb.rootDirPos << endl;
    cout << "  Next Free Block Position: " << sb.freeBlockPos << endl;
    cout << "  Total File Count: " << sb.fileCount << endl;
    cout << "  Total Directory Count: " << sb.dirCount - 1 << endl;
    cout << "  Root Directory Size: " << sb.rootDirSize << " KB" << endl;
}

//_______________________________________________________________________________________________________________________
// FILE SYSTEM OPERATIONS FUNCTIONS

// dumpe2fs used this function to print file system information
void printSuperBlockInformation(const string &fileName)
{
    ifstream fs(fileName, ios::binary);
    if (!fs)
    {
        cerr << "Error: Unable to open file system for reading!" << endl;
        return;
    }

    superBlock mySuperBlock;
    fs.seekg(0, ios::beg);
    fs.read(reinterpret_cast<char *>(&mySuperBlock), sizeof(superBlock));

    const int width = 40;
    // print
    cout << "***** File System Information *****" << endl;
    cout << " Block Size: " << mySuperBlock.blockSize << " KB " << endl;
    cout << " Total Blocks Count: " << mySuperBlock.totalBlocks << endl;
    // cout << "  Root Directory Position: " << mySuperBlock.rootDirPos << endl;
    // cout << "  Next Free Block Position: " << mySuperBlock.freeBlockPos << endl;
    cout << " Total File Count: " << mySuperBlock.fileCount << endl;
    cout << " Total Directory Count: " << mySuperBlock.dirCount - 1 << endl;
    // print how many blocks are used in file system
    cout << " Total Blocks Used: " << mySuperBlock.freeBlockPos - 1 << endl;
    cout << "***********************************" << endl;
    // cout << "  Root Directory Size: " << mySuperBlock.rootDirSize << " KB" << endl;

    // print all blocks and their entries
    /*for (const auto &block : blocks)
    {
        for (const auto &entry : block.entries)
        {
            if (!entry.isDirectory)
            {
                cout << "File: " << entry.fileName << endl;
                cout << "  Start Block: " << entry.blockLocationOfEntry << endl;
                cout << "  Size: " << entry.fileSize << " bytes" << endl;

                int blocksUsed = (entry.fileSize + sb.blockSize * 1024 - 1) / (sb.blockSize * 1024);
                cout << "  Blocks Used: " << blocksUsed << endl;

                cout << "  Data Blocks: ";
                for (int i = 0; i < blocksUsed; ++i)
                {
                    cout << entry.blockLocationOfEntry + i;
                    if (i < blocksUsed - 1)
                        cout << ", ";
                }
                cout << endl
                     << endl;
            }
        }
    }*/

    fs.close();
}

// it is used in dumpe2fs operation. read from mySystem.dat file. read blocks from file and print them in function
void readBlocksFromFile(const string &fileName)
{

    printSuperBlockInformation(fileName);
    ifstream fs(fileName, ios::binary);
    if (!fs)
    {
        cerr << "Error: Unable to open file system for reading!" << endl;
        return;
    }

    superBlock mySuperBlock;
    fs.seekg(0, ios::beg);
    fs.read(reinterpret_cast<char *>(&mySuperBlock), sizeof(superBlock));

    vector<Block> blocksFromFile; // it keeps all blocks and their entries from the file
    for (int i = 1; i <= mySuperBlock.dirCount; ++i)
    {
        Block block;
        block.blockNumber = i;
        block.entries.resize(mySuperBlock.blockSize * 1024 / sizeof(directoryEntry));
        fs.seekg(i * mySuperBlock.blockSize * 1024, ios::beg);
        fs.read(reinterpret_cast<char *>(block.entries.data()), mySuperBlock.blockSize * 1024);
        blocksFromFile.push_back(block);
    }

    fs.close();

    // print blocks from file
    for (const auto &block : blocksFromFile)
    {
        cout << " BLOCK " << block.blockNumber << ":" << endl;

        int entryCount = 1;
        for (const auto &de : block.entries)
        {                            // block.ede.fileName==0 ise boş bir entry olduğunu anlarız ve çıkarız
            if (de.fileName[0] == 0) // boş bir entry olduğunu anlarız ve çıkarız eğer boş bir entry varsa döngüden çıkarız
                break;

            /*cout << "  Entry " << entryCount++ << ": " << endl;
            cout << "    File Name: " << de.fileName << endl;
            cout << "    Block Location of Entry: " << de.blockLocationOfEntry << endl;
            cout << "    File Size: " << de.fileSize << " bytes" << endl;
            cout << "    Date: " << de.date << endl;
            cout << "    Time: " << de.time << endl;
            cout << "    Is Directory: " << (de.isDirectory ? "Yes" : "No") << endl;
            */
            cout << " Entry " << entryCount++ << ":";
            cout << " File Name: " << setw(25) << left << de.fileName << " ";
            cout << " Type: " << setw(10) << left << (de.isDirectory ? "Directory" : "File") << " ";
            cout << " Size (bytes): " << setw(10) << left << de.fileSize << " ";
            cout << " Creation Date: " << setw(10) << left << de.date << " ";
            cout << " Creation Time: " << setw(10) << left << de.time << " " << endl;
        }
    }
}

// it is used in dir operation. it reads blocks from file and prints the directory entries in the given path
void dir_command(const string &fileName, const string &path)
{
    ifstream fs(fileName, ios::binary);
    if (!fs)
    {
        cerr << "Error: Unable to open file system for reading!" << endl;
        return;
    }

    superBlock mySuperBlock;
    fs.seekg(0, ios::beg);
    fs.read(reinterpret_cast<char *>(&mySuperBlock), sizeof(superBlock));

    // Split the path into components
    vector<string> pathComponents; // it keeps path components
    size_t pos = 0, found;
    // find '/' and split the path
    while ((found = path.find_first_of('/', pos)) != string::npos)
    {
        if (found > pos)
        {
            pathComponents.push_back(path.substr(pos, found - pos));
        }
        pos = found + 1;
    }
    if (pos < path.length())
    {
        pathComponents.push_back(path.substr(pos));
    }

    // Start from the root directory
    int currentBlock = mySuperBlock.rootDirPos;
    bool directoryFound = false;

    for (const auto &component : pathComponents)
    {
        vector<Block> blocksFromFile;
        bool componentFound = false;

        // Load the current block
        Block block;
        block.blockNumber = currentBlock;
        block.entries.resize(mySuperBlock.blockSize * 1024 / sizeof(directoryEntry));
        fs.seekg(currentBlock * mySuperBlock.blockSize * 1024, ios::beg);
        fs.read(reinterpret_cast<char *>(block.entries.data()), mySuperBlock.blockSize * 1024);

        // Traverse the entries in the block to find the next component
        for (const auto &entry : block.entries)
        {
            if (entry.fileName[0] == '\0')
            {
                break; // End of valid entries. we exit from the loop
            }
            if (strcmp(entry.fileName, component.c_str()) == 0)
            {
                if (entry.isDirectory)
                {
                    currentBlock = entry.blockLocationOfEntry;
                    componentFound = true;
                    break;
                }
                else
                {
                    cerr << "Error: Path component " << component << " is not a directory." << endl;
                    fs.close();
                    return;
                }
            }
        }

        if (!componentFound)
        {
            cerr << "Error: Directory not found in path component " << component << endl;
            fs.close();
            return;
        }
    }

    // If we reach here, the final directory block has been found
    Block finalBlock;
    finalBlock.blockNumber = currentBlock;
    finalBlock.entries.resize(mySuperBlock.blockSize * 1024 / sizeof(directoryEntry));
    fs.seekg(currentBlock * mySuperBlock.blockSize * 1024, ios::beg);
    fs.read(reinterpret_cast<char *>(finalBlock.entries.data()), mySuperBlock.blockSize * 1024);

    cout << "Contents of Directory " << path << ":" << endl;
    for (const auto &entry : finalBlock.entries)
    {
        if (entry.fileName[0] == '\0')
        {
            break; // End of valid entries. we exit from the loop
        }
        cout << "  File Name: " << setw(20) << left << entry.fileName
             << " Type: " << setw(10) << left << (entry.isDirectory ? "Directory" : "File")
             << " Size (bytes): " << setw(10) << left << entry.fileSize
             << " Date: " << setw(12) << left << entry.date
             << " Time: " << setw(10) << left << entry.time
             << endl;
    }

    fs.close();
}

void read_command(const string &fileName, const string &filePath, const string &outputFileName)
{
    ifstream fs(fileName, ios::binary);
    if (!fs)
    {
        cerr << "Error: Unable to open file system for reading!" << endl;
        return;
    }

    superBlock mySuperBlock;
    fs.seekg(0, ios::beg);
    fs.read(reinterpret_cast<char *>(&mySuperBlock), sizeof(superBlock));

    // Split the file path into components
    vector<string> pathComponents; // it keeps path components
    size_t pos = 0, found;
    while ((found = filePath.find_first_of('/', pos)) != string::npos)
    {
        if (found > pos)
        {
            pathComponents.push_back(filePath.substr(pos, found - pos));
        }
        pos = found + 1;
    }
    if (pos < filePath.length())
    {
        pathComponents.push_back(filePath.substr(pos));
    }

    // Start from the root directory
    int currentBlock = mySuperBlock.rootDirPos;
    bool fileFound = false;
    int fileSize = 0;
    int fileStartBlock = 0;

    for (size_t i = 0; i < pathComponents.size(); ++i)
    {
        vector<Block> blocksFromFile;
        bool componentFound = false;

        // Load the current block
        Block block;
        block.blockNumber = currentBlock;
        block.entries.resize(mySuperBlock.blockSize * 1024 / sizeof(directoryEntry));
        fs.seekg(currentBlock * mySuperBlock.blockSize * 1024, ios::beg);
        fs.read(reinterpret_cast<char *>(block.entries.data()), mySuperBlock.blockSize * 1024);

        // Traverse the entries in the block to find the next component
        for (const auto &entry : block.entries)
        {
            if (entry.fileName[0] == '\0')
            {
                break; // End of valid entries
            }
            if (strcmp(entry.fileName, pathComponents[i].c_str()) == 0)
            {
                if (i == pathComponents.size() - 1)
                {
                    // If it's the last component, it should be a file
                    if (!entry.isDirectory)
                    {
                        fileFound = true;
                        fileSize = entry.fileSize;
                        fileStartBlock = entry.blockLocationOfEntry;
                    }
                    else
                    {
                        cerr << "Error: Path component " << pathComponents[i] << " is a directory, not a file." << endl;
                        fs.close();
                        return;
                    }
                }
                else
                {
                    // It's a directory, continue to the next level
                    currentBlock = entry.blockLocationOfEntry;
                    componentFound = true;
                    break;
                }
            }
        }

        if (!componentFound && !fileFound)
        {
            cerr << "Error: Path not found in component " << pathComponents[i] << endl;
            fs.close();
            return;
        }
    }

    if (!fileFound)
    {
        cerr << "Error: File not found at path " << filePath << endl;
        fs.close();
        return;
    }

    // Read the file's data from its starting block
    ofstream outFile(outputFileName, ios::binary | ios::trunc);
    if (!outFile)
    {
        cerr << "Error: Unable to open output file for writing!" << endl;
        fs.close();
        return;
    }

    int blocksToRead = (fileSize / (mySuperBlock.blockSize * 1024)) + 1;
    char *buffer = new char[mySuperBlock.blockSize * 1024];

    // Read the file data block by block
    for (int i = 0; i < blocksToRead; ++i)
    {
        fs.seekg((fileStartBlock + i) * mySuperBlock.blockSize * 1024, ios::beg);
        int bytesToRead = min(fileSize - i * mySuperBlock.blockSize * 1024, mySuperBlock.blockSize * 1024);
        fs.read(buffer, bytesToRead);
        outFile.write(buffer, bytesToRead); // write to the output file
    }

    delete[] buffer;
    outFile.close();
    fs.close();

    cout << "File " << filePath << " has been successfully read from the file system and written to " << outputFileName << "." << endl;
}

//_______________________________________________________________________________________________________________________
// MAIN FUNCTIONS

int make_file_system_program(int argc, char *argv[])
{
    if (argc != 4)
    {
        cerr << "Usage: " << argv[0] << " <blockSizeKB> <fileName> <dirPath>" << endl;
        return 1;
    }

    int blockSize = atoi(argv[1]);
    string fileName = argv[2];
    string dirPath = argv[3];

    makeFileSystem(blockSize, fileName);

    int startBlock = sb.rootDirPos;
    createDirectoryBlocks(dirPath, startBlock, fileName);

    finalizeFileEntries(fileName);

    int fd = open(fileName.c_str(), O_WRONLY);
    if (fd < 0)
    {
        cerr << "Error: Unable to open file for writing!" << endl;
        return 1;
    }

    // write all blocks and entries to the file after filling it with the necessary information
    for (const auto &block : blocks)
    {
        lseek(fd, block.blockNumber * sb.blockSize * 1024, SEEK_SET);
        write(fd, block.entries.data(), sizeof(directoryEntry) * block.entries.size());
    }

    close(fd);

    // if you want to print directory blocks after creating file system you can use this function
    // readBlocksFromFile(fileName);
    return 0;
}

int file_system_operations_program(int argc, char *argv[])
{
    if (argc < 3)
    {
        std::cerr << "Usage: " << argv[0] << " <fileName> <operation> [<path>] [<outputFileName>]" << std::endl;
        return 1;
    }

    const char *fileName = argv[1];
    const char *operation = argv[2];

    if (strcmp(operation, "dir") == 0)
    {
        if (argc != 4)
        {
            std::cerr << "Usage: " << argv[0] << " <fileName> dir <path>" << std::endl;
            return 1;
        }

        const char *path = argv[3];
        dir_command(fileName, path); // it prints the directory entries in the given path
    }
    else if (strcmp(operation, "dumpe2fs") == 0)
    {
        readBlocksFromFile(fileName); // it prints about file system
    }
    else if (strcmp(operation, "read") == 0)
    {
        if (argc != 5)
        {
            std::cerr << "Usage: " << argv[0] << " <fileName> read <path> <outputFileName>" << std::endl;
            return 1;
        }

        const char *filePath = argv[3];
        const char *outputFileName = argv[4];
        read_command(fileName, filePath, outputFileName); // it reads the file data from the file system and writes to the output file
    }
    else
    {
        std::cerr << "INVALID OPERATION: " << operation << std::endl;
        return 1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    string operation = argv[0];

    // there are 2 program: makeFileSystem | fileSystemOper
    if (operation == "./makeFileSystem")
    {
        return make_file_system_program(argc, argv);
    }
    else if (operation == "./fileSystemOper")
    {
        return file_system_operations_program(argc, argv);
    }
    else
    {
        cerr << "Error: Unknown program: " << operation << endl;
        return 1;
    }

    return 0;
}
