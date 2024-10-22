#include "BplusTree.h"
BufPageManager* BplusTree::bpm = BufPageManager::Instance();
#ifdef DEBUG
bool BplusTree::errorSign = false;
#endif
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

int BplusTreeNode::findFirstGreaterInInternal(const uchar* data, int cmpColNum, bool isConstant, const uchar* cmps){
    checkBuffer();
    bool multiCmp = cmps != nullptr;
    uchar* cmpData = this->data + BplusTreeNode::reservedBytes;
    for(int i = 0; i < size; i++, cmpData += tree->header->recordLenth){
        // TODO: binary search maybe ?
        // ! 因为存在重复键值,所以不能用二分查找
        // find the first element > data
        if(!multiCmp && DataType::compareArr(cmpData, data, tree->header->attrType, tree->header->attrLenth, cmpColNum, Comparator::Gt, false, isConstant) ||
        multiCmp && DataType::compareArrMultiOp(cmpData, data, tree->header->attrType, tree->header->attrLenth, cmpColNum, cmps, false, isConstant))
            return i;
    }
    return size;
}

int BplusTreeNode::findFirstGtEqInInternal(const uchar* data, int cmpColNum, bool isConstant, const uchar* cmps){
    checkBuffer();
    bool multiCmp = cmps != nullptr;
    uchar* cmpData = this->data + BplusTreeNode::reservedBytes;
    for(int i = 0; i < size; i++, cmpData += tree->header->recordLenth){
        if(!multiCmp && DataType::compareArr(cmpData, data, tree->header->attrType, tree->header->attrLenth, cmpColNum, Comparator::GtEq, false, isConstant) ||
        multiCmp && DataType::compareArrMultiOp(cmpData, data, tree->header->attrType, tree->header->attrLenth, cmpColNum, cmps, false, isConstant))
            return i;
    }
    return size;
}

