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
%token 	ON			TO
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
					Scanner* scanner = Global::dbms->ShowDatabases();
					Record rec;
					while(scanner->NextRecord(&rec)){
						printf("%s.%d\n", rec.GetData(), MAX_DB_NAME_LEN);
						rec.FreeMemory();
					}
					scanner->Reset();
				}
			;

DbStmt		:	CREATE DATABASE IDENTIFIER
				{
					printf("create db\n");
					if($3.val.str.length() > MAX_DB_NAME_LEN){
						Global::newError($3.pos, Global::format("Database name should be no longer than %d", MAX_DB_NAME_LEN));
						YYABORT;
					}
					bool createRet = Global::dbms->CreateDatabase($3.val.str.data());
					if(!createRet){
						Global::newError($3.pos, "Database with the same name already exists");
						YYABORT;
					}
				}
			|	DROP DATABASE IDENTIFIER
				{
					printf("drop db\n");
					if($3.val.str.length() > MAX_DB_NAME_LEN){
						Global::newError($3.pos, Global::format("Database name should be no longer than %d", MAX_DB_NAME_LEN));
						YYABORT;
					}
					bool dropRet = Global::dbms->DropDatabase($3.val.str.data());
					if(!dropRet){ // no such database
						Global::newError($3.pos, Global::format("No database named %s", $3.val.str.data()));
						YYABORT;
					}
				}
			|	USE IDENTIFIER
				{
					printf("use db\n");
					if($2.var.str.length() > MAX_DB_NAME_LEN){
						Global::newError($2.pos, Global::format("Database name should be no longer than %d", MAX_DB_NAME_LEN));
						YYABORT;
					}
					Database* useRet = Global::dbms->UseDatabase($2.val.str.data());
					if(!useRet){
						Global::newError($2.pos, Global::format("No database named %s", $2.val.str.data()));
						YYABORT;
					}
				}
			|	SHOW TABLES
				{
					printf("show tb\n");
					Database* curDB = Global::dbms->CurrentDatabase();
					if(!curDB){
						Global::newError($1.pos, "No database in use");
						YYABORT;
					}
					Scanner* scanner = curDB->ShowTables();
					Record rec;
					while(scanner->NextRecord(&rec)){
						printf("%s.%d\n", rec.GetData(), MAX_TABLE_NAME_LEN);
						rec.FreeMemory();
					}
					scanner->Reset();
				}
			;

