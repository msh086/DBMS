%{
/*本yacc的生成文件是yacc.tab.c和yacc.tab.h
yacc文件由3段组成，用2个%%行把这3段隔开。
第1段是声明段，包括：
1-C代码部分：include头文件、函数、类型等声明，这些声明会原样拷到生成的.c文件中。
2-记号声明，如%token
3-类型声明，如%type
第2段是规则段，是yacc文件的主体，包括每个产生式是如何匹配的，以及匹配后要执行的C代码动作。
第3段是C函数定义段，如yyerror()的定义，这些C代码会原样拷到生成的.c文件中。该段内容可以为空*/

//第1段：声明段
#include "main.h"	//lex和yacc要共用的头文件，里面包含了一些头文件，重定义了YYSTYPE
/*
extern "C"			//为了能够在C++程序里面调用C函数，必须把每一个需要使用的C函数，其声明都包括在extern "C"{}块里面，这样C++链接时才能成功链接它们。extern "C"用来在C++环境下设置C链接类型。
{					//lex.l中也有类似的这段extern "C"，可以把它们合并成一段，放到共同的头文件main.h中
	void yyerror(const char *s);
	extern int yylex(void);//该函数是在lex.yy.c里定义的，yyparse()里要调用该函数，为了能编译和链接，必须用extern加以声明
}
*/
%}

/*lex里要return的记号的声明
用token后加一对<member>来定义记号，旨在用于简化书写方式。
假定某个产生式中第1个终结符是记号OPERATOR，则引用OPERATOR属性的方式：
1-如果记号OPERATOR是以普通方式定义的，如%token OPERATOR，则在动作中要写$1.m_cOp，以指明使用YYSTYPE的哪个成员
2-用%token<m_cOp>OPERATOR方式定义后，只需要写$1，yacc会自动替换为$1.m_cOp
另外用<>定义记号后，非终结符如file, tokenlist，必须用%type<member>来定义(否则会报错)，以指明它们的属性对应YYSTYPE中哪个成员，这时对该非终结符的引用，如$$，会自动替换为$$.member*/
%token	DATABASE	DATABASES	TABLE	TABLES
%token	SHOW		CREATE		DROP	USE
%token	PRIMARY		KEY			NOT		KW_NULL		// NULL 和c++关键字冲突，改为KW_NULL
%token	INSERT		INTO 		VALUES	DELETE
%token	FROM		WHERE		UPDATE	SET
%token	SELECT		IS 			KW_INT 	VARCHAR		// INT 和c++关键字冲突，改为KW_INT
%token 	CHAR 		DEFAULT		CONSTRAINT	CHANGE
%token 	ALTER		ADD			RENAME	DESC
%token	INDEX		AND			DATE 	FLOAT
%token	FOREIGN		REFERENCES	NUMERIC
%token 	ON
// 以上是SQL关键字
%token 	INT_LIT		STRING_LIT	FLOAT_LIT	DATE_LIT
%token 	IDENTIFIER	GE			LE 			NE
%token	'+'	'-'	'*'	'/' '(' ')' '>' '<' '=' ',' '.' ';'

%%

Start		:	Stmt
				{
					printf("From yacc: start parsing\n");
					if(!Global::errors.empty()){ // errors from lexer
						YYABORT;
					}
				}

Stmt		:	SysStmt ';'
				{
					printf("sys\n");
				}
			| 	DbStmt ';'
				{
					printf("db\n");
				}
			| 	TbStmt ';'
				{
					printf("tb\n");
				}
			|	 IdxStmt ';'
				{
					printf("idx\n");
				}
			| 	AlterStmt ';'
				{
					printf("alter\n");
				}
			;

SysStmt		:	SHOW DATABASES
				{
					printf("show db\n");
				}
			;

DbStmt		:	CREATE DATABASE IDENTIFIER
				{
					printf("create db\n");
				}
			|	DROP DATABASE IDENTIFIER
				{
					printf("drop db\n");
				}
			|	USE IDENTIFIER
				{
					printf("use db\n");
				}
			|	SHOW TABLES
				{
					printf("show tb\n");
				}
			;

