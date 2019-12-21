#include "SimpleUtils.h"
template<typename T>
T& min(T& x, T& y){
    return x > y ? y : x;
} 

template<typename T>
T& max(T& x, T& y){
    return x > y ? x : y;
}

bool identical(const char* s1, const char* s2, int maxLen){
    int len1 = strnlen(s1, maxLen), len2 = strnlen(s2, maxLen);
    return len1 == len2 && strncmp(s1, s2, len1) == 0;
}

bool getBitFromLeft(const uint bitmap, int posFromLeft){
    return bitmap & (1 << (31 - posFromLeft));
}

void clearBitFromLeft(uint& bitmap, int posFromLeft){
    bitmap &= ~(1 << (31 - posFromLeft));
}

void setBitFromLeft(uint& bitmap, int posFromLeft){
    bitmap |= 1 << (31 - posFromLeft);
}