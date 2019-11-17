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
        // find the first record whose key is >= 'data'
        // store its b+ tree node and pos in node into 'node' and 'pos'
        void _search(const uchar* data, BplusTreeNode*& node, int& pos);
        int fid;
        int page;
        int headerIdx;
        uchar* data = nullptr;
        static BufPageManager* bpm; // the only usage for bpm is to reuse buffers
        Table* table; // the table that stores all B+ tree nodes, creating & deleting nodes needs to be done through table
        std::vector<BplusTreeNode*> nodes; // stores all opened tree nodes except root, for memory management

        void CalcKeyLength(){
            header->recordLenth = 0;
            colNum = 0;
            for(int i = 0; i < MAX_COL_NUM; i++){
                if(header->attrType[i] != DataType::NONE){
                    header->recordLenth += DataType::lengthOf(header->attrType[i], header->attrLenth[i]);
                    if(header->nullMask & (128 >> i))
                        header->recordLenth++;
                    colNum++;
                }
                else
                    break;
            }
        }

        void CalcColNum(){
            colNum = 0;
            for(int i = 0; i < MAX_COL_NUM; i++)
                if(header->attrType[i] != DataType::NONE)
                    colNum++;
                else
                    break;
        }


        // load an existing treenode from storage into memory
        BplusTreeNode* GetTreeNode(BplusTreeNode* curNode, uint page){
            BplusTreeNode* node = new BplusTreeNode();
            node->tree = this;
            node->fid = fid;
            node->page = page;
            node->data = (uchar*)bpm->getPage(fid, page,node->bufIdx);
            node->type = node->data[0] == 0 ? BplusTreeNode::Internal : BplusTreeNode::Leaf; //* type byte: 0 for internal, 1 for leaf
            node->size = *(uint*)(node->data + 1);
            node->ptrNum = node->type == BplusTreeNode::Internal ? node->size + 1 : node->size;
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
            node->ptrNum = 0;
            node->parent = curNode;
            nodes.push_back(node);
        }

    public:
        // create a B+ tree from a header
        // you need to specify: attrType, attrLenth, nullMask, tableName, indexColID
        // those assigned in constructor: recordLenth, internalCap, leafCap, rootPage, recordNum, 
        BplusTree(Table* table, IndexHeader* header){
            this->table = table;
            this->fid = table->FileID();

            CalcKeyLength();
            header->recordNum = 0;
            header->internalCap = (PAGE_SIZE - 5 + header->recordLenth) / (header->recordLenth + 4);
            header->leafCap = (PAGE_SIZE - 9) / (header->recordLenth + 8);
            // root init
            root = CreateTreeNode(nullptr, BplusTreeNode::Leaf);
            nodes.pop_back(); // pop the root node from stack

            this->header = header;
            header->rootPage = root->page;
            uchar* tmp = new uchar[PAGE_SIZE]{0};
            header->ToString(tmp);
            RID* rid = new RID();
            table->InsertRecord(tmp, rid);
            page = rid->GetPageNum();
            delete[] tmp;
            delete rid;
            data = (uchar*)bpm->getPage(fid, page, headerIdx);
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
            data = bpm->reusePage(fid, page, headerIdx, data);
            ((uint*)data)[1] = header->recordNum;
            bpm->markDirty(headerIdx);
        }

        void UpdateRoot(){
            data = bpm->reusePage(fid, page, headerIdx, data);
            ((uint*)data)[5] = root->page;
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

uint* BplusTreeNode::NodePtrAt(int pos){
    checkBuffer();
    return (uint*)(data + 5 + (tree->header->internalCap - 1) * tree->header->recordLenth + pos * 4);
}

uint* BplusTreeNode::LeafPtr(){
    checkBuffer();
    return (uint*)(data + 5 + tree->header->leafCap * (tree->header->recordLenth + 8));
}

uchar* BplusTreeNode::KeynPtrAt(int pos){
    checkBuffer();
    return data + 5 + pos * (tree->header->recordLenth + 8);
}

int BplusTreeNode::findFirstGreaterInInternal(const uchar* data){
    checkBuffer();
    uchar* cmpData = this->data + 5;
    for(int i = 0; i < size; i++, cmpData += tree->header->recordLenth){
        // TODO: binary search maybe ?
        // find the first element > data
        if(DataType::greaterThanArr(cmpData, data, tree->header->attrType, tree->header->attrLenth, tree->header->nullMask, tree->colNum)){
            return i;
        }
    }
    return size;
}

int BplusTreeNode::findFirstEqGreaterInLeaf(const uchar* data){
    checkBuffer();
    uchar* cmpData = this->data + 5;
    for(int i = 0; i < size; i++, cmpData += tree->header->recordLenth + 8){
        if(DataType::noLessThanArr(cmpData, data, tree->header->attrType, tree->header->attrLenth, tree->header->nullMask, tree->colNum)){
            return i;
        }
    }
}

void BplusTree::_search(const uchar* data, BplusTreeNode*& node, int& pos){
    BplusTreeNode* cur = root;
    while(cur->type == BplusTreeNode::Internal){
        // a leaf node may be under-sized, but an internal node will never be
        int posG = cur->findFirstGreaterInInternal(data); // pos is the exact subtree id to look up in
        cur = GetTreeNode(cur, *cur->NodePtrAt(posG));
        cur->posInParent = posG;
    }
    // now at the leaf node
    node = cur;
    pos = cur->findFirstEqGreaterInLeaf(data);
}

void BplusTreeNode::InsertKeyAt(int pos, const uchar* element){
    if(pos > size){
        std::printf("trying to insert key beyond size\n");
        return;
    }
    checkBuffer();
    std::memmove(KeyAt(pos + 1), KeyAt(pos), (size - pos) * tree->header->recordLenth);
    std::memcpy(KeyAt(pos), element, tree->header->recordLenth);
    size++;
    updateSize();
}

void BplusTreeNode::InsertNodePtrAt(int pos, uint pageID){
    if(pos > ptrNum){
        std::printf("trying to insert ptr beyond ptrNum\n");
        return;
    }
    checkBuffer();
    std::memmove(NodePtrAt(pos + 1), NodePtrAt(pos), (ptrNum - pos) * 4);
    *NodePtrAt(pos) = pageID;
    bpm->markDirty(bufIdx);
    ptrNum++;
}

void BplusTreeNode::InsertKeynPtrAt(int pos, const uchar* element, const RID& rid){
    if(pos > size){
        std::printf("trying to insert key&ptr beyond size\n");
        return;
    }
    checkBuffer();
    std::memmove(KeynPtrAt(pos + 1), KeynPtrAt(pos), (size - pos) * (tree->header->recordLenth + 8));
    uchar* dstpos = KeynPtrAt(pos);
    std::memcpy(dstpos, element, tree->header->recordLenth);
    dstpos += tree->header->recordLenth;
    ((uint*)dstpos)[0] = rid.GetPageNum();
    ((uint*)dstpos)[1] = rid.GetSlotNum();
    size++;
    ptrNum++;
    updateSize();
}

void BplusTree::Insert(const uchar* data, const RID& rid){
    header->recordNum++;
    UpdateRecordNum();
    BplusTreeNode* node = nullptr;
    int pos = -1;
    _search(data, node, pos); // reach the leaf node
    if(node->size < header->leafCap){ // free space
        node->InsertKeynPtrAt(pos, data, rid);
    }
    else{ // the leaf node is full
        // split the leaf node first
        BplusTreeNode* right = CreateTreeNode(node->parent, BplusTreeNode::Leaf);
        // maintain linklist of leaves
        *right->LeafPtr() = *node->LeafPtr();
        *node->LeafPtr() = right->page;
        // splitting
        int leftsize = (node->size + 1) / 2;
        int insertPos = node->findFirstEqGreaterInLeaf(data);
        if(insertPos >= leftsize){ // inserted key&ptr belongs to the right node
            memcpy(right->KeynPtrAt(0), node->KeynPtrAt(leftsize), (insertPos - leftsize) * (header->recordLenth + 8));
            right->size = insertPos - leftsize;
            right->InsertKeynPtrAt(right->size, data, rid);
            memcpy(right->KeynPtrAt(right->size), node->KeynPtrAt(insertPos), (node->size - insertPos) * (header->recordLenth + 8));
            right->size = node->size + 1 - leftsize;
            node->size = leftsize;
            node->updateSize();
            right->updateSize();
        }
        else{ // inserted key&ptr belongs to the left node. insertPos < leftsize
            memcpy(right->KeynPtrAt(0), node->KeynPtrAt(leftsize - 1), (node->size + 1 -leftsize) * (header->recordLenth + 8));
            right->size = node->size + 1 -leftsize;
            node->size--;
            node->InsertKeynPtrAt(insertPos, data, rid);
            node->updateSize();
            right->updateSize();
        }

        // recursively inserting into internal nodes
        uchar overflowKey[header->recordLenth];
        memcpy(overflowKey, right->KeynPtrAt(0), header->recordLenth);
        uint rightSubTreePage = right->page;

        while(node->parent != nullptr){
            BplusTreeNode* pNode = node->parent;
            int overflowPos = pNode->findFirstGreaterInInternal(overflowKey);
            if(pNode->size < header->internalCap){ // internal node has free space
                pNode->InsertKeyAt(overflowPos, overflowKey);
                pNode->InsertNodePtrAt(overflowPos + 1, rightSubTreePage);
                return;
            }
            else{ // internal parent node is also full
                // split the node
                BplusTreeNode* ofRight = CreateTreeNode(pNode, BplusTreeNode::Internal);
                int oldSize = pNode->size;
                int ofLeftSize = (oldSize + 1) / 2;
                if(overflowPos > ofLeftSize){
                    // split keys
                    // handle right node
                    memcpy(ofRight->KeyAt(0), pNode->KeyAt(ofLeftSize + 1), (overflowPos - ofLeftSize - 1) * header->recordLenth);
                    ofRight->size = overflowPos - ofLeftSize - 1;
                    ofRight->InsertKeyAt(ofRight->size, overflowKey);
                    memcpy(ofRight->KeyAt(ofRight->size), pNode->KeyAt(overflowPos), (oldSize - overflowPos) * header->recordLenth);
                    ofRight->size = oldSize - ofLeftSize;
                    ofRight->updateSize();
                    // handle left node
                    pNode->size = ofLeftSize;
                    pNode->updateSize();
                    // update ofKey
                    memcpy(overflowKey, pNode->KeyAt(ofLeftSize), header->recordLenth);
                    // split ptrs
                    // handle right node
                    memcpy(ofRight->NodePtrAt(0), pNode->NodePtrAt(ofLeftSize + 1), (overflowPos - ofLeftSize) * 4);
                    *ofRight->NodePtrAt(overflowPos - ofLeftSize) = rightSubTreePage; // insert passed subtree
                    memcpy(ofRight->NodePtrAt(overflowPos - ofLeftSize + 1), pNode->NodePtrAt(overflowPos + 1), (oldSize - overflowPos) * 4);
                    ofRight->ptrNum = oldSize - ofLeftSize + 1;
                    // handle left node
                    pNode->ptrNum = ofLeftSize + 1;
                    // update ofPage
                    rightSubTreePage = ofRight->page;
                }
                else if(overflowPos < ofLeftSize){
                    // split keys
                    // handle right node
                    memcpy(ofRight->KeyAt(0), pNode->KeyAt(ofLeftSize), (oldSize - ofLeftSize) * header->recordLenth);
                    ofRight->size = oldSize - ofLeftSize;
                    ofRight->updateSize();
                    // back up ofKey to pass
                    uchar tmpBuf[header->recordLenth];
                    memcpy(tmpBuf, pNode->KeyAt(ofLeftSize - 1), header->recordLenth);
                    // handle left node
                    pNode->size = ofLeftSize - 1;
                    pNode->InsertKeyAt(overflowPos, overflowKey); // updateSize is invoked inside, no need to call it once more
                    pNode->updateSize();
                    // we have backed up ofKey so it didn't get overwritten
                    // update ofKey
                    memcpy(overflowKey, tmpBuf, header->recordLenth);
                    // split ptrs
                    // handle right node
                    memcpy(ofRight->NodePtrAt(0), pNode->NodePtrAt(ofLeftSize), (oldSize - ofLeftSize + 1) * 4);
                    ofRight->ptrNum = oldSize - ofLeftSize + 1;
                    // handle left node
                    pNode->ptrNum = ofLeftSize;
                    pNode->InsertNodePtrAt(overflowPos + 1, rightSubTreePage);
                    // update ofPage
                    rightSubTreePage = ofRight->page;
                }
                else{ // overflowPos == ofLeftSize, the overflowKey is passed on
                    // split keys
                    // handle right
                    memcpy(ofRight->KeyAt(0), pNode->KeyAt(ofLeftSize), (oldSize - ofLeftSize) * header->recordLenth);
                    ofRight->size = oldSize - ofLeftSize;
                    ofRight->updateSize();
                    // handle left
                    pNode->size = ofLeftSize;
                    pNode->updateSize();
                    // split ptrs
                    // handle right
                    memcpy(ofRight->KeyAt(0), overflowKey, 4);
                    memcpy(ofRight->KeyAt(1), pNode->KeyAt(ofLeftSize + 1), (oldSize - ofLeftSize) * 4);
                    ofRight->ptrNum = oldSize - ofLeftSize + 1;
                    // update ofPage
                    rightSubTreePage = ofRight->page;
                    // handle left
                    pNode->ptrNum = ofLeftSize + 1;
                }
                node = pNode;
            } // end splitting
        } // end recursion
        // split the root node
        node->parent = root = CreateTreeNode(nullptr, BplusTreeNode::Internal); // the new root
        nodes.push_back(node); // node is not the root anymore, add it to the memory manager
        UpdateRoot();
        // construct root
        memcpy(root->KeyAt(0), overflowKey, header->recordLenth);
        *root->NodePtrAt(0) = node->page;
        *root->NodePtrAt(1) = rightSubTreePage;
        root->size = 1;
        root->ptrNum = 2;
        root->updateSize();
    } // end overflow case
}

bool BplusTree::Search(const uchar* data, const RID& rid){

}

void BplusTree::Remove(const uchar* data, const RID& rid){

}

#endif // BPLUSTREE_H