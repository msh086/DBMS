//lex.l和yacc.y共同使用的头文件
#ifndef MAIN_HPP
#define MAIN_HPP

#include <string>
#include <stdio.h>//printf和FILE要用的
#include "../utils/pagedef.h" // 全局宏定义
#include "Global.h" // lexer的辅助静态类
#include "../RM/DataType.h"
#include <vector>

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

struct Field{
	char name[MAX_ATTRI_NAME_LEN] = "";
    uchar type = DataType::NONE;
	ushort length = 0;
	bool nullable = true;
	bool hasDefault = false;
	uchar* defaultValue = nullptr;
};

// 这是flex和yacc共用的数据结构,用于:flex向yacc传递数据;yacc产生式右侧向左侧传递数据
struct Type//通常这里面每个成员，每次只会使用其中一个，一般是定义成union以节省空间(但这里用了string等复杂类型造成不可以)
{
	uint pos = 0;
	std::string str; // for lexer, record all kinds of literals. string_lit, int_lit, float_lit, data_lit
	std::vector<Field> fieldList;
	Field field;
	// 类似Union, 用uchar[8]和type来指定存储的值
	uchar type = DataType::NONE;
	uchar val[17] = {0};
};

const uchar INVALID_ARG = 1, OUT_OF_RANGE = 2, OTHER = 3;

uchar strToInt(const string& str, int& dst){
	try{
		dst = std::stoi(str);
		return 0;
	}
	catch(const std::invalid_argument& ia){
		return INVALID_ARG;
	}
	catch(const std::out_of_range& oor){
		return OUT_OF_RANGE;
	}
	catch(const std::exception& e){
		return OTHER;
	}
}

uchar strToLL(const string& str, ll& dst){
	try{
		dst = std::stoll(str);
		return 0;
	}
	catch(const std::invalid_argument& ia){
		return INVALID_ARG;
	}
	catch(const std::out_of_range& oor){
		return OUT_OF_RANGE;
	}
	catch(const std::exception& e){
		return OTHER;
	}
}

uchar strToFloat(const string& str, float dst){
	try{
		dst = std::stof(str);
		return 0;
	}
	catch(const std::invalid_argument& ia){
		return INVALID_ARG;
	}
	catch(const std::out_of_range& oor){
		return OUT_OF_RANGE;
	}
	catch(const std::exception& e){
		return OTHER;
	}
}

bool subTypeOf(uchar left, uchar right, uint llen, uint rlen){
	if(left == DataType::NONE) // null lit can be converted to any type
		return true;
	if(left == DataType::INT && right == DataType::BIGINT) // int is subtype of bigint
		return true;
	if(left == DataType::INT && right == DataType::FLOAT) // int is subtype of float
		return true;
	if(left == DataType::CHAR && right == DataType::VARCHAR) // char is subtype of varchar
		return true;
	if(left == DataType::FLOAT && right == DataType::NUMERIC) // float is subtype of numeric: precision dosen't matter
		return true;
	if(left == DataType::CHAR && right == DataType::CHAR && llen <= rlen) // shorter char is subtype of longer char
		return true;
	if(left == DataType::VARCHAR && right == DataType::VARCHAR && llen <= rlen) // shorter varchar is subtype of longer varchar
		return true;
	return left == right;
}


#define YYSTYPE Type//把YYSTYPE(即yylval变量)重定义为struct Type类型，这样lex就能向yacc返回更多的数据了

#endif
