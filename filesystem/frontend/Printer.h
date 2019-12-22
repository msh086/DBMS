#ifndef PRINTER_H
#define PRINTER_H

#include<vector>
#include<string>
#include<cstdio>
#include "../RM/Record.h"

class Printer{
    static void HorizontalLine(const int* maxWidth, int ColCount){
        for(int i = 0; i < ColCount; i++){
            putchar('+');
            for(int j = 0; j < maxWidth[i]; j++)
                putchar('-');
            putchar('-');
            putchar('-');
        }
        putchar('+');
        putchar('\n');
    }
    public:
    /**
     * @param table 是行的数组
    */
    static void PrintTable(const std::vector<std::vector<std::string>>& table, int ColCount, int RowCount){
        int maxWidth[ColCount] = {0};
        for(int i = 0; i < ColCount; i++){
            for(int j = 0; j < RowCount; j++) // j行i列
                if(table[j][i].length() > maxWidth[i])
                    maxWidth[i] = table[j][i].length();
        }
        for(int i = 0; i < RowCount; i++){
            HorizontalLine(maxWidth, ColCount);
            for(int j = 0; j < ColCount; j++){
                printf("| %*s ", maxWidth[j], table[i][j].data());
            }
            putchar('|');
            putchar('\n');
        }
        HorizontalLine(maxWidth, ColCount);
    }

    static std::string FieldToStr(const Record& tmpRec, uchar type, uchar index, ushort length, uint offset);
};

#endif