int BplusTreeNode::findFirstEqGreaterInLeaf(const uchar* data, int cmpColNum, bool isConstant, const uchar* cmps){
    checkBuffer();
    bool multiCmp = cmps != nullptr;
    uchar* cmpData = this->data + BplusTreeNode::reservedBytes;
    for(int i = 0; i < size; i++, cmpData += tree->header->recordLenth + 8){
        if(!multiCmp && DataType::compareArr(cmpData, data, tree->header->attrType, tree->header->attrLenth, cmpColNum, Comparator::GtEq, false, isConstant) ||
        multiCmp && DataType::compareArrMultiOp(cmpData, data, tree->header->attrType, tree->header->attrLenth, cmpColNum, cmps, false, isConstant))
            return i;
    }
    return size;
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

void BplusTree::_rawSearch(const uchar* data,BplusTreeNode*& node, int& pos, int cmpColNum, bool isConstant){
    if(cmpColNum == -1)
        cmpColNum = colNum;
    BplusTreeNode* cur = root;
    while(cur->type == BplusTreeNode::Internal){
        // a leaf node may be under-sized, but an internal node will never be
        // int posG = cur->findFirstGreaterInInternal(data, isConstant); // pos is the exact subtree id to look up in
        int posG = cur->findFirstGtEqInInternal(data, cmpColNum, isConstant); // ! 允许重复键值的后果,使用firstGtEq而非firstGt
        cur = GetTreeNode(cur, *cur->NodePtrAt(posG));
        cur->posInParent = posG;
    }
    // now at the leaf node
    pos = cur->findFirstEqGreaterInLeaf(data, cmpColNum, isConstant);
    node = cur;
}

/**
 * 是否存在一个索引值等于data?是则返回true,否则返回false
 * ? cmpColNum == 0 => 搜索到最左叶子
*/
bool BplusTree::_search(const uchar* data, BplusTreeNode*& node, int& pos, int cmpColNum, bool isConstant, const uchar* cmps){
    if(cmpColNum == -1)
        cmpColNum = colNum;
    bool multiCmp = cmps != nullptr;
    _rawSearch(data, node, pos, cmpColNum, isConstant);
    if(pos == node->size){
        if(*node->NextLeafPtr() != 0){
            node = GetTreeNode(nullptr, *node->NextLeafPtr());
            pos = node->findFirstEqGreaterInLeaf(data, cmpColNum, isConstant, cmps);
            if(pos == node->size)
                return false;
        }
        else
            return false;
    }
    if(!multiCmp && !DataType::compareArr(node->KeynPtrAt(pos), data, header->attrType, header->attrLenth, cmpColNum, Comparator::Eq, false, isConstant) ||
    multiCmp && !DataType::compareArrMultiOp(node->KeynPtrAt(pos), data, header->attrType, header->attrLenth, cmpColNum, cmps, false, isConstant))
        return false;
    return true;
}

/**
 * 只有当索引值和RID都相等时,才返回true
*/
bool BplusTree::_preciseSearch(const uchar* data, BplusTreeNode*& node, int& pos, const RID& rid, int cmpColNum, bool isConstant){
    if(cmpColNum == -1)
        cmpColNum = colNum;
    if(!_search(data, node, pos, cmpColNum, isConstant))
        return false;
    do{
        uchar* ridAddr = node->KeynPtrAt(pos) + header->recordLenth;
        uint dstPage = *(uint*)ridAddr, dstSlot = *(uint*)(ridAddr + 4);
        if(dstPage == rid.PageNum && dstSlot == rid.SlotNum)
            return true;
    }while(MoveNext(node, pos, Comparator::Eq, cmpColNum, data));
}

// TODO: 如果树高太大,前面的parent可能会被bpm释放  使用checkBuffer()?
// insertion of duplicate record is permitted
bool BplusTree::Insert(const uchar* data, const RID& rid){
    header->recordNum++;
    UpdateRecordNum();
    BplusTreeNode* node = nullptr;
    int pos = -1;
    bool sameValue = _search(data, node, pos); // reach the leaf node
    if(header->isUnique && sameValue)
        return false;
    if(node->size < header->leafCap){ // free space
        node->InsertKeynPtrAt(pos, data, rid); //* branch NO.1
        node->syncWithBuffer();
    }
    else{ // the leaf node is full
        // if(node->parent == nullptr)
            node->parent = GetTreeNode(nullptr, node->parentPage);
        // split the leaf node first
        BplusTreeNode* right = CreateTreeNode(node->parent, BplusTreeNode::Leaf);
        // maintain linklist of leaves
        *right->NextLeafPtr() = *node->NextLeafPtr();
        *right->PrevLeafPtr() = node->page;
        *node->NextLeafPtr() = right->page;
        // splitting
        int leftsize = (node->size + 1) / 2;
        int insertPos = pos;
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
        while(node->parentPage != 0){
            // if(node->parent == nullptr)
                node->parent = GetTreeNode(nullptr, node->parentPage);
            BplusTreeNode* pNode = node->parent;
            // int overflowPos = pNode->findFirstGreaterInInternal(overflowKey);
            int overflowPos = node->posInParent; // ? check if works
            if(pNode->size < header->internalCap - 1){ // internal node has free space
                // insertion
                pNode->InsertKeyAt(overflowPos, overflowKey);
                pNode->InsertNodePtrAt(overflowPos + 1, right->page);
                pNode->syncWithBuffer();
                // set parent info
                int childrenCount = pNode->size + 1;
                uint* childIter = pNode->NodePtrAt(overflowPos + 1);
                for(int i = overflowPos + 1; i < childrenCount; i++, childIter++)
                    ChangeParent(*childIter, pNode->page, i);
                // // set parent info of right
                // right->posInParent = overflowPos + 1;
                // right->syncWithBuffer();
                return true; //* Branch NO.2
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
                    *ofRight->NodePtrAt(0) = right->page;
                    memcpy(ofRight->NodePtrAt(1), pNode->NodePtrAt(ofLeftSize + 1), (oldSize - ofLeftSize) * 4);
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
        // nodes.pop_back(); // pop out the new root
        // nodes.push_back(node); // node is not the root anymore, add it to the memory manager
        header->rootPage = root->page;
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
    return true;
}

bool BplusTree::Search(const uchar* data, const RID& rid){ // TODO: compare equal
    BplusTreeNode* node = nullptr;
    int pos = -1;
    return _preciseSearch(data, node, pos, rid);
    // _search(data, node, pos);
    // if(pos == node->size){
    //     if(*node->NextLeafPtr() != 0){
    //         node = GetTreeNode(nullptr, *node->NextLeafPtr());
    //         pos = node->findFirstEqGreaterInLeaf(data);
    //         if(pos == node->size)
    //             return false;
    //     }
    //     else
    //         return false;
    // }
    // if(!DataType::compareArr(node->KeynPtrAt(pos), data, header->attrType, header->attrLenth, colNum, Comparator::Eq))
    //     return false;
    // bool found = false;
    // do{
    //     uint* curRID = (uint*)(node->KeynPtrAt(pos) + header->recordLenth);
    //     if(curRID[0] == rid.GetPageNum() && curRID[1] == rid.GetSlotNum()){
    //         found = true;
    //         break;
    //     }
    // }while(MoveNext(node, pos, Comparator::Eq, colNum));
    // return found;
}

bool BplusTree::ValueSearch(const uchar* data, RID* rid, int cmpColNum){
    BplusTreeNode* node = nullptr;
    int pos = -1;
    if(cmpColNum == -1)
        cmpColNum = colNum;
    return _search(data, node, pos, cmpColNum, true); // ? data is constant
    // ? no needed ?
    if(pos == node->size){
        if(*node->NextLeafPtr() != 0){
            node = GetTreeNode(nullptr, *node->NextLeafPtr());
            pos = node->findFirstEqGreaterInLeaf(data, cmpColNum, true);
            if(pos == node->size)
                return false;
        }
        else
            return false;
    }
    if(!DataType::compareArr(node->KeynPtrAt(pos), data, header->attrType, header->attrLenth, cmpColNum, Comparator::Eq, false, true))
        return false;
    uint* curRID = (uint*)(node->KeynPtrAt(pos) + header->recordLenth);
    rid->PageNum = curRID[0];
    rid->SlotNum = curRID[1];
    return true;
}

/**
 * 按照给定的条件,检索record lowerCmp begin && record upperCmp end的记录,并把其RID写入results中
 * 8种分类:
 * 有无Eq * 有无上界 * 有无下界 = 8, 下面只涵盖了7种, 不包括全都没有的
 * 三种情况:
 * 1. begin == nullptr, 仅有上界(仅有rangeCol上界/Eq+上界), 使用0定位, 使用end/upperCmp/eqCols + 1移动
 * 2. begin != nullptr, lowerCmp == Eq, end == nullptr, 仅有Eq, 使用eqCols定位, 使用begin/Eq/eqCols移动
 * 3. begin != nullptr, lowerCmp != Eq, end == nullptr, 仅有下界(仅有rangeCol下界/Eq和下界), 使用eqCols + 1定位, 使用begin/lowerCmp/eqCols + 1移动
 * 4. begin != nullptr, lowerCmp != Eq, end != nullptr, 有上下界(有/无Eq), 使用eqCols + 1定位, 使用end/upperCmp/eqCols + 1移动
*/
void BplusTree::ValueSelect(int eqCols, const uchar* begin, uchar lowerCmp, const uchar* end, uchar upperCmp, std::vector<RID> &results){
    BplusTreeNode* node = root;
    int pos = -1;
    RID tmpRID;
    uchar cmps[eqCols + 1] = {0};
    memset(cmps, Comparator::Eq, sizeof(cmps));
    if(begin != nullptr){ // case 2, 3, 4, default is case 2
        int moveCols = eqCols; // this is true for case 2
        const uchar* moveData = begin; // this is true for case 2 and case 3
        if(lowerCmp != Comparator::Eq){ // range col有下界要求 case 3, case 4
            moveCols++;
        }
        // ? 执行的实际上是混合比较,前eqCols个字段是Eq,最后一个字段(如果有的话)是lowerCmp/upperCmp
        _search(begin, node, pos, moveCols, true); // 找不到也无妨,例如_search在>0条件下,找不到0回返回false
        if(node->size == pos)
            return;
        if(lowerCmp == Comparator::Gt){
            while(MoveNext(node, pos, Comparator::Eq, moveCols, begin)){} // 跳过所有值和begin相等的索引值
            // 此时,pos所在的索引值仍然有可能等于begin
            if(DataType::compareArr(node->KeynPtrAt(pos), begin, header->attrType, header->attrLenth, moveCols, Comparator::Eq, false, true) &&
            !MoveNext(node, pos, Comparator::Any, 0, begin))
                return;
        }
        if(end != nullptr){ // case 4
            cmps[eqCols] = upperCmp;
            moveData = end;
        }
        else if(lowerCmp != Comparator::Eq) // case 3
            cmps[eqCols] = lowerCmp;
        // _search返回false,Gt导致跳过部分记录,rangeCol. 都会导致pos处的索引不符合要求
        if(!DataType::compareArrMultiOp(node->KeynPtrAt(pos), begin, header->attrType, header->attrLenth, eqCols, cmps, false, true))
            return;
        do{
            uchar* ridAddr = node->KeynPtrAt(pos) + header->recordLenth;
            tmpRID.PageNum = *(uint*)ridAddr;
            tmpRID.SlotNum = *(uint*)(ridAddr + 4);
            results.push_back(tmpRID);
        }while(MoveNext(node, pos, cmps, moveCols, moveData));
    }
    else{ // case 1
        uchar cmps[eqCols + 1] = {0};
        memset(cmps, Comparator::Eq, eqCols);
        cmps[eqCols] = upperCmp;
        int moveCols = eqCols + 1;
        if(!_search(nullptr, node, pos, 0, true))
            return;
        // 可能第一条记录就不符合要求
        if(!DataType::compareArrMultiOp(node->KeynPtrAt(pos), end, header->attrType, header->attrLenth, moveCols, cmps, false, true))
            return;
        do{
            uchar* ridAddr = node->KeynPtrAt(pos) + header->recordLenth;
            tmpRID.PageNum = *(uint*)ridAddr;
            tmpRID.SlotNum = *(uint*)(ridAddr + 4);
            results.push_back(tmpRID);
        }while(MoveNext(node, pos, cmps, moveCols, end));
    }
}

void BplusTree::Remove(const uchar* data, const RID& rid){
    BplusTreeNode* node = nullptr;
    int pos = -1;
    if(!_preciseSearch(data, node, pos, rid, colNum))
        return;
    // _search(data, node, pos);
    // // now at a leaf node
    // if(pos == node->size){ // no match is found
    //     if(*node->NextLeafPtr() != 0){
    //         node = GetTreeNode(nullptr, *node->NextLeafPtr());
    //         pos = node->findFirstEqGreaterInLeaf(data);
    //         if(pos == node->size)
    //             return;
    //     }
    //     else
    //         return;
    // }
    // bool found = false;
    // do{
    //     uint* curRID = (uint*)(node->KeynPtrAt(pos) + header->recordLenth);
    //     if(curRID[0] == rid.GetPageNum() && curRID[1] == rid.GetSlotNum()){
    //         found = true;
    //         break;
    //     }
    // }while(MoveNext(node, pos, Comparator::Eq));
    // if(!found) // there is no record with RID rid among records with key data
    //     return;
    header->recordNum--;
    UpdateRecordNum();
    // remove
    if(node->size > leafMinKey){ // simple case: no underflow
        node->RemoveKeynPtrAt(pos);
        node->syncWithBuffer();
        // no need to update parent's key
    }
    else if(node->parentPage != 0){ // leaf has exactly leafMinKey keys, underflow
        BplusTreeNode *pNode =  (node->parent = GetTreeNode(nullptr, node->parentPage)), 
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
                    // reloadRoot = true;
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
            pNode = node->parent = GetTreeNode(nullptr, node->parentPage);
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
                    for(int i = 0; i <= rmPos + 1; i++, childIter++)
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
                    memcpy(node->NodePtrAt(rmPos + 1), node->NodePtrAt(rmPos + 2), (internalMinKey - rmPos - 1) * 4); // !added on 1.9
                    memcpy(node->NodePtrAt(internalMinKey), rightNode->NodePtrAt(0), 4);
                    rightNode->RemoveKeyAt(0);
                    rightNode->RemoveNodePtrAt(0);
                    // set parent info
                    for(int i = rmPos + 1; i <= internalMinKey; i++) // !addded on 1.9
                        ChangeParent(*node->NodePtrAt(i), node->page, i);
                    // ChangeParent(*node->NodePtrAt(internalMinKey - 1), node->page, internalMinKey - 1);
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