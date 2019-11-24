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
        std::vector<uint> rmPages; // pages to remove

        // runtime variables
        // minimum keys for internal and leaf nodes
        // internalMinKey = ceiling(internalCap / 2) - 1
        // leafMinKey = ceiling(leafCap / 2);
        uint internalMinKey, leafMinKey;

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
            node->size = *(ushort*)(node->data + 1);
            node->ptrNum = node->type == BplusTreeNode::Internal ? node->size + 1 : node->size;
            node->parent = curNode;
            node->parentPage = *(uint*)(node->data + 3);
            node->posInParent = *(ushort*)(node->data + 7);
            nodes.push_back(node);
            return node;
        }
        // create a treenode
        BplusTreeNode* CreateTreeNode(BplusTreeNode* curNode, int nodeType){
            BplusTreeNode* node = new BplusTreeNode();
            node->tree = this;
            node->fid = fid;
            uchar* tmp = new uchar[PAGE_SIZE]{0};
            tmp[0] = nodeType;
            *(ushort*)(tmp + 1) = 0; // size = 0
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
            node->parentPage = curNode ? curNode->page : 0; // if curNode is nullptr
            node->syncWithBuffer();
            // posInParent cannot be decided here
            nodes.push_back(node);
            return node;
        }

    public:
        // create a B+ tree from a header
        // you need to specify: attrType, attrLenth, nullMask, tableName, indexColID
        // those assigned in constructor: recordLenth, internalCap, leafCap, rootPage, recordNum, 
        BplusTree(Table* table, IndexHeader* header){
            this->table = table;
            this->fid = table->FileID();
            this->header = header;

            CalcKeyLength();
            header->recordNum = 0;
            header->internalCap = (PAGE_SIZE - BplusTreeNode::reservedBytes + header->recordLenth) / (header->recordLenth + 4);
            header->leafCap = (PAGE_SIZE - BplusTreeNode::reservedBytes - 8) / (header->recordLenth + 8); // ? update for bidirectional linked list
            
            // ! Debug only
            // 2-4 tree
            header->internalCap = 5;
            header->leafCap = 4;
            // ! end
            
            // root init
            root = CreateTreeNode(nullptr, BplusTreeNode::Leaf);
            nodes.pop_back(); // pop the root node from stack

            header->rootPage = root->page;
            uchar* tmp = new uchar[PAGE_SIZE]{0};
            header->ToString(tmp);
            RID* rid = new RID();
            table->InsertRecord(tmp, rid);
            page = rid->GetPageNum();
            delete[] tmp;
            delete rid;
            data = (uchar*)bpm->getPage(fid, page, headerIdx);
            internalMinKey = (header->internalCap - 1) >> 1;
            leafMinKey = (header->leafCap + 1) >> 1;
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
            internalMinKey = (header->internalCap - 1) >> 1;
            leafMinKey = (header->leafCap + 1) >> 1;
            // TODO: init root ?
            root = GetTreeNode(nullptr, header->rootPage);
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


        // move the iterator right with comparison strategy 'mode', store the tree node and position into 'node' and 'pos'
        // return if the DataType::compareArr(this record, next record, mode) is true
        bool MoveNext(BplusTreeNode*& node, int& pos, uchar mode){
            if(node->size == pos + 1){ // at the tail of a leaf node
                if(*node->NextLeafPtr() == 0){ // the last leaf node
                    return false;
                }
                else{ // get the next leaf node
                    BplusTreeNode* nextNode = GetTreeNode(nullptr, *node->NextLeafPtr());
                    if(DataType::compareArr(node->KeynPtrAt(pos), nextNode->KeynPtrAt(0), header->attrType, header->attrLenth, header->nullMask, colNum, mode)){
                        node = nextNode;
                        pos = 0;
                        return true;
                    }
                    return false;
                }
            }
            // no at the tail of current leaf node
            if(DataType::compareArr(node->KeynPtrAt(pos), node->KeynPtrAt(pos + 1), header->attrType, header->attrLenth, header->nullMask, colNum, mode)){
                pos++;
                return true;
            }
            return false;
        }

        // write all opened nodes back to memory
        void ClearAndWriteBackOpenedNodes(){
            for(BplusTreeNode* node : nodes){
                node->writeBack();
                delete node;
            }
            nodes.clear();
        }

        // remove all orphaned nodes from table
        void RemoveNodes(){
            RID r = RID();
            r.SlotNum = 0;
            for(uint page : rmPages){
                r.PageNum = page;
                table->DeleteRecord(r);
            }
            rmPages.clear();
        }

        void ChangeParent(uint page, uint parent, ushort posInParent){
            BplusTreeNode* node = GetTreeNode(nullptr, page);
            node->parentPage = parent;
            node->posInParent = posInParent;
            node->syncWithBuffer();
        }

        IndexHeader* header = nullptr;
        BplusTreeNode* root = nullptr;
        int colNum = 0;
        void Insert(const uchar* data, const RID& rid);
        bool Search(const uchar* data, const RID& rid);
        void Remove(const uchar* data, const RID& rid);
    
};
BufPageManager* BplusTree::bpm = BufPageManager::Instance();