TbStmt		:	CREATE TABLE IDENTIFIER '(' fieldList ')'
				{
					printf("create tb\n");
				}
			|	DROP TABLE IDENTIFIER
				{
					printf("drop tb\n");
				}
			|	DESC IDENTIFIER
				{
					printf("desc tb\n");
				}
			|	INSERT INTO IDENTIFIER VALUES '(' valueLists ')' // valueLists = '(' valueList ')' (',' '(' valueList ')')*, 用于一次插入多条记录
				{
					printf("insert db\n");
				}
			|	DELETE FROM IDENTIFIER WHERE whereClause
				{
					printf("delete tb\n");
				}
			|	UPDATE IDENTIFIER SET setClause WHERE whereClause
				{
					printf("update tb\n");
				}
			| 	SELECT selector FROM IdList WHERE whereClause
				{
					printf("select tb\n");
				}
			;

IdxStmt		:	CREATE INDEX IDENTIFIER ON IDENTIFIER '(' IdList ')'
				{
					printf("create idx\n");
				}
			|	DROP INDEX IDENTIFIER
				{
					printf("drop idx\n");
				}
			|	ALTER TABLE IDENTIFIER ADD INDEX IDENTIFIER '(' IdList ')'
				{
					printf("alter add idx\n");
				}
			|	ALTER TABLE IDENTIFIER DROP INDEX IDENTIFIER
				{
					printf("alter drop idx\n");
				}
			;

AlterStmt	:	ALTER TABLE IDENTIFIER ADD field
				{
					printf("alter add col\n");
				}
			;	// more!

// fieldList 是建表时使用的
fieldList	:	fieldList ',' field
				{
					printf("fieldList recur\n");
					$$.fieldList = $1.fieldList;
					$$.fieldList.push_back($3.field);
				}
			|	field
				{
					printf("fieldList base\n");
					$$.fieldList.push_back($1.field);
				}
			;

