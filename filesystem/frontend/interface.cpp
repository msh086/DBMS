
#include <cstdio>
#include "../MyDB/DBMS.h"
#include "../frontend/Global.h"
// #include "yacc.tab.h" // yacc生成的头文件
using namespace std;

extern int yyparse();
extern void yyrestart(FILE* new_file);

int main(){
    DBMS* dbms = DBMS::Instance();
    dbms->Init();
    Global::dbms = dbms;

    FILE* tmp = tmpfile();

	extern FILE* yyin;				//yyin和yyout都是FILE*类型
	yyin = tmp;						//yacc会从yyin读取输入，yyin默认是标准输入，这里改为磁盘文件。yacc默认向yyout输出，可修改yyout改变输出目的
    char buf[1000];
    printf("start\n");
    long prevPos = fseek(tmp, 0, SEEK_END);
    while(fgets(buf, 1000, stdin)){
        fputs(buf, tmp);
        fflush(tmp);
        fseek(tmp, prevPos, SEEK_SET);
        // fseek(tmp, 0, SEEK_SET);
	    int res = yyparse();						//使yacc开始读取输入和解析，它会调用lex的yylex()读取记号
        // parsing errors
        bool ok = true;
        if(Global::errorSign){ // syntax error
            ok = false;
            Global::errorSign = false;
        }
        if(ok && !Global::exitSign && Global::errors.empty()){
            Global::action(Global::types);
            Global::dbms->CurrentDatabase()->CloseTables();
            Global::freeMemory();
        }
        if(!Global::errors.empty()){ // syntax ok, content error
            ok = false;
            for(auto it = Global::errors.begin(); it != Global::errors.end(); it++)
                printf("syntax error at %d: %s\n", it->pos, it->msg.data());
            Global::errors.clear();
        }
        if(!ok)
            Global::exitSign = false;
        if(Global::exitSign){
            Global::dbms->Close();
            return 0;
        }
        Global::pos = 0;
        Global::types.clear();
        Global::action = nullptr;
        fseek(tmp, 0, SEEK_END);
        // printf("old pos: %ld", prevPos);
        prevPos = ftell(tmp);
        yyrestart(tmp);
        // printf("  new pos: %ld\n", prevPos);
    }
    fclose(tmp);
	return 0;
}