/**
 * Methods in BplusTreeNode
*/

// Low level APIs

uchar* BplusTreeNode::KeyAt(int pos){
    checkBuffer();
    return data + BplusTreeNode::reservedBytes + pos * tree->header->recordLenth;
}

uint* BplusTreeNode::NodePtrAt(int pos){
    checkBuffer();
    return (uint*)(data + BplusTreeNode::reservedBytes + (tree->header->internalCap - 1) * tree->header->recordLenth + pos * 4);
}

uint* BplusTreeNode::NextLeafPtr(){
    checkBuffer();
    return (uint*)(data + BplusTreeNode::reservedBytes + tree->header->leafCap * (tree->header->recordLenth + 8) + 4);
}

uint* BplusTreeNode::PrevLeafPtr(){
    checkBuffer();
    return (uint*)(data + BplusTreeNode::reservedBytes + tree->header->leafCap * (tree->header->recordLenth + 8));
}

uchar* BplusTreeNode::KeynPtrAt(int pos){
    checkBuffer();
    return data + BplusTreeNode::reservedBytes + pos * (tree->header->recordLenth + 8);
}

int BplusTreeNode::findFirstGreaterInInternal(const uchar* data){
    checkBuffer();
    uchar* cmpData = this->data + BplusTreeNode::reservedBytes;
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
    uchar* cmpData = this->data + BplusTreeNode::reservedBytes;
    for(int i = 0; i < size; i++, cmpData += tree->header->recordLenth + 8){
        if(DataType::noLessThanArr(cmpData, data, tree->header->attrType, tree->header->attrLenth, tree->header->nullMask, tree->colNum)){
            return i;
        }
    }
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
}

void BplusTreeNode::RemoveKeyAt(int pos){
    if(pos >= size){
        std::printf("trying to remove key beyond size\n");
        return;
    }
    checkBuffer();
    std::memmove(KeyAt(pos), KeyAt(pos + 1), (size - pos - 1) * tree->header->recordLenth);
    size--;
}

void BplusTreeNode::RemoveNodePtrAt(int pos){
    if(pos >= ptrNum){
        std::printf("trying to remove node ptr beyond ptrNum\n");
        return;
    }
    checkBuffer();
    std::memmove(NodePtrAt(pos), NodePtrAt(pos + 1), (ptrNum - pos - 1) * 4);
    ptrNum--;
}

void BplusTreeNode::RemoveKeynPtrAt(int pos){
    if(pos >= size){
        std::printf("trying to remove key and ptr beyond size\n");
        return;
    }
    checkBuffer();
    std::memmove(KeynPtrAt(pos), KeynPtrAt(pos + 1), (size - pos - 1) * (tree->header->recordLenth + 8));
    size--;
    ptrNum--;
}

