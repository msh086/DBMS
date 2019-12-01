#ifndef SCANNER_H
#define SCANNER_H
#include "../RM/Record.h"
#include "Table.h"
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
    uint nullMask = 0;
    int colNum = 0;
    uchar cmp[MAX_COL_NUM] = {0};

    RID* rid = new RID(1, 0);
    Record* record = nullptr;

    const static uchar uninitialized = 0, lambda = 1, arr = 2;
    uchar mode = uninitialized;
    // lambda style Scanner
    Scanner(Table* table, bool (*demand)(const Record& record)){
        this->table = table;
        SetDemand(demand);
    }
    // array comparison style Scanner
    Scanner(Table* table, const uchar* right, const uchar* types, const ushort* lengths, uint nullMask, int colNum, uchar* cmp){
        this->table = table;
        SetDemand(right, types, lengths, nullMask, colNum, cmp);
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
                if(mode == lambda ? demand(*record) : DataType::compareArrMultiOp(record->GetData(), right, types, lengths, nullMask, colNum, cmp))
                    return record;
                record->FreeMemory();
                record = nullptr;
            }
            return nullptr;
        }

        void SetDemand(bool(*demand)(const Record& record)){
            if(demand != nullptr)
                this->mode = lambda;
            else
                this->mode = uninitialized;
            this->demand = demand;
        }

        void SetDemand(const uchar* right, const uchar* types, const ushort* lengths, uint nullMask, int colNum, uchar* cmp){
            if(right != nullptr && types != nullptr && lengths != nullptr && cmp != nullptr){
                this->mode = arr;
                if(this->right != nullptr)
                    delete[] this->right;
                int totalLength = DataType::calcTotalLength(types, lengths, nullMask, colNum);
                this->right = new uchar[totalLength];
                this->nullMask = nullMask;
                this->colNum = colNum;
                memcpy(this->right, right, totalLength);
                memcpy(this->types, types, colNum);
                memcpy(this->lengths, lengths, colNum * sizeof(ushort));
                memcpy(this->cmp, cmp, colNum);
            }
            else
                this->mode = uninitialized;
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

Scanner* Table::GetScanner(bool (*demand)(const Record& record)){
    return new Scanner(this, demand);
}

Scanner* Table::GetScanner(const uchar* right, const uchar* types, const ushort* lengths, uint nullMask, int colNum, uchar* cmp){
    return new Scanner(this, right, types, lengths, nullMask, colNum, cmp);
}

#endif