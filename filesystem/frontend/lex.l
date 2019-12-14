%{
/*本lex的生成文件是lex.yy.c
lex文件由3段组成，用2个%%行把这3段隔开。
第1段是声明段，包括：
1-C代码部分：include头文件、函数、类型等声明，这些声明会原样拷到生成的.c文件中。
2-状态声明，如%x COMMENT。
3-正则式定义，如digit ([0-9])。
第2段是规则段，是lex文件的主体，包括每个规则(如identifier)是如何匹配的，以及匹配后要执行的C代码动作。
第3段是C函数定义段，如yywrap()的定义，这些C代码会原样拷到生成的.c文件中。该段内容可以为空*/

//第1段：声明段
#include "main.h"		//lex和yacc要共用的头文件，里面包含了一些头文件，重定义了YYSTYPE
// ! 将yacc.y改为yacc.ypp以适应c++后，生成的头文件名字会变成yacc.tab.hpp和yacc.tab.cpp
#include "yacc.tab.h"	//用yacc编译yacc.y后生成的C头文件，内含%token、YYSTYPE、yylval等定义(都是C宏)，供lex.yy.c和yacc.tab.c使用

/*
extern "C"				//为了能够在C++程序里面调用C函数，必须把每一个需要使用的C函数，其声明都包括在extern "C"{}块里面，这样C++链接时才能成功链接它们。extern "C"用来在C++环境下设置C链接类型。
{						//yacc.y中也有类似的这段extern "C"，可以把它们合并成一段，放到共同的头文件main.h中
	int yywrap(void);
	int yylex(void);	//这个是lex生成的词法分析函数，yacc的yyparse()里会调用它，如果这里不声明，生成的yacc.tab.c在编译时会找不到该函数
}
*/
%}

/*lex的每个正则式前面可以带有"<状态>"，例如下面的"<COMMENT>\n"。每个状态要先用%x声明才能使用。
当lex开始运行时，默认状态是INITIAL，以后可在C代码里用"BEGIN 状态名;"切换到其它状态(BEGIN是lex/yacc内置的宏)。
这时，只有当lex状态切换到COMMENT后，才会去匹配以<COMMENT>开头的正则式，而不匹配其它状态开头的。
也就是说，lex当前处在什么状态，就考虑以该状态开头的正则式，而忽略其它的正则式。
其应用例如，在一段C代码里，同样是串"abc"，如果它写在代码段里，会被识别为标识符，如果写在注释里则就不会。所以对串"abc"的识别结果，应根据不同的状态加以区分。
本例子需要忽略掉文本中的行末注释，行末注释的定义是：从某个"//"开始，直到行尾的内容都是注释。其实现方法是：
1-lex启动时默认是INITIAL状态，在这个状态下，串"abc"会识别为标识符，串"123"会识别为整数等。
2-一旦识别到"//"，则用BEGIN宏切换到COMMENT状态，在该状态下，abc这样的串、以及其它字符会被忽略。只有识别到换行符\n时，再用BEGIN宏切换到初始态，继续识别其它记号。*/
%x STRING

/* 在前两段中写注释时,必须用块注释.如果要用行注释,这一行不能以//开头,要加缩进 */
/*非数字由大小写字母、下划线组成*/

/* 换行符，表示输入结束 */
newline		(\r|\n|\r\n)

nondigit	([_A-Za-z])

/*一位数字，可以是0到9*/
digit		([0-9])

/*整数由1至多位数字组成*/
integer		({digit}+)

/* 小数 */
float		({digit}+"."{digit}*)

/* 日期(可能不合法) */
date  		({digit}{1,4}-{digit}{1,2}-{digit}{1,2});

/*标识符，以非数字开头，后跟0至多个数字或非数字*/
identifier	([A-Za-z][_0-9a-zA-Z]*)

/*一个或一段连续的空白符*/
blank_chars	([ \f\t\v]+)

