#ifndef BPLUSTREE_H
#define BPLUSTREE_H
#include "BplusTreeNode.h"
#include "IndexHeader.h"
#include "../RM/RID.h"

class BplusTree{
        // if the record is found, store the RID of the record into rid, return true
        // otherwise, store the pageID and pos of the first record that is greater of equal to the input record into page and pos, return false
        bool _search(const uchar* data, RID* rid, int& page, int& pos);
    public:
        IndexHeader* header = nullptr;
        BplusTreeNode* root = nullptr;
        void Insert(const uchar* data);
        bool Search(const uchar* data, RID* rid);
        void Remove(const uchar* data, const RID& rid);
};

#endif // BPLUSTREE_H