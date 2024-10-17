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

    size_t i = fgoe(node, key);

    if (node->key_buff[i] == key) {
        return node->node_buff[i + 1];
    }

    return node->node_buff[i];
}


Value btnodeValueByKey(BTNode* node, Key key) {
    size_t ind = fgoe(node, key);
    if (node->key_buff[ind] != key || !btnodeFlag(node, LEAF_NODE)) {
        Value v;
        memset(v.data, 0, sizeof(char[16]));
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

        memmove(node->value_buff + res + 1, node->value_buff + res, (sz - res) * sizeof(Value));
        memcpy(&(node->value_buff[res]), val, sizeof(Value));
        node->buff_size++;
    }
    else {
        memcpy(&(node->value_buff[res]), val, sizeof(Value));
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
    size_t mid = sz / 2;

    new_right->right = node->right;
    new_right->high_key = node->high_key;

    node->right = other;
    node->high_key = node->key_buff[mid];

    
    if (btnodeFlag(node, LEAF_NODE)) {
        new_right->key_buff_size = sz - mid;
        new_right->buff_size = sz - mid;

        node->key_buff_size = mid;
        node->buff_size = mid;

        memmove(new_right->key_buff, node->key_buff + mid, (sz - mid) * sizeof(Key));
        memmove(new_right->value_buff, node->value_buff + mid, (sz - mid) * sizeof(Value));
    } 
    else {
        new_right->key_buff_size = sz - mid - 1;
        new_right->buff_size = sz - mid;

        node->key_buff_size = mid;
        node->buff_size = mid + 1;

        memmove(new_right->key_buff, node->key_buff + mid + 1, (sz - mid - 1) * sizeof(Key));
        memmove(new_right->node_buff, node->node_buff + mid + 1, sizeof(sz - mid) * sizeof(Offset));
    }

    return new_right;
}

void btnodeAddKeyChild(BTNode* parent, Key cur_high_key, Offset new_righthand) {
    size_t ind = fgoe(parent, cur_high_key);
    size_t sz = parent->key_buff_size;

    memmove(parent->key_buff + ind + 1, parent->key_buff + ind, (sz - ind) * sizeof(Key));
    parent->key_buff_size++;
    parent->key_buff[ind] = cur_high_key;

    memmove(parent->node_buff + ind + 2, parent->node_buff + ind + 1, (sz - ind) * sizeof(Key));
    parent->buff_size++;
    parent->node_buff[ind + 1] = new_righthand;
}