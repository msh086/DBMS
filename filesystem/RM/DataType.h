#ifndef DATATYPE_H
#define DATATYPE_H

class DataType{
    public:
        const static char INT = 0,
            FLOAT = 1,
            BIGINT = 2,
            CHAR = 3,
            VARCHAR = 4,
            NUMERIC = 5, // The same as decimal
            DATE = 6;
};

#endif