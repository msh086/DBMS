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
        uint primaryKeyMask = 0; // ! unused. Use primaryKeyID instead, the corresponsing primary constraint name is the same as primary index name
        uint foreignKeyMask = 0; // ! unused. Use fkMaster, masterKeyID, slaveKeyID, constraintName instead
        uint defaultKeyMask = 0; // The template record for `default` is always stored as (1, 0) and is invisible & inchangable by query
        // 主键索引b+树的header页
        uint primaryIndexPage = 0;
        // For attrLenth, if we store varchar locally, each element will take a uint
        // If we store varchar as a pointer to their real location, each element can be a char
        // For varchar no longer than 255 bytes, they can be stored in-place like chars, the length of the attribute is its actual size(0~255)
        // For varchar longer than this limit, we can do the following thing:
        // In a global table VARCHAR_TB, break a varchar into parts of VARCHAR_FRAG_LEN bytes and store a varchar as a linked list
        // A record in VARCHAR_TB looks like [nextPage, nextSlot, length, (varchar fragment data)].
        // So record length is VARCHAR_FRAG_LEN + 4 + 4 + 2
        // While in {tablename}, only the RID of the head record in VARCHAR_TB is stored, which requires 8B
        // a value > 255 in attrLenth indicates a long varchar stored in special varchar table
        // We can reserve VARCHAR_DB as a privileged table name and throw an exception when user tries to create, access or delete it.
        // ushort attrLenth[MAX_COL_NUM] = {0};
        // uchar attrType[MAX_COL_NUM] = {0}; // number of attributes in the table = positive values in this array
        uchar attrName[MAX_COL_NUM][MAX_ATTRI_NAME_LEN] = {{0}};
        // 主键的列序号, 主键名称直接使用索引名称
        uchar primaryKeyID[MAX_COL_NUM] = {0};
        // 专门存放主键索引的名称
        uchar primaryIndexName[MAX_INDEX_NAME_LEN] = {0};
        /* 外键约束 */
        // 引用别的表的ID
        uchar fkMaster[MAX_REF_SLAVE_TIME] = {0}; // 用表的id代替表名来节省空间
        //// 别的表中被引用列的ID
        // uint masterKeyID[MAX_REF_SLAVE_TIME] = {0}; // 一个uint是一个32bit位图 // ! 必然是主键,不需要记录
        // 本表中涉及到的列的ID
        uchar slaveKeyID[MAX_REF_SLAVE_TIME][MAX_COL_NUM] = {0}; // ! 顺序是重要的 e.g. foreign key(id1, id2) references master(id1, id2) vs foreign key(id2, id1) references master(id1, id2)
        // 外键约束的名称
        uchar constraintName[MAX_REF_SLAVE_TIME][MAX_CONSTRAINT_NAME_LEN] = {{0}};
        // 被别的表的外键约束引用
        uchar fkSlave[MAX_FK_MASTER_TIME]={0};
        /* 索引 */
        // 索引名字
        uchar indexName[MAX_INDEX_NUM][MAX_INDEX_NAME_LEN] = {{0}};
        // 索引列ID
        uchar indexID[MAX_INDEX_NUM][MAX_COL_NUM] = {0}; // ? 索引列的顺序很重要
        // b+树的RID
        uint bpTreePage[MAX_INDEX_NUM] = {0};

        Header(){
            // set all fkMasters, fkSlaves and indexID to 31, which means invalid table id(none)
            memset(fkMaster, TB_ID_NONE, MAX_REF_SLAVE_TIME);
            memset(fkSlave, TB_ID_NONE, MAX_FK_MASTER_TIME);
            memset(indexID, COL_ID_NONE, MAX_INDEX_NUM * MAX_COL_NUM);
            memset(primaryKeyID, COL_ID_NONE, MAX_COL_NUM);
            memset(slaveKeyID, COL_ID_NONE, MAX_REF_SLAVE_TIME * MAX_COL_NUM);
        }

        /* header的长度 */
        const static int lenth = sizeof(uint) * 9 + // 8 * uint + primaryIndexPage
            sizeof(ushort) * MAX_COL_NUM + // attrLenth
            MAX_COL_NUM + // attrType
            MAX_COL_NUM * MAX_ATTRI_NAME_LEN + // attrName
            MAX_COL_NUM + // primaryKeyID
            MAX_INDEX_NAME_LEN + // primaryIndexName
            MAX_REF_SLAVE_TIME + // fkMaster
            MAX_REF_SLAVE_TIME * MAX_COL_NUM + // slaveKeyID
            MAX_REF_SLAVE_TIME * MAX_CONSTRAINT_NAME_LEN + // constraintName
            MAX_FK_MASTER_TIME + // fkSlave
            MAX_INDEX_NUM * MAX_INDEX_NAME_LEN + // indexName
            MAX_INDEX_NUM * MAX_COL_NUM + // indexID
            MAX_INDEX_NUM * sizeof(uint); // bpTreePage
        
        /* 外键部分的offset */
        const static int fkOffset = sizeof(uint) * 9 + // 8 * uint + primaryIndexPage
            sizeof(ushort) * MAX_COL_NUM + // attrLenth
            MAX_COL_NUM + // attrType
            MAX_COL_NUM * MAX_ATTRI_NAME_LEN + // attrName
            MAX_COL_NUM + // primaryKeyID
            MAX_TABLE_NAME_LEN; // primaryIndexName
        
        /* 索引部分的offset */
        const static int idxOffset = fkOffset +
            MAX_REF_SLAVE_TIME + // fkMaster
            MAX_REF_SLAVE_TIME * MAX_COL_NUM + // slaveKeyID
            MAX_REF_SLAVE_TIME * MAX_CONSTRAINT_NAME_LEN + // constraintName
            MAX_FK_MASTER_TIME;// fkSlave

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
            uintPtr[8] = primaryIndexPage;
            uintPtr += 9;

            uchar* charPtr = (uchar*)uintPtr; // updated for ushort
            memcpy(charPtr, attrLenth, MAX_COL_NUM * sizeof(ushort));
            charPtr += sizeof(ushort) * MAX_COL_NUM;
            
            memcpy(charPtr, attrType, MAX_COL_NUM);
            charPtr += MAX_COL_NUM;

            memcpy(charPtr, attrName, MAX_COL_NUM * MAX_ATTRI_NAME_LEN);
            charPtr += MAX_COL_NUM * MAX_ATTRI_NAME_LEN;

            memcpy(charPtr, primaryKeyID, MAX_COL_NUM);
            charPtr += MAX_COL_NUM;

            memcpy(charPtr, primaryIndexName, MAX_INDEX_NAME_LEN);
            charPtr += MAX_INDEX_NAME_LEN;
            // foreign key part
            memcpy(charPtr, fkMaster, MAX_REF_SLAVE_TIME);
            charPtr += MAX_REF_SLAVE_TIME;

            memcpy(charPtr, slaveKeyID, MAX_REF_SLAVE_TIME * MAX_COL_NUM);
            charPtr += MAX_REF_SLAVE_TIME * MAX_COL_NUM;

            memcpy(charPtr, constraintName, MAX_REF_SLAVE_TIME * MAX_CONSTRAINT_NAME_LEN);
            charPtr += MAX_REF_SLAVE_TIME * MAX_CONSTRAINT_NAME_LEN;
            // index part
            memcpy(charPtr, fkSlave, MAX_FK_MASTER_TIME);
            charPtr += MAX_FK_MASTER_TIME;

            memcpy(charPtr, indexName, MAX_INDEX_NUM * MAX_INDEX_NAME_LEN);
            charPtr += MAX_INDEX_NUM * MAX_INDEX_NAME_LEN;

            memcpy(charPtr, indexID, MAX_INDEX_NUM * MAX_COL_NUM);
            charPtr += MAX_INDEX_NUM * MAX_COL_NUM;

            memcpy(charPtr, bpTreePage, MAX_INDEX_NUM * sizeof(uint));
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
            primaryIndexPage = uintPtr[8];
            uintPtr += 9;

            uchar *charPtr = (uchar*)uintPtr; // updated for ushort
            memcpy(attrLenth, charPtr, MAX_COL_NUM * sizeof(ushort));
            charPtr += sizeof(ushort) * MAX_COL_NUM;

            memcpy(attrType, charPtr, MAX_COL_NUM);
            charPtr += MAX_COL_NUM;

            memcpy(attrName, charPtr, MAX_COL_NUM * MAX_ATTRI_NAME_LEN);
            charPtr += MAX_COL_NUM * MAX_ATTRI_NAME_LEN;

            memcpy(primaryKeyID, charPtr, MAX_COL_NUM);
            charPtr += MAX_COL_NUM;

            memcpy(primaryIndexName, charPtr, MAX_INDEX_NAME_LEN);
            charPtr += MAX_INDEX_NAME_LEN;
            // foreign key part
            memcpy(fkMaster, charPtr, MAX_REF_SLAVE_TIME);
            charPtr += MAX_REF_SLAVE_TIME;

            memcpy(slaveKeyID, charPtr, MAX_REF_SLAVE_TIME * MAX_COL_NUM);
            charPtr += MAX_REF_SLAVE_TIME * MAX_COL_NUM;

            memcpy(constraintName, charPtr, MAX_REF_SLAVE_TIME * MAX_CONSTRAINT_NAME_LEN);
            charPtr += MAX_REF_SLAVE_TIME * MAX_CONSTRAINT_NAME_LEN;
            // index part
            memcpy(fkSlave, charPtr, MAX_FK_MASTER_TIME);
            charPtr += MAX_FK_MASTER_TIME;

            memcpy(indexName, charPtr, MAX_INDEX_NUM * MAX_INDEX_NAME_LEN);
            charPtr += MAX_INDEX_NUM * MAX_INDEX_NAME_LEN;

            memcpy(indexID, charPtr, MAX_INDEX_NUM * MAX_COL_NUM);
            charPtr += MAX_INDEX_NUM * MAX_COL_NUM;

            memcpy(bpTreePage, charPtr, MAX_INDEX_NUM * sizeof(uint));
        }
};

#endif