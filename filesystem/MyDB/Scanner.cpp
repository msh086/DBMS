#include "Scanner.h"

Scanner* Table::GetScanner(bool (*demand)(const Record& record)){
    return new Scanner(this, demand);
}

Scanner* Table::GetScanner(const uchar* right, const uchar* types, const ushort* lengths, uint nullMask, int colNum, uchar* cmp){
    return new Scanner(this, right, types, lengths, nullMask, colNum, cmp);
}