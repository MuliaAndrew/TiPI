#include "reader.hpp"

#include <string.h>
#include <assert.h>

#include <unordered_map>

int32_t BLCK_SIZE;
int32_t INODE_SIZE;
int32_t INODE_PER_GROUP;
int32_t NUM_OF_BLCK_GROUPS;
bool    DIR_ENTRY_TYPE;

const int32_t ROOT_INODE = 2;

const std::unordered_map<InodeType, std::string> TYPE_MSG_MAP = {
  {InodeType::BLOCK_DEV, "b"},
  {InodeType::CHAR_DEV,  "c"},
  {InodeType::DIR,       "d"},
  {InodeType::REG_FILE,  "-"},
  {InodeType::FIFO,      "f"},
  {InodeType::SOCKET,    "s"},
  {InodeType::SYM_LINK,  "l"}
};

std::string permissionString(int32_t ty) {
  std::string res = "----------";
  
  if (ty & InodePermission::USER_EX)
    res[0] = 'x';
  
  if (ty & InodePermission::USER_WR)
    res[1] = 'w';
  
  if (ty & InodePermission::USER_RD)
    res[2] = 'r';
  
  if (ty & InodePermission::GROUP_EX)
    res[3] = 'x';
  
  if (ty & InodePermission::GROUP_WR)
    res[4] = 'w';
  
  if (ty & InodePermission::GROUP_RD)
    res[5] = 'r';

  if (ty & InodePermission::OTHER_EX)
    res[6] = 'x';
  
  if (ty & InodePermission::OTHER_WR)
    res[7] = 'w';
  
  if (ty & InodePermission::OTHER_RD)
    res[8] = 'r';
  
  if (ty & InodePermission::STIKY_BIT)
    res[9] = 's';
  
  return res;
}

void Reader::fill() {
  char buff[1024] = {0};

  if (pread(image_fd, buff, 1024, 1024) != 1024 || errno) {
    auto msg = strerror(errno);
    throw std::ios_base::failure("Something went wrong with Reader::fill::pread() : " + std::string(msg));
  }

  BLCK_SIZE = 1024 << *((int32_t*)(buff + 24));
  INODE_PER_GROUP = *((int32_t*)(buff + 40));
  int32_t BLCK_PER_GROUP = *((int32_t*)(buff + 32));
  int32_t TOTAL_BLCKS = *((int32_t*)(buff + 4));
  
  int32_t MAJOR = *((int32_t*)(buff + 76));
  if (MAJOR >= 1)
    INODE_SIZE = (int32_t)(*((int16_t*)(buff + 88)));
  else
    INODE_SIZE = 128;
  
  int32_t INODE_TOTAL = *((int32_t*)(buff + 0));

  DIR_ENTRY_TYPE = 0x002 & *((int32_t*)(buff + 96));

  NUM_OF_BLCK_GROUPS = INODE_TOTAL / INODE_PER_GROUP + ((INODE_TOTAL % INODE_PER_GROUP) ? 1 : 0);

  // DBG!
  // std::cout << "BLCK_SIZE : " << BLCK_SIZE << "\n"
  //   << "INODE_SIZE : " << INODE_SIZE << "\n"
  //   << "INODE_PER_GROUP : " << INODE_PER_GROUP << "\n"
  //   << "INODE_TOTAL : " << INODE_TOTAL << "\n"
  //   << "NUM_OF_BLCK_GROUPS : " << NUM_OF_BLCK_GROUPS << "\n";
}

