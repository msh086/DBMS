#ifndef TABLE_H
#define TABLE_H
#include "Header.h"
#include "../RM/Record.h"
#include "../bufmanager/BufPageManager.h"
#include "../RM/SimpleUtils.h"
#include <vector>
#include <string>
#include <cassert>
#include <fstream>
#include "../frontend/Printer.h"
class DBMS;
class Database;
class Scanner;

class Table{
    Header* header;
    uchar* headerBuf;
    int fid;
    int headerIdx;
    char tablename[MAX_TABLE_NAME_LEN + 1] = "";
    int tmpIdx = 0;
    uchar* tmpBuf = nullptr;
    Database* db = nullptr;

    // table ID, sometimes useful...
    uchar tableID;

    static FileManager* fm;
    static BufPageManager* bpm;

    // runtime variables
    int colCount = 0;
    int fkMasterCount = 0;
    int fkSlaveCount = 0;
    int idxCount = 0;
    bool headerDirty = false;
    uint offsets[MAX_COL_NUM] = {0};

    /**
     * 用于跟踪tmpBuf和tmpIdx,以便针对性地释放缓存
    */
    struct BufTracker{
        uint page;
        int idx;
        BufTracker(uint page, int idx):page(page), idx(idx){};
    };

    std::vector<BufTracker> trackers;

    //private helper methods
    /**
     * Find the first 0 bit in the bit map
     * We assume bits are stored from higher digits to lower ones in a byte
     * Convert it to RID if needed
     * The index starts from 0
    */
    int firstZeroBit(){
        headerBuf = bpm->reusePage(fid, 0, headerIdx, headerBuf);
        uchar* src = headerBuf;
        int size = header->exploitedNum;
        int globalEnd = (size >> 3) + header->GetLenth();
        int localPos = header->GetLenth(), globalPos = header->GetLenth();
        int curPage = 0;
        while(globalPos < globalEnd){
            if(src[localPos] != 0xff){
                for(int i = 0; i < 8; i++)
                    if(src[localPos] & (0x80 >> i) == 0)
                        return ((globalPos - header->GetLenth()) << 3) + i;
                assert(false);
            }
            globalPos++;
            localPos++;
            if(localPos == PAGE_SIZE){
                localPos = 0;
                src = tmpBuf = bpm->reusePage(fid, ++curPage, tmpIdx, tmpBuf);
            }
        }
        int remain = size & 7;
        for(int i = 0; i < remain; i++)
            if(src[localPos] & (0x80 >> i) == 0)
                return ((globalPos - header->GetLenth()) << 3) + i;
        return size;

        // uchar* src = headerBuf + header->GetLenth();
        // int size = header->exploitedNum;
        // int fullByte = size >> 3;
        // uchar allOnes = 255;
        // for(int i = 0; i < fullByte; i++){
        //     if(src[i] != allOnes){
        //         int pos = 0;
        //         uchar msb = 128;
        //         while(src[i] & msb){
        //             pos++;
        //             msb >>= 1;
        //         }
        //         return (i << 3) + pos;
        //     }
        // }
        // int pos = 0, remain = size & 7;
        // uchar msb = 128;
        // src += fullByte;
        // while((src[0] & msb) && pos < remain){
        //     pos++;
        //     msb >>= 1;
        // }
        // return pos + (fullByte << 3);

        //If pos < remain, there is an empty slot before the furthest record
        //otherwise, pos == remain, it points to the empty slot right after the furthest record. The the return value equals header->exploitedNum
    }

