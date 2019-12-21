#ifndef SCANNER_H
#define SCANNER_H
#include "../RM/Record.h"
#include "Table.h"
#include<vector>
/**
 * A scanner provides a read-only interface for getting records that meet certain requirements
 * A table can have multiple scanners
 * A scanner can only be instantiated from a table. 
*/
class Scanner{
    Table* table = nullptr;
    bool (*demand)(const Record& record) = nullptr;

    uchar* right = nullptr;
    uchar types[MAX_COL_NUM] = {0};
    ushort lengths[MAX_COL_NUM] = {0};
    int colNum = 0;
    uchar cmp[MAX_COL_NUM] = {0};

    RID* rid = new RID(1, 0);
    Record* record = nullptr;

    struct SelfCmp{
        // left和right是字段下标
        uchar left;
        uchar right;
        uchar cmp;
        SelfCmp(){}
        SelfCmp(uchar left, uchar right, uchar cmp):left(left), right(right), cmp(cmp){};
    };

    std::vector<SelfCmp> selfs;
    uint offsets[MAX_COL_NUM] = {0};
    bool offsetsBuilt = false;

    const static uchar uninitialized = 0, lambda = 1, arr = 2;
    uchar mode = uninitialized;
    // lambda style Scanner
    Scanner(Table* table, bool (*demand)(const Record& record)){
        this->table = table;
        this->colNum = table->ColNum();
        memcpy(this->types, table->GetHeader()->attrType, colNum);
        memcpy(this->lengths, table->GetHeader()->attrLenth, colNum * sizeof(ushort));
        SetDemand(demand);
    }
    // array comparison style Scanner
    // 理论上,知道了table就知道了types, lengths以及colNum的上界(可以认为把colNum设的比记录的字段数少,减少比较次数)
    Scanner(Table* table, const uchar* right, int colNum, uchar* cmp){
        this->table = table;
        memcpy(this->types, table->GetHeader()->attrType, colNum);
        memcpy(this->lengths, table->GetHeader()->attrLenth, colNum * sizeof(ushort));
        SetDemand(right, colNum, cmp);
    }
    public:
        /**
         * Return the next record that meets requirements 'demand', or nullptr if none is found
         * No memory is dynamically allocated
        */
        Record* NextRecord(Record* rec){
            if(mode == uninitialized){
                printf("uninitialized scanner\n");
                return nullptr;
            }
            while(table->NextRecord(*rid)){
                record = table->GetRecord(*rid, rec); // Memory needs to be released
                if(mode == lambda ? demand(*record) : DataType::compareArrMultiOp(record->GetData(), right, types, lengths, colNum, cmp, false, true)){ // right is constant
                    bool ok = true;
                    assert(selfs.empty() || offsetsBuilt); // either no self cmp is used or offsets are already built, otherwise abort
                    for(auto it = selfs.begin(); it != selfs.end(); it++){
                        if(!DataType::CompareFamily(types[it->left], types[it->right], record->GetData() + offsets[it->left], record->GetData() + offsets[it->right], 
                            lengths[it->left], lengths[it->right], getBitFromLeft(*(uint*)record->GetData(), it->left), getBitFromLeft(*(uint*)record->GetData(), it->right), it->cmp)){
                            ok = false;
                            break;
                        }
                    }
                    if(ok)
                        return record;
                }
                record->FreeMemory();
                record = nullptr;
            }
            return nullptr;
        }

        void SetDemand(bool(*demand)(const Record& record)){
            offsetsBuilt = false;
            if(demand != nullptr)
                this->mode = lambda;
            else
                this->mode = uninitialized;
            this->demand = demand;
        }

        void SetDemand(const uchar* right, int colNum, uchar* cmp){
            offsetsBuilt = false;
            if(right != nullptr && cmp != nullptr){
                this->mode = arr;
                if(this->right != nullptr)
                    delete[] this->right;
                int totalLength = DataType::calcTotalLength(types, lengths, colNum);
                this->right = new uchar[totalLength];
                this->colNum = colNum;
                memcpy(this->right, right, totalLength);
                memcpy(this->cmp, cmp, colNum);
            }
            else
                this->mode = uninitialized;
        }

        /**
         * 不再让record中的字段和常量比较,而是和自身的另一个字段比较
         * 即attr_left cmp attr_right,selfs中的多个自比较之间的关系为'且'
        */
        void AddSelfCmp(uchar left, uchar right, uchar cmp){
            selfs.push_back(SelfCmp(left, right, cmp));
            offsetsBuilt = false;
        }

        /**
         * 清空自比较列表
        */
        void ClearSelfCmp(){
            selfs.clear();
        }

        /**
         * 计算好各个字段的偏移,以便自比较使用
        */
        void BuildOffsets(){
            DataType::calcOffsets(types, lengths, colNum, offsets);
            offsetsBuilt = true;
        }

        /**
         * 重置scanner于表的最开始
        */
        void Reset(){
            rid->PageNum = 1;
            rid->SlotNum = 0;
        }

        ~Scanner(){
            delete rid;
            if(right != nullptr)
                delete[] right;
        }
        friend class Table;
};

#endif