#include "tree.h"
#include <gtest/gtest.h>

TEST(NodeIO, NodeConstruction) {
    std::string fname = "serviceF";
    NodeIO::createEmptyServiceFile(fname);
    auto node_io = NodeIO(fname);
    auto root_offset = node_io.getRootOffset();
    EXPECT_EQ(root_offset, 100u) << "Bad creation of service file: false root offset";
    auto root = node_io.readNode(root_offset);
    EXPECT_EQ(root->self, root_offset) << "Bad creation of service file: false root read0";
    EXPECT_EQ(root->flag, LEAF_NODE | ROOT_NODE) << "Bad creation of service file: false root read1";
    EXPECT_EQ(root->right, INVALID_OFFSET) << "Bad creation of service file: false root read2";
    EXPECT_EQ(root->key_buff_size, 0) << "Bad creation of service file: false root read3";
    EXPECT_EQ(root->buff_size, 0) << "Bad creation of service file: false root read4";
    EXPECT_EQ(root->high_key, 0) << "Bad creation of service file: false root read5";

    free(root);
}

TEST(BTNode, AddKeyValue) {
    std::string fname = "serviceF";
    NodeIO::createEmptyServiceFile(fname);

    auto node_io = NodeIO(fname);
    auto root_offset = node_io.getRootOffset();
    EXPECT_EQ(root_offset, 100u) << "Bad creation of service file: false root offset";

    auto root = node_io.readNode(root_offset);
    EXPECT_EQ(root->flag, ROOT_NODE | LEAF_NODE)  << "Bad node state";

    Value v; 
    strncpy(v.data, "abvgd", 6);

    EXPECT_EQ(root->key_buff_size, 0) << "Bad (key, value) pair insertion0";

    btnodeAddKeyValue(root, 4, &v);

    EXPECT_EQ(root->key_buff_size, 1) << "Bad (key, value) pair insertion0";
    EXPECT_EQ(root->high_key, 0)  << "Bad (key, value) pair insertion1";
    EXPECT_EQ(root->buff_size, 1)  << "Bad (key, value) pair insertion2";
    EXPECT_EQ(root->flag, LEAF_NODE | ROOT_NODE);
    EXPECT_EQ(root->right, INVALID_OFFSET)  << "Bad (key, value) pair insertion3";
    EXPECT_EQ(root->buff_size, 1)  << "Bad (key, value) pair insertion4";
    EXPECT_STREQ(root->value_buff[0].data, v.data) << "Bad (key, value) pair insertion5";
    EXPECT_EQ(root->key_buff[0], 4) << "Bad (key, value) pair insertion6";

    btnodeAddKeyValue(root, 7, &v);

    EXPECT_EQ(root->key_buff_size, 2);
    EXPECT_EQ(root->buff_size, 2);
    EXPECT_EQ(root->flag, LEAF_NODE | ROOT_NODE);
    EXPECT_EQ(root->high_key, 0);
    EXPECT_EQ(root->right, INVALID_OFFSET);
    EXPECT_EQ(root->key_buff[1], 7);
    EXPECT_STREQ(root->value_buff[1].data, v.data);

    Value v1; strncpy(v1.data, "kaif", 5);
    Value v2; strncpy(v2.data, "bred", 5);

    btnodeAddKeyValue(root, 6, &v1);

    EXPECT_EQ(root->key_buff_size, 3);
    EXPECT_EQ(root->buff_size, 3);
    EXPECT_EQ(root->flag, LEAF_NODE | ROOT_NODE);
    EXPECT_EQ(root->high_key, 0);
    EXPECT_EQ(root->right, INVALID_OFFSET);
    EXPECT_EQ(root->key_buff[1], 6);
    EXPECT_STREQ(root->value_buff[1].data, v1.data);

    btnodeAddKeyValue(root, 5, &v2);
    EXPECT_EQ(root->key_buff_size, 4);
    EXPECT_EQ(root->buff_size, 4);
    EXPECT_EQ(root->flag, LEAF_NODE | ROOT_NODE);
    EXPECT_EQ(root->high_key, 0);
    EXPECT_EQ(root->right, INVALID_OFFSET);
    EXPECT_EQ(root->key_buff[1], 5);
    EXPECT_EQ(root->key_buff[2], 6);
    EXPECT_EQ(root->key_buff[3], 7);
    EXPECT_STREQ(root->value_buff[1].data, v2.data);

    btnodeAddKeyValue(root, 4, &v2);

    EXPECT_STREQ(root->value_buff[0].data, v2.data);

    free(root);
}

