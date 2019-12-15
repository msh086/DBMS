#ifndef RECORD_H_INCLUDED
#define RECORD_H_INCLUDED
#include "RID.h"
#include "../utils/pagedef.h"
class FileHandler;
class Table;
class Record
{
    private:
        uchar* data=nullptr;
        RID* id=nullptr;
        void SetAttr(uchar* data, RID* id){
            data=data;
            id=id;
        }
    public:
        uchar* GetData()const{
            return data;
        }
        RID* GetRid()const{
            return id;
        }

        void FreeMemory(){
            if(data != nullptr){
                delete[] data;
                data = nullptr;
            }
            if(id != nullptr){
                delete id;
                id = nullptr;
            }
        }
        /**
        *Memory management is always Record's duty once it's assigned to one of Record's fields
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
    friend class Table;
};


#endif // RECORD_H_INCLUDED
