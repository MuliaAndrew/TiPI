#pragma once

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>
#include <iostream>
#include <errno.h>
#include <vector>

class Reader {
    int image_fd;
  
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
    std::vector<int32_t> enumerateBlocks(int32_t inode);
};

class Inode {
    int image_fd;

    int32_t inode;
    
    int16_t type;

    // file size in bytes
    int64_t size;
    
    int32_t direct_block[12];
    size_t ind;
    // ibp = inderect block pointer
    int32_t singly_ibp; 
    int32_t doubly_ibp;
    int32_t triply_ibp;

    size_t ind1;
    size_t ind2;
    size_t ind3;

    // helper index to iterate over dir entries
    size_t dire_i;

    void fill();

  public:
    Inode(const int image_fd_, const int32_t inode) : 
      image_fd(image_fd_), 
      inode(inode), 
      ind(0), ind1(0), ind2(0), ind3(0), 
      direct_block(),
      dire_i(0) {
        fill();
    }

    /// @brief print inode data to `stdout` no more then `BLCK_SIZE` bytes or until `EOF` is reached.
    /// @return number of writed bytes (`BLCK_SIZE` or `<BLCK_SIZE` if `EOF` is reached)
    int printData();

    /// @brief obtain next `dir entry` from inode. 
    /// @return std::string representing a `dir entry`. If no more dir entries left return an empty string 
    std::string nextDirEntry();
};