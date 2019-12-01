#ifndef BASEHEADER_H
#define BASEHEADER_H
#include "../utils/pagedef.h"
/**
 * Base class for headers, providing basic interface
 * Inherited by Header, the header of ordinary data tables,
 * and by IndexHeader, the header of index tables using B+ tree
*/
class BaseHeader{
    public:
        uint recordLenth = 0;// The length of a record in BYTES. Records should have length of full bytes. Equals to the sum of attrLenth
        uint slotNum = 0;// The number of records a page can hold, equals to PAGE_SIZE / recordLenth
        uint recordNum = 0;
        uint exploitedNum = 0;// If a table's slots are: 1 0 0 1 0 1 0 0 0 0..., then the exploitedNum is 6 while recordNum is 3. 0 for empty
        uint nullMask = -1; // ? 默认情况下，字段允许插入null值
        // change to ushort
        // attrLenth doesn't include the null byte
        ushort attrLenth[MAX_COL_NUM] = {0};
        uchar attrType[MAX_COL_NUM] = {0}; // The number of attributes equals to the number of non-zero elements in this array
        virtual void ToString(void* dst) = 0;
        virtual void FromString(const void* src) = 0;
        virtual int GetLenth() = 0;
#ifdef DEBUG
        virtual void DebugPrint(){
            printf("BaseHeader debug printer\n");
        }
#endif
};

#endif // BASEHEADER_H