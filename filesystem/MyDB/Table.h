#ifndef TABLE_H
#define TABLE_H
#include "BaseHeader.h"
#include "../RM/Record.h"
#include "../bufmanager/BufPageManager.h"
class Database;
class Scanner;

class Table{
    BaseHeader* header;
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
         * Caution, this action writes back data not only in this table, but all the tables
        */
        static void WriteBack(){
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

        friend class Database;    
};
FileManager* Table::fm = FileManager::Instance();
BufPageManager* Table::bpm = BufPageManager::Instance();
#endif