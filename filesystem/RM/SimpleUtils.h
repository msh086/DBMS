#ifndef SIMPLEUTILS_H
#define SIMPLEUTILS_H
#include <cstring>
#include <string>
#include "../utils/pagedef.h"

// 分离编译时,模板函数要在.h文件中实现
template<typename T>
T& min(T& x, T& y){
    return x > y ? y : x;
} 

template<typename T>
T& max(T& x, T& y){
    return x > y ? x : y;
}

bool identical(const char* s1, const char* s2, int maxLen);

bool getBitFromLeft(uint bitmap, int posFromLeft);

void clearBitFromLeft(uint& bitmap, int posFromLeft);

void setBitFromLeft(uint& bitmap, int posFromLeft);

void removeBitFromLeft(uint& bitmap, int posFromLeft);

template<typename T>
T readNumFromString(const char* src, int& pos){
    T ans = 0;
    bool sign = false;
    if(src[0] == '-')
        sign = true;
    while(true){
        if(src[pos] >= '0' && src[pos] <= '9')
            ans = ans * 10 + src[pos] - '0';
        else
            break;
        pos++;
    }
    return sign ? -ans : ans;
}

float readFloatFromString(const char* src);

std::string extractStr(const void* src, int maxLength);

#endif