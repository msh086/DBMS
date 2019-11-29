//lex.l和yacc.y共同使用的头文件
#ifndef MAIN_HPP
#define MAIN_HPP

#include <iostream>//使用C++库
#include <string>
#include <vector>
#include <stdio.h>//printf和FILE要用的
#include "../utils/pagedef.h" // 全局宏定义

using namespace std;

/*当lex每识别出一个记号后，是通过变量yylval向yacc传递数据的。默认情况下yylval是int类型，也就是只能传递整型数据。
yylval是用YYSTYPE宏定义的，只要重定义YYSTYPE宏，就能重新指定yylval的类型(可参见yacc自动生成的头文件yacc.tab.h)。
在我们的例子里，当识别出标识符后要向yacc传递这个标识符串，yylval定义成整型不太方便(要先强制转换成整型，yacc里再转换回char*)。
这里把YYSTYPE重定义为struct Type，可存放多种信息*/
enum Keyword{DATABASE, DATABASES, TABLE, TABLES, SHOW, CREATE, DROP, USE, PRIMARY, KEY, NOT, KW_NULL, INSERT, INTO, VALUES, DELETE, FROM, WHERE, UPDATE, SET, 
	SELECT, IS, INT, VARCHAR, CHAR, DEFAULT, CONSTRAINT, CHANGE, ALTER, ADD, RENAME, DESC, INDEX, AND, DATE, FLOAT, FOREIGN, REFERENCES, NUMERIC, DECIMAL};


struct Type//通常这里面每个成员，每次只会使用其中一个，一般是定义成union以节省空间(但这里用了string等复杂类型造成不可以)
{
	string str;
	int m_nInt;
	Keyword kw;
};


class Global{
	public:
		static string buffer; // 接受字符串时的缓冲区
		static vector<int> errors; // 遇到的错误
		static int pos; // 当前位置
};
string Global::buffer;
vector<int> Global::errors;
int Global::pos = 0;
#define YYSTYPE Type//把YYSTYPE(即yylval变量)重定义为struct Type类型，这样lex就能向yacc返回更多的数据了

#endif
