#ifndef DATATYPE_H
#define DATATYPE_H
#include "../utils/pagedef.h"
#include "../RM/SimpleUtils.h"
#include "Comparator.h"
#include <iostream>
#include <cstring>
#include <string>
#include <cassert>
using namespace std;

/**
 * null值处理:
 * 1. 任何针对null的比较都只有eq和ne才有意义
 * 2. compareArrMultiOp时,如果允许一方为constant,才允许null值比较
 * 3. b+树维护时,允许null值的比较
*/

class DataType{
    private:
        // write the lower 'avail' bits in 'data' into dst[byte], skipping the highest 'offset' bits
        // it is ensured that offset < 8 and avail > 0, and the bits in dst are initially 0
        static void writeBits(uchar* dst, int& byte, int& offset, int data, int avail){ // Tested
            while(avail > 0){
                if(avail <= 8 - offset){ // current byte can hold remaining data
                    dst[byte] |= (data & ((1 << avail) - 1)) << (8 - offset - avail);
                    offset += avail;
                    avail = 0;
                    if(offset == 8){
                        offset = 0;
                        byte++;
                    }
                }
                else{ // too much avail
                    dst[byte] |= (data >> (avail - 8 + offset)) & ((1 << (8 - offset)) - 1);
                    avail -= 8 - offset;
                    offset = 0;
                    byte++;
                }
            }
        }

        // read 'bits' bits from src[byte], skipping the highest 'offset' bits
        // it is ensured that offset < 8 and bits > 0
        static int readBits(const uchar* src, int& byte, int& offset, int bits){ // Tested
            int value = 0;
            while(bits > 0){
                if(bits <= 8 - offset){ // read part of the current byte
                    value <<= bits; // make room of 'bits' bits
                    value |= (src[byte] >> (8 - offset - bits)) & ((1 << bits) - 1);
                    offset += bits;
                    bits = 0;
                    if(offset == 8){
                        offset = 0;
                        byte++;
                    }
                }
                else{ // the current byte is not enough
                    value <<= (8 - offset); // make room of 8 - offset bits
                    value |= src[byte] & ((1 <<(8 - offset)) - 1);
                    bits -= (8 - offset);
                    offset = 0;
                    byte++;
                }
            }
            return value;
        }

        static int lengthNumberic(int p){
            int p_div_3 = p /3, p_mod_3 = p % 3;
            return 1 + (p_div_3 * 10 + (p_mod_3 ? (3 * p_mod_3 + 1) : 0) + 7) / 8;
        }

        /**
         * These methods are only used as submethods
        */
       // 单字段比较时,left和right都不包括null word

