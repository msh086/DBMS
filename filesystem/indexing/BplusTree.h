#ifndef BPLUSTREE_H
#define BPLUSTREE_H
#include "BplusTreeNode.h"
#include "IndexHeader.h"
#include "../RM/RID.h"
#include "../MyDB/Table.h"
/**
 * In a B+ tree of internal order a and leaf order b
 * 1B is used to store the node type, 4B is used to store node size
 * For an internal node, there can be at most:
 *     - a node pointers
 *     - a-1 keys
 * They are stored in such order:
 *     [ keys.. ] [ node pointers.. ]
 * 
 * For a leaf node, there can be at most:
 *     - a data pointers
 *     - a keys
 *     - 1 node pointer(points to the next leaf node)
 * They are stored in such order:
 *     [ (key, RID)... ] node pointer
*/
class BplusTree{
        // if the record is found, store the RID of the record into rid, return true
        // otherwise, store the pageID and pos of the first record that is greater of equal to the input record into page and pos, return false
        bool _search(const uchar* data, RID* rid, int& page, int& pos);
        int fid;
        int page;
        int headerIdx;
        uchar* data = nullptr;
        static BufPageManager* bpm;
        Table* table; // creating & deleting nodes needs to be done through table
    public:
        BplusTree(Table* table, int pageID){
            this->table = table;
            this->fid = table->FileID();
            page = pageID;
            data = (uchar*)bpm->getPage(fid, page, headerIdx);
            header = new IndexHeader();
            header->FromString(data);
            if(header->rootPage == 0){
                table->InsertRecord()
            }
        }

        void UpdateRecordNum(){
            data = bpm->reusePage(fid, page,headerIdx, data);
            ((uint*)data)[1] = header->recordNum;
            bpm->markDirty(headerIdx);
        }
        IndexHeader* header = nullptr;
        BplusTreeNode* root = nullptr;
        void Insert(const uchar* data, const RID& rid);
        bool Search(const uchar* data, const RID& rid);
        void Remove(const uchar* data, const RID& rid);
    
};
BufPageManager* BplusTree::bpm = BufPageManager::Instance();

// Low level APIs
uchar* BplusTreeNode::KeyAt(int pos){
    checkBuffer();
    return data + 5 + pos * tree->header->recordLenth;
}

uchar* BplusTreeNode::NodePtrAt(int pos){
    checkBuffer();
    return data + 5 + (tree->header->internalOrder - 1) + pos * 4;
}

uchar* BplusTreeNode::LeafPtr(){
    checkBuffer();
    return data + 5 + tree->header->leafCap * (tree->header->recordLenth + 8);
}

uchar* BplusTreeNode::KeyData(int pos){
    checkBuffer();
    return data + 5 + pos * (tree->header->recordLenth + 8);
}



// void BplusTreeNode::InsertAt(int pos, const uchar* element, const RID& rid){
//     checkBuffer();
//     if(type == Internal && size == tree->header->internalCap || type == Leaf && size == tree->header->leafCap){
//             printf("In BplusTreeNode::InsertAt, node is full\n");
//             return;
//         }
//     if(pos > size || pos < 0){
//         printf("In BplusTreeNode::InsertAt, index out of bound\n");
//         return;
//     }
//     if(type == Internal)
//     memmove(data + (pos + 1) * tree->header->recordLenth, data + pos * tree->header->recordLenth, (size - pos) * tree->header->recordLenth);
//     memcpy(data + pos * tree->header->recordLenth, element, tree->header->recordLenth);
//     bpm->markDirty(bufIdx);
//     if(type == Leaf){
//         tree->header->recordNum++;
//         tree->UpdateRecordNum();
//     }
// }

// void BplusTreeNode::RemoveAt(int pos){
//     checkBuffer();
//     if(type == Internal &&)
// }

#endif // BPLUSTREE_H