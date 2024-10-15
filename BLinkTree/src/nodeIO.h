#ifndef NODEIO_H
#define NODEIO_H

extern "C" {
    #include "node.h"
}
#include <string>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <iostream>
#include <mutex>
#include <shared_mutex>
#include <memory>

#ifndef offset_of
#define offset_of(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

#ifndef container_of
/**
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 *
 */
#define container_of(ptr, type, member) ({			\
	const typeof(((type *)0)->member) * __mptr = (ptr);	\
	(type *)((char *)__mptr - offsetof(type, member)); })
#endif

#define member_val(type, buf, offset) \
    *((type*)((buff) + (offset)))


// read write operations on service file
// every operation is tread-unsafe so in cuncurrent programs `offset` mutexes must be locked 
class NodeIO {
        int fd;
        std::string fname;

        static std::shared_mutex mut_root;
        static std::mutex mut_free_space;
    public:
        // usage of default construct is UB
        NodeIO() = default;
        NodeIO(const std::string& fname);
        NodeIO(const NodeIO&);
        NodeIO& operator= (const NodeIO&);

        NodeIO(NodeIO&&) = delete;
        
        ~NodeIO();

        // read node to memory by its offset in service file
        BTNode* readNode(Offset offset);

        // update existing node in service file
        // if `node->self` is spoiled then using any other IO operation after "bad" `writeNide` is UB  
        void writeNode(BTNode* node);

        // add space for new node in service file. 
        // do not write it, so write must be call mannualy on this node. 
        Offset addNode();

        static void createEmptyServiceFile(std::string& fname);
        
        /// @brief get root offset from service file. Thread-safe 
        Offset getRootOffset();

    private:
        /// @brief update root offset in service file. Thread-safe
        /// @param new_root offset if new root node
        void setRootOffset(Offset new_root);        
};

#endif // NODEIO_H