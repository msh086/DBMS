#ifndef BPLUSTREE_H
#define BPLUSTREE_H
#include "BplusTreeNode.h"
#include "IndexHeader.h"
#include "../RM/RID.h"
#include "../MyDB/Table.h"
#include <vector>
#include <queue>
#include <stdlib.h>
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
        void _search(const uchar* data, BplusTreeNode*& node, int& pos, bool isConstant = false);
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

        //// helper variable
        //// when set to true, indicates that this->root need to be reloaded
        // bool reloadRoot = false;

        void CalcKeyLength(){
            header->recordLenth = 4; // null word
            colNum = 0;
            for(int i = 0; i < MAX_COL_NUM; i++){
                if(header->attrType[i] != DataType::NONE){
                    header->recordLenth += DataType::lengthOf(header->attrType[i], header->attrLenth[i]);
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
        // memory of header will be freed by B+ tree at its destruction
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
            // header->internalCap = 5;
            // header->leafCap = 4;
            // ! end
            
            // root init
            root = CreateTreeNode(nullptr, BplusTreeNode::Leaf);
            // nodes.pop_back(); // pop the root node from stack

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
            root = GetTreeNode(nullptr, header->rootPage);
            // nodes.pop_back();
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
                    if(DataType::compareArr(node->KeynPtrAt(pos), nextNode->KeynPtrAt(0), header->attrType, header->attrLenth, colNum, mode)){
                        node = nextNode;
                        pos = 0;
                        return true;
                    }
                    return false;
                }
            }
            // no at the tail of current leaf node
            if(DataType::compareArr(node->KeynPtrAt(pos), node->KeynPtrAt(pos + 1), header->attrType, header->attrLenth, colNum, mode)){
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
            root = GetTreeNode(nullptr, header->rootPage);
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
        bool Search(const uchar* data, const RID& rid); // returns true iff both data and rid match
        void Remove(const uchar* data, const RID& rid);
        bool ValueSearch(const uchar* data, RID* rid); // returns true iff data matches, store found RID in rid
        
        // Memory-safe methods for Insert, Search and Remove
        void SafeInsert(const uchar* data, const RID& rid){
            Insert(data, rid);
            ClearAndWriteBackOpenedNodes();
        }

        void SafeRemove(const uchar* data, const RID& rid){
            Remove(data, rid);
            ClearAndWriteBackOpenedNodes();
            RemoveNodes();
            // if(reloadRoot){ // root has been deleted with other opened nodes
            //     root = GetTreeNode(nullptr, header->rootPage);
            //     reloadRoot = false;
            // }
        }

        bool SafeSearch(const uchar* data, const RID& rid){
            bool ret = Search(data, rid);
            ClearAndWriteBackOpenedNodes();
            return ret;
        }

        // NOTE: data是常量记录,即varchar以字符串方式存储
        bool SafeValueSearch(const uchar* data, RID* rid){
            bool ret = ValueSearch(data, rid);
            ClearAndWriteBackOpenedNodes();
            return ret;
        }

        uint TreeHeaderPage(){
            return page;
        }

        void DeleteTreeFromDisk(){
            root->checkBuffer();
            RID rid = RID();
            rid.SlotNum = 0;
            std::queue<uint> Q = std::queue<uint>();
            Q.push(header->rootPage);
            while(!Q.empty()){
                BplusTreeNode* node = GetTreeNode(nullptr, Q.front());
                if(node->type == BplusTreeNode::Internal){
                    for(int i = 0; i < node->size; i++)
                        Q.push(*node->NodePtrAt(i));
                }
                rid.PageNum = Q.front();
                table->DeleteRecord(rid);
                Q.pop();
            }
            for(BplusTreeNode* node : nodes)
                delete node;
            nodes.clear();
            delete root;
            root = nullptr;
            rid.PageNum = page;
            table->DeleteRecord(rid);
            delete header;
            header = nullptr;
        }

        ~BplusTree(){
            if(header)
                delete header;
            if(root){
                root->writeBack();
                delete root;
            }
        }

        struct Checker{
            uint page;
            uint parentPage;
            uint posInParent;
            bool hasLeftBound = false, hasRightBound = false;
            int leftBound, rightBound; // [leftBound, righitBound)
            bool hasNext = false;
            uint nextLeaf;
            Checker(uint page, uint parent, uint pos, bool hasL, bool hasR, int L, int R, bool hasN, uint N){
                Reset(page, parent, pos, hasL, hasR, L, R, hasN, N);
            }

            Checker(){}

            void Reset(uint page, uint parent, uint pos, bool hasL, bool hasR, int L, int R, bool hasN, uint N){
                this->page = page;
                parentPage = parent;
                posInParent = pos;
                hasLeftBound = hasL;
                hasRightBound = hasR;
                leftBound = L;
                rightBound = R;
                hasNext = hasN;
                nextLeaf = N;
            }
        };
#ifdef DEBUG
        // skip null word and get the real value
        int extractInt(void* src){
            return *(int*)((char*)src + 4);
        }
        // print the tree's info
        void DebugPrint(){
            printf("index info:\n");
            printf("key length: %u, root page: %u, element count: %u, internal order: %u, leaf order: %u\n", 
                header->recordLenth, header->rootPage, header->recordNum, header->internalCap, header->leafCap);
            Checker buf[header->recordNum + 1];
            int head = 0, pos = 0;
            buf[pos++].Reset(header->rootPage, 0, 0xffff, false, false, 0, 0, false, 0);
            if(pos == header->recordNum + 1)
                pos = 0;
            while(head != pos){
                BplusTreeNode* node = GetTreeNode(nullptr, buf[head].page);

                if(node->parentPage != buf[head].parentPage || (node->parentPage != 0 && node->posInParent != buf[head].posInParent))
                    printf("*****Parent Info Doesn't Match*****\n");

                printf("node info:\n");
                printf("type: %hhu, size: %hu, page: %u, parentPage: %u, posInParent: %hu", 
                    node->type, node->size, node->page, node->parentPage, node->posInParent);
                if(node->type == BplusTreeNode::Internal)
                    printf("\n");
                else
                    printf(", next leaf: %u\n", *node->NextLeafPtr());
                if(node->type == BplusTreeNode::Internal){ // internal
                    printf("keys: ");
                    bool hasPrev = buf[head].hasLeftBound;
                    uint prevVal = buf[head].leftBound;
                    for(int i = 0; i < node->size; i++){
                        if(hasPrev && prevVal > extractInt(node->KeyAt(i)))
                            printf("*****Value Error*****\n");
                        hasPrev = true;
                        prevVal = extractInt(node->KeyAt(i));
                        printf("%u ", extractInt(node->KeyAt(i)));
                    }
                    if(buf[head].hasRightBound && prevVal >= buf[head].rightBound)
                        printf("*****Value Error*****\n");
                    printf("\nptrs: ");
                    for(int i = 0; i <= node->size; i++){
                        printf("%u ", *node->NodePtrAt(i));
                        bool hasLeft, hasRight;
                        int leftBound, rightBound;
                        if(i == 0){
                            hasLeft = buf[head].hasLeftBound;
                            leftBound = buf[head].leftBound;
                        }
                        else{
                            hasLeft = true;
                            leftBound = extractInt(node->KeyAt(i - 1));
                        }
                        if(i == node->size){
                            hasRight = buf[head].hasRightBound;
                            rightBound = buf[head].rightBound;
                        }
                        else{
                            hasRight = true;
                            rightBound = extractInt(node->KeyAt(i));
                        }
                        buf[pos++].Reset(*node->NodePtrAt(i), buf[head].page, i, hasLeft, hasRight, leftBound, rightBound, false, 0);
                        if(pos == header->recordNum + 1)
                            pos = 0;
                    }
                    printf("\n");
                }
                else{ // leaf
                    bool hasPrev = buf[head].hasLeftBound;
                    int prevVal = buf[head].leftBound;
                    for(int i = 0; i < node->size; i++){
                        if(hasPrev && prevVal > extractInt(node->KeynPtrAt(i)))
                            printf("*****Value Error In Leaf*****\n");
                        hasPrev = true;
                        prevVal = extractInt(node->KeynPtrAt(i));
                        uchar* tmpPtr = node->KeynPtrAt(i); // extract manually later
                        printf("(%u, %u, %u)\n", *(uint*)(tmpPtr + 4), *(uint*)(tmpPtr + header->recordLenth), *(uint*)(tmpPtr + 4 + header->recordLenth));
                    }
                    if(buf[head].hasRightBound && prevVal >= buf[head].rightBound)
                        printf("*****Value Error In Leaf*****\n");
                }
                // end internal & leaf
                head++;
                if(head == header->recordNum + 1)
                    head = 0;
            }
            this->ClearAndWriteBackOpenedNodes();
            // root = GetTreeNode(nullptr, header->rootPage);
        }
#endif
};

#endif // BPLUSTREE_H