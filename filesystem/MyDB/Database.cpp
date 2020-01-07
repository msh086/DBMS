#include "Database.h"
#include "../indexing/BplusTree.h"
BufPageManager* Database::bpm = BufPageManager::Instance();
FileManager* Database::fm = FileManager::Instance();
std::vector<Table*> Database::activeTables;

// table ops
int Table::CreateIndexOn(std::vector<uchar> cols, const char* idxName){
    if(idxCount == MAX_INDEX_NUM) // full
        return 2;
    headerBuf = bpm->reusePage(fid, 0, headerIdx, headerBuf);
    for(int i = 0; i < idxCount; i++)
        if(identical(idxName, (char*)header->indexName[i], MAX_INDEX_NAME_LEN)) // conflict
            return 1;
    // update idx name
    memcpy(header->indexName[idxCount], idxName, strnlen(idxName, MAX_INDEX_NAME_LEN));
    int idxColNum = cols.size();
    for(int i = 0; i < idxColNum; i++)
        header->indexID[idxCount][i] = cols[i];
    // operation on idx table
    IndexHeader* idxHeader = new IndexHeader();
    idxHeader->nullMask = -1;
    for(int i = 0; i < idxColNum; i++){
        idxHeader->attrType[i] = header->attrType[cols[i]];
        idxHeader->attrLenth[i] = header->attrLenth[cols[i]];
        idxHeader->indexColID[i] = cols[i];
        if(!getBitFromLeft(header->nullMask, cols[i]))
            clearBitFromLeft(idxHeader->nullMask, i);
    }
    memcpy(idxHeader->tableName, tablename, MAX_TABLE_NAME_LEN);
    BplusTree* tree = new BplusTree(db->idx, idxHeader);
    header->bpTreePage[idxCount] = tree->TreeHeaderPage();
    // TODO add existing record into index
    delete tree;
    idxCount++;
    headerDirty = true;
    return 0;
}

bool Table::RemoveIndex(const char* idxName){
    int pos = 0;
    while(pos < idxCount){
        if(identical((char*)header->indexName[pos], idxName, MAX_INDEX_NAME_LEN))
            break;
        pos++;
    }
    if(pos == idxCount) // not found
        return false;
    // update index name, type: uchar[]
    memcpy(header->indexName[pos], header->indexName[pos + 1], (idxCount - pos - 1) * MAX_INDEX_NAME_LEN);
    memset(header->indexName[idxCount - 1], 0, MAX_INDEX_NAME_LEN);
    // update index col ID, type: uchar[]
    memcpy(&header->indexID[pos], &header->indexID[pos + 1], (idxCount - pos - 1) * MAX_COL_NUM);
    memset(header->indexID[idxCount - 1], COL_ID_NONE, MAX_COL_NUM);
    // remove tree
    BplusTree* tree = new BplusTree(db->idx, header->bpTreePage[pos]);
    tree->DeleteTreeFromDisk();
    delete tree;
    // update tree page info, type: uint
    memcpy(&header->bpTreePage[pos], &header->bpTreePage[pos + 1], (idxCount - pos - 1) * sizeof(uint));
    header->bpTreePage[idxCount - 1] = 0;
    // sync
    idxCount--;
    headerDirty = true;
    return true;
}

