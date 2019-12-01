#ifndef TABLE_H
#define TABLE_H
#include "Header.h"
#include "../RM/Record.h"
#include "../bufmanager/BufPageManager.h"
#include "../RM/SimpleUtils.h"
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

    static FileManager* fm;
    static BufPageManager* bpm;

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
        Table(int fid, const char* tableName){
            header = new Header();
            this->fid = fid;
            headerBuf = (uchar*)bpm->getPage(fid, 0, headerIdx);
            bpm->access(headerIdx);
            header->FromString(headerBuf);
            strncpy(this->tablename, tableName, strnlen(tableName, MAX_TABLE_NAME_LEN));
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
            header->recordNum++;
            if(firstEmptySlot == header->exploitedNum)
                header->exploitedNum++;
            //* Update this when you need to change header's structure
            ((uint*)headerBuf)[2] = header->recordNum;
            ((uint*)headerBuf)[3] = header->exploitedNum;
            // Deprecated: //// header->ToString(headerBuf);
            bpm->markDirty(headerIdx);
            //inserting the record
            UintToRID(firstEmptySlot, rid);
            tmpBuf = bpm->reusePage(fid, rid->PageNum, tmpIdx, tmpBuf);
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
            int slotNumber = RIDtoUint(&rid);
            clearBit(slotNumber);
            if(slotNumber == header->exploitedNum - 1)
                header->exploitedNum--;
            header->recordNum--;
            //* Update this when you need to change header's structure
            ((uint*)headerBuf)[2] = header->recordNum;
            ((uint*)headerBuf)[3] = header->exploitedNum;
            // Deprecated: ////header->ToString(headerBuf);
            bpm->markDirty(headerIdx);
            //no need to clear the memory
        }

        /**
         * Set the record of the corresponding RID to record
        */
        void UpdateRecord(const Record& record){
            if(record.id->PageNum == 0){
                printf("In Table::UpdateRecord, trying to update a record from header page\n");
                return;
            }
            tmpBuf = bpm->reusePage(fid, record.GetRid()->GetPageNum(), tmpIdx, tmpBuf);
            memcpy(tmpBuf + record.GetRid()->GetSlotNum() * header->recordLenth, record.GetData(), header->recordLenth);
            bpm->markDirty(tmpIdx);
        }

        /**
         * Add foreign constraint, return if success
         * Same constraint conditions with different names are permitted
         * Checked: existing constraints with same name
         * ! Unchecked: primary key, conflicts with existing records
        */
        int AddFKMaster(uchar masterID, uint slaveKeys, uint masterKeys, const char* fkName){
            headerBuf = bpm->reusePage(fid, 0, headerIdx, headerBuf);
            // the start of constraintName
            uchar* fkPart = headerBuf + Header::fkOffset + MAX_REF_SLAVE_TIME * 9;
            int pos = 0;
            for(; pos < MAX_REF_SLAVE_TIME; pos++){
                if(!strnlen((char*)fkPart, MAX_CONSTRAIN_NAME_LEN)) // all existing index names are checked
                    break;
                if(identical((char*)fkPart, fkName, MAX_CONSTRAIN_NAME_LEN))
                    return 1; // existing fk with the same name
                fkPart += MAX_CONSTRAIN_NAME_LEN;
            }
            if(pos == MAX_REF_SLAVE_TIME)
                return 2;
            // update constraint name
            memcpy(fkPart, fkName, strnlen(fkName, MAX_CONSTRAIN_NAME_LEN));
            fkPart = headerBuf + Header::fkOffset;
            // update master table ID
            *fkPart = masterID;
            // update master key ID
            fkPart += MAX_REF_SLAVE_TIME;
            ((uint*)fkPart)[pos] = masterKeys;
            // update slave key ID
            fkPart += MAX_REF_SLAVE_TIME * sizeof(uint);
            ((uint*)fkPart)[pos] = slaveKeys;
            // sync
            bpm->markDirty(headerIdx);
            return 0;
        }

        /**
         * Remove foreign constraint master, return if success
        */
        bool RemoveFKMaster(const char* fkName){
            headerBuf = bpm->reusePage(fid, 0, headerIdx, headerBuf);
            uchar* fkPart = headerBuf + Header::fkOffset + MAX_REF_SLAVE_TIME * 9;
            int pos = 0;
            for(; pos < MAX_REF_SLAVE_TIME; pos++){
                if(!strnlen((char*)fkPart, MAX_CONSTRAIN_NAME_LEN)) // all existing index names are checked
                    return false;
                if(identical((char*)fkPart, fkName, MAX_CONSTRAIN_NAME_LEN))
                    break;
                fkPart += MAX_CONSTRAIN_NAME_LEN;
            }
            if(pos == MAX_REF_SLAVE_TIME)
                return false;
            int probe = pos + 1;
            uchar* probePtr = fkPart;
            while(probe < MAX_REF_SLAVE_TIME){
                if(!strnlen((char*)probePtr, MAX_CONSTRAIN_NAME_LEN))
                    break;
                probe++;
                probePtr += MAX_CONSTRAIN_NAME_LEN;
            }
            // now probe is the total num of fks in the header
            // update fk masters
            memcpy(fkPart, fkPart + MAX_REF_SLAVE_TIME, (probe - pos - 1) * MAX_CONSTRAIN_NAME_LEN);
            memset(probePtr - MAX_CONSTRAIN_NAME_LEN, 0, MAX_CONSTRAIN_NAME_LEN);
            // update master ID
            fkPart = headerBuf + Header::fkOffset;
            memcpy(fkPart + pos, fkPart + pos + 1, probe - pos - 1);
            memset(fkPart + probe - 1, 0, 1);
            // update master key ID
            fkPart += MAX_REF_SLAVE_TIME * sizeof(uint);
            memcpy(fkPart + pos * sizeof(uint), fkPart + (pos + 1) * sizeof(uint), (probe - pos - 1) * sizeof(uint));
            memset(fkPart + (probe - 1) * sizeof(uint), 0, sizeof(uint));
            // update slave key ID
            fkPart += MAX_REF_SLAVE_TIME * sizeof(uint);
            memcpy(fkPart + pos * sizeof(uint), fkPart + (pos + 1) * sizeof(uint), (probe - pos - 1) * sizeof(uint));
            memset(fkPart + (probe - 1) * sizeof(uint), 0, sizeof(uint));
            // sync
            bpm->markDirty(headerIdx);
            return true;
        }

        /**
         * Remove foreign constraint slave
        */
        bool AddFKSlave(uchar slaveID){
            headerBuf = bpm->reusePage(fid, 0, headerIdx, headerBuf);
            uchar* slaves = headerBuf + Header::idxOffset - MAX_FK_MASTER_TIME;
            int pos = 0;
            while(pos < MAX_FK_MASTER_TIME){
                if(slaves[pos] == TB_ID_NONE)
                    return false;
                if(slaves[pos] == slaveID)
                    break;
                pos++;
            }
            if(pos == MAX_FK_MASTER_TIME)
                return false;
            // get the total num of slaves
            int probe = pos + 1;
            while(probe < MAX_FK_MASTER_TIME)
                if(slaves[pos] == TB_ID_NONE)
                    break;
                else
                    probe++;
            // update slaves
            memcpy(slaves + pos, slaves + pos + 1, probe - pos - 1);
            memset(slaves + probe - 1, TB_ID_NONE, 1);
            // sync
            bpm->markDirty(headerIdx);
            return true;
        }

        /**
         * Remove foreign constraint slave
        */
        bool RemoveFKSlave(uchar slaveID){
            // TODO
        }

        /**
         * Caution, this action writes back data not only in this table, but all the tables
        */
        static void WriteBack(){ // TODO write back only pages related to this table
            bpm->close();
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

        int FileID(){
            return fid;
        }

        Scanner* GetScanner(bool (*demand)(const Record& record));
        Scanner* GetScanner(const uchar* right, const uchar* types, const ushort* lengths, uint nullMask, int colNum, uchar* cmp);

        friend class DBMS;
        friend class Database;    
};
FileManager* Table::fm = FileManager::Instance();
BufPageManager* Table::bpm = BufPageManager::Instance();
#endif