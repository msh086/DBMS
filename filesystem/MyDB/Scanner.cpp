#include "Scanner.h"

Scanner* Table::GetScanner(bool (*demand)(const Record& record)){
    return new Scanner(this, demand);
}

Scanner* Table::GetScanner(const uchar* right, int colNum, uchar* cmp){
    return new Scanner(this, right, colNum, cmp);
}