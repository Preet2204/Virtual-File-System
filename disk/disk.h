#include <string>
#include <cstdint>
#include <fstream>      // For File I/O Stream

class DiskManager {
private:
    const uint32_t blockSize = 4096;                    // Block Size in bytes
    uint32_t numBlocks;                                 // Number of Blocks in the Disk Image

    std::string diskImagePath;                          // Store Disk Image Path
    std::fstream disk;                                  // Store disk fstream
 
public:
    void readBlock(uint32_t blockNum, void* buffer);
    void writeBlock(uint32_t blockNum, void* buffer);
    void formatDisk();
    
    explicit DiskManager(std::string diskImagePath_);

};
