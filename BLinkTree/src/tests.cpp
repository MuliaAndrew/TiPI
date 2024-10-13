#include "nodeIO.h"
#include <gtest/gtest.h>

TEST(NodeIO, NodeConstruction) {
    std::string fname = "fs0";
    NodeIO::createEmptyServiceFile(fname);
    auto node_io = NodeIO(fname);
    auto root_offset = node_io.getRootOffset();
    EXPECT_EQ(root_offset, 100u) << "Bad creation of service file: false root offset";
    auto root = node_io.readNode(root_offset);
    EXPECT_EQ(root->self, root_offset) << "Bad creation of service file: false root read0";
    EXPECT_EQ(root->flag, LEAF_NODE || ROOT_NODE) << "Bad creation of service file: false root read1";
    EXPECT_EQ(root->right, INVALID_OFFSET) << "Bad creation of service file: false root read2";
    EXPECT_EQ(root->key_buff_size, 0) << "Bad creation of service file: false root read3";
    EXPECT_EQ(root->buff_size, 0) << "Bad creation of service file: false root read4";
    EXPECT_EQ(root->high_key, 0) << "Bad creation of service file: false root read5";
}

TEST(NodeIO, NodeRead) {

}

TEST(NodeIO, NodeWrite) {

}