// field可以组成fieldList用于建表,也可以在alter add里面用于增加列
// 名字长度已经经过检查
field		:	IDENTIFIER type
				{
					printf("id type\n");
					if($1.str.length() > MAX_ATTRI_NAME_LEN){ // attribute name too long
						Global::errors.push_back($1.pos);
						YYABORT;
					}
					else
						memcpy($$.field.name, $1.str.data(), $1.str.length());
					$$.field.type = $2.type;
					$$.field.length = DataType::lengthOf($2.type, *(uint*)$2.val);
				}
			|	IDENTIFIER type NOT KW_NULL
				{
					printf("not null\n");
					if($1.str.length() > MAX_ATTRI_NAME_LEN){ // attribute name too long
						Global::errors.push_back($1.pos);
						YYABORT;
					}
					else
						memcpy($$.field.name, $1.str.data(), $1.str.length());
					$$.field.type = $2.type;
					$$.field.length = DataType::lengthOf($2.type, *(uint*)$2.val);
					$$.field.nullable = false;
				}
			|	IDENTIFIER type DEFAULT value
				{
					printf("default\n");
					if($1.str.length() > MAX_ATTRI_NAME_LEN){ // attribute name too long
						Global::errors.push_back($1.pos);
						YYABORT;
					}
					else
						memcpy($$.field.name, $1.str.data(), $1.str.length());
					$$.field.type = $2.type;
					$$.field.length = DataType::lengthOf($2.type, *(uint*)$2.val);
					// decide subtype
					if($4.type == DataType::NONE){ // value is null
						// do nothing, $$.field.defaultValue will be nullptr
					}
					if($4.type == DataType::INT && $2.type == DataType::BIGINT){
						*(ll*)$$.val = *(int*)$4.val;
						$$.field.defaultValue = new uchar[8];
						memcpy($$.field.defaultValue, $$.val, 8);
					}
					else if($4.type == DataType::INT && $2.type == DataType::FLOAT){
						*(float*)$$.val = *(int*)$4.val;
						$$.field.defaultValue = new uchar[4];
						memcpy($$.field.defaultValue, $$.val, 4);
					}
					else if($4.type == DataType::FLOAT && $2.type == DataType::NUMERIC){
						uint pns = *(uint*)$2.val;
						int bytes = DataType::lengthOf(DataType::NUMERIC, pns);
						DataType::floatToBin(*(float*)$2.val < 0, $2.str.data(), $2.str.find('.'), $$.val, pns >> 8, pns & 0xff);
						$$.field.defaultValue = new uchar[bytes];
						memcpy($$.val, $4.val, bytes);
						memcpy($$.field.defaultValue, $$.val, bytes);
					}
					else if($4.type == DataType::INT && $2.type == DataType::NUMERIC){
						uint pns = *(uint*)$2.val;
						int bytes = DataType::lengthOf(DataType::NUMERIC, pns);
						DataType::floatToBin(*(int*)$2.val < 0, $2.str.data(), $2.str.length(), $$.val, pns >> 8, pns & 0xff);
						$$.field.defaultValue = new uchar[bytes];
						memcpy($$.val, $4.val, bytes);
						memcpy($$.field.defaultValue, $$.val, bytes);
					}
					else if($4.type == DataType::BIGINT && $2.type == DataType::NUMERIC){
						uint pns = *(uint*)$2.val;
						int bytes = DataType::lengthOf(DataType::NUMERIC, pns);
						DataType::floatToBin(*(ll*)$2.val < 0, $2.str.data(), $2.str.length(), $$.val, pns >> 8, pns & 0xff);
						$$.field.defaultValue = new uchar[bytes];
						memcpy($$.val, $4.val, bytes);
						memcpy($$.field.defaultValue, $$.val, bytes);
					}
					else if($4.type == DataType::CHAR && $2.type == DataType::VARCHAR){
						$$.str = $4.str;
						$$.field.defaultValue = new uchar[$$.str.length()];
						memcpy($$.field.defaultValue, $$.str.data(), $$.str.length());
					}
					else if($4.type == DataType::CHAR && $2.type == DataType::CHAR && $4.str.length() <= *(int*)$2.val){
						$$.str = $4.str;
						$$.field.defaultValue = new uchar[$$.str.length()];
						memcpy($$.field.defaultValue, $$.str.data(), $$.str.length());
					}
					else if($4.type == DataType::VARCHAR && $2.type == DataType::VARCHAR && $4.str.length() <= *(int*)$2.val){
						$$.str = $4.str;
						$$.field.defaultValue = new uchar[$$.str.length()];
						memcpy($$.field.defaultValue, $$.str.data(), $$.str.length());
					}
					else if($4.type == $2.type){
						if($2.type == DataType::INT){
							*(int*)$$.val = *(int*)$4.val;
							$$.field.defaultValue = new uchar[4];
							memcpy($$.field.defaultValue, $$.val, 4);
						}
						else if($2.type == DataType::BIGINT){
							*(ll*)$$.val = *(ll*)$4.val;
							$$.field.defaultValue = new uchar[8];
							memcpy($$.field.defaultValue, $$.val, 8);
						}
						else if($2.type == DataType::FLOAT){
							*(float*)$$.val = *(float*)$4.val;
							$$.field.defaultValue = new uchar[8];
							memcpy($$.field.defaultValue, $$.val, 8);
						}
						else if($2.type == DataType::DATE){
							memcpy($$.val, $4.val, 3);
							$$.field.defaultValue = new uchar[8];
							memcpy($$.field.defaultValue, $$.val, 8);
						}
						// a value will only be parsed as float, not numeric
						// varchar and char are solved previously
					}
					else{ // wrong
						Global::errors.push_back($4.pos);
						YYABORT;
					}
					$$.field.hasDefault = true;
				}
			|	IDENTIFIER type NOT KW_NULL DEFAULT value
				{
					printf("not null default\n");
				}
			|	PRIMARY KEY '(' IdList ')'
				{
					printf("primary key\n");
				}
			|	FOREIGN KEY '(' IDENTIFIER ')' REFERENCES IDENTIFIER '(' IDENTIFIER ')'
				{
					printf("foreign\n");
				}
			;

