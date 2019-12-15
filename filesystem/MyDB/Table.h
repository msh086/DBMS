#ifndef TABLE_H
#define TABLE_H
#include "Header.h"
#include "../RM/Record.h"
#include "../bufmanager/BufPageManager.h"
#include "../RM/SimpleUtils.h"
#include <vector>
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

    static FileManager* fm;
    static BufPageManager* bpm;

    // runtime variables
    int colCount = 0;
    int fkMasterCount = 0;
    int fkSlaveCount = 0;
    int idxCount = 0;
    bool headerDirty = false;

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
        uchar* src = headerBuf + header->GetLenth();
        int size = header->exploitedNum;
        int fullByte = size >> 3;
        uchar allOnes = 255;
        for(int i = 0; i < fullByte; i++){
            if(src[i] != allOnes){
                int pos = 0;
                uchar msb = 128;
                while(src[i] & msb){
                    pos++;
                    msb >>= 1;
                }
                return (i << 3) + pos;
            }
        }
        int pos = 0, remain = size & 7;
        uchar msb = 128;
        src += fullByte;
        while((src[0] & msb) && pos < remain){
            pos++;
            msb >>= 1;
        }
        return pos + (fullByte << 3);
        //If pos < remain, there is an empty slot before the furthest record
        //otherwise, pos == remain, it points to the empty slot right after the furthest record. The the return value equals header->exploitedNum
    }

    /**
     * Returns the position of the first 1 bit starting from pos(included), or -1 if non exists
    */
    int firstOneBitFrom(int pos){
        int startByte = pos >> 3, startBit = pos & 7;
        int endByte = header->exploitedNum >> 3, endBit = header->exploitedNum & 7;
        if(startByte > endByte)
            return -1;
        headerBuf = bpm->reusePage(fid, 0, headerIdx, headerBuf);
        uchar* src = headerBuf + header->GetLenth();
        uchar allOnes = 255;
        //[(startBytes, startBit), (startByte, endBit))
        if(startByte == endByte){
            int prober = 128 >> startBit;
            int bit = startBit;
            while(bit < endBit)
                if(prober & src[startByte])
                    return (startByte << 3) + bit;
                else{
                    prober >>= 1;
                    bit ++;
                }
            return -1;
        }

        //[(startByte, startBit), (startByte, 8))
        for(int i = startBit, prober = 128 >> startBit; i < 8; i ++, prober >>= 1){
            if(src[startByte] & prober)
                return (startByte << 3) + i;
        }

        //[(startByte + 1, 0), (endByte - 1, 8))
        for(int i = startByte + 1; i < endByte; i ++){
            if(src[i] == allOnes)
                continue;
            for(int j = 0, prober = 128; j < 8; j ++, prober >>= 1){
                if(src[i] & prober)
                    return (i << 3) + j;
            }
            printf("You should not have reached here!\n");
        }

        //[(endByte, 0), (endByte, endBit))
        for(int i = 0, prober = 128; i < endBit; i ++, prober >>= 1){
            if(src[endByte] & prober)
                return (endByte << 3) + i;
        }

        return -1;
    }

    void setBit(int pos){
        uchar* src = headerBuf + header->GetLenth();
        int bytes = pos >> 3, remain = pos & 7;
        (*(src + bytes)) |= (128 >> remain);
    }

    void clearBit(int pos){
        uchar* src = headerBuf + header->GetLenth();
        int bytes = pos >> 3, remain = pos & 7;
        (*(src + bytes)) &= ~(128 >> remain);
    }

    uint RIDtoUint(const RID* rid){
        return (rid->PageNum - 1) * header->slotNum + rid->SlotNum;
    }

    RID* UintToRID(const uint& value, RID* rid){
        rid->PageNum = value / header->slotNum + 1; // RID::pageNum starts from 1
        rid->SlotNum = value % header->slotNum;
        return rid;
    }

    public:
