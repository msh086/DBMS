#ifndef MULTISCANNER_H
#define MULTISCANNER_H
#include "../utils/pagedef.h"
#include "../RM/Record.h"
#include "../RM/DataType.h"
#include "Table.h"
#include "Scanner.h"
#include <vector>

/**
 * 跨表比较
 * tableIndexLeft和tableIndexRight是对应表在MultiScanner::scanners中的下标
 * colLeft cmp colRight
*/
struct InterCmpUnit{
    int tableIndexLeft, tableIndexRight;
    uchar colLeft, colRight;
    uchar cmp = Comparator::Any;
    InterCmpUnit(int leftTb, uchar leftCol, int rightTb, uchar rightCol, uchar cmp){
        this->tableIndexLeft = leftTb;
        this->tableIndexRight = rightTb;
        this->colLeft = leftCol;
        this->colRight = rightCol;
        this->cmp = cmp;
    }
};

class MultiScanner{
        std::vector<Scanner*> scanners;
        std::vector<InterCmpUnit> units;
    public:
        void AddScanner(Scanner* scanner){
            scanners.push_back(scanner);
        }
        void AddUnit(int leftTb, uchar leftField, int rightTb, uchar rightField, uchar cmp){
            units.push_back(InterCmpUnit(leftTb, leftField, rightTb, rightField, cmp));
        }
        void PrintSelection(std::vector<uchar>* wantedCols){
            std::vector<RID> rids[scanners.size()];
            Record tmpRec;
            bool ok = true;
            for(int i = 0; i < scanners.size(); i++){
                while(scanners[i]->NextRecord(&tmpRec)){
                    rids[i].push_back(*tmpRec.GetRid());
                    tmpRec.FreeMemory();
                }
                if(rids[i].size() == 0){
                    ok = false;
                    break;
                }
            }
            std::vector<std::vector<std::string>> printTb;
            printTb.push_back(std::vector<std::string>());
            for(int i = 0; i < scanners.size(); i++){
                for(auto it = wantedCols[i].begin(); it != wantedCols[i].end(); it++)
                    printTb[0].push_back(std::string(scanners[i]->table->GetTableName(), strnlen(scanners[i]->table->GetTableName(), MAX_TABLE_NAME_LEN)) + "." + 
                        std::string((const char*)scanners[i]->table->GetHeader()->attrName[*it], strnlen((const char*)scanners[i]->table->GetHeader()->attrName[*it], MAX_TABLE_NAME_LEN)));
            }
            uint seq[scanners.size()] = {0};
            Record recLeft, recRight;
            while(ok){
                bool units_ok = true;
                for(auto unit_it = units.begin(); unit_it != units.end(); unit_it++){
                    int left = unit_it->tableIndexLeft, right = unit_it->tableIndexRight;
                    scanners[left]->table->GetRecord(rids[left][seq[left]], &recLeft);
                    scanners[right]->table->GetRecord(rids[right][seq[right]], &recRight);
                    if(!DataType::compare(recLeft.GetData() + scanners[left]->table->ColOffset(unit_it->colLeft), 
                        recRight.GetData() + scanners[right]->table->ColOffset(unit_it->colRight), scanners[left]->table->GetHeader()->attrType[unit_it->colLeft], 
                        scanners[left]->table->GetHeader()->attrLenth[unit_it->colLeft], unit_it->cmp, getBitFromLeft(*(uint*)recLeft.GetData(), unit_it->colLeft), 
                        getBitFromLeft(*(uint*)recRight.GetData(), unit_it->colRight))){
                        units_ok = false;
                    } // end if
                    recLeft.FreeMemory();
                    recRight.FreeMemory();
                    if(!units_ok)
                        break;
                }
                if(units_ok){
                    std::vector<std::string> tmpRow;
                    for(int i = 0; i < scanners.size(); i++){ // 第i个表
                        scanners[i]->table->GetRecord(rids[i][seq[i]], &tmpRec);
                        for(auto it = wantedCols[i].begin(); it != wantedCols[i].end(); it++){ // 这个表内所有需要的字段
                            tmpRow.push_back(Printer::FieldToStr(tmpRec, scanners[i]->table->GetHeader()->attrType[*it], *it, 
                                scanners[i]->table->GetHeader()->attrLenth[*it], scanners[i]->table->ColOffset(*it)));
                        }
                        tmpRec.FreeMemory();
                    }
                    printTb.push_back(tmpRow);
                }
                for(int i = 0; i < scanners.size(); i++){
                    if(seq[i] == rids[i].size() - 1){
                        seq[i] = 0;
                        if(i == scanners.size() - 1)
                            ok = false;
                    }
                    else{
                        seq[i]++;
                        break;
                    }
                }
            } // end while(ok)
            Printer::PrintTable(printTb, printTb[0].size(), printTb.size());
        }
        void DeleteScanners(){
            for(auto it = scanners.begin(); it != scanners.end(); it++)
                delete *it;
            scanners.clear();
            units.clear();
        }
};

#endif