    /**
     * Returns the position of the first 1 bit starting from pos(included), or -1 if non exists
    */
    int firstOneBitFrom(int pos){
        int startByte = (pos >> 3) + header->GetLenth(), startBit = pos & 7;
        int endByte = (header->exploitedNum >> 3) + header->GetLenth(), endBit = header->exploitedNum & 7;
        if(startByte > endByte)
            return -1;
        int curPage = startByte / PAGE_SIZE, localPos = startByte % PAGE_SIZE;
        uchar* src = nullptr;
        if(curPage == 0)
            src = headerBuf = bpm->reusePage(fid, 0, headerIdx, headerBuf);
        else
            src = tmpBuf = bpm->reusePage(fid, curPage, tmpIdx, tmpBuf);
        //[(startBytes, startBit), (startByte, endBit))
        if(startByte == endByte){
            for(int i = startBit; i < endBit; i++)
                if(src[localPos] & (0x80 >> i))
                    return ((startByte - header->GetLenth()) << 3) + i;
            return -1;
        }

        //[(startByte, startBit), (startByte, 8))
        for(int i = startBit; i < 8; i ++){
            if(src[localPos] & (0x80 >> i))
                return ((startByte - header->GetLenth()) << 3) + i;
        }
        localPos++;
        if(localPos == PAGE_SIZE){
            localPos = 0;
            src = tmpBuf = bpm->reusePage(fid, ++curPage, tmpIdx, tmpBuf);
        }

        //[(startByte + 1, 0), (endByte - 1, 8))
        for(int i = startByte + 1; i < endByte; i++){
            if(src[localPos]){
                for(int j = 0; j < 8; j++){
                    if(src[localPos] & (0x80 >> j))
                        return ((i - header->GetLenth()) << 3) + j;
                }
                assert(false);
            }
            else{
                localPos++;
                if(localPos == PAGE_SIZE){
                    localPos = 0;
                    src = tmpBuf = bpm->reusePage(fid, ++curPage, tmpIdx, tmpBuf);
                }
            }
        }

        //[(endByte, 0), (endByte, endBit))
        for(int i = 0; i < endBit; i++){
            if(src[localPos] & (0x80 >> i))
                return ((endByte - header->GetLenth()) << 3) + i;
        }
        return -1;
    }

    /**
     * 位图变化导致的内存的markDirty和headerDirty由该函数维护
    */
    void setBit(int pos){
        int bytes = header->GetLenth() + (pos >> 3), remain = pos & 7;
        int dstPage = bytes / PAGE_SIZE, localPos = bytes % PAGE_SIZE;
        uchar* src = nullptr;
        if(dstPage == 0){
            src = headerBuf = bpm->reusePage(fid, 0, headerIdx, headerBuf);
            bpm->markDirty(headerIdx); // ? headerDirty,这里更改了buffer中的内容,只能通过bpm写回
            // ? trackers不记录header page的信息,因为header page不需要跟踪
        }
        else{
            src = tmpBuf = bpm->reusePage(fid, dstPage, tmpIdx, tmpBuf);
            bpm->markDirty(tmpIdx);
            trackers.push_back(BufTracker(dstPage, tmpIdx));
        }
        (*(src + localPos)) |= (0x80 >> remain);
    }

    /**
     * 位图变化导致的内存的markDirty和headerDirty由该函数维护
    */
    void clearBit(int pos){
        int bytes = header->GetLenth() + (pos >> 3), remain = pos & 7;
        int dstPage = bytes / PAGE_SIZE, localPos = bytes % PAGE_SIZE;
        uchar* src = nullptr;
        if(dstPage == 0){
            src = headerBuf = bpm->reusePage(fid, 0, headerIdx, headerBuf);
            bpm->markDirty(headerIdx);
        }
        else{
            src = tmpBuf = bpm->reusePage(fid, dstPage, tmpIdx, tmpBuf);
            bpm->markDirty(tmpIdx);
            trackers.push_back(BufTracker(dstPage, tmpIdx));
        }
        (*(src + localPos)) &= ~(0x80 >> remain);
    }

    uint RIDtoUint(const RID* rid){
        return (rid->PageNum - START_PAGE) * header->slotNum + rid->SlotNum;
    }

    RID* UintToRID(const uint& value, RID* rid){
        rid->PageNum = value / header->slotNum + START_PAGE; // RID::pageNum starts from START_PAGE
        rid->SlotNum = value % header->slotNum;
        return rid;
    }

    public:
#ifdef DEBUG
        void DebugPrint(){ // ! Don't use me
            header->DebugPrint();
            printf("***bitmap:\n    ");
            uchar* src = headerBuf + header->GetLenth();
            int bitmapSize = (header->exploitedNum + 7) >> 3;
            for(int i = 0; i < bitmapSize; i++)
                printf("(%d, %hhu) ", i, src[i]);
            printf("\n");
        }
#endif

        Table(){
            header = new Header();
        }

