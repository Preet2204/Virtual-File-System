#ifndef CLI_H
#define CLI_H

#include "../fs/fs.h"

class CLI {
private:
    FileSystem* fs;
    std::string diskPath;

public:
    CLI();
    ~CLI();

    void run();
};

#endif
