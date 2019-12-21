#include "DBMS.h"
DBMS* DBMS::instance = nullptr;
FileManager* DBMS::fm = FileManager::Instance();
BufPageManager* DBMS::bpm = BufPageManager::Instance();



int DataType::cmpLongVarchar(uint lpage, uint lslot, uint rpage, uint rslot){
    return DBMS::Instance()->CurrentDatabase()->CompareLongVarchar(lpage, lslot, rpage, rslot);
}

void DataType::LoadLongVarchar(uint page, uint slot, uchar* dst, ushort& len){
    RID tmpRID(page, slot);
    DBMS::Instance()->CurrentDatabase()->GetLongVarchar(tmpRID, dst, len);
}