#ifndef COMPARATOR_H
#define COMPARATOR_H
#include "../utils/pagedef.h"

class Comparator{
    public:
        const static uchar Any = 0, Eq = 1, NE = 2, Gt = 3, GtEq = 4, Lt = 5, LtEq = 6;
};

#endif