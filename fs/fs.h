#include <cstdint>
#include "../disk/disk.h"
#define MAX_NAME_LEN 52

// 0            -> Superblock
// 1-2          -> Inode bitmap
// 3-6          -> Data bitmap
// 7-2054       -> Inode table
// 2055-131071  -> Data block

struct Superblock {
    uint32_t magic;
    uint32_t block_size;
    uint32_t total_blocks;
    uint32_t total_inodes;

    uint32_t data_bitmap_start;
    uint32_t data_bitmap_count;

    uint32_t inode_bitmap_start;
    uint32_t inode_bitmap_count;

    uint32_t inode_table_start;
    uint32_t inode_table_count;

    uint32_t first_data_block;
};

// Size of one Inode is 128 bytes
struct Inode {
    uint16_t mode; // 0 for directory, 1 for file
    uint32_t size;
    uint64_t timestamps[3];
    uint32_t direct_blocks[12];
    uint32_t indirect_blocks[2];
    uint32_t ref_count;
    uint32_t pad[9];
};

// Size of one Directory Entry is 64 bytes
struct DirEntry {
    uint32_t inode;
    uint16_t name_len;
    char name[MAX_NAME_LEN];
    uint32_t pad;
};

class FileSystem {
private:

    struct OpenFile {
        uint32_t inode_index;
        uint32_t offset;
        bool in_use;
    };

    static const int MAX_OPEN_FILES = 256;
    OpenFile fd_table[MAX_OPEN_FILES];

public:
    bool isMounted;
    DiskManager disk;
    Superblock super_cache;

    explicit FileSystem(std::string diskImagePath) 
        : disk(diskImagePath), isMounted(false) {

        for (int i = 0; i < MAX_OPEN_FILES; ++i) {
            fd_table[i].in_use = false;
        }
    }

    Inode readInode(uint32_t inode_index);
    void writeInode(uint32_t inode_index, Inode inode);
    uint32_t allocateInode();
    uint32_t allocateDataBlock();
    void unmount();

    bool createFile(const std::string& fileName);

    int openFile(const std::string& fileName);
    bool closeFile(int fd);

    int readFile(int fd, char* buffer, uint32_t count);
    int writeFile(int fc, const char* buffer, uint32_t count);

    bool deleteFile(const std::string& fileName);

    void listFiles();
};

void mkfs(std::string diskImagePath);

FileSystem* mount(std::string diskImagePath);
