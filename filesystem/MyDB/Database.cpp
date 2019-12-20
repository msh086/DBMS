#include "Database.h"
#include "../indexing/BplusTree.h"
BufPageManager* Database::bpm = BufPageManager::Instance();
FileManager* Database::fm = FileManager::Instance();
std::vector<Table*> Database::activeTables;

// table ops
int Table::CreateIndexOn(uint cols, const char* idxName){
    if(idxCount == MAX_INDEX_NUM) // full
        return 2;
    headerBuf = bpm->reusePage(fid, 0, headerIdx, headerBuf);
    for(int i = 0; i < MAX_INDEX_NUM; i++)
        if(identical(idxName, (char*)header->indexName[i], MAX_INDEX_NAME_LEN)) // conflict
            return 1;
    // check if cols is illegal
    if(cols & (((uint)-1) >> colCount))
        return 3;
    // update idx name
    memcpy(header->indexName[idxCount], idxName, strnlen(idxName, MAX_INDEX_NAME_LEN));
    header->indexID[idxCount] = cols;
    // operation on idx table
    IndexHeader* idxHeader = new IndexHeader();
    idxHeader->nullMask = -1;
    int iter = 0;
    for(int i = 0; i < colCount; i++){
        if(getBitFromLeft(cols, i)){
            idxHeader->attrType[iter] = header->attrType[i];
            idxHeader->attrLenth[iter] = header->attrLenth[i];
            if(!getBitFromLeft(header->nullMask, i))
                clearBitFromLeft(idxHeader->nullMask, iter);
            idxHeader->indexColID[iter] = i;
            iter++;
        }
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
    // update index col ID, type: uint
    memcpy(&header->indexID[pos], &header->indexID[pos + 1], (idxCount - pos - 1) * sizeof(uint));
    header->indexID[idxCount - 1] = 0;
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
            if(getBitFromLeft(*(uint*)tmpRec.GetData(), i))
                tmp.push_back("NULL");
            else
                switch (header->attrType[i])
                {
                case DataType::NUMERIC:{
                    int p = header->attrLenth[i] >> 8, s = header->attrLenth[i] & 0xff;
                    uchar buf[p];
                    DataType::binToDigits(tmpRec.GetData() + offsets[i] + 1, buf, p);
                    uchar firstByte = tmpRec.GetData()[offsets[i]];
                    bool hasSign = firstByte & 128;
                    uchar dotPos = firstByte & 63;
                    string str;
                    if(hasSign)
                        str.push_back('-');
                    str.append((char*)buf, p - dotPos);
                    if(dotPos)
                        str.push_back('.');
                    str.append((char*)(buf + p - dotPos), dotPos);
                    tmp.push_back(str);
                    break;
                }
                case DataType::DATE:{
                    int y, m, d;
                    DataType::binToDate(tmpRec.GetData() + offsets[i], y, m, d);
                    tmp.push_back(std::to_string(y) + "-" + std::to_string(m) + "-" + std::to_string(d));
                    break;
                }
                case DataType::INT:{
                    tmp.push_back(std::to_string(*(int*)(tmpRec.GetData() + offsets[i])));
                    break;
                }
                case DataType::BIGINT:{
                    tmp.push_back(std::to_string(*(ll*)(tmpRec.GetData() + offsets[i])));
                    break;
                }
                case DataType::FLOAT:{
                    tmp.push_back(std::to_string(*(float*)(tmpRec.GetData() + offsets[i])));
                    break;
                }
                case DataType::CHAR:{
                    tmp.push_back(string((char*)(tmpRec.GetData() + offsets[i]), strnlen((char*)(tmpRec.GetData() + offsets[i]), 255)));
                    break;
                }
                case DataType::VARCHAR:{
                    if(header->attrLenth[i] <= 255)
                        tmp.push_back(string((char*)(tmpRec.GetData() + offsets[i]), strnlen((char*)(tmpRec.GetData() + offsets[i]), 255)));
                    else{
                        uchar buf[header->attrLenth[i]] = {0};
                        int page = *(uint*)(tmpRec.GetData() + offsets[i]), slot = *(uint*)(tmpRec.GetData() + offsets[i] + 4);
                        ushort realLen = 0;
                        db->GetLongVarchar(RID(page, slot), buf, realLen);
                        tmp.push_back(string((char*)buf, realLen));
                    }
                    break;
                }
                default:
                    assert(false);
                    break;
                }
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
        for(uint j = 0, colMask = header->slaveKeyID[i]; colMask != 0; j++, colMask <<= 1){
            if(colMask & 0x80000000)
                printf(" %.*s", MAX_ATTRI_NAME_LEN, header->attrName[j]);
        }
        printf(" references");
        for(uint j = 0, colMask = header->masterKeyID[i]; colMask != 0; j++, colMask <<= 1){
            if(colMask & 0x80000000)
                printf(" %.*s", MAX_ATTRI_NAME_LEN, master->header->attrName[j]);
        }
        putchar('\n');
        db->CloseTable(tableNameBuf);
        tmpRec.FreeMemory();
        tables->Reset();
    }
    // TODO: index
}