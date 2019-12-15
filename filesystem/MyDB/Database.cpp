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
}