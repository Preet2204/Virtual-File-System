#include "fs.h"
#include <string>
#include <cstring>
#include <algorithm>
#include <iostream>

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

    // Set Data bitmap blocks to 1 and 0
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

    // Making Directory Entries for root Inode and writing them into Data block
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


FileSystem* mount(std::string diskImagePath) {

    FileSystem* file_system = new FileSystem(diskImagePath);

    char buffer[4096];
    file_system->disk.readBlock(SUPERBLOCK, buffer);
    std::memcpy(&file_system->super_cache, buffer, sizeof(Superblock));
 
    if (file_system->super_cache.magic != MAGIC) {
        delete file_system;
        throw std::invalid_argument(std::string("Invalid magic number of disk: ") + diskImagePath);
    }

    file_system->isMounted = true;
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
        throw std::runtime_error(std::string("Disk is not mounted yet. Invalid unmount call"));
    }

    isMounted = false;

}


bool FileSystem::createFile(const std::string& fileName) {

    if (!isMounted) {
        throw std::runtime_error("Disk is not mounted.");
    }

    if (fileName.empty() || fileName.length() > MAX_NAME_LEN) {
        return false;
    }

    // Root inode is inode 0
    uint32_t root_inode_index = 0;
    Inode root = readInode(root_inode_index);

    if (root.mode != 0) {
        throw std::runtime_error("Root inode is not a directory.");
    }

    char buffer[4096];

    // Step 1: Check for duplicate filenames
    uint32_t entries_per_block = super_cache.block_size / sizeof(DirEntry);

    for (int i = 0; i < 12; ++i) {

        if (root.direct_blocks[i] == 0)
            continue;

        disk.readBlock(root.direct_blocks[i], buffer);
        DirEntry* entries = reinterpret_cast<DirEntry*>(buffer);

        for (uint32_t j = 0; j < entries_per_block; ++j) {

            if (entries[j].inode != 0) {

                std::string existingName(entries[j].name, entries[j].name_len);

                if (existingName == fileName) {
                    return false;  // Duplicate found
                }
            }
        }
    }

    // Step 2: Allocate new inode
    uint32_t new_inode_index = allocateInode();

    Inode new_inode;
    std::memset(&new_inode, 0, sizeof(Inode));
    new_inode.mode = 1;  // file
    new_inode.size = 0;
    new_inode.ref_count = 1;

    writeInode(new_inode_index, new_inode);

    // Step 3: Add directory entry to root
    bool inserted = false;

    for (int i = 0; i < 12 && !inserted; ++i) {

        // Allocate new directory block if needed
        if (root.direct_blocks[i] == 0) {
            uint32_t new_block = allocateDataBlock();
            root.direct_blocks[i] = new_block;

            std::memset(buffer, 0, sizeof(buffer));
            disk.writeBlock(new_block, buffer);
        }

        disk.readBlock(root.direct_blocks[i], buffer);
        DirEntry* entries = reinterpret_cast<DirEntry*>(buffer);

        for (uint32_t j = 0; j < entries_per_block; ++j) {

            if (entries[j].inode == 0) {

                entries[j].inode = new_inode_index;
                entries[j].name_len = fileName.length();
                std::memset(entries[j].name, 0, MAX_NAME_LEN);
                std::memcpy(entries[j].name, fileName.c_str(), fileName.length());
                entries[j].pad = 0;

                disk.writeBlock(root.direct_blocks[i], buffer);

                root.size += sizeof(DirEntry);
                inserted = true;
                break;
            }
        }
    }

    if (!inserted) {
        throw std::runtime_error("Root directory is full.");
    }

    // Step 4: Write updated root inode
    writeInode(root_inode_index, root);

    return true;
}


int FileSystem::openFile(const std::string& fileName) {

    if (!isMounted) {
        throw std::runtime_error("Disk is not mounted.");
    }

    if (fileName.empty()) {
        return -1;
    }

    // Step 1: Search root directory (inode 0)
    uint32_t root_inode_index = 0;
    Inode root = readInode(root_inode_index);

    if (root.mode != 0) {
        throw std::runtime_error("Root inode is not a directory.");
    }

    char buffer[4096];
    uint32_t entries_per_block = super_cache.block_size / sizeof(DirEntry);

    int found_inode = -1;

    for (int i = 0; i < 12; ++i) {

        if (root.direct_blocks[i] == 0)
            continue;

        disk.readBlock(root.direct_blocks[i], buffer);
        DirEntry* entries = reinterpret_cast<DirEntry*>(buffer);

        for (uint32_t j = 0; j < entries_per_block; ++j) {

            if (entries[j].inode != 0) {

                std::string existingName(entries[j].name, entries[j].name_len);

                if (existingName == fileName) {
                    found_inode = entries[j].inode;
                    break;
                }
            }
        }

        if (found_inode != -1)
            break;
    }

    if (found_inode == -1) {
        return -1;  // File not found
    }

    // Step 2: Find free file descriptor slot
    for (int fd = 0; fd < MAX_OPEN_FILES; ++fd) {

        if (!fd_table[fd].in_use) {

            fd_table[fd].inode_index = found_inode;
            fd_table[fd].offset = 0;
            fd_table[fd].in_use = true;

            return fd;
        }
    }

    return -1;  // No free file descriptor
}