#ifdef DEBUG
        void DebugPrint(){
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
        Table(int fid, const char* tableName, Database* db){
            header = new Header();
            this->fid = fid;
            headerBuf = (uchar*)bpm->getPage(fid, 0, headerIdx);
            bpm->access(headerIdx);
            header->FromString(headerBuf);
            strncpy(this->tablename, tableName, strnlen(tableName, MAX_TABLE_NAME_LEN));
            CalcColFkIdxCount();
            this->db = db;
        }

        ~Table(){
            delete header;
        }
        /**
         * Return a COPY of the record of rid
         * Only properties in ANS is dynamically allocated, which will be freed with ANS
        */
        Record* GetRecord(const RID& rid, Record* ans){
            if(rid.GetPageNum() == 0){
                printf("In Table::GetRecord, trying to get record from the header page\n");
                return nullptr;
            }
            tmpBuf = bpm->reusePage(fid, rid.GetPageNum(), tmpIdx, tmpBuf);
            bpm->access(tmpIdx);
            trackers.push_back(BufTracker(rid.PageNum, tmpIdx));
            ans->data = new uchar[header->recordLenth];
            memcpy(ans->data, ((uchar*)tmpBuf) + rid.GetSlotNum() * header->recordLenth, header->recordLenth);
            ans->id = new RID(rid.GetPageNum(), rid.GetSlotNum());
            return ans;
        }

        /**
         * Return the RID of the inserted record
         * No dynamic memory will be allocated
        */
        RID* InsertRecord(const uchar* data, RID* rid){
            //updating the header
            headerBuf = bpm->reusePage(fid, 0, headerIdx, headerBuf);
            int firstEmptySlot = firstZeroBit();
            setBit(firstEmptySlot);
            bpm->markDirty(headerIdx); // the bit map is not a part of header, it could only be written back by bpm
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
            if(rid.GetPageNum() == 0){
                printf("In Table::DeleteRecord, trying to delete a record from header page\n");
                return;
            }
            //updating the header
            headerBuf = bpm->reusePage(fid, 0, headerIdx, headerBuf);
            trackers.push_back(BufTracker(rid.PageNum, tmpIdx));
            int slotNumber = RIDtoUint(&rid);
            clearBit(slotNumber);
            bpm->markDirty(headerIdx);
            if(slotNumber == header->exploitedNum - 1)
                header->exploitedNum--;
            header->recordNum--;
            headerDirty = true;
            //no need to clear the memory
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
                if(header->indexID[i] == 0)
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
        int AddFKMaster(uchar masterID, uint slaveKeys, uint masterKeys, const char* fkName){
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
            // update master key ID
            header->masterKeyID[fkMasterCount] = masterKeys;
            // update slave key ID
            header->slaveKeyID[fkMasterCount] = slaveKeys;
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
            // update master key ID, type: uint
            memcpy(&header->masterKeyID[pos], &header->masterKeyID[pos + 1], (fkMasterCount - pos - 1) * sizeof(uint));
            header->masterKeyID[fkMasterCount - 1] = 0;
            // update slave key ID, type: uint
            memcpy(&header->slaveKeyID[pos], &header->slaveKeyID[pos + 1], (fkMasterCount - pos - 1) * sizeof(uint));
            header->slaveKeyID[fkMasterCount - 1] = 0;
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
         * Caution, this action writes back data not only in this table, but all the tables
        */
        void WriteBack(){ 
            if(headerDirty){
                headerBuf = bpm->reusePage(fid, 0, headerIdx, headerBuf);
                header->ToString(headerBuf);
                bpm->markDirty(headerIdx);
                bpm->writeBack(headerIdx); // marking dirty is not enough... write it back
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
            if(rid.PageNum == 0){
                printf("In Table::NextRecord, pageNum is 0\n");
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
         * ! Unchecked: if cols == 0, name length illegal
        */
        int CreateIndexOn(uint cols, const char* idxName);

        /**
         * Remove index by name
         * Checked: idx not found
         * ! Unchecked: name length illegal
        */
        bool RemoveIndex(const char* idxName);

        int FileID(){
            return fid;
        }

        Scanner* GetScanner(bool (*demand)(const Record& record));
        Scanner* GetScanner(const uchar* right, const uchar* types, const ushort* lengths, uint nullMask, int colNum, uchar* cmp);

        friend class DBMS;
        friend class Database;    
};
#endif