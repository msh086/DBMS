#ifndef TABLE_H
#define TABLE_H
#include "Header.h"
#include "../RM/Record.h"
#include "../bufmanager/BufPageManager.h"
class Database;

class Table{
    Header* header;
    char* headerBuf;
    int fid;
    int headerIdx;
    char tablename[MAX_TABLE_NAME_LEN + 1] = "";
    int tmpIdx = 0;
    char* tmpBuf = nullptr;

    static FileManager* fm;
    static BufPageManager* bpm;

    //private helper methods
    /**
     * Find the first 0 bit in the bit map
     * We assume bits are stored from higher digits to lower ones in a byte
    */
    int firstZeroBit(){
        char* src = headerBuf + header->lenth;
        int size = header->exploitedNum;
        int fullByte = size >> 3;
        char allOnes = 255;
        for(int i = 0; i < fullByte; i++){
            if(src[i] != allOnes){
                int pos = 0;
                char msb = 128;
                while(src[i] & msb){
                    pos++;
                    msb >>= 1;
                }
                return (i << 3) + pos;
            }
        }
        int pos = 0, remain = size - (fullByte << 3);
        char msb = 128;
        src += (fullByte << 3);
        while((src[0] & msb) && pos < remain){
            pos++;
            msb >>= 1;
        }
        return pos;
        //If pos < remain, there is an empty slot before the furthest record
        //otherwise, pos == remain, it points to the empty slot right after the furthest record
    }

    void setBit(int pos){
        char* src = headerBuf + header->lenth;
        int bytes = pos >> 3, remain = pos & 7;
        (*(src + bytes)) |= (128 >> remain);
    }

    void clearBit(int pos){
        char* src = headerBuf + header->lenth;
        int bytes = pos >> 3, remain = pos & 7;
        (*(src + bytes)) &= ~(128 >> remain);
    }

    public:
        /**
         * Return a COPY of the record of rid
        */
        Record* GetRecord(const RID& rid){
            tmpBuf = bpm->reusePage(fid, rid.GetPageNum(), tmpIdx, tmpBuf);
            bpm->access(tmpIdx);
            Record* ans = new Record();
            ans->data = new char[header->recordLenth];
            memcpy(ans->data, ((char*)tmpBuf) + rid.GetSlotNum() * header->recordLenth, header->recordLenth);
            ans->id = new RID(rid.GetPageNum(), rid.GetSlotNum());
            return ans;
        }

        /**
         * Return the RID of the inserted record
        */
        RID* InsertRecord(const char* data){
            //updating the header
            headerBuf = bpm->reusePage(fid, 0, headerIdx, headerBuf);
            int firstEmptySlot = firstZeroBit();
            setBit(firstEmptySlot);
            header->recordNum++;
            header->exploitedNum++;
            header->ToString(headerBuf);//TODO : write back only the changed parts
            bpm->access(headerIdx);
            bpm->markDirty(headerIdx);
            //inserting the record
            int page = firstEmptySlot / header->slotNum, slot = firstEmptySlot % header->slotNum;
            tmpBuf = bpm->reusePage(fid, page, tmpIdx, tmpBuf);
            memcpy(tmpBuf + slot * header->recordLenth, data, header->recordLenth);
            bpm->access(tmpIdx);
            bpm->markDirty(tmpIdx);
            return new RID(page, slot);
        }

        void DeleteRecord(const RID& rid){
            //updating the header
            headerBuf = bpm->reusePage(fid, 0, headerIdx, headerBuf);
            int slotNumber = rid.GetPageNum() * header->slotNum + rid.GetSlotNum();
            clearBit(slotNumber);
            if(slotNumber == header->exploitedNum - 1)
                header->exploitedNum--;
            header->recordNum--;
            header->ToString(headerBuf);//TODO : write back only the changed parts
            bpm->access(headerIdx);
            bpm->markDirty(headerIdx);
            //no need to clear the memory
        }

        void UpdateRecord(const Record& record){
            tmpBuf = bpm->reusePage(fid, record.GetRid()->GetPageNum(), tmpIdx, tmpBuf);
            memcpy(tmpBuf + record.GetRid()->GetSlotNum() * header->recordLenth, record.GetData(), header->recordLenth);
            bpm->access(tmpIdx);
            bpm->markDirty(tmpIdx);
        }

        /**
         * Caution, this action writes back data not only in this table, but all the tables
        */
        static void WriteBack(){
            bpm->close();
        }

        friend class Database;    
};
FileManager* Table::fm = FileManager::Instance();
BufPageManager* Table::bpm = BufPageManager::Instance();
#endif