/* 合法的转义字符 */
ESC          ("\\"[nrt\"\\])

/* 非法的转义字符 */
BAD_ESC      ("\\"[^nrt\"\\])

/* 单字符运算符 */
SIMPLE_OP	 ("+"|"-"|"*"|"/"|"("|")"|">"|"<"|"="|","|"."|";")

/*下面%%后开始第2段：规则段*/
%%
"databases"		{yylval.pos = Global::pos; Global::pos += yyleng; return DATABASES;}
"database"		{yylval.pos = Global::pos; Global::pos += yyleng; return DATABASE;}
"tables"		{yylval.pos = Global::pos; Global::pos += yyleng; return TABLES;}
"table"			{yylval.pos = Global::pos; Global::pos += yyleng; return TABLE;}
"show"			{yylval.pos = Global::pos; Global::pos += yyleng; return SHOW;}
"create"		{yylval.pos = Global::pos; Global::pos += yyleng; return CREATE;}
"drop"			{yylval.pos = Global::pos; Global::pos += yyleng; return DROP;}
"use"			{yylval.pos = Global::pos; Global::pos += yyleng; return USE;}
"primary"		{yylval.pos = Global::pos; Global::pos += yyleng; return PRIMARY;}
"key"			{yylval.pos = Global::pos; Global::pos += yyleng; return KEY;}
"not"			{yylval.pos = Global::pos; Global::pos += yyleng; return NOT;}
"null"			{yylval.pos = Global::pos; Global::pos += yyleng; return KW_NULL;}
"insert"		{yylval.pos = Global::pos; Global::pos += yyleng; return INSERT;}
"into"			{yylval.pos = Global::pos; Global::pos += yyleng; return INTO;}
"values"		{yylval.pos = Global::pos; Global::pos += yyleng; return VALUES;}
"delete"		{yylval.pos = Global::pos; Global::pos += yyleng; return DELETE;}
"from"			{yylval.pos = Global::pos; Global::pos += yyleng; return FROM;}
"where"			{yylval.pos = Global::pos; Global::pos += yyleng; return WHERE;}
"update"		{yylval.pos = Global::pos; Global::pos += yyleng; return UPDATE;}
"set"			{yylval.pos = Global::pos; Global::pos += yyleng; return SET;}
"select"		{yylval.pos = Global::pos; Global::pos += yyleng; return SELECT;}
"is"			{yylval.pos = Global::pos; Global::pos += yyleng; return IS;}
"int"			{yylval.pos = Global::pos; Global::pos += yyleng; return KW_INT;}
"varchar"		{yylval.pos = Global::pos; Global::pos += yyleng; return VARCHAR;}
"char"			{yylval.pos = Global::pos; Global::pos += yyleng; return CHAR;}
"default"		{yylval.pos = Global::pos; Global::pos += yyleng; return DEFAULT;}
"constraint"	{yylval.pos = Global::pos; Global::pos += yyleng; return CONSTRAINT;}
"change"		{yylval.pos = Global::pos; Global::pos += yyleng; return CHANGE;}
"alter"			{yylval.pos = Global::pos; Global::pos += yyleng; return ALTER;}
"add"			{yylval.pos = Global::pos; Global::pos += yyleng; return ADD;}
"rename"		{yylval.pos = Global::pos; Global::pos += yyleng; return RENAME;}
"desc"			{yylval.pos = Global::pos; Global::pos += yyleng; return DESC;}
"index"			{yylval.pos = Global::pos; Global::pos += yyleng; return INDEX;}
"and"			{yylval.pos = Global::pos; Global::pos += yyleng; return AND;}
"date"			{yylval.pos = Global::pos; Global::pos += yyleng; return DATE;}
"float"			{yylval.pos = Global::pos; Global::pos += yyleng; return FLOAT;}
"foreign"		{yylval.pos = Global::pos; Global::pos += yyleng; return FOREIGN;}
"references"	{yylval.pos = Global::pos; Global::pos += yyleng; return REFERENCES;}
"numeric"		{yylval.pos = Global::pos; Global::pos += yyleng; return NUMERIC;}
"decimal"		{yylval.pos = Global::pos; Global::pos += yyleng; return NUMERIC;} // trick: decimal is the same as numeric
"on"			{yylval.pos = Global::pos; Global::pos += yyleng; return ON;}

">="			{yylval.pos = Global::pos; Global::pos += yyleng; return GE;}
"<="			{yylval.pos = Global::pos; Global::pos += yyleng; return LE;}
"<>"			{yylval.pos = Global::pos; Global::pos += yyleng; return NE;}
{SIMPLE_OP}		{yylval.pos = Global::pos; Global::pos += yyleng; return yytext[0];}

{newline}		{
					// yyterminate();
				}

{date}			{
					yylval.val.str.assign(yytext, yyleng);
					yylval.pos = Global::pos; 
					Global::pos += yyleng;
					return DATE_LIT;
				}

{integer}		{							//匹配整数串
					yylval.val.str.assign(yytext, yyleng);
					yylval.pos = Global::pos; 
					Global::pos += yyleng;
					return INT_LIT;			//向yacc返回: 识别出的记号类型是INTEGER
				}

{float}			{
					yylval.val.str.assign(yytext, yyleng);
					yylval.pos = Global::pos; 
					Global::pos += yyleng;
					return FLOAT_LIT;
				}

{blank_chars}	{							//遇空白符时，什么也不做，忽略它们
					Global::pos += yyleng;
				}
				
\"				{					
					yylval.pos = Global::pos; 		
					Global::pos++;
					BEGIN STRING;			//用BEGIN宏切换到注释状态，去过滤这段注释，下一次lex将只匹配前面带有<COMMENT>的正则式
				}


{identifier}	{							//匹配标识符串，此时串值由yytext保存
					yylval.val.str.assign(yytext, yyleng);	//通过yylval向yacc传递识别出的记号的值，由于yylval已定义为struct Type，这里就可以把yytext赋给其m_sId成员，到了yacc里就可以用$n的方式来引用了
					yylval.pos = Global::pos; 
					Global::pos += yyleng;
					return IDENTIFIER;		//向yacc返回: 识别出的记号类型是IDENTIFIER
				}

<STRING>\"		{							//注释状态下的规则，只有当前切换到COMMENT状态才会去匹配
					Global::buffer.push_back(yytext[0]);
					Global::pos++;
					yylval.val.str = Global::buffer;
					Global::buffer.clear();
					BEGIN INITIAL;			//在注释状态下，当遇到换行符时，表明注释结束了，返回初始态
					return STRING_LIT;
				}

<STRING>{ESC}	{
					Global::buffer.push_back(yytext[0]); // 加到缓冲区
					Global::pos++;
				}

<STRING>{BAD_ESC}
				{
					Global::buffer.push_back(yytext[0]);
					Global::error.push_back(Global::pos);
					Global::pos++;
				}

<STRING>.		{							
					Global::buffer.push_back(yytext[0]);
					Global::pos++;
				}


.				{							//.表示除\n以外的其它字符，注意这个规则要放在最后，因为一旦匹配了.就不会匹配后面的规则了(以其它状态<>开头的规则除外)
					Global::errors.push_back(Global::pos); // undefined
					Global::pos++;
				}

%%

//第3段：C函数定义段
int yywrap(void)
{
	puts("From flex: end of input");
	return 1;								//返回1表示读取全部结束。如果要接着读其它文件，可以这里fopen该文件，文件指针赋给yyin，并返回0
}
