#include "SimpleUtils.h"

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

float readFloatFromString(const char* src){
    const char* dot = strchr(src, '.');
    int len = strlen(src);
    int dotPos = 0;
    if(dot)
        dotPos = dot - src;
    else
        dotPos = len;
    float ans = 0;
    for(int i = 0; i < dotPos; i++){
        ans = ans * 10 + src[i] - '0';
    }
    float unit = 0.1f;
    for(int i = dotPos + 1; i < len; i++){
        ans += unit * (src[i] - '0');
        unit /= 10;
    }
    return ans;
}