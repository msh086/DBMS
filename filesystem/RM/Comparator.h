#ifndef COMPARATOR_H
#define COMPARATOR_H
#include "../utils/pagedef.h"
#include<cassert>

class Comparator{
    public:
        const static uchar Any = 0, Eq = 1, NE = 2, Gt = 3, GtEq = 4, Lt = 5, LtEq = 6, None = 7;
        /**
         * A cmp B == B Reverse(cmp) A
        */
        static uchar Reverse(uchar cmp){
            switch (cmp)
            {
            case Any:
                return Any;
            case Eq:
                return Eq;
            case NE:
                return NE;
            case Gt:
                return Lt;
            case GtEq:
                return LtEq;
            case Lt:
                return Gt;
            case LtEq:
                return GtEq;
            case None:
                return None;
            default:
                assert(false);
            }
        }
        /**
         * A cmp B == !(A Complement(cmp) B)
        */
        static uchar Complement(uchar cmp){
            switch (cmp)
            {
            case Any:
                return None;
            case None:
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
            }
        }
};

#endif