// Type.type : 类型
// Type.val : 对于INT, DATE, FLOAT来说此字段无用; 对于VARCHAR, CHAR来说表示长度, 对于NUMERIC来说表示两个长度参数, 都将结果以int类型存到val处
// 长度都已经经过检查
type		:	KW_INT
				{
					printf("type int\n");
					$$.type = DataType::INT;
				}
			|	DATE
				{
					printf("type data\n");
					$$.type = DataType::DATE;
				}
			|	FLOAT
				{
					printf("type float\n");
					$$.type = DataType::FLOAT;
				}
			|	CHAR '(' INT_LIT ')'
				{
					printf("char\n");
					$$.type = DataType::CHAR;
					int intVal = 0;
					uchar code = Field::strToInt($3.str, intVal);
					if(code != 0 || intVal < 0 || intVal > 255){
						Global::errors.push_back($3.pos);
						YYABORT;
					}
					else
						*(int*)$$.val = intVal;
				}
			|	VARCHAR	'(' INT_LIT ')'
				{
					printf("varchar\n");
					$$.type = DataType::VARCHAR;
					int intVal = 0;
					uchar code = Field::strToInt($3.str, intVal);
					if(code != 0 || intVal < 0 || intVal > 65535){
						Global::errors.push_back($3.pos);
						YYABORT;
					}
					else
						*(int*)$$.val = intVal;
				}
			|	NUMERIC '(' INT_LIT ',' INT_LIT ')'
				{
					printf("numeric\n");
					$$.type = DataType::NUMERIC;
					int intVal = 0, intVal0 = 0;
					uchar code = Field::strToInt($3.str, intVal), code0 = Field::strToInt($5.str, intVal0);
					if(code != 0 || intVal < 1 || intVal > 38){
						Global::errors.push_back($3.pos);
						YYABORT;
					}
					else if(code0 != 0 || intVal0 < 0 || intVal0 > intVal){
						Global::errors.push_back($5.pos);
						YYABORT;
					}
					*(uint*)$$.val = (intVal << 8) + intVal0; // p, s are stored in val
				}
			;

// insert into {tablename} values(valueList), (valueList), (valueList) ...
valueLists	:	'(' valueList ')'
				{
					printf("valueLists base\n");
				}
			|	valueLists ',' '(' valueList ')'
				{
					printf("valueLists recur\n");
				}
			;

valueList	:	value
				{
					printf("valueList base\n");
				}
			|	valueList ',' value
				{
					printf("valueList recur\n");
				}
			;

value		:	INT_LIT
				{
					printf("int lit\n");
					int intVal = 0;
					ll llVal = 0;
					uchar code = Field::strToInt($1.str, intVal);
					if(code == 0){
						*(int*)$$.val = intVal;
						$$.type = DataType::INT;
					}
					else{
						code = Field::strToLL($1.str, llVal);
						if(code != 0){
							Global::errors.push_back($1.pos);
							YYABORT;
						}
						else{
							*(ll*)$$.val = llVal;
							$$.type = DataType::BIGINT;
						}
					}
				}
			|	FLOAT_LIT
				{
					printf("float lit\n");
					$$.type = DataType::FLOAT;
					float floatVal = 0;
					uchar code = Field::strToFloat($1.str, floatVal);
					if(code != 0){
						Global::errors.push_back($1.pos);
						YYABORT;
					}
					else
						*(float*)$$.val = floatVal;
				}
			|	DATE_LIT
				{
					printf("date lit\n");
					$$.type = DataType::DATE;
					int ym_sep = $1.str.find('-'), md_sep = $1.str.rfind('-');
					int year = 0, month = 0, day = 0;
					for(int i = 0; i < ym_sep; i++)
						year = year * 10 + $1.str[i] - '0';
					for(int i = ym_sep + 1; i < md_sep; i++)
						month = month * 10 + $1.str[i] - '0';
					for(int i = md_sep + 1; i < $1.str.length(); i++)
						day = day * 10 + $1.str[i] - '0';
					bool ok = true;
					if(year == 0 || month == 0 || month > 12 || day == 0 || day > 31)
						ok = false;
					if(ok && day == 31 && (month == 4 || month == 6 || month == 9 || month == 11 ))
						ok = false;
					if(ok && month == 2){
						if(day == 30)
							ok = false;
						else if(day == 29 && !((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)))
							ok = false;
					}
					if(ok)
						DataType::dateToBin(year, month, day, $$.val);
					else{
						Global::errors.push_back($1.pos);
						YYABORT;
					}
				}
			| 	STRING_LIT
				{
					printf("string lit\n");
					if($1.str.length() <= 255)
						$$.type = DataType::CHAR;
					else if($1.str.length() <= 65535)
						$$.type = DataType::VARCHAR;
					else{
						Global::errors.push_back($1.pos);
						YYABORT;
					}
				}
			|	KW_NULL
				{
					printf("null lit\n");
					$$.type = DataType::NONE; // use NONE to record null lit
				}
			| 	'-' value // minus: for float, int and bigint
				{
					if($2.type == DataType::FLOAT){
						*(float*)$2.val = -*(float*)$2.val;
					}
					else if($2.type == DataType::INT){
						*(int*)$2.val = -*(int*)$2.val;
					}
					else if($2.type == DataType::BIGINT){
						*(ll*)$2.val = -*(ll*)$2.val;
					}
					else{
						Global::errors.push_back($1.pos);
						YYABORT;
					}
				}
			;

