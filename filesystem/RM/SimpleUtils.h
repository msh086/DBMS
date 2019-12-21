#ifndef MATHUTILS_H
#define MATHUTILS_H
#include <cstring>
#include "../utils/pagedef.h"

template<typename T>
T& min(T& x, T& y);

template<typename T>
T& max(T& x, T& y);

bool identical(const char* s1, const char* s2, int maxLen);

bool getBitFromLeft(uint bitmap, int posFromLeft);

void clearBitFromLeft(uint& bitmap, int posFromLeft);

void setBitFromLeft(uint& bitmap, int posFromLeft);

#endif