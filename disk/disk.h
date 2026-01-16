#include <iostream>
#include <string>

class DiskManager {
    const uint16_t blockSize;
    const uint16_t numBlocks;

    std::string diskImagePath;
    std::fstream disk;

    void readBlock(uint32_t blockNum, void* buffer);
    void writeBlock(uint32_t blockNum, void* buffer);
    void formatDisk();
}

DiskManager::DiskManager(std::string diskImagePath_) {
    
    diskImagePath = diskImagePath_;
    disk.open(diskImagePath, disk.binary | disk.in | disk.out);

    if(!disk.is_open()) {
	
    }

}


DiskManager::readBlock(uint32_t blockNum, void* buffer) {

}

DiskManager::writeBlock(uint32_t blockNum, void* buffer) {

}

DiskManager::formatDisk() {

}
