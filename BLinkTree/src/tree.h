#ifndef TREE_H
#define TREE_H

#include <unordered_map>
#include <string>
#include <mutex>
#include "nodeIO.h"

typedef pthread_rwlock_t Lock;
typedef std::unordered_map<Offset, Lock*> Lockmap;

class BLinkTree {
        BTNode* current = nullptr;

        NodeIO node_io;

        static std::mutex lockmap_mut;
        static Lockmap lockmap;

    public:
        BLinkTree() = delete;
        BLinkTree(BLinkTree&&) = delete;
        BLinkTree(const BLinkTree&);
        BLinkTree(const std::string& fname);

        ~BLinkTree();

        Value read(Key key);

        void write(Key key, Value* value);

        bool erase(Key key);
    
    private:
        Lock* getNodeRWLock(Offset offset);
        
        void rebalance();
};

#endif // TREE_H