        /**
         * 当left,right都为null时,返回true
         * 当一方为null时,认为null为最小值
        */
        static bool noLessThan(const uchar* left, const uchar* right, uchar type, ushort length, bool nullLeft, bool nullRight, bool leftConstant = false, bool rightConstant = false){
            if(type == NONE){
                std::printf("Error: NONE type is not comparable");
                return false;
            }
            const uchar* datal = left, *datar = right;
            if(nullLeft && nullRight)
                return true;
            if(nullLeft)
                return false;
            if(nullRight)
                return true;
            switch (type)
            {
            case FLOAT:{ // we don't follow SQL server rules, float is just float. It takes 4 bytes
                float* floatl = (float*)datal, *floatr = (float*)datar;
                return *floatl >= *floatr;
                break;
            }
            case BIGINT:{ // signed long long
                ll* bigintl = (ll*)datal, *bigintr = (ll*)datar;
                return *bigintl >= *bigintr;
                break;
            }
            case CHAR:{ // 0-255 bytes
                return strncmp((const char*)datal, (const char*)datar, length) >= 0;
                break;
            }
            case NUMERIC:{ 
                // 1B: 1b for sign, 6b for dot position; 16B: 127b for 38 dec digits
                // numeric(p, s), p = 1, 2,...38, s = 0, 1,...p. p is the max dec digits to store, s is the max dec digits to the right of dot
                // p = 3k: bytes = ceiling(10k) + 1
                // p = 3k + 1: bytes = ceiling(10k + 4) + 1
                // p = 3k + 2: bytes = ceiling(10k + 7) + 1
                // p, s are stored in length(ushort)
                bool signl = datal[0] >> 7, signr = datar[0] >> 7;
                if(signl != signr) // one is positive, the other is negative
                    return signl ? false : true; // signl == true -> left <= 0
                uint p = length >> 8, s = length & ((1 << 8) - 1);
                uchar bufferl[p] = {0};
                uchar bufferr[p] = {0};
                binToDigits(datal + 1, bufferl, p);
                binToDigits(datar + 1, bufferr, p);
                int dotPosl = datal[0] & 63, dotPosr = datar[0] & 63;
                bool isNegative = signl;
                bool result = !isNegative; // comparison result of abs value, the value of this will stay untouched if two numberic are equal
                // ? when converting float to bin, dotpos is max(leftDotLength, p - s)
                // ? so when two numeric's dotpos are different, they have different leftDotLength
                if(dotPosl < dotPosr){
                    result = true; // note dotpos is the number of digits to the right of the dot
                }
                else if(dotPosl > dotPosr){
                    result = false;
                }
                else{ // when two numeric have the same dotpos
                    for(int i = 0; i < p; i++){
                        if(bufferl[i] > bufferr[i]){
                            result = true;
                            break;
                        }
                        else if(bufferl[i] < bufferr[i]){
                            result = false;
                            break;
                        }
                    }
                }
                return isNegative ? !result : result;
                break;
            }
            case DATE:{ // 14 bits of year 1~9999, 4 bits of month 1~12, 5 bits of day 1~31, total 23 bits < 3 Bytes
                int yearl, monthl, dayl, yearr, monthr, dayr;
                binToDate(datal, yearl, monthl, dayl);
                binToDate(datar, yearr, monthr, dayr);
                if(yearl < yearr)
                    return false;
                if(monthl < monthr)
                    return false;
                if(dayl < dayr)
                    return false;
                return true;
                break;
            }
            case INT: // 4 bytes as it is in cpp
                return *(int*)(datal) >= *(int*)datar;
                break;
            case VARCHAR:
                if(length <= 255)
                    return strncmp((const char*)datal, (const char*)datar, length) >= 0;
                else{
                    if(!leftConstant && !rightConstant) // both are rid
                        return cmpLongVarchar(*(uint*)datal, *(uint*)(datal + 4), *(uint*)datar, *(uint*)(datar + 4)) >= 0;
                    else{
                        uchar buf[length] = {0};
                        if(leftConstant && rightConstant){
                            return strncmp((char*)datal, (char*)datar, length) >= 0;
                        }
                        if(leftConstant){
                            ushort len;
                            LoadLongVarchar(*(uint*)datar, *(uint*)(datar + 4), buf, len);
                            return strncmp((char*)datal, (char*)buf, length) >= 0;
                        }
                        else{
                            ushort len;
                            LoadLongVarchar(*(uint*)datal, *(uint*)(datal + 4), buf, len);
                            return strncmp((char*)buf, (char*)datar, length) >= 0;
                        }
                    }
                }
                break;
            default:
                break;
            }
        }

        static bool eq(const uchar* left, const uchar* right, uchar type, ushort length, bool nullLeft, bool nullRight, bool leftConstant = false, bool rightConstant = false){
            const uchar* datal = left, *datar = right;
            if(nullLeft && nullRight)
                return true;
            if(nullLeft || nullRight)
                return false;
            switch(type){
                case NONE:
                    std::printf("trying to compare type NONE\n");
                    return false;
                    break;
                case FLOAT:
                    return *(float*)datal == *(float*)datar;
                    break;
                case BIGINT:
                    return *(ll*)datal == *(ll*)datar;
                    break;
                case CHAR:
                    return strncmp((char*)datal, (char*)datar, length) == 0;
                    break;
                case VARCHAR:{
                    if(length <= 255){
                        return strncmp((char*)datal, (char*)datar, length) == 0;
                    }
                    else{
                        if(!leftConstant && !rightConstant) // both are rid
                        return cmpLongVarchar(*(uint*)datal, *(uint*)(datal + 4), *(uint*)datar, *(uint*)(datar + 4)) == 0;
                        else{
                            uchar buf[length] = {0};
                            if(leftConstant && rightConstant){
                                return strncmp((char*)left, (char*)right, length) == 0;
                            }
                            if(leftConstant){
                                ushort len;
                                LoadLongVarchar(*(uint*)datar, *(uint*)(datar + 4), buf, len);
                                return strncmp((char*)datal, (char*)buf, length) == 0;
                            }
                            else{
                                ushort len;
                                LoadLongVarchar(*(uint*)datal, *(uint*)(datal + 4), buf, len);
                                return strncmp((char*)buf, (char*)datar, length) == 0;
                            }
                        }
                    }
                    break;
                }
                case NUMERIC:{
                    int length = lengthNumberic(length >> 8);
                    return strncmp((char*)datal, (char*)datar, length) == 0;
                    break;
                }
                case DATE:
                    return strncmp((char*)datal, (char*)datar, 3) == 0;
                    break;
                case INT:
                    return *(int*)datal == *(int*)datar;
                    break;
            }
        }
        // left > right <==> left >= right && left != right
        // 任何一方为null,结果为false
        static bool greaterThan(const uchar* left, const uchar* right, uchar type, ushort length, bool nullLeft, bool nullRight, bool leftConstant = false, bool rightConstant = false){
            return noLessThan(left, right, type, length, nullLeft, nullRight, leftConstant, rightConstant) && !eq(left, right, type, length, nullLeft, nullRight, leftConstant, rightConstant);
        }

