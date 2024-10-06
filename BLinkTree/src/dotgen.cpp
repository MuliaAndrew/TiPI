extern "C" {
    #include "tree.h"
}
#include <deque>
#include <fstream>

static std::ofstream dot_tmp;

void BLinkTreeDot(BLinkTree* tree, char* fname) {
    if (tree == nullptr)
        return;

    std::string fname_(fname);
    dot_tmp.open(fname_ + ".dot", std::ios::trunc);

    if (!dot_tmp.good()) return;

    dot_tmp << "digraph \"\" { fontname=\"Helvetica,Arial,sans-serif\"\n"
    << "node [fontname=\"Helvetica,Arial,sans-serif\", shape=\"record\"]\n"
    << "edge [fontname=\"Helvetica,Arial,sans-serif\"]\n\n";

    gen_dot(tree);

    dot_tmp << "}\n";
    dot_tmp.close();

    // std::string cmd_dot = "dot -Tpdf " + fname_ + ".dot > " + fname_ + ".pdf";
    // system(cmd_dot.c_str());

    // std::string cmd_rm = "rm " + fname_ + ".dot";
    // system(cmd_rm.c_str());
    return;
}

void gen_edge(BTNode* from, BTNode* to);

void gen_node(BTNode* node);

void gen_dot(BLinkTree* tree) {
    std::deque<BTNode*> queue;
    queue.push_back(tree->root);

    gen_node(tree->root);

    while (!queue.empty()) {
        auto from = queue.front();
        queue.pop_front();
        auto sz = from->buff_size;
        if (!btnodeFlag(from, LEAF_NODE)) {
            for (int i = 0; i < sz; i++) {
                auto to = from->node_buff[i];
                if (to != NULL) {
                    queue.push_back(to);
                    gen_node(to);
                    gen_edge(from, to);
                }
            }
        }
    }
}

void gen_node(BTNode* node) {

    dot_tmp << node << " [label=\"{" << node;

    for (int i = 0; i < node->key_buff_size; i++) {
        if (i == 0)
            dot_tmp << "|{";
        else 
            dot_tmp << "|";
        dot_tmp << node->key_buff[i];
    }
    if (!btnodeFlag(node, LEAF_NODE)) {
        dot_tmp << "}|{";
        for (int i = 0; i < node->buff_size; i++) {
            if (i != 0)
                dot_tmp << "|";
            dot_tmp << "<f" << node->node_buff[i] << ">" << node->node_buff[i];
        }
    }
    dot_tmp << "}}\"]\n";
}

void gen_edge(BTNode* from, BTNode* to) {
    dot_tmp << from << ":<f" << to << "> -> " << to;
}