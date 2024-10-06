#ifndef NODE_H
#define NODE_H

#include "string.h"
#include "stdbool.h"
#include "value.h"
#include "stdlib.h"

#define L 64

#define INVALID_OFFSET = 1

#define NODE_SIZE sizeof(BTNode)

enum BTFlags {
    ROOT_NODE = 0x0002,
    LEAF_NODE = 0x0001,
    INTERNAL_NODE = 0x0000,
    // NEED_TO_MERGE = 0x0004,
    // NEED_TO_SPLIT = 0x0008
};
typedef enum BTFlags BTFlags;

typedef size_t Key;

typedef size_t Offset; // offset of BTNode in b link tree storage file

struct __attribute__((__packed__)) BTNode {
    Offset parent;
    Offset self;
    
    size_t key_buff_size;
    Key key_buff[L];

    Offset right;
    Key high_key;

    int flag;

    size_t buff_size;
    union {
        Offset node_buff[L + 1];
        Value value_buff[L];
    };
};

typedef struct BTNode BTNode;

BTNode* btnodeInit(Offset parent, BTFlags flags);

// return true if `node` have flag `flag`, otherwise return false
bool btnodeFlag(BTNode* node, BTFlags flag);

// return nullptr if `key` was not found or if `node` is a leaf node
Offset btnodeChildByKey(BTNode* node, Key key);

// return the index of the first key in `key_buff` that greater or equal to `key`. 
// if `key` less then `key_buff[0]` return 0
size_t btnodeFirstGreaterOrEqualKey(BTNode* node, Key key);

// return nullptr if `key` was not found or if `node` is a root or internal node
Value btnodeValueByKey(BTNode* node, Key key); 


#endif // NODE_H 