        // left < right <==> right > left <==> right >= left && right != left
        // 任何一方为null,结果为false
        static bool lessThan(const uchar* left, const uchar* right, uchar type, ushort length, bool nullLeft, bool nullRight, bool leftConstant = false, bool rightConstant = false){
            return greaterThan(right, left, type, length, nullRight, nullLeft, rightConstant, leftConstant);
        }

        // left >= right <==> right <= left
        // 存在null时,只有两方都为null,结果才为true,否则为false
        static bool noGreaterThan(const uchar* left, const uchar* right, uchar type, ushort length, bool nullLeft, bool nullRight, bool leftConstant = false, bool rightConstant = false){
            return noLessThan(right, left, type, length, nullRight, nullLeft, rightConstant, leftConstant);
        }

    public:
        const static uchar NONE = 0,
            FLOAT = 1,
            BIGINT = 2,
            CHAR = 3,
            VARCHAR = 4,
            NUMERIC = 5, // The same as decimal
            DATE = 6,
            INT = 7;
        
        // get the length of a type in bytes, 
        // if useMemoryLength is true, long varchar will get a return value of 8
        // otherwise its max string length is returned
        static int lengthOf(uchar type, ushort length, bool useMemoryLength = true){
            switch (type)
            {
            case NONE:
                std::printf("trying to tell length of type NONE\n");
                return 0;
            case FLOAT:
                return 4;
            case BIGINT:
                return 8;
            case DATE:
                return 3;
            case INT:
                return 4;
            case NUMERIC:{
                if(!useMemoryLength) // keep original length
                    return length;
                int p = length >> 8;
                int p_div_3 = p / 3, p_mod_3 = p % 3;
                return 1 + (p_div_3 * 10 + (p_mod_3 ? (3 * p_mod_3 + 1) : 0) + 7) / 8;
            }
            case CHAR:
                return length;
            case VARCHAR:
                return (useMemoryLength && length > 255) ? 8 : length;
            default:
                printf("Undefined type in DataType::lengthOf\n");
                return length;
            }
        }

        static string TypeToString(uchar type, ushort length){
            switch (type)
            {
            case 0:
                return string("NONE");
            case 1:
                return "FLOAT";
            case 2:
                return "BIGINT";
            case 3:
                return "CHAR(" + std::to_string(length) + ")";
            case 4:
                return "VARCHAR(" + std::to_string(length) + ")";
            case 5:
                return "NUMERIC(" + std::to_string(length >> 8) + "," + std::to_string(length & 0xff) + ")";
            case 6:
                return "DATE";
            case 7:
                return "INT";
            default:
                return "UNDEFINED";
            }
        }

#ifdef DEBUG
        static const uchar* NameofType(int type){
            const char* ans;
            switch (type)
            {
            case 0:
                ans = ""; // none, the default value
                break;
            case 1:
                ans = "float";
                break;
            case 2:
                ans = "bigint";
                break;
            case 3:
                ans = "char";
                break;
            case 4:
                ans = "varchar";
                break;
            case 5:
                ans = "numeric";
                break;
            case 6:
                ans = "date";
                break;
            case 7:
                ans = "int";
                break;
            default:
                ans = "undefined";
                break;
            }
            return reinterpret_cast<const uchar*>(ans);
        }
#endif
        // Numeric: string <=> bin

