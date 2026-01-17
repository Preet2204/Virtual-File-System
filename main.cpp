// Virtual File System 
#include <iostream>
#include <string>
#include "fs/fs.cpp"

int main () {

    try {
        mkfs("vdisk.img");
        std::cout << "Successful" << '\n';
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
    }


}