        // load a table instance from an OPENDED table file
        // tableID: 0-30, 表对应的标识号. 31(TB_ID_NONE)代表不是任何表
        Table(int fid, const char* tableName, Database* db, uchar tableID = TB_ID_NONE){
            header = new Header();
            this->fid = fid;
            headerBuf = (uchar*)bpm->getPage(fid, 0, headerIdx);
            bpm->access(headerIdx);
            header->FromString(headerBuf);
            strncpy(this->tablename, tableName, strnlen(tableName, MAX_TABLE_NAME_LEN));
            CalcColFkIdxCount();
            DataType::calcOffsets(header->attrType, header->attrLenth, colCount, offsets);
            this->db = db;
            this->tableID = tableID;
        }

        ~Table(){
            delete header;
        }
        /**
         * Return a COPY of the record of rid
         * Only properties in ANS is dynamically allocated, which will be freed with ANS
        */
        Record* GetRecord(const RID& rid, Record* ans){
            if(rid.GetPageNum() < START_PAGE){
                printf("In Table::GetRecord, trying to get record from the header page or bitmap pages\n");
                return nullptr;
            }
            tmpBuf = bpm->reusePage(fid, rid.GetPageNum(), tmpIdx, tmpBuf);
            trackers.push_back(BufTracker(rid.PageNum, tmpIdx));
            ans->data = new uchar[header->recordLenth];
            memcpy(ans->data, ((uchar*)tmpBuf) + rid.GetSlotNum() * header->recordLenth, header->recordLenth);
            ans->id = new RID(rid.GetPageNum(), rid.GetSlotNum());
            return ans;
        }

        void ConvertTextToBin(const char* src, uchar* dst, ushort length, uchar type);

        bool BatchLoad(const char* filename, char delim){
            std::ifstream fin;
            fin.open(filename);
            if(fin.fail())
                return false;
            char lineBuf[PAGE_SIZE] = {0};
            uchar recBuf[header->recordLenth] = {0};
            int iter = 0, last = 0; // [last, iter) in linebuf
            int recPos = 4, recField = 0;
            char c;
            while(true){
                c = fin.get();
                if(c == EOF)
                    break;
                if(c == '\n'){
                    assert(recField == colCount);
                    RID rid;
                    InsertRecord(recBuf, &rid);
                    memset(lineBuf, 0, iter);
                    memset(recBuf, 0, sizeof(recBuf));
                    last = iter = 0;
                    recPos = 4;
                    recField = 0;
                    continue;
                }
                if(c == delim){ // handle field
                    assert(recField < colCount);
                    if(strncmp(lineBuf + last, "null", iter - last) == 0)
                        setBitFromLeft(*(uint*)recBuf, recField);
                    else
                        ConvertTextToBin(lineBuf + last, recBuf + recPos, header->attrLenth[recField], header->attrType[recField]);
                    recPos += DataType::lengthOf(header->attrType[recField], header->attrLenth[recField]);
                    recField++;
                    last = iter;
                }
                else
                    lineBuf[iter++] = c;
            }
            return true;
        }

        /**
         * Return the RID of the inserted record
         * No dynamic memory will be allocated
        */
        RID* InsertRecord(const uchar* data, RID* rid){
            // 更新位图
            int firstEmptySlot = firstZeroBit();
            setBit(firstEmptySlot);
            // 更新header
            header->recordNum++;
            if(firstEmptySlot == header->exploitedNum)
                header->exploitedNum++;
            headerDirty = true;
            //inserting the record
            UintToRID(firstEmptySlot, rid);
            tmpBuf = bpm->reusePage(fid, rid->PageNum, tmpIdx, tmpBuf);
            trackers.push_back(BufTracker(rid->PageNum, tmpIdx));
            memcpy(tmpBuf + rid->SlotNum * header->recordLenth, data, header->recordLenth);
            bpm->markDirty(tmpIdx);
            return rid;
        }

        void DeleteRecord(const RID& rid){
            if(rid.GetPageNum() < START_PAGE){
                printf("In Table::DeleteRecord, trying to delete a record from header page or bitmap pages\n");
                return;
            }
            // 更新位图
            int slotNumber = RIDtoUint(&rid);
            clearBit(slotNumber);
            // 更新header
            if(slotNumber == header->exploitedNum - 1)
                header->exploitedNum--;
            header->recordNum--;
            headerDirty = true;
            // 和实际记录页没有关系,不需要重新格式化内存
        }

