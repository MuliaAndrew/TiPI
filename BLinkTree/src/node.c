#include "node.h"

bool btnodeFlag(BTNode* node, BTFlags flag) {
    return node->flag & flag;
}

Offset btnodeChildByKey(BTNode* node, Key key) {
    if (btnodeFlag(node, LEAF_NODE))
        return NULL;

    size_t right = node->key_buff_size;
    size_t left = 0;

    if (right - left < 2) {
        if (key < left)
            return node->node_buff[0];
        
        if (key >= right)
            return node->node_buff[2];

        return node->node_buff[1];
    }

    size_t mid = (left + right) / 2;

    // Binary search for child
    while (right > left + 1) {
        if (mid >= key) {
            size_t tmp = right;
            right = mid;
            mid = (left + right) / 2;
        }
        else {
            size_t tmp = left;
            left = mid;
            mid = (left + right) / 2;
        }
    }

    if (key < left)
        return node->node_buff[left];
    
    if (key >= right)
        return node->node_buff[left + 2];

    return node->node_buff[left + 1];
}

// size_t btnodeFirstGreaterOrEqualKey(BTNode* node, Key key) {
//     size_t key_sz = node->key_buff_size;

//     size_t
// }

Value btnodeValueByKey(BTNode* node, Key key) {
    Key left = 0;
    Key right = node->key_buff_size;
    Key mid = (left + right) / 2;
    
    while (left < right) {
        if (node->key_buff[mid] > key) {
            left = mid;
            mid = (left + right) / 2;
        }
        else if (node->key_buff[mid] < key) {
            right = mid;
            mid = (left + right) / 2;
        }
        else return node->value_buff[mid];
    }

    return;
}

BTNode* btnodeInit(Offset parent, BTFlags flags) {
    BTNode* node = (BTNode*)malloc(sizeof(BTNode));
    node->flag = flags;

    node->key_buff_size = 0;

    node->buff_size = 0;
    
    node->parent = parent;
    node->high_key = 0;
}