        // dotleft.dotright, if there is no dot, the string would be ""
        // the bindata includes the 1 byte header
        static void floatToBin(bool isNegative, const char* strdata, int dotOffset, uchar* bindata, int p, int s){
            int p_div_3 = p / 3, p_mod_3 = p % 3;
            int totalBytes = 1 + (10 * p_div_3 + (p_mod_3 > 0 ? (p_mod_3 * 3 + 1) : 0) + 7) / 8; // ceiling(10k + f(remain)) + 1
            memset(bindata, 0, totalBytes); // clear memory
            if(isNegative)
                bindata[0] |= 128; // set sign bit
            
            uchar digits[p] = {0}; // buffer
            int ptr = 0;
            // init length
            int totalLength = strlen(strdata);
            int lenl = dotOffset, lenr = totalLength - 1 - dotOffset;
            const char* dotleft = strdata, *dotright = strdata + dotOffset + 1;
            // strip zeros
            int firstNonZero = 0, lastNonZero = lenr - 1;
            while(firstNonZero < lenl && dotleft[firstNonZero] == '0') // strip the leading and ending zeros
                firstNonZero++;
            while(lastNonZero >= 0 && dotright[lastNonZero] == '0')
                lastNonZero--;
            int digitsl = lenl - firstNonZero, digitsr = lastNonZero + 1;
            if(digitsl > p){ // overflow
                bindata[0] |= 0; // dot pos = 0
                for(int i = 0; i < p; i++)
                    digits[i] = 9; // set all to 9 in overflow
            }
            else if(digitsl >= p -s){ // left take l digits
                for(int i = firstNonZero; i < lenl; i++)
                    digits[ptr++] = dotleft[i] - '0';
                if(digitsr <= p - digitsl){ // ok
                    for(int i = 0; i <= lastNonZero; i++)
                        digits[ptr++] = dotright[i] - '0';
                    bindata[0] |= p - digitsl;
                }
                else{ // round to p - l digits
                    for(int i = 0; i < p - digitsl; i++)
                        digits[ptr++] = dotright[i] - '0';
                    bindata[0] |= p - digitsl;
                }
            }
            else{ // l <= p - s, left take p - s digits
                ptr = p - s - digitsl; // skip
                for(int i = firstNonZero; i < lenl; i++)
                    digits[ptr++] = dotleft[i] - '0';
                if(digitsr <= s){ // ok
                    for(int i = 0; i <= lastNonZero; i++)
                        digits[ptr++] = dotright[i] - '0';
                    bindata[0] |= s;
                }
                else{ // round to s digits
                    for(int i = 0; i < s; i++)
                        digits[ptr++] = dotright[i] - '0';
                    bindata[0] |= s;
                }
            }


            // Now input is converted to array of digits
            // int dotpos = bindata[0] & 127;
            // std::printf("left dot:");
            // for(int i = 0; i < p - dotpos; i++)
            //     std::printf("%hhu", digits[i]);
            // std::printf("  right dot:");
            // for(int i = p - dotpos; i < p; i++)
            //     std::printf("%hhu", digits[i]);
            // std::printf("\n");

            
            bindata++; // sign & dot byte is ok, move on
            ptr = 0;
            int byte = 0, offset = 0;
            for(int i = 0; i < p_div_3; i++) { // the number of 10^3 or 2^10
                int value = 0;
                for(int j = 0; j < 3; j++){
                    value *= 10;
                    value += digits[ptr++];
                }
                writeBits(bindata, byte, offset, value, 10);
            }

            switch (p_mod_3)
            {
            case 1:
                writeBits(bindata, byte, offset, digits[ptr++], 4);
                break;
            case 2:{
                int value = digits[ptr++] * 10;
                value += digits[ptr++];
                writeBits(bindata, byte, offset, value, 7);
                break;
            }
            default:
                break;
            }

            // only for debugging
            // bindata--;
            // for(int i = 0; i < totalBytes; i++){
            //     for(int j = 0; j < 8; j++)
            //         std::printf("%d", ((bindata[i] & (128 >> j)) >= 1));
            //     std::printf(" ");
            // }
            // std::printf("\n");
        }

        // convert binary data in bin to p decimal digits in dst
        // the bin doesn't include the 1 byte header, so this has nothing to do with s
        // NOTE: value of value[i] is decimal digit, not ascii character!!!
        static void binToDigits(const uchar* bin, uchar* dst, int p){
            int p_div_3 = p / 3, p_mod_3 = p % 3;
            memset(dst, 0, p);
            int ptr = 0;
            int byte = 0, offset = 0;
            for(int i = 0; i < p_div_3; i++){
                int value = readBits(bin, byte, offset, 10);
                dst[ptr++] = value / 100;
                value %= 100;
                dst[ptr++] = value / 10;
                dst[ptr++] = value % 10;
            }
            switch(p_mod_3){
                case 1:{
                    int value = readBits(bin, byte, offset, 4);
                    dst[ptr++] = value;
                }
                    break;
                case 2:{
                    int value = readBits(bin, byte, offset, 7);
                    dst[ptr++] = value / 10;
                    dst[ptr++] = value % 10;
                    break;
                }
                default:
                    break;
            }
            // for test only
            // for(int i = 0; i < p; i++)
            //     std::printf("%hhu ", dst[i]);
            // std::printf("\n");
        }

