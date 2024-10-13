#ifndef NODE_H
#define NODE_H

#include "string.h"
#include "stdbool.h"
#include "value.h"
#include "stdlib.h"

#ifndef L
    #define L 64
#endif

#define INVALID_OFFSET 1

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
    Offset self;
    
    size_t key_buff_size;
    Key key_buff[2 * L];

    Offset right;
    Key high_key;

    int flag;

    size_t buff_size;
    union {
        Offset node_buff[2 * L + 1];
        Value value_buff[2 * L];
    };
};

typedef struct BTNode BTNode;

// return true if `node` have flag `flag`, otherwise return false
bool btnodeFlag(BTNode* node, BTFlags flag);

// return `INVALID_OFFSET` if `key` was not found or if `node` is a leaf node
Offset btnodeChildByKey(BTNode* node, Key key);


// return default `Value` if `key` was not found or if `node` is not a leaf node
Value btnodeValueByKey(BTNode* node, Key key); 

// add a (`key`, `value`) pair to the leaf node if it have available space
void btnodeAddKeyValue(BTNode* node, Key key, Value* val);

/// @brief Split a `node` on two with ueqal number of keys. 
/// Lefthand node is written in `node`, righthand node is returned.
/// @param node node to slpit
/// @param other offset of new righthand
/// @return pointer to the new righthand node
BTNode* btnodeSplit(BTNode* node, Offset other);


#endif // NODE_H 