TbStmt		:	CREATE TABLE IDENTIFIER '(' fieldList ')'
				{
					printf("create tb\n");
					if($5.fieldList.length() > MAX_COL_NUM){
						Global::newError($5.pos, Global::format("Number of columns in a tables should be not more than %d", MAX_COL_NUM));
						YYABORT;
					}
					if(Global::dbms->CurrentDatabase() == nullptr){
						Global::newError($1.pos, "No available database");
						YYABORT;
					}
					if(Global::dbms->CurrentDatabase()->TableExists($3.val.str.data())){
						Global::newError($3.pos, Global::format("Another table named %s already exists", $3.val.str.data()));
						YYABORT;
					}
					std::set<std::string> allNames;
					Header header;
					int pos = 0;
					for(auto it = $5.fieldList.begin(); it != $5.fieldList.end(); it++){
						if(!allNames.insert(*it).second){
							Global::newError($5.pos, Global::format("Field name %s conflicts with previous field name", it->name.data()));
							YYABORT;
						}
						// update header
						header->recordLenth += DataType::lengthOf(it->type, it->length);
						if(it->nullable)
							header.nullMask |= 1 << (31 - pos);
						if(it->hasDefault)
							header.defaultMask |= 1 << (31 - pos);
						header.attrLenth[pos] = it->length;
						header.attrType[pos] = it->type;
						memcpy(header.attrName[pos], it->name.data(), it->name.length());
						pos++;
					}
					header.recordLenth += 4; // TODO: the null word?
					header.slotNum = PAGE_SIZE / header.recordLenth;
					// handle constraints
					bool hasPrimary = false;
					// lambda function, get the index of a given name
					auto getNameIndex = [=](const std::string& target)->int{
						int ans = 0;
						for(auto it = $5.fieldList.begin(); it != %4.fieldList.end(); it++){
							if(it->name == target)
								return ans;
							ans++;
						}
						return -1;
					};
					// number of foreign key constraints
					int fkIndex = 0;
					for(auto it = $5.constraintList.begin(); it != $5.constraintList.end(); it++){
						// multiple primary keys
						if(hasPrimary && !it->isFK){
							Global::newError(%5.pos, "Multiple primary key declaration");
							YYABORT;
						}
						// check names
						if(!it->isFK) { // primary key constraint, all IDs need to be declared
							hasPrimary = true;
							for(auto id_it = it->IDList.begin(); id_it != it->IDList.end(); id_it++){
								if(!allNames.count(*id_it)){
									Global::newError($5.pos, Global::format("Undeclared field name %s in constraint", (*id_it).data()));
									YYABORT;
								}
								// update header
								header.primaryKeyMask |= 1 << (31 - getNameIndex(*id_it));
							}
						}
						else{ // foreign key constraint(single key)  IDList = {slaveCol, masterTable, masterCol}
							// check slaveCol name
							if(!allNames.count(it->IDList[0])){
								Global::newError($5.pos, Global::format("Undeclared field name %s in constraint", it->IDList[0].data()));
								YYABORT;
							}
							// check masterTable name and masterCol name
							Table* master = Global::dbms->CurrentDatabase()->OpenTable(it->IDList[1]);
							if(!master){ // no such table
								Global::newError($5.pos, Global::format("No table named %s under database %s", it->IDList[1].data(), Global::dbms->CurrentDatabase()->GetName()));
								YYABORT;
							}
							int masterColIndex = master->IDofCol(it->IDList[2].data());
							if(masterColIndex == COL_ID_NONE){
								Global::newError($5.pos, Global::format("No field named %s in table %s", it->IDList[2].data(), it->IDList[1].data()));
								YYABORT;
							}
							if(fkIndex == MAX_REF_SLAVE_TIME){
								Global::newError($5.pos, Global::format("A table can reference other tables in foreign key constraint no more than %d times", MAX_REF_SLAVE_TIME));
								YYABORT;
							}
							// update header, this fk constraint is anonymous
							header.fkMaster[fkIndex] = master->GetTableID();
							header.masterKeyID = 1 << (31 - masterColIndex);
							header.slaveKeyID = 1 << (31 - getNameIndex(it->IDList[0]));
							fkIndex++;
							// TODO: update the master table's fk info ?
						}
					}
					// build default record
					uchar dftRec[header.recordLenth] = {0};
					int dftBufPos = 4; // skip the null word
					int dftIndex = 0;
					for(auto it = $5.fieldList.begin(); it != $5.fieldList.end(); it++, dftIndex++){
						int fieldLength = DataType::lengthOf(it->type, it->length);
						if(it->hasDefault){
							if(it->defaultValue->type == DataType::NONE){ // special case, default value is null
								dftRec[dftIndex >> 3] |= 128 >> (dftIndex & 7); // set null word
							}
							if(it->type == DataType::CHAR){
								memcpy(dftBufPos, it->defaultValue->str.data(), it->defaultValue->str.length());
							}
							else if(it->type == DataType::VARCHAR){
								if(it->length <= 255)
									memcpy(dftBufPos, it->defaultValue->str.data(), it->defaultValue->str.length());
								else{ // long varchar
									RID varcharRID;
									Global::dbms->InsertLongVarchar(it->defaultValue->str.data(), it->defaultValue->str.length(), &varcharRID);
									*(uint*)(dftRec + dftBufPos) = varcharRID.GetPageNum();
									*(uint*)(dftRec + dftBufPos + 4) = varcharRID.GetSlotNum();
								}
							}
							else{
								memcpy(dftBufPos, it->defaultValue->bytes, fieldLength);
							}
						}
						dftBufPos += fieldLength;
					}
					if(!Global::dbms->CurrentDatabase()->CreateTable($3.val.str.data(), &header, dftRec))
						printf("Unknown error in creating table\n");
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
			|	ALTER TABLE IDENTIFIER DROP IDENTIFIER
				{
					printf("alter drop col\n");
				}
			|	ALTER TABLE IDENTIFIER CHANGE IDENTIFIER field
				{
					printf("alter change field\n");
				}
			|	ALTER TABLE IDENTIFIER RENAME TO IDENTIFIER
				{
					printf("alter rename\n");
				}
			|	ALTER TABLE IDENTIFIER ADD PRIMARY KEY '(' colList ')'
				{
					printf("alter add primary\n");
				}
			|	ALTER TABLE IDENTIFIER DROP PRIMARY KEY
				{
					printf("alter drop primary\n");
				}
			|	ALTER TABLE IDENTIFIER ADD CONSTRAINT IDENTIFIER FOREIGN KEY '(' colList ')' REFERENCES IDENTIFIER '(' colList ')'
				{
					printf("alter add foreign\n");
				}
			|	ALTER TABLE IDENTIFIER DROP FOREIGN KEY IDENTIFIER
				{
					printf("alter drop foreign\n");
				}
			;
			// we do not support:
			// alter table {tablename} add constraint {primarykeyname} primary key ({col}...)
			// alter table {tablename} drop primary key {primarykeyname}

// fieldList 是建表时使用的
fieldList	:	fieldList ',' field
				{
					printf("fieldList recur\n");
					$$.fieldList = $1.fieldList;
					$$.constraintList = $1.constraintList;
					if($3.constraintList.empty())
						$$.fieldList.push_back($3.field);
					else
						$$.constraintList.push_back($3.constraintList[0]);
				}
			|	field
				{
					printf("fieldList base\n");
					if($1.constraintList.empty())
						$$.fieldList.push_back($1.field);
					else
						$$.constraintList.push_back($3.constraintList[0]);
				}
			;

// field可以组成fieldList用于建表,也可以在alter add里面用于增加列
// 名字长度已经经过检查
field		:	IDENTIFIER type
				{
					printf("field normal\n");
					if($1.val.str.length() > MAX_ATTRI_NAME_LEN){ // attribute name too long
						Global::newError($1.pos, Global::format("Field name should be no longer than %d", MAX_ATTRI_NAME_LEN));
						YYABORT;
					}
					else
						$$.field.name = $1.val.str;
					$$.field.type = $2.val.type;
					$$.field.length = DataType::lengthOf($2.val.type, *(uint*)$2.val.bytes);
				}
			|	IDENTIFIER type NOT KW_NULL
				{
					printf("field not null\n");
					if($1.val.str.length() > MAX_ATTRI_NAME_LEN){ // attribute name too long
						Global::newError($1.pos, Global::format("Field name should be no longer than %d", MAX_ATTRI_NAME_LEN));
						YYABORT;
					}
					else
						$$.field.name = $1.val.str;
					$$.field.type = $2.val.type;
					$$.field.length = DataType::lengthOf($2.val.type, *(uint*)$2.val.bytes);
					$$.field.nullable = false;
				}
			|	IDENTIFIER type DEFAULT value
				{
					printf("field default\n");
					if($1.val.str.length() > MAX_ATTRI_NAME_LEN){ // attribute name too long
						Global::newError($1.pos, Global::format("Field name should be no longer than %d", MAX_ATTRI_NAME_LEN));
						YYABORT;
					}
					else
						$$.field.name = $1.val.str;
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
						Global::newError($4.pos, "Default value and field type are incompatible");
						YYABORT;
					}
					$$.field.hasDefault = true;
				}
			|	IDENTIFIER type NOT KW_NULL DEFAULT value
				{
					printf("not null default\n");
					if($1.val.str.length() > MAX_ATTRI_NAME_LEN){ // attribute name too long
						Global::newError($1.pos, Global::format("Field name should be no longer than %d", MAX_ATTRI_NAME_LEN));
						YYABORT;
					}
					else
						$$.field.name = $1.val.str;
					$$.field.type = $2.val.type;
					$$.field.length = DataType::lengthOf($2.val.type, *(uint*)$2.val.bytes);
					// decide subtype
					if($6.val.type == DataType::NONE){ // value is null, which conflicts with not null constraint
						Global::newError($6.pos, "Default value for non-null field cannot be null");
						YYABORT;
					}
					if($6.val.type == DataType::INT && $2.val.type == DataType::BIGINT){
						*(ll*)$$.val.bytes = *(int*)$6.val.bytes;
						$$.field.defaultValue = &$$.val;
					}
					else if($6.val.type == DataType::INT && $2.val.type == DataType::FLOAT){
						*(float*)$$.val.bytes = *(int*)$6.val.bytes;
						$$.field.defaultValue = &$$.val;
					}
					else if($6.val.type == DataType::FLOAT && $2.val.type == DataType::NUMERIC){
						uint pns = *(uint*)$2.val.bytes;
						int byteNum = DataType::lengthOf(DataType::NUMERIC, pns);
						DataType::floatToBin(*(float*)$2.val.bytes < 0, $2.val.str.data(), $2.val.str.find('.'), $$.val.bytes, pns >> 8, pns & 0xff);
						$$.field.defaultValue = &$$.val;
					}
					else if($6.val.type == DataType::INT && $2.val.type == DataType::NUMERIC){
						uint pns = *(uint*)$2.val.bytes;
						int byteNum = DataType::lengthOf(DataType::NUMERIC, pns);
						DataType::floatToBin(*(int*)$2.val.bytes < 0, $2.val.str.data(), $2.val.str.length(), $$.val.bytes, pns >> 8, pns & 0xff);
						$$.field.defaultValue = &$$.val;
					}
					else if($6.val.type == DataType::BIGINT && $2.val.type == DataType::NUMERIC){
						uint pns = *(uint*)$2.val.bytes;
						int byteNum = DataType::lengthOf(DataType::NUMERIC, pns);
						DataType::floatToBin(*(ll*)$2.val.bytes < 0, $2.val.str.data(), $2.val.str.length(), $$.val.bytes, pns >> 8, pns & 0xff);
						$$.field.defaultValue = &$$.val;
					}
					else if($6.val.type == DataType::CHAR && $2.val.type == DataType::VARCHAR){
						$$.val.str = $6.val.str;
						$$.field.defaultValue = &$$.val;
					}
					else if($6.val.type == DataType::CHAR && $2.val.type == DataType::CHAR && $6.val.str.length() <= *(int*)$2.val.bytes){
						$$.val.str = $6.val.str;
						$$.field.defaultValue = &$$.val;
					}
					else if($6.val.type == DataType::VARCHAR && $2.val.type == DataType::VARCHAR && $6.val.str.length() <= *(int*)$2.val.bytes){
						$$.val.str = $6.val.str;
						$$.field.defaultValue = &$$.val;
					}
					else if($6.val.type == $2.val.type){
						if($2.val.type == DataType::INT){
							*(int*)$$.val.bytes = *(int*)$6.val.bytes;
							$$.field.defaultValue = &$$.val;
						}
						else if($2.val.type == DataType::BIGINT){
							*(ll*)$$.val.bytes = *(ll*)$6.val.bytes;
							$$.field.defaultValue = &$$.val;
						}
						else if($2.val.type == DataType::FLOAT){
							*(float*)$$.val.bytes = *(float*)$6.val.bytes;
							$$.field.defaultValue = &$$.val;
						}
						else if($2.val.type == DataType::DATE){
							memcpy($$.val.bytes, $6.val.bytes, 3);
							$$.field.defaultValue = &$$.val;
						}
						// a value will only be parsed as float, not numeric
						// varchar and char are solved previously
					}
					else{ // wrong
						Global::newError($6.pos, "Default value and field type are incompatible");
						YYABORT;
					}
					$$.field.hasDefault = true;
				}
			|	PRIMARY KEY '(' IdList ')'
				{
					printf("primary key\n");
					Constraint tmp;
					tmp.IDList = $4.IDList;
					tmp.isFK = false;
					$$.constraintList.push_back(tmp);
				}
			|	FOREIGN KEY '(' IDENTIFIER ')' REFERENCES IDENTIFIER '(' IDENTIFIER ')' // 建表时创建外键约束,只能建立1对1的映射; 多对多需要使用alter命令
				{
					printf("foreign\n");
					Constraint tmp;
					tmp.IDList.push_back($4.val.str);
					tmp.IDList.push_back($7.val.str);
					tmp.IDList.push_back($9.val.str);
					tmp.isFK = true;
					$$.constraintList.push_back(tmp);
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
						Global::newError($3.pos, "Length for type CHAR should be between 0 and 255");
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
						Global::newError($3.pos, "Length for type VARCHAR should be between 0 and 65535");
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
						Global::newError($3.pos, "NUMERIC precision should be between 1 and 38");
						YYABORT;
					}
					else if(code0 != 0 || intVal0 < 0 || intVal0 > intVal){
						Global::newError($5.pos, "NUMERIC scale should be between 0 and NUMERIC precision");
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
							Global::newError($1.pos, "Value cannot be converted to INT");
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
						Global::newError($1.pos, "Value cannot be converted to FLOAT");
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
						Global::newError($1.pos, "Wrong format for DATE type");
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
						Global::newError($1.pos, "String is too long, it should be no longer than 65535 bytes");
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
						Global::newError($1.pos, "Can only apply '-' to INT, BIGINT and FLOAT type");
						YYABORT;
					}
				}
			;

whereClause :	whereClause AND whereUnit
				{
					printf("where recur\n");
					$$.condList = $1.condList;
					$$.condList.push_back($3.condList[0]);
				}
			|	whereUnit
				{
					printf("where base\n");
					$$.condList.push_back($3.condList[0]);
				}

whereUnit	:	col op expr
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
			;

col			:	IDENTIFIER
				{
					printf("simple col\n");
					if($1.val.str.length() > MAX_ATTRI_NAME_LEN){
						Global::newError($1.pos, Global::format("Field name should be no longer than %d", MAX_ATTRI_NAME_LEN));
						YYABORT;
					}
					$$.column.colName = $1.val.str;
				}
			|	IDENTIFIER '.' IDENTIFIER
				{
					printf("compl col\n");
					if($1.val.str.length() > MAX_TABLE_NAME_LEN){
						Global::newError($1.pos, Global::format("Table name should be no longer than %d", MAX_ATTRI_NAME_LEN));
						YYABORT;
					}
					$$.column.tableName = $1.val.str;
					if($3.val.str.length() > MAX_ATTRI_NAME_LEN){
						Global::newError($3.pos, Global::format("Field name should be no longer than %d", MAX_ATTRI_NAME_LEN));
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