        // Date: int <=> bin

        // write a date into binary
        static void dateToBin(int year, int month, int day, uchar* dst){
            memset(dst, 0, 3); // the date type takes up 3 bytes
            int byte = 0, offset = 0;
            writeBits(dst, byte, offset, year, 14);
            writeBits(dst, byte, offset, month, 4);
            writeBits(dst, byte, offset, day, 5);
        }

        // get a date from binary
        static void binToDate(const uchar* src, int& year, int& month, int& day){
            int byte = 0, offset = 0;
            year = readBits(src, byte, offset, 14);
            month = readBits(src, byte, offset, 4);
            day = readBits(src, byte, offset, 5);
        }

        static int cmpLongVarchar(uint lpage, uint lslot, uint rpage, uint rslot);        

        // 复合属性比较

        static bool noLessThanArr(const uchar* left, const uchar* right, const uchar* types, const ushort* lengths, int colNum){
            // 跳过null word
            uint nullWordLeft = *(uint*)left, nullWordRight = *(uint*)right;
            left += 4;
            right += 4;
            for(int i = 0; i < colNum; i++){
                if(!noLessThan(left, right, types[i], lengths[i], getBitFromLeft(nullWordLeft, i), getBitFromLeft(nullWordRight, i)))
                    return false;
                int length = lengthOf(types[i], lengths[i]);
                left += length;
                right += length;
            }
            return true;
        }

        // why not use noLessThanArr && !eqArr ? Because it is not efficient
        static bool greaterThanArr(const uchar* left, const uchar* right, const uchar* types, const ushort* lengths, int colNum){
            // 跳过null word
            uint nullWordLeft = *(uint*)left, nullWordRight = *(uint*)right;
            left += 4;
            right += 4;
            for(int i = 0; i < colNum; i++){
                if(!greaterThan(left, right, types[i], lengths[i], getBitFromLeft(nullWordLeft, i), getBitFromLeft(nullWordRight, i)))
                    return false;
                int length = lengthOf(types[i], lengths[i]);
                left += length;
                right += length;
            }
            return true;
        }

        static bool eqArr(const uchar* left, const uchar* right, const uchar* types, const ushort* lengths, int colNum){
            // 跳过null word
            uint nullWordLeft = *(uint*)left, nullWordRight = *(uint*)right;
            left += 4;
            right += 4;
            for(int i = 0; i < colNum; i++){
                if(!eq(left, right, types[i], lengths[i], getBitFromLeft(nullWordLeft, i), getBitFromLeft(nullWordRight, i)))
                    return false;
                int length = lengthOf(types[i], lengths[i]);
                left += length;
                right += length;
            }
            return true;
        }

        // 可指定比较运算符

        /**
         * return if left cmp right is true. Length is the pure length excluding the null word
        */
        static bool compare(const uchar* left, const uchar* right, uchar type, ushort length, uchar cmp, bool nullLeft, bool nullRight, bool leftConstant = false, bool rightConstant = false){
            switch (cmp)
            {
            case Comparator::Any:
                return true;
            case Comparator::None:
                return false;
            case Comparator::Eq:
                return eq(left, right, type, length, nullLeft, nullRight, leftConstant, rightConstant);
            case Comparator::Gt:
                return greaterThan(left, right, type, length, nullLeft, nullRight, leftConstant, rightConstant);
            case Comparator::GtEq:
                return noLessThan(left, right, type, length, nullLeft, nullRight, leftConstant, rightConstant);
            case Comparator::Lt:
                return lessThan(left, right, type, length, nullLeft, nullRight, leftConstant, rightConstant);
            case Comparator::LtEq:
                return noGreaterThan(left, right, type, length, nullLeft, nullRight, leftConstant, rightConstant);
            case Comparator::NE:
                return !eq(left, right, type, length, nullLeft, nullRight, leftConstant, rightConstant);
            default:
                std::printf("cmp doesn't match any strategy defined in Comparator.h\n");
                return false;
            }
        }