/**
 * Methods in BplusTree
*/

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
// insertion of duplicate record is permitted
void BplusTree::Insert(const uchar* data, const RID& rid){
    header->recordNum++;
    UpdateRecordNum();
    BplusTreeNode* node = nullptr;
    int pos = -1;
    _search(data, node, pos); // reach the leaf node
    if(node->size < header->leafCap){ // free space
        node->InsertKeynPtrAt(pos, data, rid); //* branch NO.1
        node->syncWithBuffer();
    }
    else{ // the leaf node is full
        // split the leaf node first
        BplusTreeNode* right = CreateTreeNode(node->parent, BplusTreeNode::Leaf);
        // maintain linklist of leaves
        *right->NextLeafPtr() = *node->NextLeafPtr();
        *right->PrevLeafPtr() = node->page;
        *node->NextLeafPtr() = right->page;
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
        }
        else{ // inserted key&ptr belongs to the left node. insertPos < leftsize
            memcpy(right->KeynPtrAt(0), node->KeynPtrAt(leftsize - 1), (node->size + 1 -leftsize) * (header->recordLenth + 8));
            right->size = node->size + 1 -leftsize;
            node->size = node->ptrNum = leftsize - 1;
            node->InsertKeynPtrAt(insertPos, data, rid);
        }
        node->syncWithBuffer();
        right->syncWithBuffer();

        // recursively inserting into internal nodes
        uchar overflowKey[header->recordLenth];
        memcpy(overflowKey, right->KeynPtrAt(0), header->recordLenth);

        // when overflow occurs, and we are trying to insert an element into the level above
        //   pNode (split into)--> ofRight
        //"  /   \   "
        // node right
        // Note that node and right have been synced already
        // after we split pNode into pNode and ofRight, node and right will be assgined as pNode and ofRight respectively
        // then we move on to the next level
        while(node->parent != nullptr){
            BplusTreeNode* pNode = node->parent;
            // int overflowPos = pNode->findFirstGreaterInInternal(overflowKey);
            int overflowPos = node->posInParent; // ? check if works
            if(pNode->size < header->internalCap - 1){ // internal node has free space
                // insertion
                pNode->InsertKeyAt(overflowPos, overflowKey);
                pNode->InsertNodePtrAt(overflowPos + 1, right->page);
                pNode->syncWithBuffer();
                // set parent info
                int childrenCount = pNode->size;
                uint* childIter = pNode->NodePtrAt(overflowPos + 1);
                for(int i = overflowPos + 1; i < childrenCount; i++, childIter++)
                    ChangeParent(*childIter, pNode->page, i);
                // // set parent info of right
                // right->posInParent = overflowPos + 1;
                // right->syncWithBuffer();
                return; //* Branch NO.2
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
                    // handle left node
                    pNode->size = ofLeftSize;
                    // update ofKey
                    memcpy(overflowKey, pNode->KeyAt(ofLeftSize), header->recordLenth);
                    // split ptrs
                    // handle right node
                    memcpy(ofRight->NodePtrAt(0), pNode->NodePtrAt(ofLeftSize + 1), (overflowPos - ofLeftSize) * 4);
                    *ofRight->NodePtrAt(overflowPos - ofLeftSize) = right->page; // insert passed subtree
                    memcpy(ofRight->NodePtrAt(overflowPos - ofLeftSize + 1), pNode->NodePtrAt(overflowPos + 1), (oldSize - overflowPos) * 4);
                    ofRight->ptrNum = oldSize - ofLeftSize + 1;
                    // handle left node
                    pNode->ptrNum = ofLeftSize + 1;
                    // set parent info // ? check if works
                    int childrenCount = ofRight->ptrNum;
                    uint* childIter = ofRight->NodePtrAt(0);
                    for(int i = 0; i < childrenCount; i++, childIter++)
                        ChangeParent(*childIter, ofRight->page, i);
                    // // set parent info of left
                    // if(node->posInParent >= ofLeftSize){
                    //     node->parent = ofRight;
                    //     node->parentPage = ofRight->page;
                    //     if(node->posInParent >= overflowPos)
                    //         node->posInParent -= ofLeftSize;
                    //     else
                    //         node->posInParent -= ofLeftSize + 1;
                    //     node->syncWithBuffer();
                    // }
                    // // set parent info of right
                    // right->parent = ofRight;
                    // right->parentPage = ofRight->page;
                    // right->posInParent = overflowPos - ofLeftSize;
                    // right->syncWithBuffer();
                }
                else if(overflowPos < ofLeftSize){
                    // split keys
                    // handle right node
                    memcpy(ofRight->KeyAt(0), pNode->KeyAt(ofLeftSize), (oldSize - ofLeftSize) * header->recordLenth);
                    ofRight->size = oldSize - ofLeftSize;
                    // back up the new ofKey, or the insertion of the old ofKey will cause it to be overwritten
                    uchar tmpBuf[header->recordLenth];
                    memcpy(tmpBuf, pNode->KeyAt(ofLeftSize - 1), header->recordLenth);
                    // handle left node
                    pNode->size = ofLeftSize - 1;
                    pNode->InsertKeyAt(overflowPos, overflowKey);
                    // we have backed up ofKey so it didn't get overwritten
                    // update ofKey
                    memcpy(overflowKey, tmpBuf, header->recordLenth);
                    // split ptrs
                    // handle right node
                    memcpy(ofRight->NodePtrAt(0), pNode->NodePtrAt(ofLeftSize), (oldSize - ofLeftSize + 1) * 4);
                    ofRight->ptrNum = oldSize - ofLeftSize + 1;
                    // handle left node
                    pNode->ptrNum = ofLeftSize;
                    pNode->InsertNodePtrAt(overflowPos + 1, right->page);
                    // set parent info in ofRight
                    int childrenCount = ofRight->ptrNum;
                    uint* childIter = ofRight->NodePtrAt(0);
                    for(int i = 0; i < childrenCount; i++, childIter++)
                        ChangeParent(*childIter, ofRight->page, i);
                    childrenCount = ofLeftSize + 1;
                    childIter = pNode->NodePtrAt(overflowPos + 1);
                    for(int i = overflowPos + 1; i < childrenCount; i++, childIter++)
                        ChangeParent(*childIter, pNode->page, i);
                    // // set parent info of node
                    // if(node->posInParent >= ofLeftSize){
                    //     node->parent = ofRight;
                    //     node->parentPage = ofRight->page;
                    //     node->posInParent -= ofLeftSize;
                    //     node->syncWithBuffer();
                    // }
                    // else if(node->posInParent >= overflowPos){
                    //     node->posInParent++;
                    //     node->syncWithBuffer();
                    // }
                    // // set parent info of right
                    // right->posInParent = overflowPos + 1;
                    // right->syncWithBuffer();
                }
                else{ // overflowPos == ofLeftSize, the overflowKey is passed on
                    // split keys
                    // handle right
                    memcpy(ofRight->KeyAt(0), pNode->KeyAt(ofLeftSize), (oldSize - ofLeftSize) * header->recordLenth);
                    ofRight->size = oldSize - ofLeftSize;
                    // handle left
                    pNode->size = ofLeftSize;
                    // split ptrs
                    // handle right
                    memcpy(ofRight->KeyAt(0), overflowKey, 4);
                    memcpy(ofRight->KeyAt(1), pNode->KeyAt(ofLeftSize + 1), (oldSize - ofLeftSize) * 4);
                    ofRight->ptrNum = oldSize - ofLeftSize + 1;
                    // handle left
                    pNode->ptrNum = ofLeftSize + 1;
                    // no need to update ofKey, because the new ofKey is the same with the old one
                    // set parent info
                    int childrenCount = ofRight->ptrNum;
                    uint* childIter = ofRight->NodePtrAt(0);
                    for(int i = 0; i < childrenCount; i++, childIter++)
                        ChangeParent(*childIter, ofRight->page, i);
                    // // set parent info of node
                    // if(node->posInParent >= ofLeftSize){
                    //     node->parent = right;
                    //     node->parentPage = right->page;
                    //     node->posInParent -= ofLeftSize;
                    //     node->syncWithBuffer();
                    // }
                    // // set parent info of right
                    // right->parent = ofRight;
                    // right->parentPage = ofRight->page;
                    // right->posInParent = 0;
                    // right->syncWithBuffer();
                }
                node = pNode; // go a level up
                right = ofRight;
                node->syncWithBuffer();
                right->syncWithBuffer();
            } // end splitting
        } // end recursion
        // split the root node
        right->parent = node->parent = root = CreateTreeNode(nullptr, BplusTreeNode::Internal); // the new root
        nodes.pop_back(); // pop out the new root
        nodes.push_back(node); // node is not the root anymore, add it to the memory manager
        UpdateRoot();
        // construct root
        memcpy(root->KeyAt(0), overflowKey, header->recordLenth);
        *root->NodePtrAt(0) = node->page;
        *root->NodePtrAt(1) = right->page;
        root->size = 1;
        root->ptrNum = 2;
        root->syncWithBuffer();
        // set pos in parent
        node->parentPage = root->page;
        node->posInParent = 0;
        node->syncWithBuffer();
        // set pos in parent
        right->parentPage = root->page;
        right->posInParent = 1;
        right->syncWithBuffer();
        //* Branch NO.3
    } // end overflow case
}

