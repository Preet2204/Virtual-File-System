#include <cstdint>
#include "../disk/disk.h"
#define MAX_NAME_LEN 52

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

struct Inode {
    uint16_t mode; // 0 for directory, 1 for file
    uint32_t size;
    uint64_t timestamps[3];
    uint32_t direct_blocks[12];
    uint32_t indirect_blocks[2];
    uint32_t ref_count;
    uint32_t pad[9];
};

struct DirEntry {
    uint32_t inode;
    uint16_t name_len;
    char name[MAX_NAME_LEN];
    uint32_t pad;
};

class FileSystem {
private:

public:
    bool isMounted;
    DiskManager disk;
    Superblock super_cache;
    
    explicit FileSystem(std::string diskImagePath) : disk(diskImagePath), isMounted(false) {}

    Inode readInode(uint32_t inode_index);
    void writeInode(uint32_t inode_index, Inode inode);
    uint32_t allocateInode();
    uint32_t allocateDataBlock();
    void unmount();

};

void mkfs(std::string diskImagePath);

FileSystem mount(std::string diskImagePath);