        /**
         * 比较两组值，left cmp right == true iff left[i] cmp right[i] == true, i = 0, 1, ..., colNum - 1
         * NOTE: 在B+树中使用
        */
        static bool compareArr(const uchar* left, const uchar* right, const uchar* types, const ushort* lengths, int colNum, uchar cmp, bool leftConstant = false, bool rightConstant = false){
            if(colNum == 0)
                return true;
            // 跳过null word
            uint nullWordLeft = *(uint*)left, nullWordRight = *(uint*)right;
            left += 4;
            right += 4;
            if(cmp == Comparator::Gt || cmp == Comparator::Lt){
                uchar weakCmp;
                if(cmp == Comparator::Gt)
                    weakCmp = Comparator::GtEq;
                else if(cmp == Comparator::Lt)
                    weakCmp = Comparator::LtEq;
                for(int i = 0; i < colNum; i++){
                    if(!compare(left, right, types[i], lengths[i], weakCmp, getBitFromLeft(nullWordLeft, i), getBitFromLeft(nullWordRight, i)))
                        return false;
                    if(!compare(left, right, types[i], lengths[i], Comparator::Eq, getBitFromLeft(nullWordLeft, i), getBitFromLeft(nullWordRight, i)))
                        return true;
                    int length = lengthOf(types[i], lengths[i]);
                    left += (leftConstant && types[i] == DataType::VARCHAR) ? lengths[i] : length;
                    right += (rightConstant && types[i] == DataType::VARCHAR) ? lengths[i] : length;
                }
                return false;
            }
            else if(cmp == Comparator::NE){
                for(int i = 0; i < colNum; i++){
                    if(compare(left, right, types[i], lengths[i], Comparator::NE, getBitFromLeft(nullWordLeft, i), getBitFromLeft(nullWordRight, i)))
                        return true;
                    int length = lengthOf(types[i], lengths[i]);
                    left += (leftConstant && types[i] == DataType::VARCHAR) ? lengths[i] : length;
                    right += (rightConstant && types[i] == DataType::VARCHAR) ? lengths[i] : length;
                }
                return false;
            }
            else{
                for(int i = 0; i < colNum; i++){
                    if(!compare(left, right, types[i], lengths[i], cmp, getBitFromLeft(nullWordLeft, i), getBitFromLeft(nullWordRight, i)))
                        return false;
                    int length = lengthOf(types[i], lengths[i]);
                    left += (leftConstant && types[i] == DataType::VARCHAR) ? lengths[i] : length;
                    right += (rightConstant && types[i] == DataType::VARCHAR) ? lengths[i] : length;
                }
                return true;
            }
        }

        /**
         * 比较两组值，允许为每个字段指定比较方法，left cmp right == true iff left[i] cmp[i] right[i] == true, i = 0, 1, ..., colNum - 1
         * NOTE: 在select语句中使用
         * ? leftConstant和rightConstant表示left和right是否是常量,当置为true时,从其中读取long varchar类型的字段时,不再通过RID,而直接从内存中读取
        */
        static bool compareArrMultiOp(const uchar* left, const uchar* right, const uchar* types, const ushort* lengths, int colNum, const uchar* cmp, bool leftConstant = false, bool rightConstant = false){
            if(colNum == 0)
                return true;
            // 跳过null word
            uint nullWordLeft = *(uint*)left, nullWordRight = *(uint*)right;
            left += 4;
            right += 4;
            for(int i = 0; i < colNum; i++){
                if(!compare(left, right, types[i], lengths[i], cmp[i], getBitFromLeft(nullWordLeft, i), getBitFromLeft(nullWordRight, i)))
                    return false;
                int length = lengthOf(types[i], lengths[i]);
                left += (leftConstant && types[i] == DataType::VARCHAR) ? lengths[i] : length;
                right += (rightConstant && types[i] == DataType::VARCHAR) ? lengths[i] : length;
            }
            return true;
        }

        static int calcTotalLength(const uchar* types, const ushort* lengths, int colNum){
            int ans = 4; // null word
            for(int i = 0; i < colNum; i++){
                ans += lengthOf(types[i], lengths[i]);
            }
            return ans;
        }

        static int constantLengthOf(uchar type, ushort length){
            if(type == DataType::CHAR || type == DataType::VARCHAR)
                return length;
            return lengthOf(type, length);
        }

        static int calcConstantLength(const uchar* types, const ushort* lengths, int colNum){
            int ans = 4;
            for(int i = 0; i < colNum; i++)
                ans += constantLengthOf(types[i], lengths[i]);
            return ans;
        }

