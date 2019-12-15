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
					if($1.val.str.length() > MAX_ATTRI_NAME_LEN){ // attribute name too long
						Global::errors.push_back($1.pos);
						YYABORT;
					}
					else
						memcpy($$.field.name, $1.val.str.data(), $1.val.str.length());
					$$.field.type = $2.val.type;
					$$.field.length = DataType::lengthOf($2.val.type, *(uint*)$2.val.bytes);
				}
			|	IDENTIFIER type NOT KW_NULL
				{
					printf("not null\n");
					if($1.val.str.length() > MAX_ATTRI_NAME_LEN){ // attribute name too long
						Global::errors.push_back($1.pos);
						YYABORT;
					}
					else
						memcpy($$.field.name, $1.val.str.data(), $1.val.str.length());
					$$.field.type = $2.val.type;
					$$.field.length = DataType::lengthOf($2.val.type, *(uint*)$2.val.bytes);
					$$.field.nullable = false;
				}
			|	IDENTIFIER type DEFAULT value
				{
					printf("default\n");
					if($1.val.str.length() > MAX_ATTRI_NAME_LEN){ // attribute name too long
						Global::errors.push_back($1.pos);
						YYABORT;
					}
					else
						memcpy($$.field.name, $1.val.str.data(), $1.val.str.length());
					$$.field.type = $2.val.type;
					$$.field.length = DataType::lengthOf($2.val.type, *(uint*)$2.val.bytes);
					// decide subtype
					if($4.val.type == DataType::NONE){ // value is null
						// do nothing, $$.field.defaultValue will be nullptr
					}
					if($4.val.type == DataType::INT && $2.val.type == DataType::BIGINT){
						*(ll*)$$.val.bytes = *(int*)$4.val.bytes;
						$$.field.defaultValue = &$$.val;
					}
					else if($4.val.type == DataType::INT && $2.val.type == DataType::FLOAT){
						*(float*)$$.val.bytes = *(int*)$4.val.bytes;
						$$.field.defaultValue = &$$.val;
					}
					else if($4.val.type == DataType::FLOAT && $2.val.type == DataType::NUMERIC){
						uint pns = *(uint*)$2.val.bytes;
						int byteNum = DataType::lengthOf(DataType::NUMERIC, pns);
						DataType::floatToBin(*(float*)$2.val.bytes < 0, $2.val.str.data(), $2.val.str.find('.'), $$.val.bytes, pns >> 8, pns & 0xff);
						$$.field.defaultValue = &$$.val;
					}
					else if($4.val.type == DataType::INT && $2.val.type == DataType::NUMERIC){
						uint pns = *(uint*)$2.val.bytes;
						int byteNum = DataType::lengthOf(DataType::NUMERIC, pns);
						DataType::floatToBin(*(int*)$2.val.bytes < 0, $2.val.str.data(), $2.val.str.length(), $$.val.bytes, pns >> 8, pns & 0xff);
						$$.field.defaultValue = &$$.val;
					}
					else if($4.val.type == DataType::BIGINT && $2.val.type == DataType::NUMERIC){
						uint pns = *(uint*)$2.val.bytes;
						int byteNum = DataType::lengthOf(DataType::NUMERIC, pns);
						DataType::floatToBin(*(ll*)$2.val.bytes < 0, $2.val.str.data(), $2.val.str.length(), $$.val.bytes, pns >> 8, pns & 0xff);
						$$.field.defaultValue = &$$.val;
					}
					else if($4.val.type == DataType::CHAR && $2.val.type == DataType::VARCHAR){
						$$.val.str = $4.val.str;
						$$.field.defaultValue = &$$.val;
					}
					else if($4.val.type == DataType::CHAR && $2.val.type == DataType::CHAR && $4.val.str.length() <= *(int*)$2.val.bytes){
						$$.val.str = $4.val.str;
						$$.field.defaultValue = &$$.val;
					}
					else if($4.val.type == DataType::VARCHAR && $2.val.type == DataType::VARCHAR && $4.val.str.length() <= *(int*)$2.val.bytes){
						$$.val.str = $4.val.str;
						$$.field.defaultValue = &$$.val;
					}
					else if($4.val.type == $2.val.type){
						if($2.val.type == DataType::INT){
							*(int*)$$.val.bytes = *(int*)$4.val.bytes;
							$$.field.defaultValue = &$$.val;
						}
						else if($2.val.type == DataType::BIGINT){
							*(ll*)$$.val.bytes = *(ll*)$4.val.bytes;
							$$.field.defaultValue = &$$.val;
						}
						else if($2.val.type == DataType::FLOAT){
							*(float*)$$.val.bytes = *(float*)$4.val.bytes;
							$$.field.defaultValue = &$$.val;
						}
						else if($2.val.type == DataType::DATE){
							memcpy($$.val.bytes, $4.val.bytes, 3);
							$$.field.defaultValue = &$$.val;
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
// Type.val.bytes : 对于INT, DATE, FLOAT来说此字段无用; 对于VARCHAR, CHAR来说表示长度, 对于NUMERIC来说表示两个长度参数, 都将结果以int类型存到val处
// 长度都已经经过检查
type		:	KW_INT
				{
					printf("type int\n");
					$$.val.type = DataType::INT;
				}
			|	DATE
				{
					printf("type data\n");
					$$.val.type = DataType::DATE;
				}
			|	FLOAT
				{
					printf("type float\n");
					$$.val.type = DataType::FLOAT;
				}
			|	CHAR '(' INT_LIT ')'
				{
					printf("char\n");
					$$.val.type = DataType::CHAR;
					int intVal = 0;
					uchar code = Field::strToInt($3.val.str, intVal);
					if(code != 0 || intVal < 0 || intVal > 255){
						Global::errors.push_back($3.pos);
						YYABORT;
					}
					else
						*(int*)$$.val.bytes = intVal;
				}
			|	VARCHAR	'(' INT_LIT ')'
				{
					printf("varchar\n");
					$$.val.type = DataType::VARCHAR;
					int intVal = 0;
					uchar code = Field::strToInt($3.val.str, intVal);
					if(code != 0 || intVal < 0 || intVal > 65535){
						Global::errors.push_back($3.pos);
						YYABORT;
					}
					else
						*(int*)$$.val.bytes = intVal;
				}
			|	NUMERIC '(' INT_LIT ',' INT_LIT ')'
				{
					printf("numeric\n");
					$$.val.type = DataType::NUMERIC;
					int intVal = 0, intVal0 = 0;
					uchar code = Field::strToInt($3.val.str, intVal), code0 = Field::strToInt($5.val.str, intVal0);
					if(code != 0 || intVal < 1 || intVal > 38){
						Global::errors.push_back($3.pos);
						YYABORT;
					}
					else if(code0 != 0 || intVal0 < 0 || intVal0 > intVal){
						Global::errors.push_back($5.pos);
						YYABORT;
					}
					*(uint*)$$.val.bytes = (intVal << 8) + intVal0; // p, s are stored in val
				}
			;

// insert into {tablename} values(valueList), (valueList), (valueList) ...
valueLists	:	'(' valueList ')'
				{
					printf("valueLists base\n");
					$$.valLists.push_back($2.valList);
				}
			|	valueLists ',' '(' valueList ')'
				{
					printf("valueLists recur\n");
					$$.valLists = $$.valLists;
					$$.valLists.push_back($4.valList);
				}
			;

valueList	:	value
				{
					printf("valueList base\n");
					$$.valList.push_back($1.val);
				}
			|	valueList ',' value
				{
					printf("valueList recur\n");
					$$.valList = $1.valList;
					$$.valList.push_back($1.val);
				}
			;

value		:	INT_LIT
				{
					printf("int lit\n");
					int intVal = 0;
					ll llVal = 0;
					uchar code = Field::strToInt($1.val.str, intVal);
					if(code == 0){
						*(int*)$$.val.bytes = intVal;
						$$.val.type = DataType::INT;
					}
					else{
						code = Field::strToLL($1.val.str, llVal);
						if(code != 0){
							Global::errors.push_back($1.pos);
							YYABORT;
						}
						else{
							*(ll*)$$.val.bytes = llVal;
							$$.val.type = DataType::BIGINT;
						}
					}
				}
			|	FLOAT_LIT
				{
					printf("float lit\n");
					$$.val.type = DataType::FLOAT;
					float floatVal = 0;
					uchar code = Field::strToFloat($1.val.str, floatVal);
					if(code != 0){
						Global::errors.push_back($1.pos);
						YYABORT;
					}
					else
						*(float*)$$.val.bytes = floatVal;
				}
			|	DATE_LIT
				{
					printf("date lit\n");
					$$.val.type = DataType::DATE;
					int ym_sep = $1.val.str.find('-'), md_sep = $1.val.str.rfind('-');
					int year = 0, month = 0, day = 0;
					for(int i = 0; i < ym_sep; i++)
						year = year * 10 + $1.val.str[i] - '0';
					for(int i = ym_sep + 1; i < md_sep; i++)
						month = month * 10 + $1.val.str[i] - '0';
					for(int i = md_sep + 1; i < $1.val.str.length(); i++)
						day = day * 10 + $1.val.str[i] - '0';
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
						DataType::dateToBin(year, month, day, $$.val.bytes);
					else{
						Global::errors.push_back($1.pos);
						YYABORT;
					}
				}
			| 	STRING_LIT
				{
					printf("string lit\n");
					if($1.val.str.length() <= 255)
						$$.val.type = DataType::CHAR;
					else if($1.val.str.length() <= 65535)
						$$.val.type = DataType::VARCHAR;
					else{
						Global::errors.push_back($1.pos);
						YYABORT;
					}
				}
			|	KW_NULL
				{
					printf("null lit\n");
					$$.val.type = DataType::NONE; // use NONE to record null lit
				}
			| 	'-' value // minus: for float, int and bigint
				{
					if($2.val.type == DataType::FLOAT){
						*(float*)$2.val.bytes = -*(float*)$2.val.bytes;
					}
					else if($2.val.type == DataType::INT){
						*(int*)$2.val.bytes = -*(int*)$2.val.bytes;
					}
					else if($2.val.type == DataType::BIGINT){
						*(ll*)$2.val.bytes = -*(ll*)$2.val.bytes;
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
					WhereInstr tmp;
					tmp.column = $1.column;
					tmp.isExprCol = $3.isExprCol;
					tmp.cmp = $2.cmp;
					if(tmp.isExprCol)
						tmp.exprCol = $3.column;
					else
						tmp.exprVal = $3.val;
					$$.condList.push_back(tmp);
				}
			|	col IS KW_NULL
				{
					printf("if null\n");
					WhereInstr tmp;
					tmp.column = $1.column;
					tmp.cmp = Comparator::Eq;		   // equals
					tmp.exprVal.type = DataType::NONE; // null
					$$.condList.push_back(tmp);
				}
			|	col IS NOT KW_NULL
				{
					printf("if not null\n");
					WhereInstr tmp;
					tmp.column = $1.column;
					tmp.cmp = Comparator::NE;		   // not equals
					tmp.exprVal.type = DataType::NONE; // null
					$$.condList.push_back(tmp);
				}
			|	whereClause AND whereClause
				{
					printf("if and\n");
					$$.condList = $1.condList;
					$$.condList.insert($$.condList.end(), $3.condList.begin(), $3.condList.end());
				}
			;

col			:	IDENTIFIER
				{
					printf("simple col\n");
					if($1.val.str.length() > MAX_ATTRI_NAME_LEN){
						Global::errors.push_back($1.pos);
						YYABORT;
					}
					$$.column.colName = $1.val.str;
				}
			|	IDENTIFIER '.' IDENTIFIER
				{
					printf("compl col\n");
					if($1.val.str.length() > MAX_TABLE_NAME_LEN){
						Global::errors.push_back($1.pos);
						YYABORT;
					}
					$$.column.tableName = $1.val.str;
					if($3.val.str.length() > MAX_ATTRI_NAME_LEN){
						Global::errors.push_back($3.pos);
						YYABORT;
					}
					$$.column.colName = $3.val.str;
				}
			;

op			:	GE
				{
					printf("GE\n");
					$$.cmp = Comparator::GtEq;
				}
			|	LE
				{
					printf("LE\n");
					$$.cmp = Comparator::LtEq;
				}
			|	NE
				{
					printf("NE\n");
					$$.cmp = Comparator::NE;
				}
			|	'>'
				{
					printf("Gt\n");
					$$.cmp = Comparator::Gt;
				}
			|	'<'
				{
					printf("Lt\n");
					$$.cmp = Comparator::Lt;
				}
			|	'='
				{
					printf("eq\n");
					$$.cmp = Comparator::Eq;
				}
			;

expr		:	value
				{
					printf("value expr\n");
					$$.isExprCol = false;
					$$.val = $1.val;
				}
			|	col
				{
					printf("col expr\n");
					$$.isExprCol = true;
					$$.column = $1.column;
				}
			;

setClause	:	IDENTIFIER '=' value
				{
					printf("set\n");
					$$.setList.push_back(SetInstr($1.val.str, $3.val));
				}
			|	setClause ',' IDENTIFIER '=' value
				{
					printf("multiple set\n");
					$$.setList = $1.setList;
					$$.setList.push_back(SetInstr($3.val.str, $5.val));
				}
			;

selector	:	'*'
				{
					printf("select *\n");
					$$.selectAll = true;
				}
			|	colList
				{
					printf("select colList\n");
					$$.colList = $1.colList;
				}
			;

colList		:	col
				{
					printf("colList base\n");
					$$.colList.push_back($1.column);
				}
			| 	colList ',' col
				{
					printf("colList recur\n");
					$$.colList = $1.colList;
					$$.colList.push_back($3.column);
				}
			;

IdList		:	IDENTIFIER
				{
					printf("IdList base\n");
					$$.IDList.push_back($1.val.str);
				}
			|	IdList ',' IDENTIFIER
				{
					printf("IdList recur\n");
					$$.IDList = $1.IDList;
					$$.IDList.push_back($3.val.str);
				}
			;

%%
void yyerror(const char *s)			//当yacc遇到语法错误时，会回调yyerror函数，并且把错误信息放在参数s中
{
	printf("Error: %s\n", s);					//直接输出错误信息
}