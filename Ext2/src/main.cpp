#include <string>
#include <iostream>
#include <cctype>
#include "reader.hpp"

const std::string USAGE_MSG = 
  "Usage : ./main arg1 arg2 \n"
  "arg1 : {inode : inode number of file} | {fpath : absolute path to the searched file}\n"
  "arg2 : {d : searched file is directory; dir_entry list will be printed} | {r : regular file; file will be printed to std::out}\n";

enum Cmd {
  REG_INODE = 0b001,
  REG_PATH  = 0b000,
  REG_BLCKS = 0b100,
  DIR_INODE = 0b011,
  DIR_PATH  = 0b010,
};

int parseInput(std::string &&input1, std::string &&input2) {
  int res = 0;
  if (input1.find_first_not_of("0123456789") == std::string::npos)
    res |= 0b001;
  
  if (input2 == "d")
    res |= 0b010;
  else if (input2 == "r") 
    ;
  else if (input2 == "e")
    res |= 0b100;
  else return 0b1000;
  return res;
}

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "Bad program arguments\n" << USAGE_MSG;
    return 1;
  }

  int mode = parseInput(std::string(argv[1]), std::string(argv[2]));

  if (mode == 0b1000) {
    std::cerr << "Bad program arguments\n" << USAGE_MSG;
    return 1;
  }

  int32_t inode = 0; 
  Reader reader("floppy.img");

  // DBG!
  return 0;

  switch (mode) {
    case Cmd::REG_PATH: {
      inode = reader.inodeFromPath(argv[1]);
    }
    case Cmd::REG_INODE: {
      if (!inode)
        inode = std::stoi(argv[1]);
      
      reader.printFile(inode);

      break;
    }
    case Cmd::REG_BLCKS: {
      inode = std::stoi(argv[1]);

      reader.enumerateBlocks(inode);

      break;
    }
    case Cmd::DIR_PATH: {
      inode = reader.inodeFromPath(argv[1]);
    }
    case Cmd::DIR_INODE: {
      if (!inode)
        inode = std::stoi(argv[1]);
      
      reader.printDir(inode);

      break;
    }
    default: {
      std::cerr << "LOL must not end like this...\n" << USAGE_MSG;
    }
  }

  return 0;
}