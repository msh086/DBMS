
#include <cstdio>
// #include "yacc.tab.h" // yacc生成的头文件
using namespace std;

extern int yyparse();

int main(){
    FILE* tmp = tmpfile();

	extern FILE* yyin;				//yyin和yyout都是FILE*类型
	yyin = tmp;						//yacc会从yyin读取输入，yyin默认是标准输入，这里改为磁盘文件。yacc默认向yyout输出，可修改yyout改变输出目的
    char buf[100];
    printf("start\n");
    long prevPos = fseek(tmp, 0, SEEK_END);
    while(fgets(buf, 100, stdin)){
        fputs(buf, tmp);
        fflush(tmp);
        fseek(tmp, prevPos, SEEK_SET);
        // printf("before parsing, now at %ld\n", ftell(tmp));
	    int res = yyparse();						//使yacc开始读取输入和解析，它会调用lex的yylex()读取记号
        // printf("parse result:%d, now at %ld\n\n", res, ftell(tmp));
        fseek(tmp, 0, SEEK_END);
        prevPos = ftell(tmp);
    }
    fclose(tmp);
	return 0;
}