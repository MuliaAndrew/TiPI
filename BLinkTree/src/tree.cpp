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
    node_io = NodeIO(fname);
}

BLinkTree::BLinkTree(const BLinkTree& other) {
    node_io = other.node_io;
}


/// write `current` node to the service file 
/// assotiated `lock` in `lockmap` must be taken before write and released after mannualy


Lock* BLinkTree::getNodeRWLock(Offset offset) {
    lockmap_mut.lock();
    Lock* lock;
    if (lockmap.count(offset))
        lock = lockmap[offset];
    else {
        lock = (Lock*)malloc(sizeof(Lock));
        if (pthread_rwlock_init(lock, NULL) || errno) {
            std::cout << "[ERR" << getpid() << "] Bad rw_lock initialization\n";
            exit(1);
        }
        lockmap[offset] = lock;
    }
    lockmap_mut.unlock();
    if (lock == nullptr) {
        std::cout << "[ERR LOCK INIT]\n";
    }
    return lock;
}

Value BLinkTree::read(Key key) {
    auto root = node_io.getRootOffset();
    auto lock = getNodeRWLock(root);
    Lock* prev = nullptr;
    
    rdLock(lock);

    // while waiting for root lock root can be changed 
    auto root_tmp = node_io.getRootOffset();
    while (root_tmp != root) {
        unLock(lock);
        lock = getNodeRWLock(root_tmp);
        root = root_tmp;
        rdLock(lock);
        root_tmp = node_io.getRootOffset();
    } // now root is valid 

    BTNode* current = node_io.readNode(root);

    while (!btnodeFlag(current, LEAF_NODE)) {
        auto offset = btnodeChildByKey(current, key);
        if (offset == INVALID_OFFSET) {
            offset = current->right;
        }
        prev = lock;
        lock = getNodeRWLock(offset);
        
        free(current);

        // Firstly unlock self, after lock on child 
        unLock(prev);
        rdLock(lock);

        current = node_io.readNode(offset);
    }

    while (current->right != INVALID_OFFSET && current->high_key < key) {
        auto offset = current->right;
        
        prev = lock;
        lock = getNodeRWLock(offset);

        free(current);

        unLock(prev);
        rdLock(lock);

        current = node_io.readNode(offset);
    }

    auto res = btnodeValueByKey(current, key);
    free(current);
    unLock(lock);
    return res;
}

void BLinkTree::write(Key key, Value* value) {
    auto root = node_io.getRootOffset();

    Lock* lock = getNodeRWLock(root);
    Lock* prev = nullptr;

    std::stack<Offset> stk{};

    rdLock(lock);

    BTNode* current = node_io.readNode(root);

    while (!btnodeFlag(current, LEAF_NODE)) {
        auto offset = btnodeChildByKey(current, key);
        if (offset == INVALID_OFFSET) {
            if (current->right != INVALID_OFFSET) 
                offset = current->right;
            else {
                return;
            }
        } else {
            stk.push(current->self);
        }
        prev = lock;
        lock = getNodeRWLock(offset);
        
        free(current);

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
    while (current->right != INVALID_OFFSET && key >= current->high_key) {
        prev = lock;
        lock = getNodeRWLock(current->right);
        unLock(prev);
        wrLock(lock);
        auto offset = current->right;
        free(current);
        current = node_io.readNode(offset);
    }

    // check if node is "safe"
    if (current->key_buff_size < 2 * L) {
        // simple insert case
        btnodeAddKeyValue(current, key, value);
        node_io.writeNode(current);
        free(current);
        unLock(lock);
        return;
    }

    // the node is unsafe but write will only update already existing value
    auto ind = fgoe(current, key);
    if (ind < current->key_buff_size && current->key_buff[ind] == key) {
        btnodeAddKeyValue(current, key, value);
        node_io.writeNode(current);
        free(current);
        unLock(lock);
        return;
    }
    // split case
    Offset node_to_add = INVALID_OFFSET;
    Key key_to_add = key;

    // while current node not a ROOT and its unsafe locking self and parent for write
    while(current->key_buff_size == 2 * L && !btnodeFlag(current, ROOT_NODE)) {
        auto parent_offset = stk.top();
        stk.pop();

        prev = lock;
        lock = getNodeRWLock(parent_offset);
        wrLock(lock);

        auto parent = node_io.readNode(parent_offset);
        while (parent->right != INVALID_OFFSET && parent->high_key <= key_to_add) {
            parent_offset = parent->right;
            free(parent);
            unLock(lock);
            lock = getNodeRWLock(parent_offset);
            wrLock(lock);
            parent = node_io.readNode(parent_offset);
        }

        // now parent is up-to-date parent, which is having current node

        auto right_offset = node_io.addNode();
        getNodeRWLock(right_offset);

        auto right = btnodeSplit(current, right_offset);

        if (btnodeFlag(current, LEAF_NODE)) {
            if (current->high_key > key) 
                btnodeAddKeyValue(current, key, value);
            else 
                btnodeAddKeyValue(right, key, value);
        } 
        else {
            if (current->high_key > key)
                btnodeAddKeyChild(current, key_to_add, node_to_add);
            else
                btnodeAddKeyChild(right, key_to_add, node_to_add);
        }

        node_io.writeNode(current);
        node_io.writeNode(right);

        key_to_add = current->high_key;
        node_to_add = right_offset;

        free(current);
        free(right);

        current = parent;
        unLock(prev);
    }

    if (current->key_buff_size == 2 * L && btnodeFlag(current, ROOT_NODE)) {
        
        auto right_offset = node_io.addNode();
        getNodeRWLock(right_offset);

        auto right = btnodeSplit(current, right_offset);
        // node is a root and leaf simultaniously
        if (btnodeFlag(current, LEAF_NODE)) {
            if (current->high_key > key) 
                btnodeAddKeyValue(current, key, value);
            else 
                btnodeAddKeyValue(right, key, value);
        } 
        else {
            if (current->high_key > key)
                btnodeAddKeyChild(current, key_to_add, node_to_add);
            else    
                btnodeAddKeyChild(right, key_to_add, node_to_add);
        }

        node_io.writeNode(current);
        node_io.writeNode(right);

        auto root_offset = node_io.addNode();
        auto root_lock = getNodeRWLock(root_offset);
        wrLock(root_lock);

        node_io.setRootOffset(root_offset);

        BTNode* root = (BTNode*)malloc(sizeof(BTNode));
        root->self = root_offset;
        root->flag = ROOT_NODE;
        root->key_buff_size = 1;
        root->buff_size = 2;
        root->right = INVALID_OFFSET;
        root->high_key = 0;
        root->key_buff[0] = current->high_key;
        root->node_buff[0] = current->self;
        root->node_buff[1] = right->self;

        node_io.writeNode(root);

        free(current);
        free(right);
        free(root);

        unLock(lock);
        unLock(root_lock);

    }
    else {
        btnodeAddKeyChild(current, key_to_add, node_to_add);
        node_io.writeNode(current);

        free(current);

        unLock(prev);
        unLock(lock);
    }
}