void Inode::fill() {
  int32_t blck_grp = (inode - 1) / INODE_PER_GROUP;
  int32_t inode_ind = (inode - 1) % INODE_PER_GROUP;

  // Firstly find and read neaded block group

  // BGDT -- Block group descriptor table
  #define BGDT_SIZE (NUM_OF_BLCK_GROUPS * 32)
  auto BGDT_buff = new char[BGDT_SIZE];

  if (pread(image_fd, BGDT_buff, BGDT_SIZE, 2048) != BGDT_SIZE || errno) {
    auto msg = strerror(errno);
    throw std::ios_base::failure("Something went wrong with first Inode::fill::pread() : " + std::string(msg));
  }

  int32_t inode_table_blck = *((int32_t*)(BGDT_buff + 32 * blck_grp + 8));
  auto inode_buff = new char[INODE_SIZE];

  delete[] BGDT_buff;

  // extracting inode table from block group
  // and finally getting inode
  if (pread(image_fd, inode_buff, INODE_SIZE, inode_table_blck * BLCK_SIZE + inode_ind * INODE_SIZE) != INODE_SIZE || errno) {
    auto msg = strerror(errno);
    throw std::ios_base::failure("Something went wrong with second Inode::fill::pread() : " + std::string(msg));
  }

  type = *((int16_t*)(inode_buff + 0));

  int32_t size_lower = *((int32_t*)(inode_buff + 4));
  int32_t size_higher = *((int32_t*)(inode_buff + 108));

  size = 0;
  size |= size_higher;
  size << 32;
  size |= size_lower;

  memcpy(direct_block, inode_buff + 40, 48);

  singly_ibp = *((int32_t*)(inode_buff + 88));
  doubly_ibp = *((int32_t*)(inode_buff + 92));
  triply_ibp = *((int32_t*)(inode_buff + 96));

  delete[] inode_buff;

  // DBG!
  // std::cout  << "inode : " << inode << "\n"
  //   << "type : " << std::hex << type << std::dec << "\n"
  //   << "size : " << size << "\n"
  //   << "direct_block[0] : " << direct_block[0] << "\n"
  //   << "direct_block[1] : " << direct_block[1] << "\n"
  //   << "direct_block[2] : " << direct_block[2] << "\n"
  //   << "direct_block[3] : " << direct_block[3] << "\n"
  //   << "direct_block[4] : " << direct_block[4] << "\n"
  //   << "direct_block[5] : " << direct_block[5] << "\n"
  //   << "direct_block[6] : " << direct_block[6] << "\n"
  //   << "direct_block[7] : " << direct_block[7] << "\n"
  //   << "direct_block[8] : " << direct_block[8] << "\n"
  //   << "direct_block[9] : " << direct_block[9] << "\n"
  //   << "direct_block[10] : " << direct_block[10] << "\n"
  //   << "direct_block[11] : " << direct_block[11] << "\n"
  //   << "singly_ibp : " << singly_ibp << "\n"
  //   << "doubly_ibp : " << doubly_ibp << "\n"
  //   << "triply_ibp : " << triply_ibp << "\n";
}

int32_t Inode::nextBlock() const {
  if (ind * BLCK_SIZE > size)
    return 0;

  if (ind < 12)
    return direct_block[ind];

  if (ind < 12 + BLCK_SIZE / 4) {
    auto layer1 = new int32_t[BLCK_SIZE / 4];

    if (pread(image_fd, layer1, BLCK_SIZE, singly_ibp * BLCK_SIZE) != BLCK_SIZE || errno) {
      auto msg = strerror(errno);
      throw std::ios_base::failure("Something went wrong with indirect1 Inode::nextBlock::pread() : " + std::string(msg));
    }

    auto res = layer1[ind - 12];
    delete[] layer1;
    return res;
  }

  if (ind < 12 + (BLCK_SIZE / 4 + 1) * BLCK_SIZE / 4) {
    auto layer1 = new int32_t[BLCK_SIZE / 4];

    if (pread(image_fd, layer1, BLCK_SIZE, doubly_ibp * BLCK_SIZE) != BLCK_SIZE || errno) {
      auto msg = strerror(errno);
      throw std::ios_base::failure("Something went wrong with indirect2.1 Inode::nextBlock::pread() : " + std::string(msg));
    }

    auto layer2 = new int32_t[BLCK_SIZE / 4];

    auto ind1 = (ind - 12 - BGDT_SIZE / 4) / (BLCK_SIZE / 4);
    auto ind2 = (ind - 12 - BGDT_SIZE / 4) % (BLCK_SIZE / 4);

    if (pread(image_fd, layer2, BLCK_SIZE, layer1[ind1] * BLCK_SIZE) != BLCK_SIZE || errno) {
      auto msg = strerror(errno);
      throw std::ios_base::failure("Something went wrong with indirect2.2 Inode::nextBlock::pread() : " + std::string(msg));
    }

    delete[] layer1;
    auto res = layer2[ind2];
    delete[] layer2;
    return res;
  }

  // TODO! 
  // triply indirect block obtainer algorithm

  return 0;
}

