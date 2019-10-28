#ifndef HEADER_H
#define HEADER_H
#include "../utils/pagedef.h"
#include <cstring>
/**
 * The class corresponding to the header page in a table.
*/
class Header{
    public:
        uint recordLenth = 0;// The length of a record in BYTES. Records should have length of full bytes. Equals to the sum of attrLenth
        uint slotNum = 0;// The number of records a page can hold, equals to PAGE_SIZE / recordLenth
        uint recordNum = 0;
        uint exploitedNum = 0;// If a table's slots are: 1 0 0 1 0 1 0 0 0 0..., then the exploitedNum is 6 while recordNum is 3
        uint nullMask = 0;
        uint primaryKeyMask = 0;
        uint foreignKeyMask = 0;
        uint defaultKeyMask = 0; // The template record for `default` is always stored as (1, 0) and is invisible & inchangable by query
        // For attrLenth, if we store varchar locally, each element will take a uint
        // If we store varchar as a pointer to their real location, each element can be a char
        // An idea: in another table {tablename}_VAR, break a varchar into parts of VARCHAR_FRAG_LEN bytes and store a varchar as a linked list
        // A record in {tablename}_VAR looks like [(varchar fragment data), nextPage, nextSlot, length]. So record length is VARCHAR_FRAG_LEN + 12
        // While in {tablename}, only the RID of the head record in {tablename}_VAR is stored
        uint attrLenth[MAX_COL_NUM] = {0}; // The number of attributes equals to the number of non-zero elements in this array
        char attrType[MAX_COL_NUM] = "";
        char attrName[MAX_COL_NUM][MAX_ATTRI_NAME_LEN] = {{""}};
        char foreignTable[MAX_COL_NUM][MAX_TABLE_NAME_LEN] = {{""}};
        char foreignKeyID[MAX_COL_NUM]="";
        char refTables[MAX_FOREIGN_TIME][MAX_TABLE_NAME_LEN]={""};

        const static int lenth = sizeof(uint) * (8 + MAX_COL_NUM) +
            MAX_COL_NUM * MAX_ATTRI_NAME_LEN +
            MAX_COL_NUM * MAX_TABLE_NAME_LEN +
            MAX_COL_NUM * 2 +
            MAX_FOREIGN_TIME * MAX_TABLE_NAME_LEN;

        void ToString(void* dst){
            uint* uintPtr = (uint*)dst;
            uintPtr[0] = recordLenth;
            uintPtr[1] = slotNum;
            uintPtr[2] = recordNum;
            uintPtr[3] = exploitedNum;
            uintPtr[4] = nullMask;
            uintPtr[5] = primaryKeyMask;
            uintPtr[6] = foreignKeyMask;
            uintPtr[7] = defaultKeyMask;
            uintPtr += 8;
            memcpy(uintPtr, attrLenth, MAX_COL_NUM * sizeof(uint));
            uintPtr += MAX_COL_NUM;

            char* charPtr = (char*)uintPtr;
            
            memcpy(charPtr, attrType, MAX_COL_NUM);
            charPtr += MAX_COL_NUM;

            memcpy(charPtr, attrName, MAX_COL_NUM * MAX_ATTRI_NAME_LEN);
            charPtr += MAX_COL_NUM * MAX_ATTRI_NAME_LEN;

            memcpy(charPtr, foreignTable, MAX_COL_NUM * MAX_TABLE_NAME_LEN);
            charPtr += MAX_COL_NUM * MAX_TABLE_NAME_LEN;

            memcpy(charPtr, foreignKeyID, MAX_COL_NUM);
            charPtr += MAX_COL_NUM;

            memcpy(charPtr, refTables, MAX_FOREIGN_TIME * MAX_TABLE_NAME_LEN);
        }
        void FromString(const void* src){
            uint *uintPtr = (uint*)src;
            recordLenth = uintPtr[0];
            slotNum = uintPtr[1];
            recordNum = uintPtr[2];
            exploitedNum = uintPtr[3];
            nullMask = uintPtr[4];
            primaryKeyMask = uintPtr[5];
            foreignKeyMask = uintPtr[6];
            defaultKeyMask = uintPtr[7];
            uintPtr += 8;
            memcpy(attrLenth, uintPtr, MAX_COL_NUM * sizeof(uint));
            uintPtr += MAX_COL_NUM;

            char *charPtr = (char*)uintPtr;

            memcpy(attrType, charPtr, MAX_COL_NUM);
            charPtr += MAX_COL_NUM;

            memcpy(attrName, charPtr, MAX_COL_NUM * MAX_ATTRI_NAME_LEN);
            charPtr += MAX_COL_NUM * MAX_ATTRI_NAME_LEN;

            memcpy(foreignTable, charPtr, MAX_COL_NUM * MAX_TABLE_NAME_LEN);
            charPtr += MAX_COL_NUM * MAX_TABLE_NAME_LEN;

            memcpy(foreignKeyID, charPtr, MAX_COL_NUM);
            charPtr += MAX_COL_NUM;

            memcpy(refTables, charPtr, MAX_FOREIGN_TIME * MAX_TABLE_NAME_LEN);
        }
};

#endif