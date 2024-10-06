#include "tree.h"
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <errno.h>

#ifndef offset_of
#define offset_of(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

#ifndef container_of
/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({			\
	const typeof(((type *)0)->member) * __mptr = (ptr);	\
	(type *)((char *)__mptr - offsetof(type, member)); })
#endif

#define member_val(type, buf, offset) \
    *((type*)((buff) + (offset)))

const Offset ROOT_OFFSET = 100;

std::mutex BLinkTree::lockmap_mut{};
Lockmap BLinkTree::lockmap{};

// каждый тред имеет свой дескриптор 
BLinkTree::BLinkTree(std::string file_name) {
    tmp1 = tmp2 = nullptr;

    fname = file_name;
    fd = open(fname.c_str(), O_RDWR || O_CREAT || O_SYNC, S_IRUSR || S_IWUSR);

    current = readNode(ROOT_OFFSET);
}

BLinkTree::BLinkTree(const BLinkTree&) {
    tmp1 = tmp2 = nullptr;

    fd = open(fname.c_str(), O_RDWR || O_CREAT || O_SYNC, S_IRUSR || S_IWUSR);

    current = readNode(ROOT_OFFSET);
}

BLinkTree::~BLinkTree() {
    if (current) delete current;
    if (tmp1)    delete tmp1;
    if (tmp2)    delete tmp2;

    close(fd);
}

// assotiated `lock` in `lockmap` must be taken before read and released after mannualy
BTNode* BLinkTree::readNode(Offset offset) {
    char buff[NODE_SIZE + 1] = {};
    if (lseek(fd, offset, SEEK_SET) != offset || errno) {
        std::cout << "[ERR " << getpid() << "] Bad read lseek\n";
        exit(1);
    }
    if (::read(fd, buff, NODE_SIZE) != NODE_SIZE || errno) {
        std::cout << "[ERR " << getpid() << "] Bad node read\n";
        exit(1);
    }

    auto node = new BTNode;

    size_t t_offset = 0;
    node->parent = member_val(Offset, buff, t_offset);

    t_offset = offset_of(BTNode, self);
    node->self = member_val(Offset, buff, t_offset);

    t_offset = offset_of(BTNode, key_buff_size);
    node->key_buff_size = member_val(size_t, buff, t_offset);

    t_offset = offset_of(BTNode, key_buff);
    memcpy(node->key_buff, buff + t_offset, sizeof(Key[L]));

    t_offset = offset_of(BTNode, right);
    node->right = member_val(Offset, buff, t_offset);

    t_offset = offset_of(BTNode, high_key);
    node->high_key = member_val(Key, buff, t_offset);

    t_offset = offset_of(BTNode, flag);
    node->flag = member_val(int, buff, t_offset);

    t_offset = offset_of(BTNode, buff_size);
    node->buff_size = member_val(size_t, buff, t_offset);

    if (btnodeFlag(node, LEAF_NODE)) {
        t_offset = offset_of(BTNode, value_buff);
        memcpy(node->value_buff, buff + t_offset, sizeof(Value[L]));
    } 
    else {
        t_offset = offset_of(BTNode, node_buff);
        memcpy(node->node_buff, buff + t_offset, sizeof(Offset[L + 1]));
    }

    return node;
}
/// write `current` node to the service file 
/// assotiated `lock` in `lockmap` must be taken before write and released after mannualy
void BLinkTree::writeNode() {
    char buff[NODE_SIZE + 1] = {};
    
    size_t t_offset = 0;

    size_t t_offset = 0;
    member_val(Offset, buff, t_offset) = current->parent;

    t_offset = offset_of(BTNode, self);
    member_val(Offset, buff, t_offset) = current->self;

    t_offset = offset_of(BTNode, key_buff_size);
    member_val(size_t, buff, t_offset) = current->key_buff_size;

    t_offset = offset_of(BTNode, key_buff);
    memcpy(buff + t_offset, current->key_buff, sizeof(Key[L]));

    t_offset = offset_of(BTNode, right);
    member_val(Offset, buff, t_offset) = current->right;

    t_offset = offset_of(BTNode, high_key);
    member_val(Key, buff, t_offset) = current->high_key;

    t_offset = offset_of(BTNode, flag);
    member_val(int, buff, t_offset) = current->flag;

    t_offset = offset_of(BTNode, buff_size);
    member_val(size_t, buff, t_offset) = current->buff_size;

    if (btnodeFlag(current, LEAF_NODE)) {
        t_offset = offset_of(BTNode, value_buff);
        memcpy(buff + t_offset, current->value_buff, sizeof(Value[L]));
    } 
    else {
        t_offset = offset_of(BTNode, node_buff);
        memcpy(buff + t_offset, current->node_buff, sizeof(Offset[L + 1]));
    }

    if (lseek(fd, current->self, SEEK_SET) != current->self || errno) {
        std::cout << "[ERR " << getpid() << "] Bad write lseek\n";
        exit(1);
    }
    if (::write(fd, buff, NODE_SIZE) != NODE_SIZE || errno) {
        std::cout << "[ERR " << getpid() << "] Bad node write\n"; 
    }
}

Lock* BLinkTree::getNodeRWLock(Offset offset) {
    lockmap_mut.lock();
    Lock* lock;
    if (lockmap.contains(offset))
        lock = lockmap[offset];
    else {
        if (pthread_rwlock_init(lock, NULL) || errno) {
            std::cout << "[ERR" << getpid() << "] Bad rw_lock initialization\n";
            exit(1);
        }
        lockmap[offset] = lock;
    }
    lockmap_mut.unlock();
    return lock;
}

Value BLinkTree::read(Key key) {
    auto lock = getNodeRWLock(ROOT_OFFSET);
    Lock* prev = nullptr;
    
    pthread_rwlock_rdlock(lock);
    if (current == nullptr || current->self != ROOT_OFFSET) {
        current = readNode(ROOT_OFFSET);
    }

    while (!btnodeFlag(current, LEAF_NODE)) {
        auto offset = btnodeChildByKey(current, key);
        if (offset == INVALID_OFFSET) {
            if (current->right) 
                offset = current->right;
            else {
                std::cout << "[ERR " << getpid() 
                    << "] Bad read obtained offset or pointer to righthand node\n";
                return {};
            }
        }
        prev = lock;
        lock = getNodeRWLock(offset);
        
        delete current;

        // Firstly lock child node, only after unlock the current node
        pthread_rwlock_rdlock(lock);
        pthread_rwlock_unlock(prev);

        current = readNode(offset);
    }

    auto res = btnodeValueByKey(current, key);
    delete current;
    pthread_rwlock_unlock(lock);
    return res;
}