int Inode::printData() {
  auto blck_ind = nextBlock();

  if (!blck_ind || ind * BLCK_SIZE > size)
    return 0;

  auto N_TO_READ = BLCK_SIZE;
  if (size - ind * BLCK_SIZE < BLCK_SIZE)
    N_TO_READ = size - ind * BLCK_SIZE;
  
  auto buff = new char[BLCK_SIZE];

  auto readed = pread(image_fd, buff, N_TO_READ, blck_ind * BLCK_SIZE);
  if (errno) {
    auto msg = strerror(errno);
    throw std::ios_base::failure("Something went wrong with direct Inode::printData::pread() : " + std::string(msg));
  }
  if (write(STDOUT_FILENO, buff, readed) != readed || errno) {
    auto msg = strerror(errno);
    throw std::ios_base::failure("Something went wrong with direct Inode::printData::write() : " + std::string(msg));
  }
  delete[] buff;
  ind++;
  return readed;
}

void Reader::printFile(int32_t inode_num) {
  Inode inode(image_fd, inode_num);
  while (inode.printData() == BLCK_SIZE);
}

void Reader::enumerateBlocks(int32_t inode_num) {
  Inode inode(image_fd, inode_num);

  std::cout << "Inode number : " << inode_num << "\n";

  for (auto blck_addr = inode.nextBlock(); blck_addr != 0; blck_addr = inode.nextBlock()) {
    inode.inc();
    std::cout << "  BLCK[ " << inode.cur() << " ] : " << blck_addr << "\n";
  }
  
  std::cout << "Total accupated blocks : " << inode.cur() << "\n";
}

std::vector<std::string> split(std::string& s, const std::string& delimiter) {
    std::vector<std::string> tokens;
    size_t pos = 0;
    std::string token;
    while ((pos = s.find(delimiter)) != std::string::npos) {
        token = s.substr(0, pos);
        tokens.push_back(token);
        s.erase(0, pos + delimiter.length());
    }
    tokens.push_back(s);

    return tokens;
}

