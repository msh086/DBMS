#ifndef RECORD_H_INCLUDED
#define RECORD_H_INCLUDED
#include "RID.h"
class FileHandler;
class Record
{
    private:
        char* data=nullptr;
        RID* id=nullptr;
        void SetAttr(char* data, RID* id){
            data=data;
            id=id;
        }
    public:
        bool GetData(char *&pData)const{
            if(data==nullptr)
                return false;
            pData=data;
            return true;
        }
        bool GetRid(RID &rid)const{
            if(id==nullptr)
                return false;
            rid=*id;
            return true;
        }
        /**
        *Memory management is always Record's duty once 
        */
        ~Record(){
            if(data != nullptr){
                delete[] data;
                data = nullptr;
            }
            if(id != nullptr){
                delete id;
                id = nullptr;
            }
        }
    friend class FileHandler;//need to assign rid and data in FileHandler.GetRec
};


#endif // RECORD_H_INCLUDED
