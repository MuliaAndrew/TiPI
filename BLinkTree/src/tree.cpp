#include "tree.h"
#include "nodeIO.h"
#include <iostream>
#include <stack>

#define rdLock(lock) pthread_rwlock_rdlock((lock))
#define wrLock(lock) pthread_rwlock_wrlock((lock))
#define unLock(lock) pthread_rwlock_unlock((lock))


const Offset ROOT_OFFSET = 100;

std::mutex BLinkTree::lockmap_mut{};
Lockmap BLinkTree::lockmap{};
 
BLinkTree::BLinkTree(const std::string& fname) {
    current = nullptr;
    node_io = NodeIO(fname);
}

BLinkTree::BLinkTree(const BLinkTree& other) {
    current = nullptr;
    node_io = other.node_io;
}

BLinkTree::~BLinkTree() {
    if (current) delete current;
}


/// write `current` node to the service file 
/// assotiated `lock` in `lockmap` must be taken before write and released after mannualy


Lock* BLinkTree::getNodeRWLock(Offset offset) {
    lockmap_mut.lock();
    Lock* lock;
    if (lockmap.count(offset))
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
    auto root = node_io.getRootOffset();
    auto lock = getNodeRWLock(root);
    Lock* prev = nullptr;
    
    rdLock(lock);
    current = node_io.readNode(root);

    while (!btnodeFlag(current, LEAF_NODE)) {
        auto offset = btnodeChildByKey(current, key);
        if (offset == INVALID_OFFSET) {
            if (current->right && current->right != INVALID_OFFSET) 
                offset = current->right;
            else {
                return {};
            }
        }
        prev = lock;
        lock = getNodeRWLock(offset);
        
        delete current;

        // Firstly unlock self, after lock on child 
        unLock(prev);
        rdLock(lock);

        current = node_io.readNode(offset);
    }

    auto res = btnodeValueByKey(current, key);
    delete current;
    unLock(lock);
    return res;
}

void BLinkTree::write(Key key, Value* value) {
    auto root = node_io.getRootOffset();
    Lock* lock = getNodeRWLock(root);
    Lock* prev = nullptr;

    std::stack<Offset> stk({root});

    rdLock(lock);
    
    current = node_io.readNode(root);

    while (!btnodeFlag(current, LEAF_NODE)) {
        auto offset = btnodeChildByKey(current, key);
        if (offset == INVALID_OFFSET) {
            if (current->right && current->right != INVALID_OFFSET) 
                offset = current->right;
            else {
                return;
            }
        }
        prev = lock;
        lock = getNodeRWLock(offset);
        
        delete current;

        // Firstly unlock self, after lock on child 
        unLock(prev);
        rdLock(lock);

        current = node_io.readNode(offset);
    }

    // release read lock and take write lock
    unLock(lock);
    wrLock(lock);

    // case when node was splited and value must be inserted to the right node
    // case may only happen just after split on this node
    while (key > current->high_key) {
        prev = lock;
        lock = getNodeRWLock(current->right);
        unLock(prev);
        wrLock(lock);
    }

    // check if node is "safe"
    if (current->key_buff_size < 2 * L) {
        // split case
    } 
    else {
        // simple insert case
        btnodeAddKeyValue(current, key, value);
        delete current;
        unLock(lock);
    }
}