bool FileSystem::closeFile(int fd) {

    if (!isMounted) {
        throw std::runtime_error("Disk is not mounted.");
    }

    if (fd < 0 || fd >= MAX_OPEN_FILES) {
        return false;
    }

    if (!fd_table[fd].in_use) {
        return false;
    }

    fd_table[fd].in_use = false;
    fd_table[fd].offset = 0;
    fd_table[fd].inode_index = 0;

    return true;
}


int FileSystem::readFile(int fd, char* buffer, uint32_t count) {

    if (!isMounted) {
        throw std::runtime_error("Disk is not mounted.");
    }

    if (fd < 0 || fd >= MAX_OPEN_FILES || !fd_table[fd].in_use) {
        return -1;
    }

    uint32_t inode_index = fd_table[fd].inode_index;
    Inode inode = readInode(inode_index);

    if (inode.mode != 1) {
        return -1;  // Not a file
    }

    uint32_t offset = fd_table[fd].offset;

    if (offset >= inode.size) {
        return 0;  // EOF
    }

    uint32_t bytes_available = inode.size - offset;
    uint32_t bytes_to_read = std::min(count, bytes_available);

    uint32_t total_read = 0;
    char blockBuffer[4096];

    while (total_read < bytes_to_read) {

        uint32_t current_offset = offset + total_read;

        uint32_t block_index = current_offset / super_cache.block_size;
        uint32_t block_offset = current_offset % super_cache.block_size;

        if (block_index >= 12) {
            break;  // Only supporting direct blocks for now
        }

        uint32_t disk_block = inode.direct_blocks[block_index];

        if (disk_block == 0) {
            break;
        }

        disk.readBlock(disk_block, blockBuffer);

        uint32_t bytes_from_block =
            std::min(
                super_cache.block_size - block_offset,
                bytes_to_read - total_read
            );

        std::memcpy(
            buffer + total_read,
            blockBuffer + block_offset,
            bytes_from_block
        );

        total_read += bytes_from_block;
    }

    fd_table[fd].offset += total_read;

    return total_read;
}


int FileSystem::writeFile(int fd, const char* buffer, uint32_t count) {

    if (!isMounted) {
        throw std::runtime_error("Disk is not mounted.");
    }

    if (fd < 0 || fd >= MAX_OPEN_FILES || !fd_table[fd].in_use) {
        return -1;
    }

    uint32_t inode_index = fd_table[fd].inode_index;
    Inode inode = readInode(inode_index);

    if (inode.mode != 1) {
        return -1;  // Not a regular file
    }

    uint32_t offset = fd_table[fd].offset;
    uint32_t total_written = 0;

    char blockBuffer[4096];

    while (total_written < count) {

        uint32_t current_offset = offset + total_written;

        uint32_t block_index = current_offset / super_cache.block_size;
        uint32_t block_offset = current_offset % super_cache.block_size;

        if (block_index >= 12) {
            break;  // Only supporting direct blocks
        }

        // Allocate block if needed
        if (inode.direct_blocks[block_index] == 0) {

            uint32_t new_block = allocateDataBlock();
            inode.direct_blocks[block_index] = new_block;

            // Initialize block with zeros
            std::memset(blockBuffer, 0, super_cache.block_size);
            disk.writeBlock(new_block, blockBuffer);
        }

        uint32_t disk_block = inode.direct_blocks[block_index];

        disk.readBlock(disk_block, blockBuffer);

        uint32_t bytes_to_block =
            std::min(
                super_cache.block_size - block_offset,
                count - total_written
            );

        std::memcpy(
            blockBuffer + block_offset,
            buffer + total_written,
            bytes_to_block
        );

        disk.writeBlock(disk_block, blockBuffer);

        total_written += bytes_to_block;
    }

    fd_table[fd].offset += total_written;

    uint32_t new_end = offset + total_written;
    if (new_end > inode.size) {
        inode.size = new_end;
    }

    writeInode(inode_index, inode);

    return total_written;
}


