#include "cli.h"
#include <iostream>
#include <sstream>

CLI::CLI() : fs(nullptr), diskPath("vdisk.img") {}

CLI::~CLI() {
    if (fs) {
        fs->unmount();
        delete fs;
        fs = nullptr;
    }
}

void CLI::run() {

    std::cout << "Mini VFS CLI\n";
    std::cout << "Type 'help' for commands.\n";

    while (true) {

        std::cout << "vfs> ";

        std::string line;
        if (!std::getline(std::cin, line))
            break;

        std::stringstream ss(line);
        std::string command;
        ss >> command;

        if (command.empty())
            continue;

        try {

            if (command == "help") {

                std::cout << "Commands:\n";
                std::cout << "  mkfs\n";
                std::cout << "  mount\n";
                std::cout << "  create <filename>\n";
                std::cout << "  write <filename> <text>\n";
                std::cout << "  cat <filename>\n";
                std::cout << "  delete <filename>\n";
                std::cout << "  ls\n";
                std::cout << "  exit\n";

            } else if (command == "mkfs") {

                mkfs(diskPath);
                std::cout << "Disk formatted.\n";

            } else if (command == "mount") {

                if (fs) {
                    std::cout << "Already mounted.\n";
                    continue;
                }

                fs = mount(diskPath);
                std::cout << "Filesystem mounted.\n";

            } else if (command == "create") {

                if (!fs) { std::cout << "Not mounted.\n"; continue; }

                std::string filename;
                ss >> filename;

                if (filename.empty()) {
                    std::cout << "Filename required.\n";
                    continue;
                }

                if (fs->createFile(filename))
                    std::cout << "File created.\n";
                else
                    std::cout << "Create failed.\n";

            } else if (command == "write") {

                if (!fs) { std::cout << "Not mounted.\n"; continue; }

                std::string filename;
                ss >> filename;

                std::string text;
                std::getline(ss, text);

                if (!text.empty() && text[0] == ' ')
                    text.erase(0, 1);

                if (filename.empty() || text.empty()) {
                    std::cout << "Usage: write <filename> <text>\n";
                    continue;
                }

                int fd = fs->openFile(filename);
                if (fd < 0) {
                    std::cout << "File not found.\n";
                    continue;
                }

                int written = fs->writeFile(fd, text.c_str(), text.size());
                fs->closeFile(fd);

                if (written >= 0)
                    std::cout << "Wrote " << written << " bytes.\n";
                else
                    std::cout << "Write failed.\n";

            } else if (command == "cat") {

                if (!fs) { std::cout << "Not mounted.\n"; continue; }

                std::string filename;
                ss >> filename;

                if (filename.empty()) {
                    std::cout << "Filename required.\n";
                    continue;
                }

                int fd = fs->openFile(filename);
                if (fd < 0) {
                    std::cout << "File not found.\n";
                    continue;
                }

                const uint32_t BUFFER_SIZE = 512;
                char buffer[BUFFER_SIZE];

                while (true) {
                    int readBytes = fs->readFile(fd, buffer, BUFFER_SIZE);
                    if (readBytes <= 0)
                        break;
                    std::cout.write(buffer, readBytes);
                }

                std::cout << "\n";
                fs->closeFile(fd);

            } else if (command == "delete") {

                if (!fs) { std::cout << "Not mounted.\n"; continue; }

                std::string filename;
                ss >> filename;

                if (fs->deleteFile(filename))
                    std::cout << "Deleted.\n";
                else
                    std::cout << "Delete failed.\n";

            } else if (command == "exit") {

                std::cout << "Exiting...\n";
                break;

            } else if (command == "ls") {
                if (!fs) { std::cout << "Not Mounted.\n"; continue; }
                
                fs->listFiles();

            } else {

                std::cout << "Unknown command.\n";
            }

        } catch (const std::exception& e) {
            std::cout << "Error: " << e.what() << "\n";
        }
    }
}

