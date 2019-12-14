#ifndef DATATYPE_H
#define DATATYPE_H
#include "../utils/pagedef.h"
#include "Comparator.h"
#include <iostream>
#include <cstring>
using namespace std;

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

    public:
        const static uchar NONE = 0,
            FLOAT = 1,
            BIGINT = 2,
            CHAR = 3,
            VARCHAR = 4,
            NUMERIC = 5, // The same as decimal
            DATE = 6,
            INT = 7;
        
        // get the length of a type in bytes
        static int lengthOf(uchar type, ushort length){
            switch (type)
            {
            case NONE:
                std::printf("trying to tell length of type NONE\n");
                return 0;
                break;
            case FLOAT:
                return 4;
                break;
            case BIGINT:
                return 8;
                break;
            case DATE:
                return 3;
                break;
            case INT:
                return 4;
                break;
            case NUMERIC:{
                int p = length >> 8;
                int p_div_3 = p / 3, p_mod_3 = p % 3;
                return 1 + (p_div_3 * 10 + (p_mod_3 ? (3 * p_mod_3 + 1) : 0) + 7) / 8;
                break;
            }
            default: // char, varchar don't have a fixed length
                return length; // TODO: varchar ?
                break;
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

        // when both are null, return false(left < right)
        static bool noLessThan(const uchar* left, const uchar* right, uchar type, ushort length, bool nullable){
            if(type == NONE){
                std::printf("Error: NONE type is not comparable");
                return false;
            }
            const uchar* datal = left, *datar = right;
            if(nullable){
                if(datal[0] == 1) // left is null
                    return false;
                if(datar[0] == 1) // right is null
                    return true;
                datal++; // skip the null byte
                datar++;
            }

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
                return strncmp((const char*)datal, (const char*)datar, length);
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
            default:
                break;
            }
        }

        static bool eq(const uchar* left, const uchar* right, uchar type, ushort length, bool nullable){
            const uchar* datal = left, *datar = right;
            if(nullable){
                bool nulll = datal[0] == 1, nullr = datar[0] == 1;
                if(nulll && nullr)
                    return true;
                if(nulll || nullr)
                    return false;
                datal++; // skip null byte
                datar++;
            }
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
                case VARCHAR:
                    if(length <= 255){
                        return strncmp((char*)datal, (char*)datar, length) == 0;
                    }
                    else{ // TODO: index on long varchar

                    }
                    break;
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

        static bool noLessThanArr(const uchar* left, const uchar* right, const uchar* types, const ushort* lengths, uint nullMask, int colNum){
            for(int i = 0; i < colNum; i++){
                if(!noLessThan(left, right, types[i], lengths[i], nullMask & (1 << (31 - i))))
                    return false;
                int length = lengthOf(types[i], lengths[i]) + ((nullMask & (1 << (31 - i))) != 0);
                left += length;
                right += length;
            }
            return true;
        }

        static bool greaterThan(const uchar* left, const uchar* right, uchar type, ushort length, bool nullable){
            return noLessThan(left, right, type, length, nullable) && !eq(left, right, type, length, nullable);
        }

        static bool lessThan(const uchar* left, const uchar* right, uchar type, ushort length, bool nullable){
            return greaterThan(right, left, type, length, nullable);
        }

        static bool noGreaterThan(const uchar* left, const uchar* right, uchar type, ushort length, bool nullable){
            return noGreaterThan(right, left, type, length, nullable);
        }

        // why not use noLessThanArr && !eqArr ? Because it is not efficient
        static bool greaterThanArr(const uchar* left, const uchar* right, const uchar* types, const ushort* lengths, uint nullMask, int colNum){
            for(int i = 0; i < colNum; i++){
                if(greaterThan(left, right, types[i], lengths[i], nullMask & (1 << (31 - i))) == false)
                    return false;
                int length = lengthOf(types[i], lengths[i]) + ((nullMask & (1 << (31 - i))) != 0);
                left += length;
                right += length;
            }
            return true;
        }

        static bool eqArr(const uchar* left, const uchar* right, const uchar* types, const ushort* lengths, uint nullMask, int colNum){
            for(int i = 0; i < colNum; i++){
                if(!eq(left, right, types[i], lengths[i], nullMask & (1 << (31 - i))));
                    return false;
                int length = lengthOf(types[i], lengths[i]) + ((nullMask & (1 << (31 - i))) != 0);
                left += length;
                right += length;
            }
            return true;
        }

        /**
         * return if left cmp right is true. Length is the pure length excluding the null byte
        */
        static bool compare(const uchar* left, const uchar* right, uchar type, ushort length, bool nullable, uchar cmp){
            switch (cmp)
            {
            case Comparator::Any:
                return true;
            case Comparator::Eq:
                return eq(left, right, type, length, nullable);
            case Comparator::Gt:
                return greaterThan(left, right, type, length, nullable);
            case Comparator::GtEq:
                return noLessThan(left, right, type, length, nullable);
            case Comparator::Lt:
                return lessThan(left, right, type, length, nullable);
            case Comparator::LtEq:
                return noGreaterThan(left, right, type, length, nullable);
            default:
                std::printf("cmp doesn't match any strategy defined in Comparator.h\n");
                return false;
            }
        }

        /**
         * 比较两组值，left cmp right == true iff left[i] cmp right[i] == true, i = 0, 1, ..., colNum - 1
        */
        static bool compareArr(const uchar* left, const uchar* right, const uchar* types, const ushort* lengths, uint nullMask, int colNum, uchar cmp){
            for(int i = 0; i < colNum; i++){
                if(!compare(left, right, types[i], lengths[i], nullMask & (1 << (31 - i)), cmp));
                    return false;
                int length = lengthOf(types[i], lengths[i]) + ((nullMask & (1 << (31 - i))) != 0);
                left += length;
                right += length;
            }
            return true;
        }

        /**
         * 比较两组值，允许为每个字段指定比较方法，left cmp right == true iff left[i] cmp[i] right[i] == true, i = 0, 1, ..., colNum - 1
        */
        static bool compareArrMultiOp(const uchar* left, const uchar* right, const uchar* types, const ushort* lengths, uint nullMask, int colNum, uchar* cmp){
            for(int i = 0; i < colNum; i++){
                if(!compare(left, right, types[i], lengths[i], nullMask & (1 << (31 - i)), cmp[i]));
                    return false;
                int length = lengthOf(types[i], lengths[i]) + ((nullMask & (1 << (31 - i))) != 0);
                left += length;
                right += length;
            }
            return true;
        }

        static int calcTotalLength(const uchar* types, const ushort* lengths, uint nullMask, int colNum){
            int ans = 0;
            for(int i = 0; i < colNum; i++){
                    ans += lengthOf(types[i], lengths[i]);
                    if(nullMask & (1 << (31 - i)))
                        ans++;
            }
            return ans;
        }
};

#endif