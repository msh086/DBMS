#ifndef COMPARATOR_H
#define COMPARATOR_H
#include "../utils/pagedef.h"
#include<cassert>

class Comparator{
    public:
        const static uchar Any = 0, Eq = 1, NE = 2, Gt = 3, GtEq = 4, Lt = 5, LtEq = 6;
        static uchar Opposite(uchar cmp){
            switch (cmp)
            {
            case Any:
                return Any;
            case Eq:
                return NE;
            case NE:
                return Eq;
            case Gt:
                return LtEq;
            case GtEq:
                return Lt;
            case Lt:
                return GtEq;
            case LtEq:
                return Gt;
            default:
                assert(false);
                return Any;
            }
        }
};

#endif