        /**
         * 更新由RID指定的记录
         * 从data的srcOffset开始的length个字节被复制到对应记录从dstOffset开始的length个字节
        */
        void UpdateRecord(const RID& rid, const uchar* data, uint dstOffset, uint srcOffset, uint length){
            if(rid.PageNum == 0){
                printf("In Table::UpdateRecord, trying to update a record from header page\n");
                return;
            }
            tmpBuf = bpm->reusePage(fid, rid.PageNum, tmpIdx, tmpBuf);
            trackers.push_back(BufTracker(rid.PageNum, tmpIdx));
            memcpy(tmpBuf + rid.SlotNum * header->recordLenth + dstOffset, data + srcOffset, length);
            bpm->markDirty(tmpIdx);
        }


        /**
         * Calc the number of columns, fk constraints and indexs
         * Based on attrType / fkMaster / fkSlave / indexID
        */
        void CalcColFkIdxCount(){
            colCount = 0;
            for(int i = 0; i < MAX_COL_NUM; i++)
                if(header->attrType[i] == DataType::NONE)
                    break;
                else
                    colCount++;
            fkMasterCount = 0;
            for(int i = 0; i < MAX_REF_SLAVE_TIME; i++)
                if(header->fkMaster[i] == TB_ID_NONE)
                    break;
                else
                    fkMasterCount++;
            fkSlaveCount = 0;
            for(int i = 0; i < MAX_FK_MASTER_TIME; i++)
                if(header->fkSlave[i] == TB_ID_NONE)
                    break;
                else
                    fkSlaveCount++;
            idxCount = 0;
            for(int i = 0; i < MAX_INDEX_NUM; i++)
                if(header->indexID[i][0] == COL_ID_NONE)
                    break;
                else
                    idxCount++;
        }

        // map the name of a column to its id
        uchar IDofCol(const char* colName){
            for(int i = 0; i < colCount; i++){
                if(identical(colName, (char*)header->attrName[i], MAX_ATTRI_NAME_LEN))
                    return i;
            }
            return COL_ID_NONE;
        }

        void AlterTableAddCol(const char* colName, uchar type, bool nullable, ushort length, bool isDft, const uchar* dftVal){
            // TODO
        }

        void AlterTableDropCol(const char* colName){
            // TODO
        }

        void AlterTableChangeCol(const char* colName, uchar type, bool nullable, ushort length, bool isDft, const uchar* dftVal){
            // TODO
        }

        void AlterTableAddPrimaryKey(const char** colNames, uint colNum){
            // TODO
        }

        void AlterTableDropPrimaryKey(){
            // TODO
        }

        /**
         * Add foreign constraint, return if success
         * Same constraint conditions with different names are permitted
         * Checked: existing constraints with same name
         * ! Unchecked: primary key
         * TODO: conflicts with existing records
        */
        int AddFKMaster(uchar masterID, std::vector<uchar> slaveKeys, uint masterKeys, const char* fkName){
            if(fkMasterCount == MAX_REF_SLAVE_TIME) // full
                return 2;
            headerBuf = bpm->reusePage(fid, 0, headerIdx, headerBuf);
            for(int i = 0; i < fkMasterCount; i++){
                if(identical((char*)header->constraintName[i], fkName, MAX_CONSTRAINT_NAME_LEN))
                    return 1; // existing fk with the same name
            }
            // update constraint name
            memcpy(header->constraintName[fkMasterCount], fkName, strnlen(fkName, MAX_CONSTRAINT_NAME_LEN));
            // update master table ID
            header->fkMaster[fkMasterCount] = masterID;
            // // update master key ID
            // header->masterKeyID[fkMasterCount] = masterKeys;
            // update slave key ID
            int slave_pos = 0;
            for(auto slave_key_it = slaveKeys.begin(); slave_key_it != slaveKeys.end(); slave_key_it++)
                header->slaveKeyID[fkMasterCount][slave_pos] = *slave_key_it;
            memset(header->slaveKeyID[fkMasterCount] + slave_pos, COL_ID_NONE, MAX_COL_NUM - slave_pos);
            // sync
            headerDirty = true;
            fkMasterCount++;
            return 0;
        }

