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

std::string Printer::FieldToStr(const Record& tmpRec, uchar type, uchar index, ushort length, uint offset){
        if(getBitFromLeft(*(uint*)tmpRec.GetData(), index))
            return "null";
        else
            switch (type)
            {
            case DataType::NUMERIC:{
                int p = length >> 8, s = length & 0xff;
                uchar buf[p];
                DataType::binToDigits(tmpRec.GetData() + offset + 1, buf, p);
                uchar firstByte = tmpRec.GetData()[offset];
                bool hasSign = firstByte & 128;
                uchar dotPos = firstByte & 63;
                string str;
                if(hasSign)
                    str.push_back('-');
                // convert digit to ascii
                bool firstNonZero = false;
                for(int i = 0; i < p - dotPos; i++){
                    if(firstNonZero)
                        str.push_back(buf[i] + '0');
                    else if(buf[i]){
                        firstNonZero = true;
                        str.push_back(buf[i] + '0');
                    }
                }
                if(!firstNonZero)
                    str.push_back('0');
                if(dotPos){
                    str.push_back('.');
                    str.append((char*)(buf + p - dotPos), dotPos);
                }
                return str;
            }
            case DataType::DATE:{
                int y, m, d;
                DataType::binToDate(tmpRec.GetData() + offset, y, m, d);
                return std::to_string(y) + "-" + std::to_string(m) + "-" + std::to_string(d);
            }
            case DataType::INT:{
                return std::to_string(*(int*)(tmpRec.GetData() + offset));
            }
            case DataType::BIGINT:{
                return std::to_string(*(ll*)(tmpRec.GetData() + offset));
            }
            case DataType::FLOAT:{
                return std::to_string(*(float*)(tmpRec.GetData() + offset));
            }
            case DataType::CHAR:{
                return string((char*)(tmpRec.GetData() + offset), strnlen((char*)(tmpRec.GetData() + offset), 255));
            }
            case DataType::VARCHAR:{
                if(length <= 255)
                    return string((char*)(tmpRec.GetData() + offset), strnlen((char*)(tmpRec.GetData() + offset), 255));
                else{
                    uchar buf[length] = {0};
                    int page = *(uint*)(tmpRec.GetData() + offset), slot = *(uint*)(tmpRec.GetData() + offset + 4);
                    ushort realLen = 0;
                    DBMS::Instance()->CurrentDatabase()->GetLongVarchar(RID(page, slot), buf, realLen);
                    return string((char*)buf, realLen);
                }
                break;
            }
            default:
                assert(false);
                break;
            }
    }