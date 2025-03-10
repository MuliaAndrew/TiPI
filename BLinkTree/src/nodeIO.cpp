#include "nodeIO.h"
#include <sys/types.h>
#include <sys/stat.h>


std::mutex NodeIO::mut_free_space{};
std::shared_mutex NodeIO::mut_root{};

// assotiated `lock` in `lockmap` must be taken before read and released after mannualy
BTNode* NodeIO::readNode(Offset offset) {
    BTNode* node = (BTNode*)malloc(NODE_SIZE);
    if (::pread(fd, node, NODE_SIZE, offset) != NODE_SIZE || errno) {
        perror("bad node read");
        exit(1);
    }
    return node;
}

void NodeIO::writeNode(BTNode* current) {
    auto offset = current->self;
    if (::pwrite(fd, current, NODE_SIZE, offset) != NODE_SIZE || errno) {
        perror("bad node write");
        exit(1);
    }
}

void NodeIO::createEmptyServiceFile(std::string& fname) {
    const Offset INITIAL_ROOT_OFFSET = 100;
    const Offset INITIAL_FREE_SPACE_OFFSET = INITIAL_ROOT_OFFSET + NODE_SIZE;

    errno = 0;

    int fd = open(fname.c_str(), O_CREAT | O_RDWR | O_TRUNC, 0644);

    char buff[NODE_SIZE];

    if (::write(fd, &INITIAL_ROOT_OFFSET, sizeof(Offset)) != sizeof(Offset) || errno) {
        std::cout << "[ERR " << getpid() << "] Bad new tree 1 write\n";
        perror("WTF");
        exit(1); 
    }
    if (::pwrite(fd, &INITIAL_FREE_SPACE_OFFSET, sizeof(Offset), 8ul) != sizeof(Offset) || errno) {
        std::cout << "[ERR " << getpid() << "] Bad new tree 2 write\n";
        exit(1); 
    }
    size_t t_offset;

    t_offset = offset_of(BTNode, self);
    member_val(Offset, buff, t_offset) = INITIAL_ROOT_OFFSET;

    t_offset = offset_of(BTNode, key_buff_size);
    member_val(size_t, buff, t_offset) = 0;

    Key tmp[2 * L] = {};
    t_offset = offset_of(BTNode, key_buff);
    memcpy(buff + t_offset, tmp, sizeof(Key[2 * L]));

    t_offset = offset_of(BTNode, right);
    member_val(Offset, buff, t_offset) = INVALID_OFFSET;

    t_offset = offset_of(BTNode, high_key);
    member_val(Key, buff, t_offset) = 0;

    t_offset = offset_of(BTNode, flag);
    member_val(int, buff, t_offset) = ROOT_NODE | LEAF_NODE;

    t_offset = offset_of(BTNode, buff_size);
    member_val(size_t, buff, t_offset) = 0;
    
    Value tmp1[2 * L] = {};
    t_offset = offset_of(BTNode, value_buff);
    memcpy(buff + t_offset, tmp1, sizeof(Value[2 * L]));

    if (lseek(fd, INITIAL_ROOT_OFFSET, SEEK_SET) != INITIAL_ROOT_OFFSET || errno) {
        std::cout << "[ERR " << getpid() << "] Bad new tree write lseek\n";
        exit(1);
    }
    if (::write(fd, buff, NODE_SIZE) != NODE_SIZE || errno) {
        std::cout << "[ERR " << getpid() << "] Bad new tree 3 write\n";
        exit(1); 
    }

    close(fd);
}

void NodeIO::setRootOffset(Offset new_root) {
    mut_root.lock();
    if (pwrite(fd, &new_root, sizeof(Offset), 0) != sizeof(Offset) || errno) {
        std::cout << "[ERR " << getpid() << "] bad setRootOffset write\n";
        exit(1);
    }
    mut_root.unlock();
}

Offset NodeIO::getRootOffset() {
    mut_root.lock_shared();
    Offset root;
    if (pread(fd, &root, sizeof(Offset), 0) != sizeof(Offset) || errno) {
        std::cout << "[ERR " << getpid() << "] bad getRootOffset read\n";
        exit(1);
    }
    mut_root.unlock_shared();

    return root;
}

Offset NodeIO::addNode() {
    mut_free_space.lock();
    Offset new_node_offset;
    if (pread(fd, &new_node_offset, sizeof(Offset), 8) != sizeof(Offset) || errno) {
        std::cout << "[ERR " << getpid() << "] bad addNode read\n";
        exit(1);
    }
    new_node_offset += NODE_SIZE;
    if (pwrite(fd, &new_node_offset, sizeof(Offset), 8) != sizeof(Offset) || errno) {
        std::cout << "[ERR " << getpid() << "] bad addNode write\n";
        exit(1);
    }
    mut_free_space.unlock();

    return new_node_offset - NODE_SIZE;
}

NodeIO::NodeIO(const std::string& file_name) {
    fname = file_name;
    fd = open(fname.c_str(), O_RDWR | O_SYNC);
}

NodeIO::NodeIO(const NodeIO& other) {
    fname = other.fname;
    fd = open(fname.c_str(), O_RDWR | O_SYNC);
}

NodeIO::~NodeIO() {
    close(fd);
}

NodeIO& NodeIO::operator= (const NodeIO& other) {
    NodeIO tmp(other);
    std::swap(fname, tmp.fname);

    fd = open(fname.c_str(), O_RDWR | O_SYNC);
    return *this;
}