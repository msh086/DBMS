#ifndef DATABASE_H
#define DATABASE_H
#include "Table.h"
#include "../bufmanager/BufPageManager.h"
#include "../fileio/FileManager.h"
#include "Header.h"

class Database{
    char name[MAX_DB_NAME_LEN] = "";
    int tableNum = 0;

    static BufPageManager* bpm;
    static FileManager* fm;

    static Table* activeTables[MAX_TB_NUM];
    static int activeTableNum;

    // Private helper methods added by msh
    int getActiveTableIndex(const char* tablename){
        for(int i = 0; i < activeTableNum; i++)
            if(activeTables[i]->tablename == tablename)
                return i;
        return -1;
    }

    void removeTableAt(int index){
        for(int i = index + 1; i < activeTableNum; i++)
            activeTables[i - 1] = activeTables[i];
        activeTableNum--;
    }

    void resetBufInTable(int index){
        activeTables[index]->tmpBuf = nullptr;
        activeTables[index]->headerBuf = nullptr;
    }

    void resetAllBuf(){
        for(int i = 0; i < tableNum; i++)
            resetBufInTable(i);
    }


    public:
        void CreateTable(const char* tablename, Header* header, const char* defaultRecord){
            //TODO : check on header validity?
            bool createret = fm->createFile(tablename);
            if(!createret){
                printf("In Database::CreateTable, cannot create file\n");
                return;
            }
            int fid;
            bool openret = fm->openFile(tablename, fid);
            if(!openret){
                printf("In Database::CreateTable, cannot open file\n");
                return;
            }
            char* buffer = new char[PAGE_SIZE];
            header->ToString(buffer);
            int writeret = fm->writePage(fid, 0, (BufType)buffer, 0);
            delete[] buffer;
            if(writeret != 0){
                printf("In Database::CreateTable, cannot write file\n");
            }
            int closeret = fm->closeFile(fid); // Even if file cannot be written, it still should be closed
            if(closeret != 0){
                printf("In Databse::CreateTable, cannot close file\n");
            }
        }

        void RemoveTable(const char* tablename){
            remove(tablename);
            //TODO : cascade deletion for foreign keys
        }
        /**
         * Open the table of tablename
         * NOTE: Trying to open an opened table may get a result of nullptr
        */
        Table* OpenTable(const char* tablename){
            int fid;
            bool openret = fm->openFile(tablename, fid);
            if(!openret){
                printf("In Database::OpenTable, cannot open file\n");
                return nullptr;
            }
            Table* ans = new Table();
            int index;
            BufType headerBuf = bpm->getPage(fid, 0, index);
            bpm->access(index);
            ans->header->FromString(headerBuf);
            ans->fid = fid;
            ans->headerIdx = index;
            ans->headerBuf = (char*)headerBuf;
            memcpy(ans->tablename, tablename, strlen(tablename));
            activeTables[activeTableNum++] = ans;
            return ans;
        }

        void CloseTable(const char* tablename){
            int index = getActiveTableIndex(tablename);
            if(index >= 0){
                bpm->close();
                resetAllBuf();
                int closeret = fm->closeFile(activeTables[index]->fid);
                removeTableAt(index);
                if(closeret != 0)
                    printf("In Database::CloseTable, cannot close file\n");
            }
        }
};
BufPageManager* Database::bpm = BufPageManager::Instance();
FileManager* Database::fm = FileManager::Instance();
Table* Database::activeTables[MAX_TB_NUM] = {0};
int Database::activeTableNum = 0;
#endif