bool BplusTree::Search(const uchar* data, const RID& rid){
    BplusTreeNode* node = nullptr;
    int pos = -1;
    _search(data, node, pos);
    if(pos == node->size) // no match is found
        return false;
    bool found = false;
    do{
        uint* curRID = (uint*)(node->KeynPtrAt(pos) + header->recordLenth);
        if(curRID[0] == rid.GetPageNum() && curRID[1] == rid.GetSlotNum()){
            found = true;
            break;
        }
    }while(MoveNext(node, pos, Comparator::Eq));
    return found;
}

void BplusTree::Remove(const uchar* data, const RID& rid){
    BplusTreeNode* node = nullptr;
    int pos = -1;
    _search(data, node, pos);
    // now at a leaf node
    if(pos == node->size) // no match is found
        return;
    bool found = false;
    do{
        uint* curRID = (uint*)(node->KeynPtrAt(pos) + header->recordLenth);
        if(curRID[0] == rid.GetPageNum() && curRID[1] == rid.GetSlotNum()){
            found = true;
            break;
        }
    }while(MoveNext(node, pos, Comparator::Eq));
    if(!found) // there is no record with RID rid among records with key data
        return;
    header->recordNum--;
    UpdateRecordNum();
    // remove
    if(node->size > leafMinKey){ // simple case: no underflow
        node->RemoveKeynPtrAt(pos);
        node->syncWithBuffer();
        // no need to update parent's key
    }
    else if(node->parentPage != 0){ // leaf has exactly leafMinKey keys, underflow
        BplusTreeNode *pNode =  node->parent ? node->parent : (node->parent = GetTreeNode(nullptr, node->parentPage)), 
            *leftNode = nullptr, *rightNode = nullptr;
        if(node->posInParent > 0){ // has left sibling
            leftNode = GetTreeNode(pNode, *pNode->NodePtrAt(node->posInParent - 1));
            if(leftNode->size > leafMinKey){ // borrow a key from left sibling
                memmove(node->KeynPtrAt(1), node->KeynPtrAt(0), pos * (header->recordLenth + 8));
                memcpy(node->KeynPtrAt(0), leftNode->KeynPtrAt(leftNode->size - 1), header->recordLenth + 8);
                // update parent's split key
                memcpy(pNode->KeyAt(leftNode->posInParent), leftNode->KeynPtrAt(leftNode->size - 1), header->recordLenth); 
                leftNode->size--;
                leftNode->ptrNum--;
                leftNode->syncWithBuffer();
                node->MarkDirty();
                pNode->MarkDirty();
                return; // TODO: clear memory?
            }
            // left sibling also at min keys
        }
        else{ // has right sibling
            rightNode = GetTreeNode(pNode, *pNode->NodePtrAt(node->posInParent + 1));
            if(rightNode->size > leafMinKey){ // borrow a key from right sibling
                memmove(node->KeynPtrAt(pos), node->KeynPtrAt(pos + 1), (node->size - pos - 1) * (header->recordLenth + 8));
                memmove(node->KeynPtrAt(leafMinKey - 1), rightNode->KeynPtrAt(0), header->recordLenth + 8);
                rightNode->RemoveKeynPtrAt(0);
                // update parent's split key
                memcpy(pNode->KeyAt(node->posInParent), rightNode->KeynPtrAt(0), header->recordLenth);
                rightNode->syncWithBuffer();
                node->MarkDirty();
                pNode->MarkDirty();
                return; // TODO: clear memory?
            }
            // right sibling also at min keys
        }
        int rmPos; // remove the key at rmPos and ptr at rmPos + 1
        BplusTreeNode* mergedNode = nullptr;
        // either sibling is at min keys or doesn't exist, merge this leaf node with one of its siblings
        if(leftNode){ // merge with left
            memcpy(leftNode->KeynPtrAt(leafMinKey), node->KeynPtrAt(0), pos * (header->recordLenth + 8));
            memcpy(leftNode->KeynPtrAt(leafMinKey + pos), node->KeynPtrAt(pos + 1), (leafMinKey - pos - 1) * (header->recordLenth + 8));
            leftNode->size += leafMinKey - 1;
            memcpy(leftNode->NextLeafPtr(), node->NextLeafPtr(), 8); // update next leaf
            leftNode->syncWithBuffer();
            rmPos = leftNode->posInParent;
            rmPages.push_back(node->page);
            mergedNode = leftNode;
        }
        else{ // merge with right
            memcpy(node->KeynPtrAt(pos), node->KeynPtrAt(pos + 1), (leafMinKey - pos - 1) * (header->recordLenth + 8));
            memcpy(node->KeynPtrAt(leafMinKey - 1), rightNode->KeynPtrAt(0), leafMinKey * (header->recordLenth + 8));
            node->size += leafMinKey - 1;
            memcpy(node->NextLeafPtr(), rightNode->NextLeafPtr(), 8); // update next leaf
            node->syncWithBuffer();
            rmPos = node->posInParent;
            rmPages.push_back(rightNode->page);
            mergedNode = node;
        }
        leftNode = nullptr;
        rightNode = nullptr;
        node = pNode;
        pNode = nullptr;
        // recursively remove key in internal node
        while(true){
            // remove key at rmPos and ptr at rmPos + 1 in 'node'
            if(node->size > internalMinKey){ // simple case, remove directly
                node->RemoveKeyAt(rmPos);
                node->RemoveNodePtrAt(rmPos + 1);
                node->syncWithBuffer();
                // set parent info
                int childrenCount = node->size + 1;
                uint* childIter = node->NodePtrAt(rmPos + 1);
                for(int i = rmPos + 1; i < childrenCount; i++, childIter++)
                    ChangeParent(*childIter, node->page, i);
                //* branch 1
                break;
            }
            // recursively remove in internal nodes
            if(node->parentPage == 0){ // if this is the root node
                if(node->size == 1){ // one key remaining, tree height decreases by 1
                    root = mergedNode;
                    header->rootPage = mergedNode->page;
                    UpdateRoot();
                    mergedNode->parentPage = 0;
                    mergedNode->syncWithBuffer();
                    rmPages.push_back(node->page);
                    //* branch 2
                    break;
                }
                else{ // more than one key remaining, simply remove it
                    node->RemoveKeyAt(rmPos);
                    node->RemoveNodePtrAt(rmPos + 1);
                    node->syncWithBuffer();
                    // set parent info
                    int childrenCount = node->size + 1;
                    uint* childIter = node->NodePtrAt(rmPos + 1);
                    for(int i = rmPos + 1; i < childrenCount; i++, childIter++)
                        ChangeParent(*childIter, node->page, i);
                    //* branch 3
                    break;
                }
            }
            // this node is not the root node
            pNode = node->parent == nullptr ? (node->parent = GetTreeNode(nullptr, node->parentPage)) : node->parent;
            if(node->posInParent > 0){ // has left sibling
                leftNode = GetTreeNode(pNode, *pNode->NodePtrAt(node->posInParent - 1));
                if(leftNode->size > internalMinKey){ // borrow a key from left sibling
                    // handle key, perform a clockwise rotation
                    memmove(node->KeyAt(1), node->KeyAt(0), rmPos * header->recordLenth); // * memmove
                    memcpy(node->KeyAt(0), pNode->KeyAt(leftNode->posInParent), header->recordLenth);
                    memcpy(pNode->KeyAt(leftNode->posInParent), leftNode->KeyAt(leftNode->size - 1), header->recordLenth);
                    // handle ptrs
                    memmove(node->NodePtrAt(1), node->NodePtrAt(0), (rmPos + 1) * 4); //* memmove
                    memcpy(node->NodePtrAt(0), leftNode->NodePtrAt(leftNode->size), 4);
                    // set parent info
                    uint* childIter = node->NodePtrAt(0);
                    for(int i = 0; i <= rmPos + 1; i++)
                        ChangeParent(*childIter, node->page, i);
                    // updates
                    leftNode->size--;
                    leftNode->ptrNum--;
                    leftNode->syncWithBuffer();
                    node->MarkDirty();
                    pNode->MarkDirty();
                    //* branch 4
                    break;
                }
                // left node also at min keys
            }
            else{ // has right sibling
                rightNode = GetTreeNode(pNode, *pNode->NodePtrAt(node->posInParent + 1));
                if(rightNode->size > internalMinKey){ // borrow a key from right sibling
                    // handle key, perform a counter-clockwise rotation
                    memcpy(node->KeyAt(rmPos), node->KeyAt(rmPos + 1), (internalMinKey - rmPos - 1) * header->recordLenth);
                    memcpy(node->KeyAt(internalMinKey - 1), pNode->KeyAt(node->posInParent), header->recordLenth);
                    memcpy(pNode->KeyAt(node->posInParent), rightNode->KeyAt(0), header->recordLenth);
                    // handle ptrs
                    memcpy(node->NodePtrAt(internalMinKey), rightNode->NodePtrAt(0), 4);
                    rightNode->RemoveKeyAt(0);
                    rightNode->RemoveNodePtrAt(0);
                    // set parent info
                    ChangeParent(*node->NodePtrAt(internalMinKey - 1), node->page, internalMinKey - 1);
                    int childrenCount = rightNode->size + 1;
                    uint* childIter = rightNode->NodePtrAt(0);
                    for(int i = 0; i < childrenCount; i++, childIter++)
                        ChangeParent(*childIter, rightNode->page, i);
                    // update
                    rightNode->syncWithBuffer();
                    node->MarkDirty();
                    pNode->MarkDirty();
                    //* branch 5
                    break;
                }
                // right node also at min keys
            }
            // either sibling is at min keys or doesn't exist
            // merge this internal node with one of its siblings and the parent's split key
            if(leftNode){ // merge with left
                // handle key, drag the parent's split key down
                memcpy(leftNode->KeyAt(internalMinKey), pNode->KeyAt(leftNode->posInParent), header->recordLenth);
                memcpy(leftNode->KeyAt(internalMinKey + 1), node->KeyAt(0), rmPos * header->recordLenth);
                memcpy(leftNode->KeyAt(internalMinKey + 1 + rmPos), node->KeyAt(rmPos + 1), (internalMinKey - rmPos - 1) * header->recordLenth);
                // handle ptrs
                memcpy(leftNode->NodePtrAt(internalMinKey + 1), node->NodePtrAt(0), (rmPos + 1) * 4);
                memcpy(leftNode->NodePtrAt(internalMinKey + 1 + rmPos + 1), node->NodePtrAt(rmPos + 2), (internalMinKey - rmPos - 1) * 4);
                // set parent info
                int childrenCount = 2 * internalMinKey + 1;
                uint* childIter = leftNode->NodePtrAt(internalMinKey + 1);
                for(int i = internalMinKey + 1; i < childrenCount; i++, childIter++)
                    ChangeParent(*childIter, leftNode->page, i);
                // updates
                leftNode->size += internalMinKey;
                leftNode->ptrNum += internalMinKey;
                leftNode->syncWithBuffer();
                rmPages.push_back(node->page);
                rmPos = leftNode->posInParent;
                mergedNode = leftNode;
            }
            else{ // merge with right
                // handle key, drag the parent's split key down
                memcpy(node->KeyAt(rmPos), node->KeyAt(rmPos + 1), (internalMinKey - rmPos - 1) * header->recordLenth);
                memcpy(node->KeyAt(internalMinKey - 1), pNode->KeyAt(node->posInParent), header->recordLenth);
                memcpy(node->KeyAt(internalMinKey), rightNode->KeyAt(0), internalMinKey * header->recordLenth);
                // handle ptrs
                memcpy(node->NodePtrAt(rmPos + 1), node->NodePtrAt(rmPos + 2), (internalMinKey - rmPos - 1) * 4);
                memcpy(node->NodePtrAt(internalMinKey), rightNode->NodePtrAt(0), (internalMinKey + 1) * 4);
                // set parent info
                int childrenCount = 2 * internalMinKey + 1;
                uint* childIter = node->NodePtrAt(rmPos + 1);
                for(int i = rmPos + 1; i < childrenCount; i++, childIter++)
                    ChangeParent(*childIter, node->page, i);
                // handle update
                node->size += internalMinKey;
                node->ptrNum += internalMinKey;
                node->syncWithBuffer();
                rmPages.push_back(rightNode->page);
                rmPos = node->posInParent;
                mergedNode = node;
            }
            // go loop
            leftNode = nullptr;
            rightNode = nullptr;
            node = pNode;
            pNode = nullptr;
        }
    }
    else{ // otherwise, the leaf node is the only node in this tree
        node->RemoveKeynPtrAt(pos);
        node->syncWithBuffer();
    }
    // TODO: remove nodes from table and free allocated memory
}

// TODO: bulk-loading of b+ tree to realized fast construction
#endif // BPLUSTREE_H