DirEntry Inode::nextDirEntry() {
  int32_t dir_entry_inode;
  int16_t total_size;
  int8_t  len_low;
  int8_t  type_id, len_top; // type_id is used if `DIR_ENTRY_TYPE` is `true`
  char buff[sizeof(dir_entry_inode) + sizeof(total_size) + sizeof(len_low) + sizeof(type_id)];

  int32_t block;
  try {
    block = nextBlock();
  }
  catch(const std::exception& e) {
    return {};
  }

  if (!block)
    return {};

  if (BLCK_SIZE - dire_i < sizeof(buff)) {
    if (pread(image_fd, buff, BLCK_SIZE - dire_i, dire_i + block * BLCK_SIZE) != BLCK_SIZE - dire_i || errno) {
      auto msg = strerror(errno);
      throw std::ios_base::failure("Something went wrong with Inode::nextDirEntry() 1: " + std::string(msg));
    }
    inc();
    block = nextBlock();

    if (!block)
      throw std::ios_base::failure("Bad block obtaining Inode::nextDirEntry() 1");
    
    if (pread(image_fd, buff + BLCK_SIZE - dire_i, sizeof(buff) - BLCK_SIZE + dire_i, (0 + block) * BLCK_SIZE) !=  sizeof(buff) - BLCK_SIZE + dire_i || errno) {
      auto msg = strerror(errno);
      throw std::ios_base::failure("Something went wrong with Inode::nextDirEntry() 2: " + std::string(msg));
    }
  }
  else {
    if (pread(image_fd, buff, sizeof(buff), dire_i + block * BLCK_SIZE) != sizeof(buff) || errno) {
      auto msg = strerror(errno);
      throw std::ios_base::failure("Something went wrong with Inode::nextDirEntry() 3 : " + std::string(msg));
    }
  }

  dir_entry_inode   = *((int32_t*)(buff + 0));
  total_size        = *((int16_t*)(buff + 4));
  len_low           = *((int8_t *)(buff + 6));
  type_id = len_top = *((int8_t *)(buff + 7));

  int16_t len = len_low;
  if (!DIR_ENTRY_TYPE)
    *((int8_t*)&len) = len_top;

  size_t to_read = len;
  auto name = new char[len];

  bool first_iter = true;

  while (to_read != 0) {
    block = nextBlock();

    if (!block)
      throw std::ios_base::failure("Bad block obtaining Inode::nextDirEntry() 2");

    auto tmp_to_read = std::min(to_read, (size_t)BLCK_SIZE);
    int32_t offset = 0;
    if (first_iter) [[likely]] {
      offset = dire_i + block * BLCK_SIZE + 8;
      first_iter = false;
    }
    else 
      offset = block * BLCK_SIZE;
    
    if (pread(image_fd, name + (len - to_read), tmp_to_read, offset) != tmp_to_read || errno) {
      auto msg = strerror(errno);
      throw std::ios_base::failure("Something went wrong with Inode::nextDirEntry() 4: " + std::string(msg));
    }

    to_read -= tmp_to_read;
    if (to_read)
      inc();
  }

  if ((dire_i + total_size) / BLCK_SIZE)
    inc();

  dire_i = (dire_i + total_size) % BLCK_SIZE;

  auto res = DirEntry {
    dir_entry_inode,
    std::string(name)
  };

  delete[] name;

  return res;
}

int32_t Reader::inodeFromPath(const char* path_to_file) {
  auto path = std::string(path_to_file);
  std::string delimiter = "/";
  auto name_list = split(path, delimiter);

  if (name_list[0] == "")
    name_list.erase(name_list.begin());

  auto inode = Inode(image_fd, ROOT_INODE);
  
  DirEntry dir;
  for (const auto & name : name_list) {
    
    do {
      dir = inode.nextDirEntry();
    }
    while (std::get<0>(dir) != int32_t() && std::get<1>(dir) != name);
    
    if (std::get<1>(dir) != name)
      return -1;
    
    
    inode = Inode(image_fd, std::get<0>(dir));
    if (name != name_list.back() && inode.t() != InodeType::DIR)
      return -1;
  }

  return std::get<0>(dir);
}

void Reader::printDir(int32_t inode_num) {
  Inode inode(image_fd, inode_num);
  if (inode.t() != InodeType::DIR) {
    auto msg = std::to_string(inode_num);
    throw std::ios_base::failure("File inode=" + msg + " is not a directory", std::make_error_code(std::errc::invalid_argument));
  }
  while (inode.printDirEntry() != 0);
}

int Inode::printDirEntry() {
  auto dire = nextDirEntry();

  if (std::get<0>(dire) == 0)
    return 0;

  Inode inode(image_fd, std::get<0>(dire));
  inode.printMeta();
  std::cout << " " << std::get<1>(dire) << "\n";
  
  return std::get<0>(dire);
}

void Inode::printMeta() {
  std::string inode_tp;
  
  try {
    inode_tp = TYPE_MSG_MAP.at(t());
  } catch (std::exception& e) {
    std::cout << "Undefined file type";
    return;
  }

  inode_tp += permissionString(type);

  std::cout << inode_tp;
}