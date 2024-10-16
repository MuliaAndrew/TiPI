#include "tree.h"
#include <gtest/gtest.h>

// Тесты проверены для L=64 и L=16

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

TEST(BTNode, Search) {
    std::string fname = "serviceF";
    NodeIO::createEmptyServiceFile(fname);

    auto node_io = NodeIO(fname);
    auto root_offset = node_io.getRootOffset();
    EXPECT_EQ(root_offset, 100u) << "Bad creation of service file: false root offset";

    auto root = node_io.readNode(root_offset);
    EXPECT_EQ(root->flag, ROOT_NODE | LEAF_NODE)  << "Bad node state";

    Value v1, v2, v3; 
    strncpy(v1.data, "abvgd", 6);

    for (int i = 1; root->key_buff_size < 5; i++) {
        btnodeAddKeyValue(root, i * 12 - 11, &v1);
    }
    for (int i = 1; root->key_buff_size < 10; i++) {
        btnodeAddKeyValue(root, 1000 - i * 10, &v2);
    }
    for (int i = 1; root->key_buff_size < 15; i++) {
        btnodeAddKeyValue(root, i * 6 + 100, &v3);
    }

    Value ret1 = btnodeValueByKey(root, 49);
    Value ret2 = btnodeValueByKey(root, 950);
    Value ret3 = btnodeValueByKey(root, 106);
    Value false_ret = btnodeValueByKey(root, 2);

    EXPECT_STREQ(v1.data, ret1.data);
    EXPECT_STREQ(v2.data, ret2.data);
    EXPECT_STREQ(v3.data, ret3.data);
    EXPECT_STRNE(v1.data, false_ret.data);
    EXPECT_STRNE(v2.data, false_ret.data);
    EXPECT_STRNE(v3.data, false_ret.data);

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

TEST(BLinkSingleThread, SimpleRead) {
     std::string fname = "serviceF";
    NodeIO::createEmptyServiceFile(fname);

    BLinkTree tree{fname};

    Value v[3];
    strcpy(v[0].data, "aaa");
    strcpy(v[1].data, "bbb");
    strcpy(v[2].data, "ccc");

    auto node_io = NodeIO(fname);
    auto root_offset = node_io.getRootOffset();
    EXPECT_EQ(root_offset, 100u) << "Bad creation of service file: false root offset";

    for (int i = 1; i < 2 * L; i++) {
        tree.write(i, &v[i % 3]);
    }
    auto root = node_io.readNode(root_offset);
    EXPECT_EQ(root->buff_size, 2 * L - 1);
    free(root);

    auto v1 = tree.read(30);
    auto v2 = tree.read(5);
    auto v3 = tree.read(1);
    auto v_bad1 = tree.read(0);
    auto v_bad2 = tree.read(222);

    EXPECT_STREQ(v1.data, "aaa");
    EXPECT_STREQ(v2.data, "ccc");
    EXPECT_STREQ(v3.data, "bbb");
    EXPECT_STREQ(v_bad1.data, "");
    EXPECT_STREQ(v_bad2.data, "");
}
// Проверяем, умеем ли мы добавлять, сплититься, менять корень
TEST(BLinkSingleThread, Write) {
    std::string fname = "serviceF";
    NodeIO::createEmptyServiceFile(fname);

    BLinkTree tree{fname};

    Value v1, v2, v3;
    strcpy(v1.data, "aaaaaa");
    strcpy(v2.data, "bbbbbb");
    strcpy(v3.data, "cccccc");
    
    tree.write(1, &v1);
    tree.write(2, &v2);
    tree.write(3, &v3);

    auto node_io = NodeIO(fname);
    auto root_offset = node_io.getRootOffset();
    EXPECT_EQ(root_offset, 100u) << "Bad creation of service file: false root offset";

    auto root = node_io.readNode(root_offset);
    EXPECT_EQ(root->flag, ROOT_NODE | LEAF_NODE)  << "Bad node state";
    EXPECT_EQ(root->buff_size, 3);
    EXPECT_EQ(root->key_buff_size, 3);
    EXPECT_EQ(root->right, INVALID_OFFSET);
    EXPECT_EQ(root->self, 100u);
    EXPECT_EQ(root->key_buff[0], 1);
    EXPECT_EQ(root->key_buff[1], 2);
    EXPECT_EQ(root->key_buff[2], 3);

    for (int i = 4; i <= 2 * L; i++) {
        tree.write(i, &v3);
    }
    free(root);
    root = node_io.readNode(root_offset);
    EXPECT_EQ(root->buff_size, 2 * L);
    free(root);
    // Сплит ноды. сплит рута, новый рут.
    tree.write(129, &v1);

    root_offset = node_io.getRootOffset();
    EXPECT_EQ(root_offset, 100u + 2 * NODE_SIZE) << "Bad new root offset\n";
    root = node_io.readNode(root_offset);

    EXPECT_EQ(root->key_buff_size, 1);
    EXPECT_EQ(root->buff_size, 2);
    EXPECT_EQ(root->node_buff[0], 100ul);
    EXPECT_EQ(root->node_buff[1], 100ul + NODE_SIZE);

    free(root);
}

TEST(BLinkSingleThread, Read) {
    // делаем дерево высоты 3
    std::string fname = "serviceF";
    NodeIO::createEmptyServiceFile(fname);

    BLinkTree tree{fname};

    Value v1, v2, v3;
    strcpy(v1.data, "aaaaaa");
    strcpy(v2.data, "bbbbbb");
    strcpy(v3.data, "cccccc");

    for (int i = 0; i < (2 * L + 1) * (2 * L); i++) {
        tree.write(i, &v1);
    }

    NodeIO io{fname};
    // Проверим что рут менялся
    auto root_offset = io.getRootOffset();
    EXPECT_NE(root_offset, 100ul);

    // Проверим высоту дерева и тот факт что крайняя левая нижняя нода лежит левее всех
    auto root = io.readNode(root_offset);
    auto tmp1_offset = root->node_buff[0];
    free(root);

    auto tmp1 = io.readNode(tmp1_offset);
    EXPECT_FALSE(btnodeFlag(tmp1, LEAF_NODE));
    EXPECT_FALSE(btnodeFlag(tmp1, ROOT_NODE));

    auto tmp2_offset = tmp1->node_buff[0];
    auto tmp2 = io.readNode(tmp2_offset);
    free(tmp1);

    EXPECT_TRUE(btnodeFlag(tmp2, LEAF_NODE));
    EXPECT_FALSE(btnodeFlag(tmp2, ROOT_NODE));
    EXPECT_EQ(tmp2_offset, 100ul);
    free(tmp2);
}

TEST(GlobalData, Free) {
    for (auto lock : BLinkTree::lockmap) {
        pthread_rwlock_destroy(lock.second);
        free(lock.second);
    }
}