#ifndef SCANNER_H
#define SCANNER_H
#include "../RM/Record.h"
#include "Table.h"
#include "../frontend/Printer.h"
#include<vector>

/**
 * 一个CmpUnit是一组比较条件
 * 当一个字段有多个不可合并的条件时(e.g. a > 1 && a < 3)
 * 
 * 由于内部内存是动态分配的,而且使用了vector.push_back(const T&),所以要手动重写移动构造函数和移动赋值函数
*/
struct CmpUnit{
    uchar* right = nullptr;
    int colNum = 0;
    uchar cmp[MAX_COL_NUM] = {0};
    ~CmpUnit(){
        if(right)
            delete[] right;
    }
    CmpUnit(){}
    CmpUnit(CmpUnit &&other){
        this->right = other.right;
        other.right = nullptr;
        this->colNum = other.colNum;
        memcpy(this->cmp, other.cmp, MAX_COL_NUM);
    }
    CmpUnit(const CmpUnit& other) = delete;
    CmpUnit& operator=(const CmpUnit& other) = delete;
    CmpUnit& operator=(CmpUnit &&other){
        this->right = other.right;
        other.right = nullptr;
        this->colNum = other.colNum;
        memcpy(this->cmp, other.cmp, MAX_COL_NUM);
    }
};

/**
 * A scanner provides a read-only interface for getting records that meet certain requirements
 * A table can have multiple scanners
 * A scanner can only be instantiated from a table. 
*/
class Scanner{
    Table* table = nullptr;
    // demand是一个lambda函数,仅在特殊情况才使用,如any -> true
    bool (*demand)(const Record& record) = nullptr;

    // types and lengths are table properties, unrelated to comparison conditions
    uchar types[MAX_COL_NUM] = {0};
    ushort lengths[MAX_COL_NUM] = {0};

    RID* rid = new RID(START_PAGE, 0);
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

    std::vector<CmpUnit> units;

    const static uchar uninitialized = 0, lambda = 1, arr = 2;
    uchar mode = uninitialized;
    // lambda style Scanner
    // 如果要构造空Scanner请使用Scanner(Table, nullptr)而非Scanner(Table, nullptr, 0, nullptr)
    Scanner(Table* table, bool (*demand)(const Record& record)){
        this->table = table;
        memcpy(this->types, table->GetHeader()->attrType, table->ColNum());
        memcpy(this->lengths, table->GetHeader()->attrLenth, table->ColNum() * sizeof(ushort));
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
                bool ok = true;
                if(mode == lambda){
                    if(!demand(*record))
                        ok = false;
                }
                else{
                    for(auto cmp_it = units.begin(); cmp_it != units.end(); cmp_it++)
                        if(!DataType::compareArrMultiOp(record->GetData(), cmp_it->right, types, lengths, cmp_it->colNum, cmp_it->cmp, false, true)){
                            ok = false;
                            break;
                        }
                }
                if(ok)
                    for(auto it = selfs.begin(); it != selfs.end(); it++){
                        if(!DataType::CompareFamily(types[it->left], types[it->right], record->GetData() + table->ColOffset(it->left), record->GetData() + table->ColOffset(it->right), 
                            lengths[it->left], lengths[it->right], getBitFromLeft(*(uint*)record->GetData(), it->left), getBitFromLeft(*(uint*)record->GetData(), it->right), it->cmp)){
                            ok = false;
                            break;
                        }
                    }
                if(ok)
                    return record;
                record->FreeMemory();
                record = nullptr;
            }
            return nullptr;
        }

        void SetDemand(bool(*demand)(const Record& record)){
            units.clear();
            if(demand != nullptr)
                this->mode = lambda;
            else
                this->mode = uninitialized;
            this->demand = demand;
        }

        /**
         * 设置单一条件,清空之前设置的条件
        */
        void SetDemand(const uchar* right, int colNum, uchar* cmp){
            units.clear();
            if(right != nullptr && cmp != nullptr){
                this->mode = arr;
                int totalLength = DataType::calcTotalLength(types, lengths, colNum); // colNum可能比记录的字段数少,不能直接使用recordenth
                CmpUnit unit;
                unit.right = new uchar[totalLength];
                unit.colNum = colNum;
                memcpy(unit.right, right, totalLength);
                memcpy(unit.cmp, cmp, colNum);
                units.push_back(std::move(unit));
            }
            else
                this->mode = uninitialized;
        }
        void AddDemand(const uchar* right, int colNum, uchar* cmp){
            if(right != nullptr && cmp != nullptr){
                this->mode = arr;
                int totalLength = DataType::calcTotalLength(types, lengths, colNum); // colNum可能比记录的字段数少,不能直接使用recordenth
                CmpUnit unit;
                unit.right = new uchar[totalLength];
                unit.colNum = colNum;
                memcpy(unit.right, right, totalLength);
                memcpy(unit.cmp, cmp, colNum);
                units.push_back(std::move(unit));
            }
            else
                this->mode = uninitialized;
        }
        /**
         * 不再让record中的字段和常量比较,而是和自身的另一个字段比较
         * 即attr_left cmp attr_right,selfs中的多个自比较之间的关系为'且'
        */
        void AddSelfCmp(uchar left, uchar right, uchar cmp){
            mode = arr;
            selfs.push_back(SelfCmp(left, right, cmp));
        }
        /**
         * 清空自比较列表
        */
        void ClearSelfCmp(){
            selfs.clear();
        }
        /**
         * 遍历表,输出符合要求的记录的指定字段
        */
        void PrintSelection(const std::vector<uchar>& selected){
            std::vector<std::vector<std::string>> tb;
            int colCount = selected.size();
            tb.push_back(std::vector<std::string>());
            for(auto select_it = selected.begin(); select_it != selected.end(); select_it++)
                tb[0].push_back(std::string((char*)table->GetHeader()->attrName[*select_it], strnlen((char*)table->GetHeader()->attrName[*select_it], MAX_ATTRI_NAME_LEN)));
            int rowCount = 1; // the top row
            Record tmpRec;
            while(NextRecord(&tmpRec)){
                std::vector<std::string> tmpRow;
                for(auto select_it = selected.begin(); select_it != selected.end(); select_it++)
                    tmpRow.push_back(Printer::FieldToStr(tmpRec, table->GetHeader()->attrType[*select_it], *select_it, table->GetHeader()->attrLenth[*select_it], table->ColOffset(*select_it)));
                tb.push_back(tmpRow);
                rowCount++;
            }
            Printer::PrintTable(tb, colCount, rowCount);
        }
        /**
        * 重置scanner于表的最开始
        */
        void Reset(){
            rid->PageNum = START_PAGE;
            rid->SlotNum = 0;
        }
        ~Scanner(){
            delete rid;
            units.clear();
        }
        friend class Table;
};

#endif