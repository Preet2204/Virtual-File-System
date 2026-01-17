#include <iostream>
#include <string>       // For  
#include <fstream>      // For File I/O Stream
#include <stdexcept>    // For Exception Handling

class DiskManager {
private:
    const uint16_t blockSize = 4096;                    // Block Size in bytes
    uint32_t numBlocks;                                 // Number of Blocks in the Disk Image

    std::string diskImagePath;                          // Store Disk Image Path
    std::fstream disk;                                  // Store disk fstream
 
public:
    void readBlock(uint32_t blockNum, void* buffer);
    void writeBlock(uint32_t blockNum, void* buffer);
    void formatDisk();
    
    explicit DiskManager(std::string diskImagePath_);

};

DiskManager::DiskManager(std::string diskImagePath_) : diskImagePath(diskImagePath_) {

    // Open Disk Image as binary
    disk.open(diskImagePath, std::ios::binary | std::ios::in | std::ios::out);

    if(!disk.is_open()) {
        throw std::runtime_error(std::string("Disk Not Found at ") + diskImagePath);
    }

    // Find Disk Size
    disk.seekg(0, std::ios::end);
    std::streamoff diskSize = disk.tellg();
    
    if (diskSize < 0) {
        throw std::runtime_error(std::string("Failed to determine disk size ") + diskImagePath);
    }

    if (diskSize == 0) {
        throw std::runtime_error(std::string("Disk is Empty: ") + diskImagePath);
    }

    if (diskSize % blockSize != 0) {
        throw std::runtime_error(std::string("Disk is incompatible with Block Size(") + std::to_string(blockSize) + std::string(" bytes) and Disk ") + diskImagePath);
    } 

    numBlocks = static_cast<std::uint32_t>(diskSize) / blockSize;

}

DiskManager::readBlock(uint32_t blockNum, void* buffer) {

    if(buffer == nullptr) {
        throw std::invalid_argument(std::string("buffer is nullptr"));
    }

    if (blockNum >= numBlocks) {
        throw std::invalid_argument(std::string("blockNum is >= total number of block: ") + std::to_string(blockNum) + std::string(" > ") + std::to_string(numBlocks));
    }

    char* bufferChar = static_cast<char*>(buffer);

    disk.clear();

    disk.seekg(static_cast<std::uint64_t>(blockNum) * blockSize, std::ios::beg);

    if(!disk.good()) {
        throw std::runtime_error(std::string("disk.seekg() was failed."));
    }

    disk.read(bufferChar, blockSize);

    if(disk.gcount() < blockSize) {
        throw std::runtime_error(std::string("Only Partial block was read: ") + std::to_string(disk.gcount()) + std::string(" bytes"));
    }

}

DiskManager::writeBlock(uint32_t blockNum, void* buffer) {

    if (buffer == nullptr) {
        throw std::invalid_argument(std::string("buffer is nullptr"));
    }

    if (blockNum >= numBlocks) {
        throw std::invalid_argument(std::string("blockNum is >= total number of block: ") + std::to_string(blockNum) + std::string(" > ") + std::to_string(numBlocks));
    }

    char* bufferChar = static_cast<char*>(buffer);

    disk.clear();
    disk.seekp(static_cast<uint64_t>(blockNum) * blockSize, std::ios::beg);

    if(!disk.good()) {
        throw std::runtime_error(std::string("disk.seekp() was failed."));
    }

    disk.write(bufferChar, blockSize);

    if(!disk.good()) {
        throw std::runtime_error(std::string("disk.write() was failed."));
    }

    disk.flush();

}

DiskManager::formatDisk() {

    char* bufferChar = new char[blockSize];

    std::fill(bufferChar, bufferChar + blockSize, 0);
    void* buffer = static_cast<void*>(bufferChar);

    for(uint32_t i = 0; i < numBlocks; ++i) {
        writeBlock(i, buffer);
    }

    delete[] bufferChar;

}
