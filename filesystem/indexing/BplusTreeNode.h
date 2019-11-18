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
        // for memory reuse

        static BufPageManager* bpm;
        int fid;
        int bufIdx;
        
        // runtime variables

        BplusTree* tree = nullptr;
        BplusTreeNode* parent;
        uint page;

        // data stored physically

        uint parentPage = 0; // TODO 4 bytes
        uchar* data; // 8192B of data
        ushort posInParent = -1; // TODO 2 bytes 

        // check if data is still in buffer
        void checkBuffer(){
            data = bpm->reusePage(fid, page, bufIdx, data);
        }

        // whether this node has already been released
        bool isActive = true;

        // write back dirty node to storage
        void writeBack(){
            if(!isActive)
                return;
            isActive = false;
            int realFid, realPid;
            bpm->getKey(bufIdx, realFid, realPid);
            if(realFid == fid && realFid == page)
                bpm->writeBack(bufIdx);
            // otherwise, the page is already cleared from bpm
        }

    public:
        uchar type = 0; // 0 for Internal node, 1 for Leaf node
        ushort size = 0; // number of keys in the node // TODO 2 bytes

        // runtime only
        uint ptrNum = 0; // number of ptrs in the node

        // the bytes reserved for node info at the beginning
        // which includes: 1 byte of type, 2 byte of size, 4 byte of parent page, 2 byte of pos in parent page. 9 bytes in total
        const static int reservedBytes = 9;
        // In the type byte of the page, 0 also represents internal and 1 represents leaf
        const static uchar Internal = 0, Leaf = 1;
        const static uchar Any = 0, Eq = 1, Gt = 2, GE = 3, Lt = 4, LE = 5;
        // Low level APIs

        // update node size with bpm
        void updateSize(){
            checkBuffer();
            *(ushort*)(data + 1) = size;
            bpm->markDirty(bufIdx);
        }

        // sync the header of a tree node with bpm
        void syncWithBuffer(){
            checkBuffer();
            *(uchar*)data = type;
            *(ushort*)(data + 1) = size;
            *(uint*)(data + 3) = parentPage;
            *(ushort*)(data + 7) = posInParent;
            bpm->markDirty(bufIdx);
        }

        void MarkDirty(){
            checkBuffer();
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

        // insertions

        // Insert a key at pos, only for internal nodes
        void InsertKeyAt(int pos, const uchar* element);
        // Insert a node pointer at pos, only for internal nodes
        void InsertNodePtrAt(int pos, uint pageID);
        // Insert a key and related data pointer at pos, only for leaf nodes
        void InsertKeynPtrAt(int pos, const uchar* element, const RID& rid);

        // removals

        // Remove a key at pos, only for internal nodes
        void RemoveKeyAt(int pos);
        // Remove a node pointer at pos, only for internal nodes
        void RemoveNodePtrAt(int pos);
        // Remove a key and related data pointer at pos, only for leaf nodes
        void RemoveKeynPtrAt(int pos);

        friend class BplusTree;
};

BufPageManager* BplusTreeNode::bpm = BufPageManager::Instance();
#endif // BPLUSTREENODE_H