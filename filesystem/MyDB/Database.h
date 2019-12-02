#ifndef DATABASE_H
#define DATABASE_H
#include "Table.h"
#include "../bufmanager/BufPageManager.h"
#include "../fileio/FileManager.h"
#include "Header.h"
#include "../RM/SimpleUtils.h"
#include "Scanner.h"

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
            if(!strncmp(activeTables[i]->tablename, tablename, MAX_TABLE_NAME_LEN))
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

    // 判断表名是否为预留表名
    bool tablenameReserved(const char* tableName){
        return identical(tableName, DB_RESERVED_TABLE_NAME, MAX_TABLE_NAME_LEN) ||
            identical(tableName, IDX_RESERVED_TABLE_NAME, MAX_TABLE_NAME_LEN) ||
            identical(tableName, VARCHAR_RESERVED_TABLE_NAME, MAX_TABLE_NAME_LEN);
    }

    // 判断用户表是否存在
    bool tableExists(const char* tableName){
        uchar type = DataType::CHAR;
        ushort length = MAX_TABLE_NAME_LEN;
        uchar cmp = Comparator::Eq;
        infoScanner->SetDemand((const uchar*)tableName, &type, &length, 0, 1, &cmp);
        if(infoScanner->NextRecord(rec)){
            infoScanner->Reset();
            return true;
        }
        infoScanner->Reset();
        return true;
    }


    // 存储所有用户表
    Table *info = nullptr;
    Scanner* infoScanner = nullptr;
    Record* rec = nullptr;
    RID* rid = nullptr;
    // 存储archar
    Table *varchar = nullptr;

    public:
        // 存储索引节点
        Table *idx = nullptr;

        Database(const char* databaseName){
            memcpy(name, databaseName, strnlen(databaseName, MAX_DB_NAME_LEN));
            int fid_info, fid_idx, fid_varchar;
            bool openret_info = fm->openFile(DB_RESERVED_TABLE_NAME, fid_info),
                openret_idx = fm->openFile(DB_RESERVED_TABLE_NAME, fid_idx),
                openret_varchar = fm->openFile(DB_RESERVED_TABLE_NAME, fid_varchar);
            if(!openret_info){
                printf("Initializing db info\n");
                // create info
                fm->createFile(DB_RESERVED_TABLE_NAME);
                fm->openFile(DB_RESERVED_TABLE_NAME, fid_info);
                uchar* buffer = new uchar[PAGE_SIZE]{};
                Header* header = new Header();
                header->recordLenth = MAX_TABLE_NAME_LEN;
                header->slotNum = PAGE_SIZE / header->recordLenth;
                header->attrLenth[0] = header->recordLenth;
                header->attrType[0] = DataType::CHAR;
                header->nullMask = 0;
                //handle default record, which always exists not matter the value of defaultKeyMask
                buffer[header->GetLenth()] = 128; // manually set the first bit in bitmap to 1
                header->recordNum = 1;
                header->exploitedNum = 1;
                header->ToString(buffer);
                delete header;
                fm->writePage(fid_info, 0, (BufType)buffer, 0);
                delete[] buffer;
                printf("db info init success\n");
            }
            if(!openret_idx){
                printf("Initializing db index\n");
                // create info
                fm->createFile(IDX_RESERVED_TABLE_NAME);
                fm->openFile(IDX_RESERVED_TABLE_NAME, fid_info);
                uchar* buffer = new uchar[PAGE_SIZE]{};
                Header* header = new Header();
                header->recordLenth = PAGE_SIZE; // TODO the null byte?
                header->slotNum = 1;
                header->nullMask = 0;
                //handle default record, which always exists not matter the value of defaultKeyMask
                buffer[header->GetLenth()] = 128; // manually set the first bit in bitmap to 1
                header->recordNum = 1;
                header->exploitedNum = 1;
                header->ToString(buffer);
                delete header;
                fm->writePage(fid_info, 0, (BufType)buffer, 0);
                delete[] buffer;
                printf("db index init success\n");
            }
            if(!openret_varchar){
                printf("Initializing db varchar\n");
                // create info
                fm->createFile(VARCHAR_RESERVED_TABLE_NAME);
                fm->openFile(VARCHAR_RESERVED_TABLE_NAME, fid_info);
                uchar* buffer = new uchar[PAGE_SIZE]{};
                Header* header = new Header();
                header->recordLenth = VARCHAR_RECORD_LEN;;
                header->slotNum = PAGE_SIZE / header->recordLenth;
                // part 0, str data
                header->attrLenth[0] = VARCHAR_RECORD_LEN - 10; // 502 // TODO the null byte?
                header->attrType[0] = DataType::CHAR;
                // part 1, next page
                header->attrLenth[1] = 4;
                header->attrType[1] = DataType::CHAR; // convenient for comparison
                // part 2, next slot
                header->attrLenth[2] = 4;
                header->attrType[2] = DataType::CHAR; // convenient for comparison
                // part 3, length of frag
                header->attrLenth[3] = 2;
                header->attrType[3] = DataType::CHAR; // convenient for comparison
                // cannot be null
                header->nullMask = 0;
                //handle default record, which always exists not matter the value of defaultKeyMask
                buffer[header->GetLenth()] = 128; // manually set the first bit in bitmap to 1
                header->recordNum = 1;
                header->exploitedNum = 1;
                header->ToString(buffer);
                delete header;
                fm->writePage(fid_info, 0, (BufType)buffer, 0);
                delete[] buffer;
                printf("db varchar init success\n");
            }
            // load info from disk
            info = new Table(fid_info, DB_RESERVED_TABLE_NAME, this);
            infoScanner = info->GetScanner(nullptr);
            rec = new Record();
            rid = new RID();
            // load idx from disk
            idx = new Table(fid_idx, IDX_RESERVED_TABLE_NAME, this);
            // load varchar from disk
            varchar = new Table(fid_varchar, VARCHAR_RESERVED_TABLE_NAME, this);
        }

        void CreateTable(const char* tablename, Header* header, const uchar* defaultRecord){
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
            uchar* buffer = new uchar[PAGE_SIZE]{};
            //handle default record, which always exists not matter the value of defaultKeyMask
            buffer[header->GetLenth()] = 128; // manually set the first bit in bitmap to 1
            header->recordNum = 1;
            header->exploitedNum = 1;

            if(header->defaultKeyMask){ // insert the default record. Note that even there is no default record, page 1 is still seen as occupied
                uchar* defaultBuf = new uchar[PAGE_SIZE]{};
                    memcpy(defaultBuf, defaultRecord, header->recordLenth);
                int defaultret = fm->writePage(fid, 1, (BufType)defaultBuf, 0);
                delete[] defaultBuf;
                if(defaultret != 0)
                    printf("In Database::CreateTable, cannot write default record\n");
            }

            header->ToString(buffer);
            int writeret = fm->writePage(fid, 0, (BufType)buffer, 0);
            delete[] buffer;
            if(writeret != 0){
                printf("In Database::CreateTable, cannot write header\n");
            }
            int closeret = fm->closeFile(fid); // Even if file cannot be written, it still should be closed
            if(closeret != 0){
                printf("In Databse::CreateTable, cannot close file\n");
            }
        }

        /**
         * Do not delete an opened file!
        */
        void DeleteTable(const char* tablename){
            remove(tablename);
            //TODO : cascade deletion for foreign keys
        }
        /**
         * Open the table of tablename
         * NOTE: Trying to open an opened table may get a result of nullptr
         * NOTE: DO NOT free the memory of Table by yourself, this is done by Database
        */
        Table* OpenTable(const char* tablename){
            int fid;
            bool openret = fm->openFile(tablename, fid);
            if(!openret){
                printf("In Database::OpenTable, cannot open file\n");
                return nullptr;
            }
            Table* ans = new Table(fid, tablename, this);
            activeTables[activeTableNum++] = ans;
            return ans;
        }

        /**
         * This will remove a table from activeTables
         * WARNING: this will clear the bpm
        */
        void CloseTable(const char* tablename){
            int index = getActiveTableIndex(tablename);
            if(index >= 0){
                bpm->close();
                resetAllBuf();
                int closeret = fm->closeFile(activeTables[index]->fid);
                delete activeTables[index]; // free memory
                removeTableAt(index);
                if(closeret != 0)
                    printf("In Database::CloseTable, cannot close file\n");
            }
            else
                printf("No table named %.*s\n", MAX_TABLE_NAME_LEN, tablename);
        }

        /**
         * 关闭这个数据库
        */
        void Close(){
            info->WriteBack();
            idx->WriteBack();
            varchar->WriteBack();
            delete info;
            delete idx;
            delete varchar;
            delete rec;
            delete rid;
            // TODO write back all tables
        }

        const char* GetName(){
            return name;
        }
};
BufPageManager* Database::bpm = BufPageManager::Instance();
FileManager* Database::fm = FileManager::Instance();
Table* Database::activeTables[MAX_TB_NUM] = {0};
int Database::activeTableNum = 0;
#endif