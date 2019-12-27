#ifndef SIMPLEUTILS_H
#define SIMPLEUTILS_H
#include <cstring>
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

#endif