        /**
         * Build offsets for columns
        */
        static void calcOffsets(const uchar* types, const ushort* lengths, int colNum, uint* dst){
            int offset = 4;
            for(int i = 0; i < colNum; i++){
                dst[i] = offset;
                offset += lengthOf(types[i], lengths[i]);
            }
        }

        const static uchar DateFamily = 0, StringFamily = 1, RealFamily = 2;

        static uchar GetFamily(uchar type){
            if(type == DATE)
                return DateFamily;
            if(type == CHAR || type == VARCHAR)
                return StringFamily;
            return RealFamily; // FLOAT and NUMERIC
        }

        static bool Comparable(uchar srcType, uchar dstType){
            return GetFamily(srcType) == GetFamily(dstType);
        }

        static void LoadLongVarchar(uint page, uint slot, uchar* dst, ushort& len);

        /**
         * 比较两个同一family的值
        */
        static bool CompareFamily(uchar leftType, uchar rightType, const uchar* left, const uchar* right, ushort leftLength, ushort rightLength, bool nullLeft, bool nullRight, uchar cmp){
            uchar familyLeft = GetFamily(leftType), familyRight = GetFamily(rightType);
            assert(familyLeft == familyRight);
            if(nullLeft || nullRight){ // 当一方为null时,比较结果仅取决于谁是null以及cmp,与数据类型和具体数值无关
                return compare(left, right, INT, 0, cmp, nullLeft, nullRight);
            }
            switch (familyLeft)
            {
                case DateFamily:{
                    return compare(left, right, DATE, 3, cmp, false, false);
                }
                case StringFamily:{ // 
                    if(rightType == VARCHAR && leftType == VARCHAR) // both are varchar, length doesn't matter
                        return compare(left, right, VARCHAR, 256, cmp, false, false);
                    int maxLen = leftLength > rightLength ? leftLength : rightLength;
                    uchar leftBuf[maxLen] = {0}, rightBuf[maxLen] = {0};
                    if(leftLength > 255){
                        ushort len;
                        LoadLongVarchar(*(uint*)left, *(uint*)(left + 4), leftBuf, len);
                    }
                    else
                        memcpy(leftBuf, left, leftLength);
                    if(rightLength > 255){
                        ushort len;
                        LoadLongVarchar(*(uint*)right, *(uint*)(right + 4), rightBuf, len);
                    }
                    else
                        memcpy(rightBuf, right, rightLength);
                    return compare(leftBuf, rightBuf, CHAR, maxLen, cmp, false, false);
                }
                case RealFamily:{
                    if(leftType != NUMERIC && rightType != NUMERIC){ // neither left nor right is numeric
                        if(leftType == FLOAT || rightType == FLOAT){ // has float
                            uchar lfloat[4], rfloat[4];
                            // cast left
                            if(leftType == INT)
                                *(float*)lfloat = *(int*)left;
                            else if(leftType == BIGINT)
                                *(float*)lfloat = *(ll*)left;
                            else
                                memcpy(lfloat, left, 4);
                            // cast right
                            if(rightType == INT)
                                *(float*)rfloat = *(int*)right;
                            else if(leftType == BIGINT)
                                *(float*)rfloat = *(ll*)right;
                            else
                                memcpy(rfloat, right, 4);
                            return compare(lfloat, rfloat, FLOAT, 4, cmp, false, false);
                        }
                        if(leftType == BIGINT || rightType == BIGINT){ // no float, but has ll
                            uchar lll[8], rll[8];
                            // cast left
                            if(leftType == INT)
                                *(ll*)lll = *(int*)left;
                            else
                                memcpy(lll, left, 8);
                            // cast right
                            if(rightType == INT)
                                *(ll*)rll = *(int*)right;
                            else
                                memcpy(rll, right, 8);
                            return compare(lll, rll, BIGINT, 8, cmp, false, false);
                        }
                        return compare(left, right, INT, 4, cmp, false, false); // only int
                    }
                    else{ // has numeric
                        if(leftType == NUMERIC && rightType == NUMERIC){ // both are numeric
                            if(leftLength == rightLength)
                                return compare(left, right, NUMERIC, leftLength, cmp, false, false);
                            uchar leftP = leftLength >> 8, leftS = leftLength & 0xff;
                            uchar rightP = rightLength >> 8, rightS = rightLength & 0xff;
                            bool leftSign = left[0] & 128, rightSign = right[0] & 128;
                            uchar leftDot = left[0] & 63, rightDot = right[0] & 63;
                            uchar maxP = max(leftP, rightP), maxS = max(leftS, rightS);
                            if(maxP + maxS > 38)
                                maxS = 38 - maxP;
                            uchar numLeft[39] = {0}, numRight[39] = {0};
                            binToDigits(left + 1, numLeft, leftP);
                            binToDigits(right + 1, numRight, rightP);
                            for(int i = 0; i < 39; i++){ // ? floatToBin needs input as ascii instead of digit
                                numLeft[i] += '0';
                                numRight[i] += '0';
                            }
                            uchar binLeft[17] = {0}, binRight[17] = {0};
                            floatToBin(leftSign, (char*)numLeft, leftP - leftDot, binLeft, maxP, maxS);
                            floatToBin(rightSign, (char*)numRight, rightP - rightDot, binRight, maxP, maxS);
                            return compare(binLeft, binRight, NUMERIC, (maxP << 8) & maxS, cmp, false, false);
                        }
                        if(leftType == FLOAT || rightType == FLOAT){ // one is float
                            auto castNumericToFloat = [](ushort length, const uchar* data)->float{
                                uchar sign = data[0] & 128;
                                uchar dotPos = data[0] & 63;
                                uchar p = length >> 8, s = length & 0xff;
                                uchar digits[39] = {0};
                                binToDigits(data + 1, digits, p);
                                float ans = 0;
                                for(int i = 0; i < p - dotPos; i++)
                                    ans = ans * 10 + digits[i];
                                float unit = 0.1f;
                                for(int i = p - dotPos; i < p; i++, unit /= 10){
                                    ans += unit * digits[i];
                                }
                                if(sign)
                                    return -ans;
                                else
                                    return ans;
                            };
                            if(leftType == NUMERIC){
                                uchar fbuf[4] = {0};
                                *(float*)fbuf = castNumericToFloat(leftLength, left);
                                return compare(fbuf, right, FLOAT, 4, cmp, false, false);
                            }
                            else{
                                uchar fbuf[4] = {0};
                                *(float*)fbuf = castNumericToFloat(rightLength, right);
                                return compare(left, fbuf, FLOAT, 4, cmp, false, false);
                            }
                        }
                        // return the result of bigint cmp data
                        // decDigits should be 19 for ll and 10 for int
                        auto IntNumCmp = [](ll bigint, ushort length, const uchar* data, uchar cmp, int decDigits)->bool{
                            auto decPow = [](uchar pow)->ll{
                                ll ans = 1;
                                while(pow--)
                                    ans *= 10;
                                return ans;
                            };
                            uchar p = length >> 8;
                            // bigint can store 19 dec digits (+-)
                            if(p < decDigits){ // numeric cannot hold bigint, bigint -> numeric may cause overflow
                                ll ofBound = decPow(p);
                                if(bigint >= ofBound){
                                    if(cmp == Comparator::Any || cmp == Comparator::Gt || cmp == Comparator::GtEq)
                                        return true;
                                    else
                                        return false; // Lt, LtEq, NE
                                }
                                else if(bigint <= -ofBound){
                                    if(cmp == Comparator::Any || cmp == Comparator::Lt || cmp == Comparator::LtEq)
                                        return true;
                                    else
                                        return false; // Gt, GtEq, NE
                                }
                                // otherwise, cast bigint to numeric won't cause overflow
                            }
                            // p is big enough, now cast bigint to numeric
                            uchar numBuf[p + 1] = {0};
                            bool llSign = bigint < 0;
                            for(int i = p - 1; i >= 0; i--){
                                numBuf[i] = (bigint % 10) + '0'; // ? ascii for floatToBin
                                bigint /= 10;
                            }
                            uchar llBin[17] = {0};
                            floatToBin(llSign, (char*)numBuf, p, llBin, p, length & 0xff);
                            return compare(llBin, data, NUMERIC, length, cmp, false, false);
                        };
                        if(leftType == BIGINT || rightType == BIGINT){ // one is BIGINT
                            if(leftType == BIGINT)
                                return IntNumCmp(*(ll*)left, rightLength, right, cmp, 19);
                            else
                                return IntNumCmp(*(ll*)right, leftLength, left, Comparator::Reverse(cmp), 19);
                        }
                        else{ // one is INT
                            if(leftType == INT)
                                return IntNumCmp(*(int*)left, rightLength, right, cmp, 10);
                            else
                                return IntNumCmp(*(int*)right, leftLength, left, Comparator::Reverse(cmp), 10);
                        }
                    }
                }
                default:
                    assert(false); // impossible to reach here
                    break;
            }
            return false; // never reach here
        }
};

#endif