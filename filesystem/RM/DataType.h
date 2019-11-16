#ifndef DATATYPE_H
#define DATATYPE_H
#include "../utils/pagedef.h"
#include <iostream>
#include <cstring>
using namespace std;

class DataType{
    private:
        // write the lower 'avail' bits in 'data' into dst[byte], skipping the highest 'offset' bits
        // it is ensured that offset < 8 and remain > 0, and the bits in dst are initially 0
        static void writeBits(uchar* dst, int& byte, int& offset, int data, int avail){ // TODO unit test?
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


    public:
        const static uchar NONE = 0,
            FLOAT = 1,
            BIGINT = 2,
            CHAR = 3,
            VARCHAR = 4,
            NUMERIC = 5, // The same as decimal
            DATE = 6,
            INT = 7;

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
        static bool noLessThan(void* left, void* right, uchar type, uint length, bool nullable){
            if(type == NONE){
                std::printf("Error: NONE type is not comparable");
                return false;
            }
            uchar* datal = (uchar*)left, *datar = (uchar*)right;
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
                return strncmp(reinterpret_cast<char*>(datal), reinterpret_cast<char*>(datar), length);
                break;
            }
            case NUMERIC:{ 
                // 1B: 1b for sign, 6b for dot position; 16B: 127b for 38 dec digits
                // numeric(p, s), p = 1, 2,...38, s = 0, 1,...p. p is the max dec digits to store, s is the max dec digits to the right of dot
                // p = 3k: bytes = ceiling(10k) + 1
                // p = 3k + 1: bytes = ceiling(10k + 4) + 1
                // p = 3k + 2: bytes = ceiling(10k + 7) + 1
                // p, s are stored in length(uint)
                bool signl = datal[0] >> 7, signr = datar[0] >> 7;
                if(signl != signr) // one is positive, the other is negative
                    return signl ? false : true; // signl == 1 -> left <= 0
                uint p = length >> 16, s = length & ((1 << 16) - 1);
                auto fun = [](uchar* buf, uchar* data, uint p){
                    uint byte = 0, offset = 0, pos = 0; // byte: current byte, offset: used bits, pos: position in buffer
                    uint n3digit = p / 3, remain = p % 3;
                    uint n3digitPtr = 0;
                    while(n3digitPtr < n3digit){
                        uint remain = 10;
                        uint ans = 0;
                        while(remain > 0){
                            uint takeBits = min(remain, offset);
                            ans <<= takeBits;
                            ans &= (data[byte] >> (8 - takeBits - offset)) & (255 >> (8 - takeBits));
                            uint firstPart = data[n3digitPtr] & (255 >> takeBits);
                            remain -= takeBits;
                            offset += takeBits;
                            if(offset == 8){
                                offset = 0;
                                byte++;
                            }
                        }
                        buf[pos++] = ans / 100;
                        ans %= 100;
                        buf[pos++] = ans / 10;
                        ans %= 10;
                        buf[pos++] = ans;
                        n3digitPtr++;
                    }
                    switch (remain)
                    {
                    case 1:{ // 4 bits, 1 dec
                            uchar value = 0;
                            if(offset <= 4)
                                value = (data[byte] >> (8 - 4 - offset)) & 15;
                            else{
                                value = data[byte] & (255 >> offset); // take rightmost 8 - offset bits
                                value <<= (offset - 4); // offset - 4 bits to take
                                value &= data[byte + 1] >> (12 - offset); // take leftmost offset - 4 bits
                            }
                            buf[pos++] = value;
                            break;
                        }
                    case 2:{ // 7 bits, 2 dec
                            uchar value = 0;
                            if(offset <= 1)
                                value = (data[byte] >> (1- offset)) & 127;
                            else{
                                value = data[byte] & (255 >> offset); // take rightmost 8 - offset bits
                                value <<= (offset - 1); // offset - 1 bits to take
                                value &= data[byte + 1] >> (9 - offset); // take leftmost offset - 1 bits
                            }
                            buf[pos++] = value / 10;
                            buf[pos++] = value % 10;
                            break;
                        }
                    default:
                        break;
                    }
                };
                break;
            }
            case DATE:
                break;
            case INT:
                break;
            default:
                break;
            }
        }
        // dotleft.dotright, if there is no dot, the string would be ""
        static void floatToBin(bool isNegative, const char* dotleft, const char* dotright, uchar* bindata, int p, int s){
            int p_div_3 = p / 3, p_mod_3 = p % 3;
            int totalBytes = 1 + (10 * p_div_3 + (p_mod_3 > 0 ? (p_mod_3 * 3 + 1) : 0) + 7) / 8; // ceiling(10k + f(remain)) + 1
            memset(bindata, 0, totalBytes); // clear memory
            if(isNegative)
                bindata[0] |= 128; // set sign bit
            
            uchar digits[p] = {0}; // buffer
            int ptr = 0;

            int lenl = strlen(dotleft), lenr = strlen(dotright);
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
};

#endif