#ifndef INDEXHEADER_H
#define INDEXHEADER_H
#include "../MyDB/BaseHeader.h"
#include <cstring>

/**
 * NOTE: strictly speaking this is not a header, just a method for serialization & deserialization
 * This "header" is stored on a data page in an index table, marking the root node of a B+ tree
*/
class IndexHeader : public BaseHeader{
    public:
        // Note: here the recordLenth indicates the size of a key, not including the ptr next to it
        // uint recordLenth = 0;
        //// uint slotNum = 0;
        // uint recordNum = 0;
        //// uint exploitedNum = 0;
        // uint nullMask = 0;

        // capacity of internal node and leaf node (in the number of stored subtrees or RIDs)
        // internalCap = (PAGE_SIZE - BplusTreeNode::reservedBytes + recordLenth) / (recordLenth + 4)
        // leafCap = (PAGE_SIZE - BplusTreeNode::reservedBytes - 8) / (recordLenth + 8) // ? Now the leaves form a bidirectional linked list
        // ! Maybe bidirectional linked list is not necessary. We can replace the backward pointer with FindSmallestRecord() and FindLargestRecord()
        uint internalCap, leafCap; // order of the internal node and leaf node
        uint rootPage; // the pageID of root node
        // uint attrLenth[MAX_COL_NUM] = {0};
        // uchar attrType[MAX_COL_NUM] = {0};
        uchar tableName[MAX_TABLE_NAME_LEN] = {0}; // name of the table this index belongs to
        uchar indexColID[MAX_COL_NUM] = {0}; // the id of the indexed columns

        IndexHeader(){
            memset(indexColID, COL_ID_NONE, MAX_COL_NUM);
        }

        const static int lenth = sizeof(uint) * (6 + MAX_COL_NUM) +
            MAX_TABLE_NAME_LEN + 
            MAX_COL_NUM * 2;
        
        int GetLenth()override{
            return lenth;
        }

        void ToString(void* dst)override{
            uint* uintPtr = (uint*)dst;
            uintPtr[0] = recordLenth; // length of a key
            uintPtr[1] = recordNum; // number of elements in all leaf nodes
            uintPtr[2] = nullMask; // nullable attributes need an extra byte to record if it is null. null is treated as the smallest value in cmp
            uintPtr[3] = internalCap;
            uintPtr[4] = leafCap;
            uintPtr[5] = rootPage;
            uintPtr += 6;
            memcpy(uintPtr, attrLenth, MAX_COL_NUM * sizeof(uint));
            uintPtr += MAX_COL_NUM;

            uchar* charPtr = (uchar*)uintPtr;
            
            memcpy(charPtr, attrType, MAX_COL_NUM);
            charPtr += MAX_COL_NUM;

            memcpy(charPtr, tableName, MAX_TABLE_NAME_LEN);
            charPtr += MAX_TABLE_NAME_LEN;

            memcpy(charPtr, indexColID, MAX_COL_NUM);
        }

        void FromString(const void* src)override{
            uint *uintPtr = (uint*)src;
            recordLenth = uintPtr[0];
            recordNum = uintPtr[1];
            nullMask = uintPtr[2];
            internalCap = uintPtr[3];
            leafCap = uintPtr[4];
            rootPage = uintPtr[5];
            uintPtr += 6;
            memcpy(attrLenth, uintPtr, MAX_COL_NUM * sizeof(uint));
            uintPtr += MAX_COL_NUM;

            uchar *charPtr = (uchar*)uintPtr;

            memcpy(attrType, charPtr, MAX_COL_NUM);
            charPtr += MAX_COL_NUM;

            memcpy(tableName, charPtr, MAX_TABLE_NAME_LEN);
            charPtr += MAX_TABLE_NAME_LEN;

            memcpy(indexColID, charPtr, MAX_COL_NUM);
        }
};

#endif // INDEXHEADER_H