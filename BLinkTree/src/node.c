#include "node.h"
#include <assert.h>

bool btnodeFlag(BTNode* node, BTFlags flag) {
    return node->flag & flag;
}

// first greater or equal key in key_buff. key `index` is returned. if key was not found returned key_buff size
size_t fgoe(BTNode* node, Key key) {
    int low=0;
    int high=node->key_buff_size;
    int mid;
    Key* nums = node->key_buff;
    if (key > nums [high - 1]) {
        return high;
    }
    while (low <= high) {
            mid = (low + high) / 2;
        if (nums[mid] == key) {  
            return mid;
        }
        
        if (key < nums[mid]) {     
        high = mid - 1;    
        } else {
        low = mid + 1;        
        }
        
    }
    return  low; 
}

Offset btnodeChildByKey(BTNode* node, Key key) {
    if (btnodeFlag(node, LEAF_NODE))
        return INVALID_OFFSET;

    return node->node_buff[fgoe(node, key) + 1];
}


Value btnodeValueByKey(BTNode* node, Key key) {
    size_t ind = fgoe(node, key);
    if (node->key_buff[ind] != key || !btnodeFlag(node, LEAF_NODE)) {
        Value v;
        return v;
    }
    else {
        return node->value_buff[ind];
    }
}

void btnodeAddKeyValue(BTNode* node, Key key, Value* val) {
    size_t sz = node->key_buff_size;
    
    if (sz == 2 * L || !btnodeFlag(node, LEAF_NODE)) return;

    size_t res = fgoe(node, key);

    if (node->key_buff[res] != key || res == sz) {
        size_t check1 = node->buff_size; 

        memmove(node->key_buff + res + 1, node->key_buff + res, (sz - res) * sizeof(Key));
        node->key_buff[res] = key;
        node->key_buff_size++;

        size_t check2 = node->buff_size;

        memmove(node->value_buff + res + 1, node->value_buff + res, (sz - res) * sizeof(Value));
        node->value_buff[res] = *val;
        node->buff_size++;

        size_t check3 = node->buff_size;
        // assert(check1 == check2);
        // assert(check2 == check3);
    }
    else {
        node->value_buff[res] = *val;
    }
}

// if splitting root node, then node and its sibling become internal node
// so new root must be mannualy constructed
BTNode* btnodeSplit(BTNode* node, Offset other) {
    BTNode* new_right = (BTNode*)malloc(sizeof(BTNode));
    new_right->self = other;
    node->flag &= ~ROOT_NODE;
    new_right->flag = node->flag;
    
    size_t sz = node->key_buff_size;
    size_t mid = (sz + 1) / 2;
    
    size_t pref;
    if (btnodeFlag(node, LEAF_NODE)) 
        pref = 0;
    else pref = 1;

    node->key_buff_size = mid;
    node->buff_size = mid + pref;

    new_right->key_buff_size = sz - mid;
    new_right->buff_size = sz - mid + pref;
    
    new_right->right = INVALID_OFFSET;

    node->right = other;
    node->high_key = node->key_buff[mid];

    memmove(new_right->key_buff, node->key_buff + mid, (sz - mid) * sizeof(Key));
    if (btnodeFlag(node, LEAF_NODE)) {
        memmove(new_right->value_buff, node->value_buff + mid, (sz - mid) * sizeof(Value));
    } 
    else {
        memmove(new_right->node_buff + 1, node->node_buff + mid + 1, sizeof(sz - mid) * sizeof(Offset));
    }

    return new_right;
}