TEST(BTNode, Split) {
    std::string fname = "serviceF";
    NodeIO::createEmptyServiceFile(fname);

    auto node_io = NodeIO(fname);
    auto root_offset = node_io.getRootOffset();
    EXPECT_EQ(root_offset, 100u) << "Bad creation of service file: false root offset";

    auto root = node_io.readNode(root_offset);
    EXPECT_EQ(root->flag, ROOT_NODE | LEAF_NODE)  << "Bad node state";

    Value v; 
    strncpy(v.data, "abvgd", 6);

    for (int i = 1; root->key_buff_size < 2 * L; i++) {
        btnodeAddKeyValue(root, i * 10, &v);
    }

    EXPECT_EQ(root->key_buff_size, 2 * L);
    EXPECT_EQ(root->flag, ROOT_NODE | LEAF_NODE);
    EXPECT_EQ(root->right, INVALID_OFFSET);
    EXPECT_EQ(root->buff_size, 2 * L);
    EXPECT_EQ(root->high_key, 0);

    for (int i = 1; i < 2 * L; i++) {
        EXPECT_EQ(root->key_buff[i - 1], i * 10);
        EXPECT_STREQ(root->value_buff[i - 1].data, v.data);
    }

    BTNode* right = btnodeSplit(root, root->self + NODE_SIZE);

    EXPECT_EQ(right->key_buff_size, L);
    EXPECT_EQ(right->flag, LEAF_NODE);
    EXPECT_EQ(right->right, INVALID_OFFSET);
    EXPECT_EQ(right->buff_size, L);
    EXPECT_EQ(right->high_key, 0);

    EXPECT_EQ(root->key_buff_size, L);
    EXPECT_EQ(root->flag, LEAF_NODE);
    EXPECT_EQ(root->right, right->self);
    EXPECT_EQ(root->buff_size, L);
    EXPECT_EQ(root->high_key, right->key_buff[0]);

    for (int i = 0; i < L; i++) {
        EXPECT_EQ(root->key_buff[i], (i + 1) * 10);
        EXPECT_EQ(right->key_buff[i], (i + 1 + L) * 10);
    }

    free(root);
    free(right);
}

TEST(NodeIO, NodeAdd) {
    std::string fname = "serviceF";
    NodeIO::createEmptyServiceFile(fname);

    auto node_io = NodeIO(fname);
    auto root_offset = node_io.getRootOffset();
    EXPECT_EQ(root_offset, 100u) << "Bad creation of service file: false root offset";

    auto root = node_io.readNode(root_offset);
    EXPECT_EQ(root->flag, ROOT_NODE | LEAF_NODE)  << "Bad node state";

    Value v; 
    strncpy(v.data, "abvgd", 6);

    for (int i = 1; root->key_buff_size < 2 * L; i++) {
        btnodeAddKeyValue(root, i * 10, &v);
    }

    auto new_place = node_io.addNode();
    EXPECT_EQ(new_place, root->self + NODE_SIZE);

    BTNode* right = btnodeSplit(root, new_place);
    node_io.writeNode(root);
    node_io.writeNode(right);

    free(root);
    free(right);

    auto left = node_io.readNode(root_offset);
    right = node_io.readNode(new_place);

    EXPECT_EQ(left->flag, LEAF_NODE);
    EXPECT_EQ(left->high_key, right->key_buff[0]);
    EXPECT_EQ(left->right, right->self);
    EXPECT_EQ(left->buff_size, L);
    EXPECT_EQ(left->key_buff_size, L);

    EXPECT_EQ(right->flag, LEAF_NODE);
    EXPECT_EQ(right->high_key, 0);
    EXPECT_EQ(right->right, INVALID_OFFSET);
    EXPECT_EQ(right->buff_size, L);
    EXPECT_EQ(right->key_buff_size, L);

    free(left);
    free(right);
}


TEST(BLinkSingleThread, Read) {

}

TEST(BLinkSingleThread, Write) {

}