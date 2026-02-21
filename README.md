# Virtual File System

This project is on making a Unix-like Virtual File System

## Usage:
```bash
git clone https://github.com/Preet2204/Virtual-File-System.git
cd Virtual-File-System
```

- You would first need to create ``` vdisk.img ``` file of ```512MB```.

For Windows (Execute this command in cmd (Admin)):
```cmd
fsutil file createnew vdisk.img 536870912
```

For Linux Systems:
```bash
truncate -s 512M vdisk.img
```

- For Compilation:
```bash
g++ main.cpp cli/cli.cpp disk/disk.cpp fs/fs.cpp -o vfs.out
```

- For Execution:
```bash
./vfs.out
```

