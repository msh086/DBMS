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
        // uint recordLenth = 0;
        //// uint slotNum = 0;
        // uint recordNum = 0;
        //// uint exploitedNum = 0;
        // uint nullMask = 0;
        uint internalOrder, leafOrder; // order of internal node and leaf node
        // capacity of internal node and leaf node
        // internalCap = (PAGE_SIZE - 5 + recordLenth) / (recordLenth + 4)
        // leafCap = (PAGE_SIZE - 5) / (recordLenth + 8)
        uint internalCap, leafCap;
        // uint attrLenth[MAX_COL_NUM] = {0};
        // uchar attrType[MAX_COL_NUM] = {0};
        uchar tableName[MAX_TABLE_NAME_LEN] = {0}; // name of the table this index belongs to
        uchar indexColID[MAX_COL_NUM] = {0}; // the id of the indexed columns

        const static int lenth = sizeof(uint) * (7 + MAX_COL_NUM) +
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
            uintPtr[3] = internalOrder;
            uintPtr[4] = leafOrder;
            uintPtr[5] = internalCap;
            uintPtr[6] = leafCap;
            uintPtr += 7;
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
            internalOrder = uintPtr[3];
            leafOrder = uintPtr[4];
            internalCap = uintPtr[5];
            leafCap = uintPtr[6];
            uintPtr += 7;
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