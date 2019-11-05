#ifndef BPLUSTREENODE_H
#define BPLUSTREENODE_H
#include "../utils/pagedef.h"
class BplusTree;
class BplusTreeNode{
    private:
        BplusTree* tree = nullptr;
    public:
        uchar type = 0; // 0 for internal node, 1 for leaf node
        uint size = 0; // number of elements in the node
        uchar* data;
        
        void InsertAt(int pos, uchar* element);
        void RemoveAt(int pos);
        uchar* EleAt(int pos);

        friend class BplusTree;
};

#endif // BPLUSTREENODE_H