#pragma once

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>
#include <iostream>
#include <errno.h>
#include <vector>

typedef std::tuple<int32_t, std::string> DirEntry;

enum InodeType : int32_t {
  FIFO      = 0x1000,
  CHAR_DEV  = 0x2000,
  DIR       = 0x4000,
  BLOCK_DEV = 0x6000,
  REG_FILE  = 0x8000,
  SYM_LINK  = 0xA000, 
  SOCKET    = 0xC000
};

#define INODE_TYPE_MASK 0xF000

enum InodePermission : int32_t {
  OTHER_EX  = 0x001,
  OTHER_WR  = 0x002,
  OTHER_RD  = 0x004,
  GROUP_EX  = 0x008,
  GROUP_WR  = 0x010,
  GROUP_RD  = 0x020,
  USER_EX   = 0x040,
  USER_WR   = 0x080,
  USER_RD   = 0x100,
  STIKY_BIT = 0x200
};

#define INODE_PERMISSION_MASK 0x3FF

class Reader {
  public:
    int image_fd;
  
  private:
    void fill();
  
  public:
    Reader(std::string &&path_to_image) {
      image_fd = open(path_to_image.c_str(), O_RDONLY);
      if (errno)
        throw std::ios_base::failure("Can not open a filesystem image");
      
      fill();
    }
    ~Reader() {
      close(image_fd);
    }

    void printDir(int32_t inode);
    void printFile(int32_t inode);

    int32_t inodeFromPath(const char* path_to_file);
    void enumerateBlocks(int32_t inode);
};

class Inode {
    int image_fd;

    int32_t inode;
    
    int32_t type;

    // file size in bytes
    int64_t size;
    size_t ind;

    int32_t direct_block[12];
    // ibp = inderect block pointer
    int32_t singly_ibp; 
    int32_t doubly_ibp;
    int32_t triply_ibp;

    // helper index to iterate over dir entries
    size_t dire_i;

    void fill();

  public:
    Inode(const int image_fd_, const int32_t inode) : 
      image_fd(image_fd_), 
      inode(inode), 
      ind(0),
      direct_block(),
      dire_i(0) {
        fill();
    }

    // return address of next block of file data. if no more blocks left return 0
    int32_t nextBlock() const;

    /// @brief print inode data to `stdout` no more then `BLCK_SIZE` bytes or until `EOF` is reached.
    /// @return number of writed bytes (`BLCK_SIZE` or `<BLCK_SIZE` if `EOF` is reached)
    int printData();

    /// @brief obtain next `dir entry` from inode. 
    /// @return std::string representing a `dir entry`. If no more dir entries left return default DirEntry
    ///
    DirEntry nextDirEntry();

    ///| inode | this_entry_size | name length low 8 bits | type ind |   name    |
    /// 0       4                 6                        7          8   this_entry_size
    
    int printDirEntry();

    void inc() {
      ind++;
    };
    void dec() {
      ind--;
    };
    size_t cur() const {
      return ind;
    }

    InodeType t() const {
      return static_cast<InodeType>(INODE_TYPE_MASK & type);
    }

    InodePermission perm() const {
      return static_cast<InodePermission>(INODE_PERMISSION_MASK & type);
    }

    void printMeta();
};