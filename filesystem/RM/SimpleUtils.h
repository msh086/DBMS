#ifndef MATHUTILS_H
#define MATHUTILS_H
#include <cstring>

template<typename T>
T& min(const T& x, const T& y){
    return x > y ? y : x;
} 

bool identical(const char* s1, const char* s2, int maxLen){
    int len1 = strnlen(s1, maxLen), len2 = strnlen(s2, maxLen);
    return len1 == len2 && strncmp(s1, s2, len1) == 0;
}

#endif