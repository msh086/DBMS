#ifndef HEADER_H
#define HEADER_H
#include "../RM/DataType.h"
#include <cstring>
#include "BaseHeader.h"
/**
 * The class corresponding to the header page in a table.
*/
class Header : public BaseHeader{
    public:

        // if you add or remove any member here, update them in lenth, ToString, FromString and Table.h

        // uint recordLenth = 0;// The length of a record in BYTES. Records should have length of full bytes. Equals to the sum of attrLenth
        // uint slotNum = 0;// The number of records a page can hold, equals to PAGE_SIZE / recordLenth
        // uint recordNum = 0;
        // uint exploitedNum = 0;// If a table's slots are: 1 0 0 1 0 1 0 0 0 0..., then the exploitedNum is 6 while recordNum is 3. 0 for empty
        // uint nullMask = 0;
        uint primaryKeyMask = 0; // A cluster primary key can include multiple columns
        uint foreignKeyMask = 0;
        uint defaultKeyMask = 0; // The template record for `default` is always stored as (1, 0) and is invisible & inchangable by query
        // For attrLenth, if we store varchar locally, each element will take a uint
        // If we store varchar as a pointer to their real location, each element can be a char
        // For varchar no longer than 255 bits, they can be stored in-place like chars.
        // For varchar longer than this limit, we can do the following thing:
        // In a global table VARCHAR_TB, break a varchar into parts of VARCHAR_FRAG_LEN bytes and store a varchar as a linked list
        // A record in VARCHAR_TB looks like [(varchar fragment data), nextPage, nextSlot, length, isDefault].
        // So record length is VARCHAR_FRAG_LEN + 4 + 4 + 2 (8192B = 2^13, so length can be recorded in 14 bits; isDefault flag needs only one bit)
        // While in {tablename}, only the RID of the head record in VARCHAR_TB is stored, which requires 8B
        // We can reserve VARCHAR_DB as a privileged table name and throw an exception when user tries to create, access or delete it.
        // uint attrLenth[MAX_COL_NUM] = {0}; // The number of attributes equals to the number of non-zero elements in this array
        // uchar attrType[MAX_COL_NUM] = {0};
        uchar attrName[MAX_COL_NUM][MAX_ATTRI_NAME_LEN] = {{0}};
        uchar foreignTable[MAX_COL_NUM][MAX_TABLE_NAME_LEN] = {{0}};
        uchar foreignKeyID[MAX_COL_NUM]={0};
        uchar refTables[MAX_FOREIGN_TIME][MAX_TABLE_NAME_LEN]={0};

        const static int lenth = sizeof(uint) * (8 + MAX_COL_NUM) +
            MAX_COL_NUM * MAX_ATTRI_NAME_LEN +
            MAX_COL_NUM * MAX_TABLE_NAME_LEN +
            MAX_COL_NUM * 2 +
            MAX_FOREIGN_TIME * MAX_TABLE_NAME_LEN;

#ifdef DEBUG
        void DebugPrint()override{
            printf("recordLenth = %u\n", recordLenth);
            printf("slotNum = %u\n", slotNum);
            printf("recordNum = %u\n", recordNum);
            printf("exploitedNum = %u\n", exploitedNum);
            printf("nullMask = %u\n", nullMask);
            printf("primaryKeyMask = %u\n", primaryKeyMask);
            printf("foreignKeyMask = %u\n", foreignKeyMask);
            printf("defaultKeyMask = %u\n", defaultKeyMask);
            printf("***attrLenth:\n    ");
            for(int i = 0; i < MAX_COL_NUM; i ++)
                printf("(%d, %u) ", i, attrLenth[i]);
            printf("\n");
            printf("***attrType:\n    ");
            for(int i = 0; i < MAX_COL_NUM; i ++)
                printf("(%d, %s) ", i, DataType::NameofType(attrType[i]));
            printf("\n");
            printf("***attriName:\n    ");
            for(int i = 0; i < MAX_COL_NUM; i ++)
                printf("(%d, %.*s) ", i, MAX_ATTRI_NAME_LEN, attrName[i]);
            printf("\n");
            printf("***foreignTable:\n    ");
            for(int i = 0; i < MAX_COL_NUM; i++)
                printf("(%d, %.*s) ", i, MAX_TABLE_NAME_LEN, foreignTable[i]);
            printf("\n");
            printf("***foreignKeyID:\n    ");
            for(int i = 0; i < MAX_COL_NUM; i++)
                printf("(%d, %d) ", i, foreignKeyID[i]);
            printf("\n");
            printf("***refTables:\n    ");
            for(int i = 0; i < MAX_FOREIGN_TIME; i++)
                printf("(%d, %.*s) ", i, MAX_TABLE_NAME_LEN, refTables[i]);
            printf("\n");
            printf("<<<End of table\n");
            // TODO: more information?
        }
#endif
        int GetLenth()override{
            return lenth;
        }

        void ToString(void* dst) override {
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

            uchar* charPtr = (uchar*)uintPtr;
            
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
        void FromString(const void* src) override{
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

            uchar *charPtr = (uchar*)uintPtr;

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