        /**
         * Remove foreign constraint master, return if success
        */
        bool RemoveFKMaster(const char* fkName){
            headerBuf = bpm->reusePage(fid, 0, headerIdx, headerBuf);
            // uchar* fkPart = headerBuf + Header::fkOffset + MAX_REF_SLAVE_TIME * 9;
            int pos = 0;
            for(; pos < fkMasterCount; pos++){
                if(identical((char*)header->constraintName[pos], fkName, MAX_CONSTRAINT_NAME_LEN))
                    break;
            }
            if(pos == fkMasterCount) // all is checked
                return false;
            // update fk masters: type uchar[]
            memcpy(header->constraintName[pos], header->constraintName[pos + 1], (fkMasterCount - pos - 1) * MAX_CONSTRAINT_NAME_LEN);
            memset(header->constraintName[fkMasterCount - 1], 0, MAX_CONSTRAINT_NAME_LEN);
            // update master ID, type: uchar
            memcpy(&header->fkMaster[pos], &header->fkMaster[pos + 1], fkMasterCount - pos - 1);
            header->fkMaster[fkMasterCount - 1] = TB_ID_NONE;
            // // update master key ID, type: uint
            // memcpy(&header->masterKeyID[pos], &header->masterKeyID[pos + 1], (fkMasterCount - pos - 1) * sizeof(uint));
            // header->masterKeyID[fkMasterCount - 1] = 0;
            // update slave key ID, type: uint
            memcpy(header->slaveKeyID[pos], header->slaveKeyID[pos + 1], (fkMasterCount - pos - 1) * MAX_COL_NUM);
            memset(header->slaveKeyID[fkMasterCount - 1], 0, MAX_COL_NUM);
            // sync
            headerDirty = true;
            fkMasterCount--;
            return true;
        }

        /**
         * Add foreign constraint slave
         * NOTE: duplicate slave is permitted, since a table can have multiple constraints
        */
        bool AddFKSlave(uchar slaveID){
            if(fkSlaveCount == MAX_FK_MASTER_TIME) // full
                return false;
            headerBuf = bpm->reusePage(fid, 0, headerIdx, headerBuf);
            uchar* slaves = header->fkSlave;
            // update slaves
            slaves[fkSlaveCount++] = slaveID;
            // sync
            headerDirty = true;
            fkSlaveCount++;
            return true;
        }

        /**
         * Remove foreign constraint slave
        */
        bool RemoveFKSlave(uchar slaveID){
            headerBuf = bpm->reusePage(fid, 0, headerIdx, headerBuf);
            uchar* slaves = header->fkSlave;
            int pos = 0;
            while(pos < fkSlaveCount){
                if(slaves[pos] == slaveID) // found
                    break;
                pos++;
            }
            if(pos == fkSlaveCount) // not found
                return false;
            // update slaves
            memcpy(slaves + pos, slaves + pos + 1, fkSlaveCount - pos - 1);
            slaves[fkSlaveCount - 1] = TB_ID_NONE;
            // sync
            headerDirty = true;
            fkSlaveCount--;
            return true;
        }

        /**
         * Given the columns to match, return the best index for it //? bptree page
         * 可以使用索引的情况:
         * 查询列为c1, c2, ..., ck
         * 且条件为c1:eq, c2:eq, ..., ck:range(low,high)
         * 且存在索引(c1, c2, ..., ck, ck+1, ...)
         * 如何从where子句中获得c1, c2, ..., ck由parser负责
        */
        uchar BestIndexFor(uint eqColMask, uchar rangeCol){
            uchar ans = INDEX_ID_NONE;
            int minUnusedLength = MAX_COL_NUM * 65535;
            int eqColCount = 0;
            uint copy = eqColMask;
            while(copy){ // 索引列较少时性能比暴力法好
                eqColCount++;
                copy &= copy - 1;
            }
            for(int i = 0; i < idxCount; i++){ // 第i个索引
                bool ok = true;
                for(int j = 0; j < eqColCount; j++){ // 索引的前eqColCount个字段构成的集合应该等于eqColMask指定的集合
                    uchar colID = header->indexID[i][j];
                    if(colID == COL_ID_NONE || !getBitFromLeft(eqColMask, colID)){
                        ok = false;
                        break;
                    }
                }
                // 如果有范围条件:
                if(ok && rangeCol != COL_ID_NONE){
                    uchar colID = header->indexID[i][eqColCount];
                    if(colID == COL_ID_NONE || colID != rangeCol)
                        ok = false;
                }
                if(ok){
                    int tmpLength = 0;
                    for(int j = rangeCol == COL_ID_NONE ? eqColCount : eqColCount + 1; j < MAX_COL_NUM; j++){
                        uchar colID = header->indexID[i][j];
                        if(colID == COL_ID_NONE)
                            break;
                        else
                            tmpLength += DataType::constantLengthOf(header->attrType[colID], header->attrLenth[colID]); // 对长varchar有惩罚
                    }
                    if(tmpLength < minUnusedLength){
                        minUnusedLength = tmpLength;
                        ans = i;
                        if(tmpLength == 0)
                            break;
                    }
                }
            }
            return ans;
        }

