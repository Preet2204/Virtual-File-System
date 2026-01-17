// Virtual File System 
#include <iostream>
#include "fs/fs.h"
// #include "disk/disk.h"

int main () {

    DiskManager disk("vdisk.img");
    mkfs("vdisk.img");

}
