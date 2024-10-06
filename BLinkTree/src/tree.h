#ifndef TREE_H
#define TREE_H

#include "node.h"
#include "value.h"

struct BLinkTree {
    BTNode* root;
    int f; /// file descriptor
};

typedef struct BLinkTree BLinkTree;

BLinkTree* initBLinkTree(char* fname);

deleteBLinkTree(BLinkTree* tree);

void BLinkTreeWrite(BLinkTree* tree, Key key, Value* value);

// if key was not found, return 1 and set buff to nullptr, otherwise return 0
// buff: buffer where value under key will be writen
bool BLinkTreeRead(BLinkTree* tree, Key key, Value* buff);

// if `key` was not found, return 1, otherwise delete the key and related data and return 0
bool BLinkTreeDelete(BLinkTree* tree, Key key);

void BLinkTreeSerialize(BLinkTree* tree, char* fname);

BLinkTree* BLinkTreeDeserialize(char* fname);

void BLinkTreeDot(BLinkTree* tree, char* fname);

#endif // TREE_H