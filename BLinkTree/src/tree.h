#ifndef TREE_H
#define TREE_H

#include <unordered_map>
#include <string>
#include <mutex>

extern "C" {
    #include "node.h"
}

typedef pthread_rwlock_t Lock;
typedef std::unordered_map<Offset, Lock*> Lockmap;

class BLinkTree {
        BTNode* current = nullptr;
        BTNode* tmp1 = nullptr, *tmp2 = nullptr;

        std::string fname;
        int fd; // file descriptor

        static std::mutex lockmap_mut;
        static Lockmap lockmap;

    public:
        BLinkTree() = delete;
        BLinkTree(BLinkTree&&) = delete;
        BLinkTree(const BLinkTree&);
        BLinkTree(std::string file_name);

        ~BLinkTree();

        Value read(Key key);

        void write(Key key, Value* value);

        bool erase(Key key);

        static void createEmptyServiceFile(std::string fname);
    
    private:
        Lock* getNodeRWLock(Offset offset);

        BTNode* readNode(Offset offset);

        void writeNode();
        
        // TODO: Split, Rebalance, Merge
};

#endif // TREE_H