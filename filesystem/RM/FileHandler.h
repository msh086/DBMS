#ifndef FILEHANDLER_INCLUDED
#define FILEHANDLER_INCLUDED
#include "RID.h"
#include "Record.h"
#include "../bufmanager/BufPageManager.h"
#include <cstring>
class RM_Manager;
/**
@brief
This class manipulates record on bit level
Only the record lenth is stored in the header, instead of the fields
Records are given data types only in comparison op
*/
class FileHandler
{
    int fid=0;
    bool hasInit=false;

    /**
    *Note that recordLenth is the lenth of bytes, not ints.
    *As a result, maxRecPerPage should be PAGE_SIZE/recordLenth instead of PAGE_INT_SIZE/recordLenth
    */
    int recordLenth=0;
    int recordNum=0;
    BufPageManager* bpm=nullptr;

    int bufIdx=0;
    BufType pageData=nullptr;

    int headIdx=0;
    BufType headData=nullptr;

    int maxRecPerPage;

    void init(int fid, BufPageManager* bpm){
        fid=fid;
        hasInit=true;
        bpm=bpm;
        GetHeadBuf();
        bpm->access(headIdx);
        recordLenth=headData[0];
        recordNum=headData[1];
        maxRecPerPage=PAGE_SIZE/recordLenth;
    }
    public:
        /**
         * One of the few methods that can materialized a Record object
        */
        bool GetRec(const RID &rid, Record & record){
            if(!hasInit){
                std::cout<<"In FileHandler::GetRec: called without init\n";
                return false;
            }
            if(record.data!=nullptr){
                std::cout<<"Caution! In FileHandler::GetRec: record.data not null, risk of memory leak\n";
            }
            GetPageBuf(rid.GetPageNum());
            bpm->access(bufIdx);
            record.data=new char[recordLenth];
            memcpy(record.data,((char*)pageData)+rid.GetSlotNum()*recordLenth,recordLenth);
            record.id = new RID(rid.GetPageNum(), rid.GetSlotNum());
            return true;
        }

        bool InsertRec(const char* pData, RID& rid){
            //write data
            GetPageBuf(rid.GetPageNum());
            bpm->access(bufIdx);
            memcpy(((char*)pageData)+rid.GetSlotNum()*recordLenth,pData,recordLenth);
            bpm->markDirty(bufIdx);
            //change bit array
            GetHeadBuf();
            bpm->access(headIdx);
            SetBitArray(rid.GetPageNum(),rid.GetSlotNum());
            headData[1]++;
            bpm->markDirty(headIdx);
            return true;
        }

        bool DeleteRec(const RID& rid){
            GetHeadBuf();
            bpm->access(headIdx);
            ClearBitArray(rid.GetPageNum(),rid.GetSlotNum());
            headData[1]--;
            bpm->markDirty(headIdx);
            return true;
        }

        bool UpdateRec(Record &record){
            RID* rid=new RID();
            bool ridret=record.GetRid(*rid);
            if(!ridret){
                std::cout<<"In FileHandler::UpdateRec: cannot get rid\n";
                delete rid;
                return false;
            }
            GetPageBuf(rid->GetPageNum());
            bpm->access(bufIdx);
            char *data = nullptr;
            bool dataret = record.GetData(data);
            if(!dataret){
                std::cout<<"In FileHandler::UpdateRec: cannot get data\n";
                delete rid;
                return false;
            }
            memcpy((((char*)pageData)+rid->GetSlotNum()*recordLenth), data, recordLenth);
            bpm->markDirty(bufIdx);
            delete rid;
            return true;
        }
        /**
        *Write all pages back to storage
        **/
        bool Close(){
            bpm->close();
            pageData = nullptr;
            headData = nullptr;
            hasInit = false;
            return true;
        }

        //below are private helper methods defined by msh
    private:
        BufType GetBuf(BufType &buf, int& idx, int page){
            if(buf == nullptr)
                return buf=bpm->getPage(fid,page,idx);
            int curId,curPage;
            bpm->getKey(idx,curId,curPage);
            if(curPage==page && curId==fid)//compare fileID and pageNum. If cache hit, use cache. Else request a new one
                return buf;
            else
                return buf=bpm->getPage(fid,page,idx);
        }
        BufType GetPageBuf(int page){
            return GetBuf(pageData,bufIdx,page);
        }
        BufType GetHeadBuf(){
            return GetBuf(headData,headIdx,0);
        }
        ///set a bit to 1
        void SetBitArray(int pageNum, int slotNum){
            int globalIdx=pageNum*maxRecPerPage+slotNum;
            int byteOffset=globalIdx/8, bitOffset=globalIdx&7;
            (*(((char*)(headData+2))+byteOffset))|=(128>>bitOffset);
        }
        ///clear a bit
        void ClearBitArray(int pageNum, int slotNum){
            int globalIdx=pageNum*maxRecPerPage+slotNum;
            int byteOffset=globalIdx/8, bitOffset=globalIdx&7;
            (*(((char*)(headData+2))+byteOffset))&=(~(128>>bitOffset));
        }
    friend class RM_Manager;
};


#endif // FILEHANDLER_INCLUDED
