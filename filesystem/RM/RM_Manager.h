#ifndef RM_MANAGER_H_INCLUDED
#define RM_MANAGER_H_INCLUDED
#include "../fileio/FileManager.h"
#include "../bufmanager/BufPageManager.h"
#include "FileHandler.h"
class RM_Manager
{
    private:
        FileManager* fm=nullptr;
        BufPageManager *bpm = nullptr;

    public:
        RM_Manager(FileManager &fm){
            this->fm=&fm;
        }

        void SetBPM(BufPageManager *bpm){
            this->bpm = bpm;
        }
        /**
        *@brief
        *If the file doesn't exist: create file and return true/false
        *If the file already exists: an error happens! the first page of the old file will be overwritten by the new file!
        *Return false if recordSize >= PAGE_SIZE
        *@param filename The name of the file to be created
        */
        bool CreateFile(const char* filename, int recordSize){
            if(recordSize>=PAGE_SIZE){
                std::cout<<"In RM_Manager::CreateFile: record size >= page size\n";
                return false;
            }
            bool ret=fm->createFile(filename);
            if(!ret){
                std::cout<<"In RM_Manager::CreateFile: create failed\n";
                return false;
            }
            int fid;
            bool openret=fm->openFile(filename,fid);
            if(!openret){
                std::cout<<"In RM_Manager::CreateFile: open failed\n";
                return false;
            }
            unsigned int buf[PAGE_INT_NUM]={0};//initialize all int to 0
            buf[0]=recordSize;//record size
            buf[1]=0;//record num
            //from buf[2], use bit array to record if the slot status in each page. bit 0 for empty slot
            int writeret=fm->writePage(fid,0,buf,0);
            if(writeret!=0){
                std::cout<<"In RM_Manager::CreateFile: write failed\n";
                return false;
            }
            return true;
        }

        bool DestroyFile(const char* fileName){
            int rmret=remove(fileName);
            if(rmret!=0){
                std::cout<<"In RM_Manager::DestroyFile: error in remove\n";
                return false;
            }
            return true;
        }

        bool OpenFile(const char* fileName, FileHandler &fileHandler){
            int fid;
            bool openret=fm->openFile(fileName,fid);
            if(!openret){
                std::cout<<"In RM_Manager::OpenFile: open failed\n";
                return false;
            }
            fileHandler.init(fid, bpm);
        }

        bool CloseFile(FileHandler& handler){
            int closeret = fm->closeFile(handler.fid);
            if(closeret != 0){
                std::cout<<"In RM_Manager::CloseFile: close failed\n";
                return false;
            }
            handler.init = false;
            return true;
        }
};


#endif // RM_MANAGER_H_INCLUDED
