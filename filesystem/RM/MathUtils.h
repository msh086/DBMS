#ifndef MATHUTILS_H
#define MATHUTILS_H

template<typename T>
T& min(const T& x, const T& y){
    return x > y ? y : x;
} 

#endif