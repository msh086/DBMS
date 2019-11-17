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
        int posInParent = -1;

        // check if data is still in buffer
        void checkBuffer(){
            data = bpm->reusePage(fid, page, bufIdx, data);
        }
    public:
        uchar type = 0; // 0 for Internal node, 1 for Leaf node
        uint size = 0; // number of keys in the node
        uint ptrNum = 0; // number of ptrs in the node

        // In the type byte of the page, 0 also represents internal and 1 represents leaf
        const static uchar Internal = 0, Leaf = 1;
        const static uchar Any = 0, Eq = 1, Gt = 2, GE = 3, Lt = 4, LE = 5;
        // Low level APIs

        // update node size with bpm
        void updateSize(){
            checkBuffer();
            *(uint*)(data + 1) = size;
            bpm->markDirty(bufIdx);
        }

        // Return the key at pos, only for internal nodes
        uchar* KeyAt(int pos);
        // Return the node pointer at pos, only for internal nodes
        uint* NodePtrAt(int pos);
        // Return the node pointer to next leaf node, only for leaf nodes
        uint* NextLeafPtr();
        // Return the node pointer to previous leaf node, only for leaf nodes
        uint* PrevLeafPtr();
        // Return the key and data pointer at pos, only for leaf nodes
        uchar* KeynPtrAt(int pos);
        /**
         * Find the first element > data in an internal node
         * @return The index of the found element, it will be 'BplusTreeNode.size' if data is the largest one
        */
        int findFirstGreaterInInternal(const uchar* data);
        /**
         * Find the first element > data in a leaf node
         * @return the index of the found element, it will be 'BplusTreeNode.size' if data is the largest one
        */
        int findFirstEqGreaterInLeaf(const uchar* data);

        // Slightly higher level APIs

        // Insert a key at pos at pos, only for internal nodes
        void InsertKeyAt(int pos, const uchar* element);
        // Insert a node pointer at pos, only for internal nodes
        void InsertNodePtrAt(int pos, uint pageID);
        // Insert a key and related data pointer at pos, only for leaf nodes
        void InsertKeynPtrAt(int pos, const uchar* element, const RID& rid);

        void MoveNext();
        friend class BplusTree;
};

BufPageManager* BplusTreeNode::bpm = BufPageManager::Instance();
#endif // BPLUSTREENODE_H