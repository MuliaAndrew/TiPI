#include "reader.hpp"

#include <string.h>

int32_t BLCK_SIZE;
int32_t INODE_SIZE;
int32_t INODE_PER_GROUP;
int32_t NUM_OF_BLCK_GROUPS;
int32_t INODE_TABLE_SIZE;

void Reader::fill() {
  char buff[1024] = {0};

  if (pread(image_fd, buff, 1024, 1024) != 1024 || errno) {
    auto msg = strerror(errno);
    throw std::ios_base::failure("Something went wrong with Reader::fill::pread() : " + std::string(msg));
  }

  BLCK_SIZE = 1 << (*((int32_t*)(buff + 24)) + 10);
  INODE_PER_GROUP = *((int32_t*)(buff + 40));
  
  int32_t MAJOR = *((int32_t*)(buff + 76));
  if (MAJOR >= 1)
    INODE_SIZE = (int32_t)(*((int16_t*)(buff + 88)));
  else
    INODE_SIZE = 128;
  
  int32_t INODE_TOTAL = *((int32_t*)(buff + 0));

  NUM_OF_BLCK_GROUPS = INODE_TOTAL / INODE_PER_GROUP + (INODE_TOTAL % INODE_PER_GROUP) ? 1 : 0;
  INODE_TABLE_SIZE = INODE_PER_GROUP * INODE_SIZE;

  // DBG!
  std::cout << "BLCK_SIZE : " << BLCK_SIZE << "\n"
    << "INODE_SIZE : " << INODE_SIZE << "\n"
    << "INODE_PER_GROUP : " << INODE_PER_GROUP << "\n"
    << "NUM_OF_BLCK_GROUPS : " << NUM_OF_BLCK_GROUPS << "\n"
    << "INODE_TABLE_SIZE : " << INODE_TABLE_SIZE << "\n";
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
  auto inode_table = new char[INODE_TABLE_SIZE];

  delete[] BGDT_buff;

  // extracting inode table from block group
  if (pread(image_fd, inode_table, INODE_TABLE_SIZE, inode_table_blck * BLCK_SIZE) != INODE_TABLE_SIZE || errno) {
    auto msg = strerror(errno);
    throw std::ios_base::failure("Something went wrong with second Inode::fill::pread() : " + std::string(msg));
  }

  // finally getting inode
  auto inode_buff = inode_table + INODE_SIZE * inode_ind;

  type = *((int16_t*)(inode_buff + 0));

  int32_t size_lower = *((int32_t*)(inode_buff + 4));
  int32_t size_higher = *((int32_t*)(inode_buff + 108));

  size = ((int64_t)size_higher) << 32 + size_lower;

  memcpy(direct_block, inode_buff + 40, 48);

  singly_ibp = *((int32_t*)(inode_buff + 88));
  doubly_ibp = *((int32_t*)(inode_buff + 92));
  triply_ibp = *((int32_t*)(inode_buff + 96));

  delete[] inode_table;
}

int Inode::printData() {
  auto buff = new char[BLCK_SIZE];
  if (ind * BLCK_SIZE > size)
    return 0;

  auto N_TO_READ = (size - ind * BLCK_SIZE > BLCK_SIZE) ? BLCK_SIZE : size - ind * BLCK_SIZE;
  
  int readed = 0;
  // reading from direct blocks
  if (ind < 12) {
    readed = pread(image_fd, buff, N_TO_READ, direct_block[ind] * BLCK_SIZE);
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

  // reading from singly indirect block
  if (ind < 12 + BLCK_SIZE / 4) {
    if (pread(image_fd, buff, BLCK_SIZE, singly_ibp * BLCK_SIZE) != BLCK_SIZE || errno) {
      auto msg = strerror(errno);
      throw std::ios_base::failure("Something went wrong with indidrect1 Inode::printData::pread() : " + std::string(msg));
    }
    
    auto tmp_buff = new char[BLCK_SIZE];
    memcpy(tmp_buff, buff, BLCK_SIZE);
    auto i = (ind - 12); 
    
    readed = pread(image_fd, buff, N_TO_READ, *((int32_t*)(tmp_buff + i)));
    
    if (write(STDOUT_FILENO, buff, readed) != readed || errno) {
      auto msg = strerror(errno);
      throw std::ios_base::failure("Something went wrong with indirect1 Inode::printData::write() : " + std::string(msg));
    }

    delete[] tmp_buff;
    delete[] buff;
    ind++;
    return readed;
  }

  // reading from doubly indirect block
  if (ind < 12 + BLCK_SIZE / 4 + BLCK_SIZE * BLCK_SIZE / 16) {
    if (pread(image_fd, buff, BLCK_SIZE, doubly_ibp * BLCK_SIZE) != BLCK_SIZE || errno) {
      auto msg = strerror(errno);
      throw std::ios_base::failure("Something went wrong with indiderect2.1 Inode::printData::pread() : " + std::string(msg));
    }

    auto tmp_buff = new char[BLCK_SIZE];
    memcpy(tmp_buff, buff, BLCK_SIZE);

    auto tmp1_ind = (ind - 12 - BLCK_SIZE / 4) / (BLCK_SIZE / 4);
    auto tmp2_ind = (ind - 12 - BLCK_SIZE / 4) % (BLCK_SIZE / 4);

    if (pread(image_fd, buff, BLCK_SIZE, *((int32_t*)(tmp_buff + tmp1_ind)) * BLCK_SIZE) != BLCK_SIZE || errno) {
      auto msg = strerror(errno);
      throw std::ios_base::failure("Something went wrong with indiderect2.2 Inode::printData::pread() : " + std::string(msg));
    }
    memcpy(tmp_buff, buff, BLCK_SIZE);

    readed = pread(image_fd, buff, N_TO_READ, *((int32_t*)(tmp_buff + tmp2_ind)) * BLCK_SIZE);

    if (write(STDOUT_FILENO, buff, readed) != readed || errno) {
      auto msg = strerror(errno);
      throw std::ios_base::failure("Something went wrong with indirect2 Inode::printData::write() : " + std::string(msg));
    }

    delete[] tmp_buff;
    delete[] buff;
    ind++;
    return readed;
  }

  return readed; 
  // reading from triply indirect block

  // TODO
}