whereClause	:	col op expr
				{
					printf("if op\n");
				}
			|	col IS KW_NULL
				{
					printf("if null\n");
				}
			|	col IS NOT KW_NULL
				{
					printf("if not null\n");
				}
			|	whereClause AND whereClause
				{
					printf("if and\n");
				}
			;

col			:	IDENTIFIER
				{
					printf("simple col\n");
				}
			|	IDENTIFIER '.' IDENTIFIER
				{
					printf("compl col\n");
				}
			;

op			:	GE
				{
					printf("GE\n");
				}
			|	LE
				{
					printf("LE\n");
				}
			|	NE
				{
					printf("NE\n");
				}
			|	'>'
				{
					printf("Gt\n");
				}
			|	'<'
				{
					printf("Lt\n");
				}
			|	'='
				{
					printf("eq\n");
				}
			;

expr		:	value
				{
					printf("value expr\n");
				}
			|	col
				{
					printf("col expr\n");
				}
			;

setClause	:	IDENTIFIER '=' value
				{
					printf("set\n");
				}
			|	setClause ',' IDENTIFIER '=' value
				{
					printf("multiple set\n");
				}
			;

selector	:	'*'
				{
					printf("select *\n");
				}
			|	colList
				{
					printf("select colList\n");
				}
			;

colList		:	col
				{
					printf("colList base\n");
				}
			| 	colList ',' col
				{
					printf("colList recur\n");
				}
			;

IdList		:	IDENTIFIER
				{
					printf("IdList base\n");
				}
			|	IdList ',' IDENTIFIER
				{
					printf("IdList recur\n");
				}
			;

/*
file:								//文件，由记号流组成
	tokenlist						//这里仅显示记号流中的ID
	{
		cout<<"all id:"<<$1<<endl;	//$1是非终结符tokenlist的属性，由于该终结符是用%type<m_sId>定义的，即约定对其用YYSTYPE的m_sId属性，$1相当于$1.m_sId，其值已经在下层产生式中赋值(tokenlist IDENTIFIER)
	};
tokenlist:							//记号流，或者为空，或者由若干数字、标识符、及其它符号组成
	{
	}
	| tokenlist INTEGER
	{
		cout<<"int: "<<$2<<endl;	//$2是记号INTEGER的属性，由于该记号是用%token<m_nInt>定义的，即约定对其用YYSTYPE的m_nInt属性，$2会被替换为yylval.m_nInt，已在lex里赋值
	}
	| tokenlist IDENTIFIER
	{
		$$+=" " + $2;				//$$是非终结符tokenlist的属性，由于该终结符是用%type<m_sId>定义的，即约定对其用YYSTYPE的m_sId属性，$$相当于$$.m_sId，这里把识别到的标识符串保存在tokenlist属性中，到上层产生式里可以拿出为用
		cout<<"id: "<<$2<<endl;		//$2是记号IDENTIFIER的属性，由于该记号是用%token<m_sId>定义的，即约定对其用YYSTYPE的m_sId属性，$2会被替换为yylval.m_sId，已在lex里赋值
	}
	| tokenlist OPERATOR
	{
		cout<<"op: "<<$2<<endl;		//$2是记号OPERATOR的属性，由于该记号是用%token<m_cOp>定义的，即约定对其用YYSTYPE的m_cOp属性，$2会被替换为yylval.m_cOp，已在lex里赋值
	};
*/

%%
void yyerror(const char *s)			//当yacc遇到语法错误时，会回调yyerror函数，并且把错误信息放在参数s中
{
	printf("Error: %s\n", s);					//直接输出错误信息
}

/*
int main()							//程序主函数，这个函数也可以放到其它.c, .cpp文件里
{
	const char* sFile="file.txt";	//打开要读取的文本文件
	FILE* fp=fopen(sFile, "r");
	if(fp==nullptr)
	{
		printf("cannot open %s\n", sFile);
		return -1;
	}
	extern FILE* yyin;				//yyin和yyout都是FILE*类型
	yyin=fp;						//yacc会从yyin读取输入，yyin默认是标准输入，这里改为磁盘文件。yacc默认向yyout输出，可修改yyout改变输出目的

	printf("-----begin parsing %s\n", sFile);
	yyparse();						//使yacc开始读取输入和解析，它会调用lex的yylex()读取记号
	puts("-----end parsing");

	fclose(fp);

	return 0;
}
*/