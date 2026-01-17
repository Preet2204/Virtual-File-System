#include "fs.h"
#include <string>
#include <cstring>
#include <algorithm>

#define BLOCK_SIZE 4096
#define TOTAL_BLOCKS 131072
#define MAGIC 0x12345678
#define TOTAL_INODES 65536

#define SUPERBLOCK 0
#define INODE_BITMAP_START 1
#define INODE_BITMAP_END 2
#define DATA_BITMAP_START 3
#define DATA_BITMAP_END 6
#define INODE_DATA_START 7
#define INODE_DATA_END 2054
#define FIRST_DATA_BLOCK 2055

#define DISKPATH "vdisk.img"

void mkfs(std::string diskImagePath) {

    DiskManager disk(diskImagePath);
    disk.formatDisk();

    // Making Superblock and writing it onto disk on first block
    Superblock super;
    super.magic = MAGIC;
    super.block_size = BLOCK_SIZE;
    super.total_blocks = TOTAL_BLOCKS;
    super.total_inodes = TOTAL_INODES;
    super.data_bitmap_start = DATA_BITMAP_START;
    super.data_bitmap_count = DATA_BITMAP_END - DATA_BITMAP_START + 1;
    super.inode_bitmap_start = INODE_BITMAP_START;
    super.inode_bitmap_count = INODE_BITMAP_END - INODE_BITMAP_START + 1;
    super.inode_table_start = INODE_DATA_START;
    super.inode_table_count = INODE_DATA_END - INODE_DATA_START + 1;
    super.first_data_block = FIRST_DATA_BLOCK;

    char buffer[4096];

    std::memset(buffer, 0, sizeof(buffer));
    std::memcpy(buffer, &super, sizeof(Superblock));
    disk.writeBlock(SUPERBLOCK, buffer);
    // Set Inode bitmap blocks to 0
    std::memset(buffer, 0, sizeof(buffer));
    for (int i = INODE_BITMAP_START; i <= INODE_BITMAP_END; ++i) {
        disk.writeBlock(i, buffer);
    }

    std::memset(buffer, 0, sizeof(buffer));

    for(uint32_t i = 0; i <= FIRST_DATA_BLOCK; ++i) {
        buffer[i / 8] |= (1 << (i % 8));
    }

    disk.writeBlock(DATA_BITMAP_START, buffer);
    std::memset(buffer, 0, sizeof(buffer));
    for (uint32_t i = DATA_BITMAP_START + 1; i <= DATA_BITMAP_END; ++i) {
        disk.writeBlock(i, buffer);
    }

    // Set inode bitmap first bit to 1 for root Inode
    std::memset(buffer, 0, sizeof(buffer));
    buffer[0] |= 1;
    disk.writeBlock(INODE_BITMAP_START, buffer);

    // Making rootInode and writing it in first Inode Data block
    Inode rootInode;
    std::memset(&rootInode, 0, sizeof(Inode));
    rootInode.mode = 0; // 0 for Directory, 1 for File
    rootInode.size = 2 * sizeof(DirEntry);
    rootInode.direct_blocks[0] = FIRST_DATA_BLOCK;
    rootInode.ref_count = 2;

    std::memset(buffer, 0, sizeof(buffer));
    std::memcpy(buffer, &rootInode, sizeof(rootInode));
    disk.writeBlock(INODE_DATA_START, buffer);

    // Making Directory Entries and writing them into Data block
    std::memset(buffer, 0, sizeof(buffer));
    DirEntry* dirEntries = reinterpret_cast<DirEntry*>(buffer);

    dirEntries[0].inode = 0;
    dirEntries[0].name_len = 1;
    std::memcpy(dirEntries[0].name, ".", 1);
    dirEntries[0].pad = 0;

    dirEntries[1].inode = 0;
    dirEntries[1].name_len = 2;
    std::memcpy(dirEntries[1].name, "..", 2);
    dirEntries[1].pad = 0;

    disk.writeBlock(FIRST_DATA_BLOCK, buffer);

}


FileSystem mount(std::string diskImagePath) {

    FileSystem file_system(diskImagePath);

    char buffer[4096];
    file_system.disk.readBlock(SUPERBLOCK, buffer);
    std::memcpy(&file_system.super_cache, buffer, sizeof(Superblock));
 
    if (file_system.super_cache.magic != MAGIC) {
        throw std::invalid_argument(std::string("Invalid magic number of disk: ") + diskImagePath);
    }

    file_system.isMounted = true;
    return file_system;

}