void Table::Print(){
    Record tmpRec;
    RID tmpRID(1, 0);
    GetRecord(tmpRID, &tmpRec);
    std::vector<std::vector<std::string>> table;
    // name, type, nullable, default
    table.push_back(std::vector<std::string>());
    table[0].push_back("Name");
    table[0].push_back("Type");
    table[0].push_back("Nullable");
    table[0].push_back("Default");
    int i = 0;
    while(i < MAX_COL_NUM && header->attrType[i] != DataType::NONE){
        std::vector<std::string> tmp;
        tmp.push_back(string((char*)(header->attrName[i]), strnlen((char*)(header->attrName[i]), MAX_ATTRI_NAME_LEN)));
        tmp.push_back(DataType::TypeToString(header->attrType[i], header->attrLenth[i]));
        tmp.push_back(getBitFromLeft(header->nullMask, i) ? "" : "not null");
        if(getBitFromLeft(header->defaultKeyMask, i)){
            tmp.push_back(Printer::FieldToStr(tmpRec, header->attrType[i], i, header->attrLenth[i], offsets[i]));
        }
        else
            tmp.push_back(""); // no default value
        i++;
        table.push_back(tmp);
    }
    Printer::PrintTable(table, 4, i + 1);
    // primary key
    if(!header->primaryKeyMask)
        printf("Primary key: NONE\n");
    else{
        printf("Primary key: (");
        bool prev = false;
        for(int i = 0; i < colCount; i++){
            if(getBitFromLeft(header->primaryKeyMask, i)){
                if(prev)
                    printf(", ");
                printf("%.*s", MAX_ATTRI_NAME_LEN, header->attrName[i]);
                prev = true;
            }
        }
        printf(")\n");
    }
    // foreign key
    Scanner* tables = db->ShowTables();
    tmpRec.FreeMemory();
    char tableNameBuf[MAX_TABLE_NAME_LEN + 1] = "";
    for(int i = 0; i < fkMasterCount; i++){
        bool found = false;
        while(tables->NextRecord(&tmpRec)){
            if(identical((char*)tmpRec.GetData(), (char*)header->attrName[header->fkMaster[i]], MAX_TABLE_NAME_LEN)){
                found = true;
                break;
            }
            else
                tmpRec.FreeMemory();
        }
        assert(found);
        memcpy(tableNameBuf, tmpRec.GetData(), MAX_TABLE_NAME_LEN);
        Table* master = db->OpenTable(tableNameBuf);
        assert(master);

        printf("Foreign key constraint %.*s", MAX_CONSTRAINT_NAME_LEN, header->constraintName[i]);
        for(int j = 0; j < MAX_COL_NUM && header->slaveKeyID[i][j] != COL_ID_NONE; j++)
            printf(" %.*s", MAX_ATTRI_NAME_LEN, header->attrName[header->slaveKeyID[i][j]]);
        printf(" references");
        for(int j = 0; j < MAX_COL_NUM && master->header->primaryKeyID[j] != COL_ID_NONE; j++)
            printf(" %.*s", MAX_ATTRI_NAME_LEN, master->header->attrName[master->header->primaryKeyID[j]]);
        putchar('\n');
        db->CloseTable(tableNameBuf);
        tmpRec.FreeMemory();
        tables->Reset();
    }
    // index
    for(int i = 0; i < idxCount; i++){
        printf("index %.*s on", MAX_INDEX_NAME_LEN, header->indexName[i]);
        for(int j = 0; header->indexID[i][j] != COL_ID_NONE; j++){
            printf(" %.*s", MAX_ATTRI_NAME_LEN, header->attrName[header->indexID[i][j]]);
        }
        printf("\n");
    }
}

void Table::ConvertTextToBin(const char* src, uchar* dst, ushort length, uchar type){
            switch(type){
                case DataType::NUMERIC:{
                    bool hasSign = false;
                    if(src[0] == '-'){
                        hasSign = true;
                        src++;
                    }
                    const char* dot = strchr(src, '.');
                    int dotPos = 0;
                    if(dot)
                        dotPos = dot - src;
                    else
                        dotPos = strlen(src);
                    DataType::floatToBin(hasSign, src, dotPos, dst, length >> 8, length & 0xff);
                    break;
                }
                case DataType::DATE:{
                    int pos = 0;
                    int year = readNumFromString<int>(src, pos),
                        month = readNumFromString<int>(src, ++pos),
                        day = readNumFromString<int>(src, ++pos);
                    DataType::dateToBin(year, month, day, dst);
                    break;
                }
                case DataType::CHAR:{
                    memcpy(dst, src, strlen(src));
                    break;
                }
                case DataType::VARCHAR:{
                    if(length <= 255)
                        memcpy(dst, src, strlen(src));
                    else{
                        RID rid;
                        db->InsertLongVarchar(src, strlen(src), &rid);
                        *(uint*)dst = rid.PageNum;
                        *(uint*)(dst + 4) = rid.SlotNum;
                    }
                    break;
                }
                case DataType::INT:{
                    int pos = 0;
                    int num = readNumFromString<int>(src, pos);
                    *(int*)dst = num;
                    break;
                }
                case DataType::FLOAT:{
                    const char* dot = strchr(src, '.');
                    int dotPos = 0;
                    if(dot)
                        dotPos = dot - src;
                    float val = readFloatFromString(src);
                    *(float*)dst = val;
                    break;
                }
                case DataType::BIGINT:{
                    int pos = 0;
                    ll num = readNumFromString<ll>(src, pos);
                    *(ll*)dst = num;
                    break;
                }
            }
        }