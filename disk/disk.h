#include <iostream>

class DiskManager {
    uint16_t blockSize = 4096;
    uint16_t numBlocks = 2048;



    void readBlock(uint32_t blockNum, void* buffer);
    void writeBlock(uint32_t blockNum, void* buffer);
    void formatDisk();
}

DiskManager::readBlock(uint32_t blockNum, void* buffer) {

}

DiskManager::writeBlock(uint32_t blockNum, void* buffer) {

}

DiskManager::formatDisk() {

}