bool FileSystem::deleteFile(const std::string& fileName) {

    if (!isMounted) {
        throw std::runtime_error("Disk is not mounted.");
    }

    if (fileName.empty()) {
        return false;
    }

    uint32_t root_inode_index = 0;
    Inode root = readInode(root_inode_index);

    char buffer[4096];
    uint32_t entries_per_block = super_cache.block_size / sizeof(DirEntry);

    int target_block = -1;
    int target_entry = -1;
    uint32_t target_inode = 0;

    // Step 1: Find directory entry
    for (int i = 0; i < 12; ++i) {

        if (root.direct_blocks[i] == 0)
            continue;

        disk.readBlock(root.direct_blocks[i], buffer);
        DirEntry* entries = reinterpret_cast<DirEntry*>(buffer);

        for (uint32_t j = 0; j < entries_per_block; ++j) {

            if (entries[j].inode != 0) {

                std::string existing(entries[j].name, entries[j].name_len);

                if (existing == fileName) {
                    target_block = root.direct_blocks[i];
                    target_entry = j;
                    target_inode = entries[j].inode;
                    break;
                }
            }
        }

        if (target_inode != 0)
            break;
    }

    if (target_inode == 0) {
        return false;  // File not found
    }

    // Step 2: Ensure file not open
    for (int i = 0; i < MAX_OPEN_FILES; ++i) {
        if (fd_table[i].in_use &&
            fd_table[i].inode_index == target_inode) {
            return false;
        }
    }

    Inode inode = readInode(target_inode);

    // Step 3: Free data blocks
    for (int i = 0; i < 12; ++i) {

        if (inode.direct_blocks[i] != 0) {

            uint32_t disk_block = inode.direct_blocks[i];

            uint32_t bitmap_block =
                super_cache.data_bitmap_start +
                (disk_block / (super_cache.block_size * 8));

            uint32_t bit_offset =
                disk_block % (super_cache.block_size * 8);

            char bitmap_buffer[4096];
            disk.readBlock(bitmap_block, bitmap_buffer);

            bitmap_buffer[bit_offset / 8] &=
                ~(1 << (bit_offset % 8));

            disk.writeBlock(bitmap_block, bitmap_buffer);

            inode.direct_blocks[i] = 0;
        }
    }

    // Step 4: Free inode bitmap
    uint32_t inode_bitmap_block =
        super_cache.inode_bitmap_start +
        (target_inode / (super_cache.block_size * 8));

    uint32_t inode_bit_offset =
        target_inode % (super_cache.block_size * 8);

    char inode_bitmap_buffer[4096];
    disk.readBlock(inode_bitmap_block, inode_bitmap_buffer);

    inode_bitmap_buffer[inode_bit_offset / 8] &=
        ~(1 << (inode_bit_offset % 8));

    disk.writeBlock(inode_bitmap_block, inode_bitmap_buffer);

    // Step 5: Remove directory entry
    disk.readBlock(target_block, buffer);
    DirEntry* entries = reinterpret_cast<DirEntry*>(buffer);

    entries[target_entry].inode = 0;
    entries[target_entry].name_len = 0;
    std::memset(entries[target_entry].name, 0, MAX_NAME_LEN);

    disk.writeBlock(target_block, buffer);

    // Step 6: Update root size
    root.size -= sizeof(DirEntry);
    writeInode(root_inode_index, root);

    return true;
}


void FileSystem::listFiles() {

    if (!isMounted) {
        throw std::runtime_error("Disk is not mounted.");
    }

    uint32_t root_inode_index = 0;
    Inode root = readInode(root_inode_index);

    if (root.mode != 0) {
        throw std::runtime_error("Root inode is not a directory.");
    }

    char buffer[4096];
    uint32_t entries_per_block = super_cache.block_size / sizeof(DirEntry);

    for (int i = 0; i < 12; ++i) {

        if (root.direct_blocks[i] == 0)
            continue;

        disk.readBlock(root.direct_blocks[i], buffer);
        DirEntry* entries = reinterpret_cast<DirEntry*>(buffer);

        for (uint32_t j = 0; j < entries_per_block; ++j) {

            if (entries[j].inode != 0) {

                std::string name(entries[j].name, entries[j].name_len);

                if (name == "." || name == "..")
                    continue;

                std::cout << name << "\n";
            }
        }
    }
}

