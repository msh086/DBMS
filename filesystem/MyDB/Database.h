#ifndef DATABASE_H
#define DATABASE_H
#include "Table.h"
#include "../bufmanager/BufPageManager.h"
#include "../fileio/FileManager.h"
#include "Header.h"
#include "../RM/SimpleUtils.h"
#include "Scanner.h"
#include <vector>

class Database{
    char name[MAX_DB_NAME_LEN] = "";

    static BufPageManager* bpm;
    static FileManager* fm;

    // static Table* activeTables[MAX_TB_NUM];
    static std::vector<Table*> activeTables;
    // static int activeTableNum;

    // Private helper methods added by msh
    int getActiveTableIndex(const char* tablename){
        int ans = 0;
        for(auto it = activeTables.begin(); it != activeTables.end(); it++, ans++)
            if(strncmp((*it)->tablename, tablename, MAX_TABLE_NAME_LEN) == 0)
                return ans;
        return -1;
    }

    void closeAndRemoveTableAt(int index){
        activeTables[index]->WriteBack();
        int closeret = fm->closeFile(activeTables[index]->fid);
        delete activeTables[index]; // free memory
        activeTables.erase(activeTables.begin() + index);
        if(closeret != 0)
            printf("In Database::CloseTable, cannot close file\n");
    }

    // void removeTableAt(int index){
    //     activeTables.erase(activeTables.begin() + index);
    // }

    void resetBufInTable(int index){
        activeTables[index]->tmpBuf = nullptr;
        activeTables[index]->headerBuf = nullptr;
    }

    // void resetAllBuf(){
    //     for(int i = 0; i < tableNum; i++)
    //         resetBufInTable(i);
    // }

    // 判断表名是否为预留表名
    bool tablenameReserved(const char* tableName){
        return identical(tableName, DB_RESERVED_TABLE_NAME, MAX_TABLE_NAME_LEN) ||
            identical(tableName, IDX_RESERVED_TABLE_NAME, MAX_TABLE_NAME_LEN) ||
            identical(tableName, VARCHAR_RESERVED_TABLE_NAME, MAX_TABLE_NAME_LEN);
    }

    char filePathBuf[MAX_DB_NAME_LEN + MAX_TABLE_NAME_LEN + 3 + 4 + 1] = ""; // 3 = ./***/***, 4 = ***-TMP, 1 = \0

