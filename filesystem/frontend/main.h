//lex.l和yacc.y共同使用的头文件
#ifndef MAIN_HPP
#define MAIN_HPP

#include <string>
#include <stdio.h>
#include <vector>
#include "../utils/pagedef.h" // 全局宏定义
#include "Global.h" // lexer的辅助静态类
#include "../RM/DataType.h"
#include "Field.h"

extern "C"			//为了能够在C++程序里面调用C函数，必须把每一个需要使用的C函数，其声明都包括在extern "C"{}块里面，这样C++链接时才能成功链接它们。extern "C"用来在C++环境下设置C链接类型。
{					//lex.l中也有类似的这段extern "C"，可以把它们合并成一段，放到共同的头文件main.h中
	int yywrap(void);
	int yylex(void);	//这个是lex生成的词法分析函数，yacc的yyparse()里会调用它，如果这里不声明，生成的yacc.tab.c在编译时会找不到该函数
	void yyerror(const char *s);
	extern int yylex(void);//该函数是在lex.yy.c里定义的，yyparse()里要调用该函数，为了能编译和链接，必须用extern加以声明
}


/*当lex每识别出一个记号后，是通过变量yylval向yacc传递数据的。默认情况下yylval是int类型，也就是只能传递整型数据。
yylval是用YYSTYPE宏定义的，只要重定义YYSTYPE宏，就能重新指定yylval的类型(可参见yacc自动生成的头文件yacc.tab.h)。
在我们的例子里，当识别出标识符后要向yacc传递这个标识符串，yylval定义成整型不太方便(要先强制转换成整型，yacc里再转换回char*)。
这里把YYSTYPE重定义为struct Type，可存放多种信息*/
// enum Keyword{DATABASE, DATABASES, TABLE, TABLES, SHOW, CREATE, DROP, USE, PRIMARY, KEY, NOT, KW_NULL, INSERT, INTO, VALUES, DELETE, FROM, WHERE, UPDATE, SET, 
// 	SELECT, IS, INT, VARCHAR, CHAR, DEFAULT, CONSTRAINT, CHANGE, ALTER, ADD, RENAME, DESC, INDEX, AND, DATE, FLOAT, FOREIGN, REFERENCES, NUMERIC, DECIMAL};

// literals, including INT, BIGINT, FLOAT, NUMERIC, NULL, CHAR, VARCHAR, DATE
// 字符串类型直接存储在str中
// INT, BIGINT, FLOAT, DATE所占空间固定,存贮在bytes的开头
// NUMERIC连header byte和数据部分一起存贮在bytes的开头, 至多17B
// NULL由type == DataType::NONE来决定
struct Val{
	std::string str;
	uchar type = DataType::NONE;
	uchar bytes[17] = {0};
};

struct Col{

};

// 这是flex和yacc共用的数据结构,用于:flex向yacc传递数据;yacc产生式右侧向左侧传递数据
struct Type//通常这里面每个成员，每次只会使用其中一个，一般是定义成union以节省空间(但这里用了string等复杂类型造成不可以)
{
	uint pos = 0;
	// field related
	std::vector<Field> fieldList;
	Field field;
	// val related, varLists = varList[] = var[][]
	std::vector<std::vector<Val>> valLists;
	std::vector<Val> valList;
	Val val;
	// cmp
	uchar cmp = Comparator::Any;
};


#define YYSTYPE Type//把YYSTYPE(即yylval变量)重定义为struct Type类型，这样lex就能向yacc返回更多的数据了

#endif
