#ifndef BPLUSTREE_H
#define BPLUSTREE_H
#include "BplusTreeNode.h"
#include "IndexHeader.h"
#include "../RM/RID.h"
#include "../MyDB/Table.h"
#include <vector>
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
        static BufPageManager* bpm; // the only usage for bpm is to reuse buffers
        Table* table; // the table that stores all B+ tree nodes, creating & deleting nodes needs to be done through table
        std::vector<BplusTreeNode*> nodes; // stores all opened tree nodes, for memory management

        void CalcColNum(){
            colNum = 0;
            for(int i = 0; i < MAX_COL_NUM; i++)
                if(header->attrType[i] != DataType::NONE)
                    colNum++;
                else
                    break;
        }


        // load an existing treenode from storage into memory
        BplusTreeNode* GetTreeNode(BplusTreeNode* curNode, int page){
            BplusTreeNode* node = new BplusTreeNode();
            node->tree = this;
            node->fid = fid;
            node->page = page;
            node->data = (uchar*)bpm->getPage(fid, page,node->bufIdx);
            node->type = node->data[0] == 0 ? BplusTreeNode::Internal : BplusTreeNode::Leaf; //* type byte: 0 for internal, 1 for leaf
            node->size = *(uint*)(node->data + 1);
            node->parent = curNode;
            nodes.push_back(node);
        }
        // create a treenode
        BplusTreeNode* CreateTreeNode(BplusTreeNode* curNode, int nodeType){
            BplusTreeNode* node = new BplusTreeNode();
            node->tree = this;
            node->fid = fid;
            uchar* tmp = new uchar[PAGE_SIZE]{0};
            tmp[0] = nodeType;
            *(uint*)(tmp + 1) = 0; // size = 0
            RID* rid = new RID();
            table->InsertRecord(tmp, rid);
            delete[] tmp;
            node->page = rid->GetPageNum();
            delete rid;
            node->data = (uchar*)bpm->getPage(fid, node->page, node->bufIdx);
            node->type = nodeType;
            node->size = 0;
            node->parent = curNode;
            nodes.push_back(node);
        }

    public:
        // create a B+ tree
        BplusTree(Table* table, IndexHeader* header){
            this->table = table;
            this->fid = table->FileID();

            header->internalCap = (PAGE_SIZE - 5 + header->recordLenth) / (header->recordLenth + 4);
            header->leafCap = (PAGE_SIZE - 9) / (header->recordLenth + 8);

            CalcColNum();

            this->header = header;
            uchar* tmp = new uchar[PAGE_SIZE]{0};
            header->ToString(tmp);
            RID* rid = new RID();
            table->InsertRecord(tmp, rid);
            page = rid->GetPageNum();
            delete[] tmp;
            data = (uchar*)bpm->getPage(fid, page, headerIdx);
            // TODO: root node
        }
        // load an existing B+ tree
        BplusTree(Table* table, int pageID){
            this->table = table;
            this->fid = table->FileID();
            page = pageID;
            data = (uchar*)bpm->getPage(fid, page, headerIdx);
            header = new IndexHeader();
            header->FromString(data);
            CalcColNum();
        }

        void UpdateRecordNum(){
            data = bpm->reusePage(fid, page,headerIdx, data);
            ((uint*)data)[1] = header->recordNum;
            bpm->markDirty(headerIdx);
        }
        IndexHeader* header = nullptr;
        BplusTreeNode* root = nullptr;
        int colNum = 0;
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
    return data + 5 + (tree->header->internalCap - 1) * tree->header->recordLenth + pos * 4;
}

uchar* BplusTreeNode::LeafPtr(){
    checkBuffer();
    return data + 5 + tree->header->leafCap * (tree->header->recordLenth + 8);
}

uchar* BplusTreeNode::KeyData(int pos){
    checkBuffer();
    return data + 5 + pos * (tree->header->recordLenth + 8);
}

int BplusTreeNode::findFirstGreater(const uchar* data){
    for(int i = 0; i < size; i++){
        checkBuffer();
        uchar* cmpData = this->data;
        // TODO: binary search maybe ?
        // find the first element > data
        if(DataType::greaterThanArr(cmpData, data, tree->header->attrType, tree->header->attrLenth, tree->header->nullMask, tree->colNum)){
            return i;
        }
    }
    return size;
}

bool BplusTree::_search(const uchar* data, RID* rid, int& page, int& pos){
    BplusTreeNode* cur = root;
    while(cur->type == BplusTreeNode::Internal){
        // a leaf node may be under-sized, but an internal node will never be
        int pos = cur->findFirstGreater(data); // pos is the exact subtree id to look up in
        // TODO
    }
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