    const char* getPath(const char* tableName){
        sprintf(filePathBuf, "%s/%s", name, tableName);
        return filePathBuf;
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
            bool openret_info = fm->openFile(getPath(DB_RESERVED_TABLE_NAME), fid_info),
                openret_idx = fm->openFile(getPath(IDX_RESERVED_TABLE_NAME), fid_idx),
                openret_varchar = fm->openFile(getPath(VARCHAR_RESERVED_TABLE_NAME), fid_varchar);
            if(!openret_info){
                printf("Initializing db info\n");
                // create info
                fm->createFile(getPath(DB_RESERVED_TABLE_NAME));
                fm->openFile(getPath(DB_RESERVED_TABLE_NAME), fid_info);
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
                fm->createFile(getPath(IDX_RESERVED_TABLE_NAME));
                fm->openFile(getPath(IDX_RESERVED_TABLE_NAME), fid_idx);
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
                fm->writePage(fid_idx, 0, (BufType)buffer, 0);
                delete[] buffer;
                printf("db index init success\n");
            }
            if(!openret_varchar){
                printf("Initializing db varchar\n");
                // create info
                fm->createFile(getPath(VARCHAR_RESERVED_TABLE_NAME));
                fm->openFile(getPath(VARCHAR_RESERVED_TABLE_NAME), fid_varchar);
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
                fm->writePage(fid_varchar, 0, (BufType)buffer, 0);
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

        // 判断用户表是否存在
        bool TableExists(const char* tableName){
            uchar type = DataType::CHAR;
            ushort length = MAX_TABLE_NAME_LEN;
            uchar cmp = Comparator::Eq;
            rec->FreeMemory();
            infoScanner->SetDemand((const uchar*)tableName, &type, &length, 0, 1, &cmp);
            if(infoScanner->NextRecord(rec)){
                infoScanner->Reset();
                return true;
            }
            infoScanner->Reset();
            return false;
        }

        bool CreateTable(const char* tablename, Header* header, const uchar* defaultRecord){
            if(TableExists(tablename))
                return false;
            //TODO : check on header validity?
            bool createret = fm->createFile(getPath(tablename));
            if(!createret){
                printf("In Database::CreateTable, cannot create file\n");
                return false;
            }
            int fid;
            bool openret = fm->openFile(getPath(tablename), fid);
            if(!openret){
                printf("In Database::CreateTable, cannot open file\n");
                return false;
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
            uchar data[MAX_TABLE_NAME_LEN] = {0};
            memcpy(data, tablename, strlen(tablename));
            info->InsertRecord(data, rid);
            return true;
        }

        /**
         * Do not delete an opened file!
        */
        bool DeleteTable(const char* tablename){
            // TODO: protection for reserved tables? or in parser?
            if(TableExists(tablename)){
                remove(getPath(tablename));
                info->DeleteRecord(*rec->GetRid());
                return true;
            }
            return false;
            //TODO : cascade deletion for foreign keys? or in parser?
        }
        /**
         * Open the table of tablename
         * NOTE: Trying to open an opened table may get a result of nullptr
         * NOTE: DO NOT free the memory of Table by yourself, this is done by Database
        */
        Table* OpenTable(const char* tablename){
            if(!TableExists(tablename))
                return nullptr;
            int fid;
            bool openret = fm->openFile(getPath(tablename), fid);
            if(!openret){
                printf("In Database::OpenTable, cannot open file\n");
                return nullptr;
            }
            Table* ans = new Table(fid, tablename, this, rec->GetRid()->GetSlotNum() - 1); // ? This way of acquiring table ID may be ok if only page 1 is used in info
            activeTables.push_back(ans);
            return ans;
        }

        /**
         * This will remove a table from activeTables, writing its buffer back to disk
        */
        void CloseTable(const char* tablename){
            int index = getActiveTableIndex(tablename);
            if(index >= 0)
                closeAndRemoveTableAt(index);
            else
                printf("No table named %.*s\n", MAX_TABLE_NAME_LEN, tablename);
        }

        // ! All method below should be at table level

        // void AlterTableAddCol(const char* colName, uchar type, bool nullable, ushort length, bool isDft, const uchar* dftVal){
        //     // TODO
        // }

        // void AlterTableDropCol(const char* colName){
        //     // TODO
        // }

        // void AlterTableChangeCol(const char* colName, uchar type, bool nullable, ushort length, bool isDft, const uchar* dftVal){
        //     // TODO
        // }

        // void AlterTableAddPrimaryKey(const char** colNames, uint colNum){
        //     // TODO
        // }

        // void AlterTableDropPrimaryKey(){
        //     // TODO
        // }

        bool RenameTable(const char* oldName, const char* newName){
            // TODO, close first if opened?
            if(TableExists(oldName)){ // after calling tableExist(name), the found record is stored in 'rec'
                memset(rec->GetData(), 0, MAX_TABLE_NAME_LEN);
                memcpy(rec->GetData(), newName, strlen(newName));
                info->UpdateRecord(*rec->GetRid(), rec->GetData(), 0, 0, MAX_TABLE_NAME_LEN);
                return true;
            }
            else
                return false;
        }

        /**
         * 向varchar表内存储一个字符串,其起始地址保存到entry中
        */
        RID* InsertLongVarchar(const char* str, uint length, RID* entry){
            uchar buf[VARCHAR_RECORD_LEN] = {0};
            RID prev = RID(), cur = RID();
            bool hasPrev = false;
            while(length > VARCHAR_FRAG_LEN){
                // next page(4), next slot(4), length(2), data(502)
                memset(buf, 0, 10);
                *(ushort*)(buf + 8) = VARCHAR_FRAG_LEN;
                memcpy(buf + 10, str, VARCHAR_FRAG_LEN);
                varchar->InsertRecord(buf, &cur);
                // update prev record's nextPage & nextSlot
                if(hasPrev){
                    *(uint*)buf = cur.GetPageNum();
                    *(uint*)(buf + 4) = cur.GetSlotNum();
                    varchar->UpdateRecord(prev, buf, 0, 0, 8);
                }
                else{
                    entry->PageNum = cur.PageNum;
                    entry->SlotNum = cur.SlotNum;
                }
                // update loop
                hasPrev = true;
                prev = cur;
                str += VARCHAR_FRAG_LEN;
                length -= VARCHAR_FRAG_LEN;
            }
            // now length < VARCHAR_FRAG_LEN
            if(length > 0){
                memset(buf, 0, 10);
                *(ushort*)(buf + 8) = length;
                memcpy(buf + 10, str, length);
                memset(buf + 10 + length, 0, VARCHAR_FRAG_LEN - length);
                varchar->InsertRecord(buf, &cur);
                if(hasPrev){
                    *(uint*)buf = cur.GetPageNum();
                    *(uint*)(buf + 4) = cur.GetSlotNum();
                    varchar->UpdateRecord(prev, buf, 0, 0, 8);
                }
                else{
                    entry->PageNum = cur.PageNum;
                    entry->SlotNum = cur.SlotNum;
                }
            }
            return entry;
        }

        /**
         * 根据提供的RID获取长varchar,其内容被存在dst中,长度被存在len中
         * 请将dst开到足够大,否则会访问非法内存
        */
        void GetLongVarchar(const RID& rid, uchar* dst, ushort& len){
            Record tmpRec;
            len = 0;
            RID tmpRID = RID(rid.PageNum, rid.SlotNum);
            while(tmpRID.PageNum && tmpRID.SlotNum){
                varchar->GetRecord(tmpRID, &tmpRec);
                tmpRID.PageNum = *(uint*)tmpRec.GetData();
                tmpRID.SlotNum = *(uint*)(tmpRec.GetData() +  4);
                ushort fragLen = *(ushort*)(tmpRec.GetData() + 8);
                memcpy(dst, tmpRec.GetData() + 10, fragLen);
                dst += fragLen;
                len += fragLen;
                tmpRec.FreeMemory();
            }
        }

        void RemoveLongVarchar(const RID& rid){
            Record tmpRec = Record();
            RID tmpRID = RID(rid.PageNum, rid.SlotNum);
            while(tmpRID.PageNum && tmpRID.SlotNum){
                varchar->GetRecord(tmpRID, &tmpRec);
                uint pn = *(uint*)tmpRec.GetData(), sn = *(uint*)(tmpRec.GetData() + 4);
                varchar->DeleteRecord(tmpRID);
                tmpRec.FreeMemory();
                tmpRID.PageNum = pn;
                tmpRID.SlotNum = sn;
            }
        }

        Scanner* ShowTables(){
            infoScanner->SetDemand([](const Record& record)->bool{return true;});
            return infoScanner;
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
            for(auto it = activeTables.begin(); it != activeTables.end(); it++){
                (*it)->WriteBack();
                int closeRet = fm->closeFile((*it)->fid);
                delete *it;
                if(closeRet != 0)
                    printf("In Database::Close, error when closing table\n");
            }
            activeTables.clear();
        }

        const char* GetName(){
            return name;
        }
};
#endif