        /**
         * Caution, this action writes back data not only in this table, but all the tables
        */
        void WriteBack(){ 
            if(headerDirty){
                headerBuf = bpm->reusePage(fid, 0, headerIdx, headerBuf);
                header->ToString(headerBuf);
                bpm->markDirty(headerIdx);
                bpm->writeBack(headerIdx); // marking dirty is not enough... write it back
            }
            else{ // ? Even header isn't dirty, we need to give it back to storage, or the copy in bpm may be outdated
                // ? e.g. this table is closed and another table is opened. The new table has the same fid as previously opened one
                // ? if headerIdx in bpm is not written back, the bpm will assume falsely that it has buffer for the header in the new table
                int curPage, curFid;
                bpm->getKey(headerIdx, curFid, curPage);
                if(fid == curFid && curPage == 0)
                    bpm->writeBack(headerIdx);
            }
            for(auto it = trackers.begin(); it != trackers.end(); it++){
                int curPage, curFid;
                bpm->getKey(it->idx, curFid, curPage);
                if(fid == curFid && it->page == curPage)
                    bpm->writeBack(it->idx);
            }
            trackers.clear();
        }

        /**
         * Get the RID of the first record after rid (excluded), return success/failure
        */
        bool NextRecord(RID& rid){
            if(rid.PageNum < START_PAGE){
                printf("In Table::NextRecord, pageNum < START_PAGE\n");
                return false;
            }
            int begin = RIDtoUint(&rid) + 1; // start from the next position
            int nextRecordSlot = firstOneBitFrom(begin);
            if(nextRecordSlot >= 0){
                UintToRID(nextRecordSlot, &rid);
                return true;
            }
            else
                return false;
        }

        /**
         * Create index on cols
         * Checked: name conflict, not more room, cols illegal
         * ? 索引列的顺序是有意义的,不能用位图表示
        */
        int CreateIndexOn(std::vector<uchar>, const char* idxName);

        /**
         * Remove index by name
         * Checked: idx not found
        */
        bool RemoveIndex(const char* idxName);

        uint GetPrimaryIndexPage(){
            for(int i = 0; i < idxCount; i++){
                if(identical((char*)header->indexName[i], PRIMARY_RESERVED_IDX_NAME, MAX_INDEX_NAME_LEN))
                    return header->bpTreePage[i];
            }
            return 0;
        }

        int FileID(){
            return fid;
        }

        uchar GetTableID(){
            return tableID;
        }

        /**
         * Get header(read only)
        */
        const Header* GetHeader(){
            return header;
        }

        int ColNum(){
            return colCount;
        }

        int IdxNum(){
            return idxCount;
        }

        int FKMasterNum(){
            return fkMasterCount;
        }

        int ColOffset(int i){
            return offsets[i];
        }

        void Print();
        void PrintSelection(const std::vector<uchar>& wantedCols, const std::vector<RID> &selected){
            std::vector<std::vector<std::string>> printTB;
            printTB.push_back(std::vector<std::string>());
            int colCount = wantedCols.size();
            for(auto it = wantedCols.begin(); it != wantedCols.end(); it++)
                printTB[0].push_back(std::string((char*)header->attrName[*it], strnlen((char*)header->attrName[*it], MAX_ATTRI_NAME_LEN)));
            Record tmpRec;
            for(auto rid_it = selected.begin(); rid_it != selected.end(); rid_it++){
                std::vector<std::string> tmpVec;
                GetRecord(*rid_it, &tmpRec);
                for(auto field_it = wantedCols.begin(); field_it != wantedCols.end(); field_it++)
                    tmpVec.push_back(Printer::FieldToStr(tmpRec, header->attrType[*field_it], *field_it, header->attrLenth[*field_it], offsets[*field_it]));
                printTB.push_back(std::move(tmpVec));
            }
            Printer::PrintTable(printTB, colCount, printTB.size());
        }

        Scanner* GetScanner(bool (*demand)(const Record& record));
        Scanner* GetScanner(const uchar* right, int colNum, uchar* cmp);

        friend class DBMS;
        friend class Database;    
};
#endif