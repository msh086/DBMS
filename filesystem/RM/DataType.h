#ifndef DATATYPE_H
#define DATATYPE_H
#include "../utils/pagedef.h"
class DataType{
    public:
        const static char NONE = 0,
            FLOAT = 1,
            BIGINT = 2,
            CHAR = 3,
            VARCHAR = 4,
            NUMERIC = 5, // The same as decimal
            DATE = 6,
            INT = 7;

#ifdef DEBUG
        static const char* NameofType(int type){
            const char* ans;
            switch (type)
            {
            case 0:
                ans = "";
                break;
            case 1:
                ans = "float";
                break;
            case 2:
                ans = "bigint";
                break;
            case 3:
                ans = "char";
                break;
            case 4:
                ans = "varchar";
                break;
            case 5:
                ans = "numeric";
                break;
            case 6:
                ans = "date";
                break;
            case 7:
                ans = "int";
                break;
            default:
                ans = "undefined";
                break;
            }
            return ans;
        }
#endif
};

#endif