Inode FileSystem::readInode(uint32_t inode_index) {

    if (!isMounted) {
        throw std::runtime_error(std::string("disk is not mounted yet. Invalid readInode call"));
    }

    if (inode_index >= super_cache.total_inodes) {
        throw std::invalid_argument(std::string("Invalid inode index: ") + std::to_string(inode_index));
    }

    uint32_t inode_bitmap_block = super_cache.inode_bitmap_start + (inode_index  / (super_cache.block_size * 8));
    uint32_t inode_bitmap_bit = inode_index % (super_cache.block_size * 8);

    char buffer[4096];

    disk.readBlock(inode_bitmap_block, buffer);

    if((buffer[inode_bitmap_bit / 8] & (1 << (inode_bitmap_bit % 8))) == 0) {
        throw std::runtime_error(std::string("Invalid Inode index: Inode is unallocated - ") + std::to_string(inode_index));
    }

    uint32_t inode_per_block = super_cache.block_size / sizeof(Inode);
    uint32_t inode_block = super_cache.inode_table_start + (inode_index / inode_per_block);
    uint32_t inode_offset = sizeof(Inode) * (inode_index % inode_per_block);

    disk.readBlock(inode_block, buffer);
    Inode inode;
    std::memcpy(&inode, buffer + inode_offset, sizeof(Inode));

    return inode;
}


void FileSystem::writeInode(uint32_t inode_index, Inode inode) {

    if (!isMounted) {
        throw std::runtime_error(std::string("disk is not mounted yet. Invalid writeInode call"));
    }

    if (inode_index >= super_cache.total_inodes) {
        throw std::invalid_argument(std::string("Invalid inode index: ") + std::to_string(inode_index));
    }

    uint32_t inode_bitmap_block = super_cache.inode_bitmap_start + (inode_index  / (super_cache.block_size * 8));
    uint32_t inode_bitmap_bit = inode_index % (super_cache.block_size * 8);

    char buffer[4096];

    disk.readBlock(inode_bitmap_block, buffer);

    if((buffer[inode_bitmap_bit / 8] & (1 << (inode_bitmap_bit % 8))) == 0) {
        throw std::runtime_error(std::string("Invalid Inode index: Inode is unallocated - ") + std::to_string(inode_index));
    }

    uint32_t inode_per_block = super_cache.block_size / sizeof(Inode);
    uint32_t inode_block = super_cache.inode_table_start + (inode_index / inode_per_block);
    uint32_t inode_offset = sizeof(Inode) * (inode_index % inode_per_block);

    disk.readBlock(inode_block, buffer);

    std::memcpy(buffer + inode_offset, &inode, sizeof(Inode));

    disk.writeBlock(inode_block, buffer);

}

uint32_t FileSystem::allocateInode() {

    if (!isMounted) {
        throw std::runtime_error(std::string("disk is not mounted yet. Invalid allocateInode call"));
    }

    char buffer[4096];
    uint32_t free_block = 0;
    uint32_t free_offset = 0;
    bool found = false;
    for(uint32_t block = super_cache.inode_bitmap_start; block <= super_cache.inode_bitmap_start + super_cache.inode_bitmap_count - 1; ++block) {
        disk.readBlock(block, buffer);

        for(uint32_t offset = 0; offset < 8 * super_cache.block_size; ++offset) {
            if ((buffer[offset / 8] & (1 << (offset % 8))) == 0) {
                uint32_t inode_index = (block - super_cache.inode_bitmap_start) * (8 * super_cache.block_size) + offset;
                if (inode_index >= super_cache.total_inodes) {
                    continue;
                }
                found = true;
                free_block = block;
                free_offset = offset;
                break;
            }
        }

        if (found) break;
    }

    if (!found) {
        throw std::runtime_error(std::string("No Free Inode in disk"));
    }

    uint32_t inode_index = (free_block - super_cache.inode_bitmap_start) * (8 * super_cache.block_size) + free_offset;

    buffer[free_offset / 8] |= (1 << (free_offset % 8));
    disk.writeBlock(free_block, buffer);

    Inode inode;
    std::memset(&inode, 0, sizeof(Inode));
    
    inode.ref_count = 1;

    writeInode(inode_index, inode);

    return inode_index;
}

uint32_t FileSystem::allocateDataBlock() {

    if (!isMounted) {
        throw std::runtime_error(std::string("disk is not mounted yet. Invalid allocateDataBlock call"));
    }

    char buffer[4096];

    uint32_t free_block = 0;
    uint32_t free_offset = 0;
    bool found = false;

    for(uint32_t block = super_cache.data_bitmap_start; block <= super_cache.data_bitmap_start + super_cache.data_bitmap_count - 1; ++block) {
        disk.readBlock(block, buffer);
        for(uint32_t offset = 0; offset < 8 * super_cache.block_size; ++offset) {
            if((buffer[offset / 8] & (1 << (offset % 8))) == 0) {
                uint32_t disk_block = (block - super_cache.data_bitmap_start) * (8 * super_cache.block_size) + offset;
                if(disk_block >= super_cache.total_blocks) {
                    continue;
                }
                free_block = block;
                free_offset = offset;
                found = true;
                break;
            }
        }
        if(found) break;
    }

    if(!found) {
        throw std::runtime_error(std::string("No Free Data Block in disk"));
    }

    uint32_t disk_block = (free_block - super_cache.data_bitmap_start) * (8 * super_cache.block_size) + free_offset;

    buffer[free_offset / 8] |= (1 << (free_offset % 8));
    disk.writeBlock(free_block, buffer);

    return disk_block;

}

void FileSystem::unmount() {

    if (!isMounted) {
        throw std::runtime_error(std::string("disk is not mounted yet. Invalid unmount call"));
    }

    isMount = false;

}
