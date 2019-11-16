#ifndef BPLUSTREENODE_H
#define BPLUSTREENODE_H
#include "../utils/pagedef.h"
#include "../bufmanager/BufPageManager.h"
#include "../RM/RID.h"
class BplusTree;

/**
 * Provides only basic operations like insert and delete
 * Its functions are memory-safe, but not logic safe. e.g. the B+ tree constraints
 * High-level logics are implements in BplusTree
*/
class BplusTreeNode{
    private:
        static BufPageManager* bpm;
        BplusTree* tree = nullptr;
        int fid;
        int page;
        int bufIdx;
        uchar* data; // 8192B of data
        BplusTreeNode* parent;

        // check if data is still in buffer
        void checkBuffer(){
            data = bpm->reusePage(fid, page, bufIdx, data);
        }
    public:
        uchar type = 0; // 0 for Internal node, 1 for Leaf node
        uint size = 0; // number of elements in the node

        // In the type byte of the page, 0 also represents internal and 1 represents leaf
        const static uchar Internal = 0, Leaf = 1;
        // Low level APIs

        // Return the key at pos, only for internal nodes
        uchar* KeyAt(int pos);
        // Return the node pointer at pos, only for internal nodes
        uchar* NodePtrAt(int pos);
        // Return the node pointer to next leaf node, only for leaf nodes
        uchar* LeafPtr();
        // Return the key and data pointer at pos, only for leaf nodes
        uchar* KeyData(int pos);
        /**
         * Find the first element > data.
         * @return The index of the found element, it will be 'BplusTreeNode.size' if data is the largest one
        */
        int findFirstGreater(const uchar* data);

        // Slightly higher level APIs

        // Insert a key at pos at pos, only for internal nodes
        void InsertKeyAt(int pos, const uchar* element);
        // Insert a node pointer at pos, only for internal nodes
        void InsertNodePtrAt(int pos, uint pageID);
        // Insert a data pointer at pos, only for leaf nodes
        void InsertDataPtrAt(int pos, uchar* element, const RID& rid);
        friend class BplusTree;
};

BufPageManager* BplusTreeNode::bpm = BufPageManager::Instance();
#endif // BPLUSTREENODE_H