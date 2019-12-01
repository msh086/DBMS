#ifndef DBMS_H
#define DEBMS_H
#include "../utils/pagedef.h"
#include "Database.h"
#include "Scanner.h"
#include "../RM/SimpleUtils.h"
// 一个DBMS内可以存放多个database
class DBMS{
    private:
        DBMS(){};
        static DBMS* instance;
        static FileManager* fm;
        static BufPageManager* bpm;
        Table* tb = nullptr;
        Database* currentDB = nullptr;
        // To save the cost of allcating and freeing memory
        Scanner* scanner = nullptr;
        Record* rec = nullptr;
        RID* rid = nullptr;

        // 判断数据库是否已经存在
        bool databaseExists(const char* databaseName){
            uchar type = DataType::CHAR;
            ushort length = MAX_DB_NAME_LEN;
            uchar cmp = Comparator::Eq;
            scanner->SetDemand((const uchar*)databaseName, &type, &length, 0, 1, &cmp);
            if(scanner->NextRecord(rec)){
                scanner->Reset();
                return true;
            }
            scanner->Reset();
            return true;
        }

        // 关闭已经打开的数据库,请先检查currentDB是否为空
        void closeCurrentDatabase(){ // TODO ???
            currentDB->Close();
            delete currentDB;
            currentDB = nullptr;
        }

        // 新建目录
        bool createDir(const char* dirName){
            return mkdir(dirName, S_IRWXU | S_IRWXG | S_IRWXO) == 0;
        }

        // 删除目录
        bool removeDir(const char* dirName){
            return rmdir(dirName) == 0;
        }

    public:
        // 获得全局单例
        static DBMS* Instance(){
            if(instance == nullptr)
                instance = new DBMS();
            return instance;
        }

        // 读取数据库列表，如果文件不存在（第一次打开），则新建DBMS_RESERVED_TABLE_NAME
        bool Init(){
            int fid;
            bool openret = fm->openFile(DBMS_RESERVED_TABLE_NAME, fid);
            if(!openret){
                printf("First time opening DBMS, initializing...\n");
                bool createret = fm->createFile(DBMS_RESERVED_TABLE_NAME);
                if(!createret){
                    printf("DBMS: cannot create file\n");
                    return false;
                }
                openret = fm->openFile(DBMS_RESERVED_TABLE_NAME, fid);
                if(!openret){
                    printf("DBMS: cannot open file\n");
                    return false;
                }
                uchar* buffer = new uchar[PAGE_SIZE]{};
                Header* header = new Header();
                header->recordLenth = MAX_DB_NAME_LEN;
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
                int writeret = fm->writePage(fid, 0, (BufType)buffer, 0);
                delete[] buffer;
                if(writeret != 0){
                    printf("DBMS: cannot write header\n");
                    return false;
                }
                printf("DBMS init success\n");
            }
            Table* ans = new Table();
            int index;
            BufType headerBuf = bpm->getPage(fid, 0, index);
            bpm->access(index);
            ans->header->FromString(headerBuf);
            ans->fid = fid;
            ans->headerIdx = index;
            ans->headerBuf = (uchar*)headerBuf;
            memcpy(ans->tablename, DBMS_RESERVED_TABLE_NAME, strlen(DBMS_RESERVED_TABLE_NAME));
            tb = ans;
            scanner = tb->GetScanner(nullptr);
            rec = new Record();
            rid = new RID();
            return true;
        }
        
        /**
         * 试图创建名为databaseName的数据库，如果已经存在，返回false,否则将其插入至DBMS_RESERVED_TABLE_NAME并返回true
         * 非法名字长度（0或者>MAX_DB_NAME_LEN)由frontend检查
        */
        bool CreateDatabase(const char* databaseName){
            char buf[MAX_DB_NAME_LEN] = "";
            memcpy(buf, databaseName, strnlen(databaseName, MAX_DB_NAME_LEN));
            if(databaseExists(databaseName))
                return false;
            createDir(databaseName);
            tb->InsertRecord((const uchar*)databaseName, rid);
            return true;
        }

        // 使用数据库
        // 非法名字长度（0或>MAX_DB_NAME_LEN）由frontend检查
        // 不存在时返回nullptr
        Database* UseDatabase(const char* databaseName){
            if(databaseExists(databaseName)){
                if(currentDB){ // there is an open database
                    if(!identical(databaseName, DBMS_RESERVED_TABLE_NAME, MAX_DB_NAME_LEN)) // switch to another database
                        closeCurrentDatabase();
                    else // target database already opened
                        return currentDB;
                }
                // no db is open or the opened db is not the target
                currentDB = new Database(databaseName);
                // TODO ???
                return currentDB;
            }
            else
                return nullptr;
        }

        // 删除数据库
        // 非法名字长度由frontend检查
        // 不存在时返回false
        bool DropDatabase(const char* databaseName){
            if(databaseExists(databaseName)){
                if(currentDB && identical(databaseName, DBMS_RESERVED_TABLE_NAME, MAX_DB_NAME_LEN)) // 要被删除的数据库已经打开
                    closeCurrentDatabase();
                tb->DeleteRecord(*rec->GetRid()); // 如果databaseExists返回true，对应记录的RID和数据会被存到rec里
                // TODO delete file and folder
                return true;
            }
            else
                return false;
        }

        Scanner* ShowDatabases(){
            scanner->SetDemand([](const Record& rec)->bool{return true;});
            return scanner;
        }

        /**
         * 退出DBMS
        */
        void Close(){
            if(tb){
                tb->WriteBack();
                delete tb;
            }
            if(scanner)
                delete scanner;
            if(rec)
                delete rec;
            if(rid)
                delete rid;
        }
};
DBMS* DBMS::instance = nullptr;
FileManager* DBMS::fm = FileManager::Instance();
BufPageManager* DBMS::bpm = BufPageManager::Instance();
#endif