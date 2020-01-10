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
%token	FOREIGN		REFERENCES	NUMERIC	ON
%token 	TO			EXIT		COPY	WITH
%token 	DELIMITER
// 以上是SQL关键字
%token 	INT_LIT		STRING_LIT	FLOAT_LIT	DATE_LIT
%token 	IDENTIFIER	GE			LE 			NE
%token	'+'	'-'	'*'	'/' '(' ')' '>' '<' '=' ',' '.' ';'

%%

Start		:	Stmt
				{
					// printf("From yacc: start parsing\n");
					if(!Global::errors.empty()){ // errors from lexer
						YYABORT;
					}
				}

Stmt		:	SysStmt ';'
				{
					printf("YACC: sys\n");
					// Global::types = $1.typeBuf;
					// Global::action = $1.action;
				}
			| 	DbStmt ';'
				{
					printf("YACC: db\n");
					// Global::types = $1.typeBuf;
					// Global::action = $1.action;
				}
			| 	TbStmt ';'
				{
					printf("YACC: tb\n");
					// Global::types = $1.typeBuf;
					// Global::action = $1.action;
				}
			|	 IdxStmt ';'
				{
					printf("YACC: idx\n");
					// Global::types = $1.typeBuf;
					// Global::action = $1.action;
				}
			| 	AlterStmt ';'
				{
					printf("YACC: alter\n");
					// Global::types = $1.typeBuf;
					// Global::action = $1.action;
				}
			| 	EXIT ';'
				{
					printf("YACC: exit\n");
					Global::exitSign = true;
				}
			;

SysStmt		:	SHOW DATABASES
				{
					printf("YACC: show db\n");
					Global::action = [](std::vector<Type> &typeVec)->bool{
						Scanner* scanner = Global::dbms->ShowDatabases();
						Record rec;
						while(scanner->NextRecord(&rec)){
							printf("%.*s\n", MAX_DB_NAME_LEN, rec.GetData() + 4); // null word
							rec.FreeMemory();
						}
						scanner->Reset();
						return true;
					};
				}
			;

DbStmt		:	CREATE DATABASE IDENTIFIER
				{
					printf("YACC: create db\n");
					Global::types.push_back($3);
					Global::action = [](std::vector<Type> &typeVec)->bool{
						Type &T3 = typeVec[0];
						if(T3.val.str.length() > MAX_DB_NAME_LEN){
							Global::DbNameTooLong(T3.pos);
							return false;
						}
						bool createRet = Global::dbms->CreateDatabase(T3.val.str.data());
						if(!createRet){
							Global::DbNameConflict(T3.pos);
							return false;
						}
						return true;
					};
				}
			|	DROP DATABASE IDENTIFIER
				{
					printf("YACC: drop db\n");
					Global::types.push_back($3);
					Global::action = [](std::vector<Type> &typeVec)->bool{
						Type& T3 = typeVec[0];
						if(T3.val.str.length() > MAX_DB_NAME_LEN){
							Global::DbNameTooLong(T3.pos);
							return false;
						}
						bool dropRet = Global::dbms->DropDatabase(T3.val.str.data());
						if(!dropRet){ // no such database
							Global::NoSuchDb(T3.pos, T3.val.str.data());
							return false;
						}
						return true;
					};
				}
			|	USE IDENTIFIER
				{
					printf("YACC: use db\n");
					Global::types.push_back($2);
					Global::action = [](std::vector<Type> &typeVec)->bool{
						Type &T2 = typeVec[0];
						if(T2.val.str.length() > MAX_DB_NAME_LEN){
							Global::DbNameTooLong(T2.pos);
							return false;
						}
						Database* useRet = Global::dbms->UseDatabase(T2.val.str.data());
						if(!useRet){
							Global::NoSuchDb(T2.pos, T2.val.str.data());
							return false;
						}
						return true;
					};
				}
			|	SHOW TABLES
				{
					printf("YACC: show tb\n");
					Global::types.push_back($1);
					Global::action = [](std::vector<Type> &typeVec)->bool{
						Type &T1 = typeVec[0];
						Database* curDB = Global::dbms->CurrentDatabase();
						if(!curDB){
							Global::NoActiveDb(T1.pos);
							return false;
						}
						Scanner* scanner = curDB->ShowTables();
						Record rec;
						while(scanner->NextRecord(&rec)){
							printf("%.*s\n", MAX_TABLE_NAME_LEN, rec.GetData() + 4); // null word
							rec.FreeMemory();
						}
						scanner->Reset();
						return true;
					};
				}
			|	COPY IDENTIFIER FROM STRING_LIT WITH '(' DELIMITER STRING_LIT ')'
				{
					printf("YACC: copy table\n");
					Global::types.push_back($1);
					Global::types.push_back($2);
					Global::types.push_back($4);
					Global::types.push_back($8);
					Global::action = [](std::vector<Type> &typeVec){
						Type &T1 = typeVec[0], &T2 = typeVec[1], &T4 = typeVec[2], &T8 = typeVec[3];
						if(Global::dbms->CurrentDatabase() == nullptr){
							Global::NoActiveDb(T1.pos);
							return false;
						}
						Table* table = Global::dbms->CurrentDatabase()->OpenTable(T2.val.str.data());
						if(table == nullptr){
							Global::NoSuchTable(T2.pos, T2.val.str.data());
							return false;
						}
						if(T8.val.str.length() != 1){
							Global::NotCharacter(T8.pos);
							return false;
						}
						if(!table->BatchLoad(T4.val.str.data(), T8.val.str[0])){
							Global::FileNotFound(T4.pos, T4.val.str.data());
							return false;
						}
						return true;
					};
				}
			;

// header.attrLenth是原始长度
TbStmt		:	CREATE TABLE IDENTIFIER '(' fieldList ')'
				{
					printf("YACC: create tb\n");
					Global::types.push_back($1);
					Global::types.push_back($3);
					Global::types.push_back($5);
					Global::action = [](std::vector<Type> &typeVec)->bool{
						Type &T1 = typeVec[0], &T3 = typeVec[1], &T5 = typeVec[2];
						if(T3.val.str.length() > MAX_TABLE_NAME_LEN){
							Global::TableNameTooLong(T3.pos);
							return false;
						}
						if(T5.fieldList.size() > MAX_COL_NUM){
							Global::TooManyFields(T5.pos);
							return false;
						}
						if(Global::dbms->CurrentDatabase() == nullptr){
							Global::NoActiveDb(T1.pos);
							return false;
						}
						if(Global::dbms->CurrentDatabase()->TableExists(T3.val.str.data())){
							Global::TableNameConflict(T3.pos);
							return false;
						}
						std::set<std::string> allNames;
						Header header;
						header.nullMask = 0; // the default value is 0xffffffff
						int pos = 0;
						for(auto it = T5.fieldList.begin(); it != T5.fieldList.end(); it++){
							if(it->name.length() > MAX_ATTRI_NAME_LEN){
								Global::FieldNameTooLong(T5.pos);
								return false;
							}
							if(!allNames.insert(it->name).second){
								Global::FieldNameConflict(T5.pos, it->name.data());
								return false;
							}
							// update header
							header.recordLenth += DataType::lengthOf(it->type, it->length); // 内存长度而非原始长度
							if(it->nullable)
								header.nullMask |= 1 << (31 - pos);
							if(it->hasDefault)
								header.defaultKeyMask |= 1 << (31 - pos);
							header.attrLenth[pos] = it->length; // 使用原始长度
							header.attrType[pos] = it->type;
							memcpy(header.attrName[pos], it->name.data(), it->name.length());
							pos++;
						}
						header.recordLenth += 4;
						header.slotNum = PAGE_SIZE / header.recordLenth;
						// handle constraints
						bool hasPrimary = false;
						// lambda function, get the index of a given name
						auto getNameIndex = [=](const std::string& target)->int{
							int ans = 0;
							for(auto it = T5.fieldList.begin(); it != T5.fieldList.end(); it++){
								if(it->name == target)
									return ans;
								ans++;
							}
							return -1;
						};
						// number of foreign key constraints
						int fkIndex = 0;
						// indexes of primary fields, used in creating primary index
						std::vector<uchar> primaryCols;
						for(auto it = T5.constraintList.begin(); it != T5.constraintList.end(); it++){ // 对于每个主键或者外键约束
							// multiple primary keys
							if(hasPrimary && !it->isFK){
								Global::MultiplePrimaryKey(T5.pos);
								return false;
							}
							std::set<std::string> IDListFields; // 用来检查主键和slave外键中是否有重复字段
							// check names
							if(!it->isFK) { // primary key constraint, all IDs need to be declared
								hasPrimary = true;
								for(auto id_it = it->IDList.begin(); id_it != it->IDList.end(); id_it++){
									if(!allNames.count(*id_it)){
										Global::NoSuchField(T5.pos, id_it->data());
										return false;
									}
									if(!IDListFields.insert(*id_it).second){
										Global::DuplicateFieldInConstraint(T5.pos);
										return false;
									}
									// update header
									uchar colIndex = getNameIndex(*id_it);
									clearBitFromLeft(header.nullMask, colIndex); // 字段可以不声明为not null,但仍在建表时声明为主键,这时自动加上not null约束
									header.primaryKeyMask |= 1 << (31 - colIndex);
									header.primaryKeyID[primaryCols.size()] = colIndex;
									primaryCols.push_back(colIndex);
								}
							}
							else{ // foreign key constraint
								// check for too many slave times
								if(fkIndex == MAX_REF_SLAVE_TIME){
									Global::TooManySlaveTime(T5.pos);
									return false;
								}
								if(it->IDList.size() != it->masterIDList.size()){
									Global::MasterFieldNumNotMatch(T5.pos);
									return false;
								}
								// check slaveCol names
								int slavePos = 0;
								for(auto slave_it = it->IDList.begin(); slave_it != it->IDList.end(); slave_it++){
									if(!allNames.count(*slave_it)){
										Global::NoSuchField(T5.pos, slave_it->data());
										return false;
									}
									if(!IDListFields.insert(*slave_it).second){
										Global::DuplicateFieldInConstraint(T5.pos);
										return false;
									}
									header.slaveKeyID[fkIndex][slavePos++] = getNameIndex(*slave_it);
								}
								// check masterTable name
								Table* master = Global::dbms->CurrentDatabase()->OpenTable(it->masterName.data());
								if(!master){ // no such table
									Global::NoSuchTable(T5.pos, it->masterName.data());
									return false;
								}
								// check master cols
								int primaryPos = 0;
								for(auto master_it = it->masterIDList.begin(); master_it != it->masterIDList.end(); master_it++){
									int masterColIndex = master->IDofCol(master_it->data());
									if(masterColIndex == COL_ID_NONE){
										Global::NoSuchField(T5.pos, master_it->data());
										return false;
									}
									if(master->GetHeader()->primaryKeyID[primaryPos] != masterColIndex){
										Global::MasterFieldNotPrimary(T5.pos);
										return false;
									}
									if(master->GetHeader()->attrType[masterColIndex] != header.attrType[header.slaveKeyID[fkIndex][primaryPos]] ||
										master->GetHeader()->attrLenth[masterColIndex] != header.attrLenth[header.slaveKeyID[fkIndex][primaryPos]]){
										Global::MasterTypeNotMatch(T5.pos);
										return false;
									}
									primaryPos++;
								}
								if(primaryPos < MAX_COL_NUM && master->GetHeader()->primaryKeyID[primaryPos] != COL_ID_NONE){
									Global::MasterFieldNotPrimary(T5.pos);
									return false;
								}
								// update header, this fk constraint is anonymous!!!
								header.fkMaster[fkIndex] = master->GetTableID();
								fkIndex++;
								Global::dbms->CurrentDatabase()->CloseTable(it->masterName.data());
							}
						}
						// build default record
						uchar dftRec[header.recordLenth] = {0};
						int dftBufPos = 4; // skip the null word
						int dftIndex = 0;
						for(auto it = T5.fieldList.begin(); it != T5.fieldList.end(); it++, dftIndex++){
							int fieldLength = DataType::lengthOf(it->type, it->length); // 使用内存长度而非原始长度
							if(it->hasDefault){
								if(it->defaultValue.type == DataType::NONE){ // special case, default value is null
									if(getBitFromLeft(header.nullMask, dftIndex)){
										Global::IncompatibleTypes(T5.pos, header.attrName[dftIndex]);
										return false;
									}
									setBitFromLeft(*(uint*)dftRec, dftIndex); // set null word
								}
								if(it->type == DataType::CHAR){
									memcpy(dftRec + dftBufPos, it->defaultValue.str.data(), it->defaultValue.str.length());
								}
								else if(it->type == DataType::VARCHAR){
									if(it->length <= 255)
										memcpy(dftRec + dftBufPos, it->defaultValue.str.data(), it->defaultValue.str.length());
									else{ // long varchar
										RID varcharRID;
										Global::dbms->CurrentDatabase()->InsertLongVarchar(it->defaultValue.str.data(), it->defaultValue.str.length(), &varcharRID);
										*(uint*)(dftRec + dftBufPos) = varcharRID.GetPageNum();
										*(uint*)(dftRec + dftBufPos + 4) = varcharRID.GetSlotNum();
									}
								}
								else{
									memcpy(dftRec + dftBufPos, it->defaultValue.bytes, fieldLength);
								}
							}
							dftBufPos += fieldLength;
						}
						uchar tableIdx = Global::dbms->CurrentDatabase()->CreateTable(T3.val.str.data(), &header, dftRec);
						if(tableIdx == TB_ID_NONE)
							printf("Unknown error in creating table\n");
						else{
							// update fk masters' info
							for(auto it = T5.constraintList.begin(); it != T5.constraintList.end(); it++){
								if(it->isFK){
									Table* master = Global::dbms->CurrentDatabase()->OpenTable(it->masterName.data());
									master->AddFKSlave(tableIdx);
									Global::dbms->CurrentDatabase()->CloseTable(it->masterName.data());
									// too many fkslaves?
									// TODO: record fkslaves as (table id, times), which requires MAX_TB_NUM * 2 bytes and make fkSlaves overflow almost impossible
								}
							}
							if(hasPrimary){ // create index on primary keys
								Table* newTable = Global::dbms->CurrentDatabase()->OpenTable(T3.val.str.data());
								int idxRet = newTable->CreateIndexOn(primaryCols, PRIMARY_RESERVED_IDX_NAME);
								assert(idxRet == 0);
								// Global::dbms->CurrentDatabase()->CloseTable(T3.val.str.data());
							}
						}
						return true;
					};
				}
			|	DROP TABLE IDENTIFIER
				{
					printf("YACC: drop tb\n");
					Global::types.push_back($1);
					Global::types.push_back($3);
					Global::action = [](std::vector<Type> &typeVec)->bool{
						Type &T1 = typeVec[0], &T3 = typeVec[1];
						if(Global::dbms->CurrentDatabase() == nullptr){
							Global::NoActiveDb(T1.pos);
							return false;
						}
						if(T3.val.str.length() > MAX_TABLE_NAME_LEN){
							Global::TableNameTooLong(T3.pos);
							return false;
						}
						if(!Global::dbms->CurrentDatabase()->DeleteTable(T3.val.str.data())){
							Global::NoSuchTable(T3.pos, T3.val.str.data());
							return false;
						}
						// TODO: update fk master info?
						// TODO: remove indexes?
						// TODO: cascade deletion in fkSlaves? or simply remove constraint?
						// TODO: remove long varchars?
						return true;
					};
				}
			|	DESC IDENTIFIER
				{
					printf("YACC: desc tb %s\n", $2.val.str.data());
					Global::types.push_back($1);
					Global::types.push_back($2);
					Global::action = [](std::vector<Type> &typeVec)->bool{
						Type &T1 = typeVec[0], &T2 = typeVec[1];
						if(Global::dbms->CurrentDatabase() == nullptr){
							Global::NoActiveDb(T1.pos);
							return false;
						}
						if(T2.val.str.length() > MAX_TABLE_NAME_LEN){
							Global::TableNameTooLong(T2.pos);
							return false;
						}
						Table* table = Global::dbms->CurrentDatabase()->OpenTable(T2.val.str.data());
						if(!table){
							Global::NoSuchTable(T2.pos, T2.val.str.data());
							return false;
						}
						table->Print();
						// Global::dbms->CurrentDatabase()->CloseTable(T2.val.str.data());
					};
				}
			|	INSERT INTO IDENTIFIER VALUES valueLists // valueLists = '(' valueList ')' (',' '(' valueList ')')*, 用于一次插入多条记录
				{
					printf("YACC: insert db\n");
					Global::types.push_back($$);
					Global::types.push_back($1);
					Global::types.push_back($3);
					Global::types.push_back($5);

					Global::action = [](std::vector<Type> &typeVec)->bool{
						Type &TT = typeVec[0], &T1 = typeVec[1], &T3 = typeVec[2], &T5 = typeVec[3];
						if(Global::dbms->CurrentDatabase() == nullptr){
							Global::NoActiveDb(T1.pos);
							return false;
						}
						if(T3.val.str.length() > MAX_TABLE_NAME_LEN){
							Global::TableNameTooLong(T3.pos);
							return false;
						}
						Table* table = Global::dbms->CurrentDatabase()->OpenTable(T3.val.str.data());
						if(!table){
							Global::NoSuchTable(T3.pos, T3.val.str.data());
							return false;
						}
						Record tmpRec;
						RID tmpRID(START_PAGE, 0);
						table->GetRecord(tmpRID, &tmpRec); // the default record
						const Header* header = table->GetHeader();
						for(auto list_it = T5.valLists.begin(); list_it != T5.valLists.end(); list_it++){
							int arg_pos = 0;
							for(auto arg_it = list_it->begin(); arg_it != list_it->end(); arg_it++){
								if(arg_pos == MAX_COL_NUM || header->attrType[arg_pos] == DataType::NONE){
									Global::FieldNumNotMatch(T5.pos, table->ColNum(), list_it->size());
									// Global::dbms->CurrentDatabase()->CloseTable(T3.val.str.data()); // remember to close table before exit
									return false;
								}
								if(!ParsingHelper::ConvertValue(TT.val, header->attrType[arg_pos], header->attrLenth[arg_pos], *arg_it, getBitFromLeft(header->nullMask, arg_pos))){
									Global::IncompatibleTypes(T5.pos, header->attrName[arg_pos]);
									// Global::dbms->CurrentDatabase()->CloseTable(T3.val.str.data());
									return false;
								}
								if(arg_it->type == DataType::NONE) // null value
									TT.val.type = DataType::NONE;
								else
									TT.val.type = header->attrType[arg_pos];
								arg_pos++;
								TT.valList.push_back(TT.val);
							}
							while(arg_pos < MAX_COL_NUM && header->attrType[arg_pos] != DataType::NONE){
								if(!getBitFromLeft(header->defaultKeyMask, arg_pos)){
									Global::NoDefaultValue(T5.pos, table->GetHeader()->attrName[arg_pos]);
									// Global::dbms->CurrentDatabase()->CloseTable(T3.val.str.data());
									return false;
								}
								else{
									if(getBitFromLeft(*tmpRec.GetData(), arg_pos)){ // default value is null
										TT.val.type = DataType::NONE;
									}
									else{
										memcpy(TT.val.bytes, tmpRec.GetData() + table->ColOffset(arg_pos), DataType::lengthOf(header->attrType[arg_pos], header->attrLenth[arg_pos]));
										// NOTE: if default value is long varchar -> insert a COPY of the long varchar
									}
									TT.valList.push_back(TT.val);
								}
							}
							TT.valLists.push_back(TT.valList);
							TT.valList.clear();
						}
						// calc primary key const length
						int idxRecLength = 4;
						bool primarySelection[MAX_COL_NUM] = {0};
						int primaryColCount = 0;
						for(int i = 0; i < MAX_COL_NUM; i++){
							uchar col_id = table->GetHeader()->primaryKeyID[i];
							if(col_id != COL_ID_NONE){
								primaryColCount++;
								primarySelection[col_id] = true;
								idxRecLength += DataType::constantLengthOf(table->GetHeader()->attrType[col_id], table->GetHeader()->attrLenth[col_id]);
							}
							else
								break;
						}
						// check primary key constraint
						// TODO: 检查插入多个元素时,元素之间不满足主键约束的情况
						BplusTree* primaryIdx = nullptr;
						if(primaryColCount){
							uint idxPage = table->GetPrimaryIndexPage();
							assert(idxPage);
							primaryIdx = new BplusTree(Global::dbms->CurrentDatabase()->idx, idxPage);
							Global::globalTree = primaryIdx;
							std::set<const uchar*, bool(*)(const uchar*, const uchar*)> primaryCheck([](const uchar* l, const uchar* r)->bool{
								return DataType::compareArr(l, r, Global::globalTree->header->attrType, Global::globalTree->header->attrLenth,
									Global::globalTree->colNum, Comparator::Lt, true, true);
							});
							const uchar* vals[primaryColCount] = {0};
							bool ok = true;
							for(auto list_it = TT.valLists.begin(); list_it != TT.valLists.end(); list_it++){ // for every insertion
								uchar* buf = new uchar[idxRecLength]{0};
								memset(vals, 0, sizeof(vals));
								for(int i = 0; i < primaryColCount; i++){
									uchar colID = primaryIdx->header->indexColID[i];
									uchar colType = table->GetHeader()->attrType[colID];
									if((*list_it)[colID].type == DataType::NONE)
										setBitFromLeft(*(uint*)buf, colID);
									else if(colType == DataType::CHAR || colType == DataType::VARCHAR)
										vals[colID] = (const uchar*)(*list_it)[colID].str.data();
									else
										vals[colID] = (*list_it)[colID].bytes;
								}
								ParsingHelper::extractFields(table->GetHeader(), primaryColCount, primarySelection, vals, buf, true, true);
								if(!primaryCheck.insert(buf).second){
									Global::PrimaryKeyConflict(T5.pos);
									ok = false;
									delete[] buf;
									break;
								}
								RID tmpRID;
								if(primaryIdx->SafeValueSearch(buf, &tmpRID)){
									Global::PrimaryKeyConflict(T5.pos);
									ok = false;
									break;
								}
							}
							for(auto check_it = primaryCheck.begin(); check_it != primaryCheck.end(); check_it++)
								delete *check_it;
							delete primaryIdx;
							if(!ok)
								return false;
						}
						// check foreign key constraints
						int masterCount = table->FKMasterNum();
						if(masterCount){
							// for every fk master
							for(int i = 0; i < masterCount; i++){
								uchar masterName[MAX_TABLE_NAME_LEN + 1] = {0};
								Global::dbms->CurrentDatabase()->GetTableName(table->GetHeader()->fkMaster[i], masterName);
								Table* master = Global::dbms->CurrentDatabase()->OpenTable((char*)masterName);
								uint idxPage = master->GetPrimaryIndexPage();
								assert(idxPage);
								BplusTree* tree = new BplusTree(Global::dbms->CurrentDatabase()->idx, idxPage);
								// variables used to build constant record
								const uchar* vals[table->ColNum()] = {0};
								bool selection[table->ColNum()] = {0};
								int colCount = tree->colNum;
								int constantLength = 4;
								for(int i = 0;i < colCount; i++){
									constantLength += DataType::constantLengthOf(tree->header->attrType[i], tree->header->attrLenth[i]);
									selection[tree->header->indexColID[i]] = true;
								}
								uchar constantRecord[constantLength] = {0};
								// for every record to be inserted, check its primary constaint in master
								for(auto record_it = TT.valLists.begin(); record_it != TT.valLists.end(); record_it++){
									memset(vals, 0, sizeof(vals));
									memset(constantRecord, 0, sizeof(constantRecord));
									for(int i = 0; i < colCount; i++){ // No.i indexed field
										uchar colID = tree->header->indexColID[i];
										if((*record_it)[colID].type == DataType::NONE)
											setBitFromLeft(*(uint*)constantRecord, i);
										else if(tree->header->attrType[i] == DataType::CHAR || tree->header->attrType[i] == DataType::VARCHAR)
											vals[colID] = (const uchar*)(*record_it)[colID].str.data();
										else
											vals[colID] = (*record_it)[colID].bytes;
									}
									ParsingHelper::extractFields(table->GetHeader(), colCount, selection, vals, constantRecord, true, true);
									if(!tree->SafeValueSearch(constantRecord, &tmpRID)){
										Global::ForeignKeyNotFound(T5.pos, masterName);
										delete tree;
										return false;
									}
								}
								delete tree;
							}
						}
						// now there is no error in input, start insertion
						int idxRecPos = 0;
						for(auto list_it = TT.valLists.begin(); list_it != TT.valLists.end(); list_it++){ // for every valList, build a record
							int arg_pos = 0;
							memset(tmpRec.GetData(), 0, 4); // set null word to 0
							for(auto arg_it = list_it->begin(); arg_it != list_it->end(); arg_it++){ // for every field, perpare its value
								if(arg_it->type == DataType::NONE) // null value
									setBitFromLeft(*(uint*)(tmpRec.GetData()), arg_pos);
								else if(header->attrType[arg_pos] == DataType::VARCHAR && header->attrLenth[arg_pos] > 255){ // long varchar
									uchar varcharBuf[header->attrLenth[arg_pos]] = {0};
									ushort len = 0;
									RID dftRID = RID(*(uint*)arg_it->bytes, *(uint*)(arg_it->bytes + 4));
									Global::dbms->CurrentDatabase()->GetLongVarchar(dftRID, varcharBuf, len);
									Global::dbms->CurrentDatabase()->InsertLongVarchar((const char*)varcharBuf, len, &dftRID);
									*(uint*)(tmpRec.GetData() + table->ColOffset(arg_pos)) = dftRID.GetPageNum();
									*(uint*)(tmpRec.GetData() + table->ColOffset(arg_pos) + 4) = dftRID.GetSlotNum();
								}
								else if(header->attrType[arg_pos] == DataType::VARCHAR || header->attrType[arg_pos] == DataType::CHAR){
									memcpy(tmpRec.GetData() + table->ColOffset(arg_pos), arg_it->str.data(), arg_it->str.length());
									memset(tmpRec.GetData() + table->ColOffset(arg_pos) + arg_it->str.length(), 0, table->GetHeader()->attrLenth[arg_pos] - arg_it->str.length());
								}
								else
									memcpy(tmpRec.GetData() + table->ColOffset(arg_pos), arg_it->bytes, DataType::lengthOf(header->attrType[arg_pos], header->attrLenth[arg_pos]));
								arg_pos++;
							}
							table->InsertRecord(tmpRec.GetData(), tmpRec.GetRid());
							// update indexes
							ParsingHelper::InsertIndexes(table, tmpRec);
						}
						return true;
					};
				}
			|	DELETE FROM IDENTIFIER WHERE whereClause
				{
					printf("YACC: delete tb\n");
					Global::types.push_back($1);
					Global::types.push_back($3);
					Global::types.push_back($5);
					Global::action = [](std::vector<Type> &typeVec){
						Type &T1 = typeVec[0], &T3 = typeVec[1], &T5 = typeVec[2];
						if(!Global::dbms->CurrentDatabase()){
							Global::NoActiveDb(T1.pos);
							return false;
						}
						if(T3.val.str.length() > MAX_TABLE_NAME_LEN){
							Global::TableNameTooLong(T3.pos);
							return false;
						}
						Table* table = Global::dbms->CurrentDatabase()->OpenTable(T3.val.str.data());
						if(!table){
							Global::NoSuchTable(T3.pos, T3.val.str.data());
							return false;
						}
						// check whereClause validity
						std::vector<SelectHelper> helpers;
						std::vector<SelectHelper> whereHelpersCol[table->ColNum()]; // FIXING
						int cmpUnitsNeeded = 0;
						if(!ParsingHelper::checkWhereClause(helpers, whereHelpersCol, cmpUnitsNeeded, T3.val.str, T5.condList, table, T5.pos))
							return false;
						// TODO: use index where possible
						// build scanner
						Scanner* scanner = ParsingHelper::buildScanner(table, helpers, whereHelpersCol, cmpUnitsNeeded);
						// delete
						// TODO: update index
						Record tmpRec;
						while(scanner->NextRecord(&tmpRec)){
							table->DeleteRecord(*tmpRec.GetRid());
							tmpRec.FreeMemory();
							// update index
						}
						delete scanner;
					};
				}
			|	UPDATE IDENTIFIER SET setClause WHERE whereClause // 不涉及多表; set col = value, 不会出现set col = col
				{
					printf("YACC: update tb\n");
					Global::types.push_back($1);
					Global::types.push_back($2);
					Global::types.push_back($4);
					Global::types.push_back($6);
					Global::action = [](std::vector<Type>& typeVec)->bool{
						Type &T1 = typeVec[0], &T2 = typeVec[1], &T4 = typeVec[2], &T6 = typeVec[3];
						if(!Global::dbms->CurrentDatabase()){
							Global::NoActiveDb(T1.pos);
							return false;
						}
						if(T2.val.str.length() > MAX_TABLE_NAME_LEN){
							Global::TableNameTooLong(T2.pos);
							return false;
						}
						Table* table = Global::dbms->CurrentDatabase()->OpenTable(T2.val.str.data());
						if(!table){
							Global::NoSuchTable(T2.pos, T2.val.str.data());
							return false;
						}
						// check setClause validity
						std::vector<SetHelper> setHelpers;
						for(auto set_it = T4.setList.begin(); set_it != T4.setList.end(); set_it++){
							if(set_it->colName.length() > MAX_ATTRI_NAME_LEN){
								Global::FieldNameTooLong(T4.pos);
								return false;
							}
							uchar setColID = table->IDofCol(set_it->colName.data());
							if(setColID == COL_ID_NONE){
								Global::NoSuchField(T4.pos, set_it->colName.data());
								return false;
							}
							SetHelper helper;
							if(!ParsingHelper::ConvertValue(helper.value, table->GetHeader()->attrType[setColID], table->GetHeader()->attrLenth[setColID], set_it->value, getBitFromLeft(table->GetHeader()->nullMask, setColID))){
								Global::IncompatibleTypes(T4.pos, table->GetHeader()->attrName[setColID]);
								return false;
							}
							helper.colID = setColID;
							setHelpers.push_back(helper);
						}
						// check whereClause validity
						std::vector<SelectHelper> helpers;
						std::vector<SelectHelper> whereHelpersCol[table->ColNum()]; // FIXING
						int cmpUnitsNeeded = 0;
						if(!ParsingHelper::checkWhereClause(helpers, whereHelpersCol, cmpUnitsNeeded, T2.val.str, T6.condList, table, T6.pos))
							return false;
						// check primary key: 只要update的字段和主键字段交集为空即可.反之,设剩余主键集合为R,如果符合要求的记录中,R集合有重复元素,则报错.这是由于update都是常量
						// 需要知道的: 被update的主键,没被update的主键
						uint primaryMask = 0;
						int primaryIdxLength = 4; // 常量长度!
						for(int i = 0; i < MAX_COL_NUM; i++){
							uchar tmpCol = table->GetHeader()->primaryKeyID[i];
							if(tmpCol == COL_ID_NONE)
								break;
							else{
								setBitFromLeft(primaryMask, tmpCol);
								primaryIdxLength += DataType::constantLengthOf(table->GetHeader()->attrType[tmpCol], table->GetHeader()->attrLenth[tmpCol]);
							}
						}
						uint updateMask = 0;
						SetHelper* helperByIndex[MAX_COL_NUM] = {0};
						for(auto set_it = setHelpers.begin(); set_it != setHelpers.end(); set_it++){
							if(helperByIndex[set_it->colID]){ // 多次set一个字段是不被允许的
								Global::MultipleSetForField(T4.pos);
								return false;
							}
							helperByIndex[set_it->colID] = &(*set_it);
							setBitFromLeft(updateMask, set_it->colID);
						}

						// build scanner
						Scanner* scanner = ParsingHelper::buildScanner(table, helpers, whereHelpersCol, cmpUnitsNeeded);
						Global::scanners.push_back(scanner);
						// check primary constraint
						Record tmpRec;
						if(primaryMask){
							BplusTree* primaryIdx = new BplusTree(Global::dbms->CurrentDatabase()->idx, table->GetPrimaryIndexPage());
							Global::globalTree = primaryIdx;
							std::set<const uchar*, bool(*)(const uchar*, const uchar*)> primaryCheck([](const uchar* left, const uchar* right)->bool{
								return DataType::compareArr(left, right, Global::globalTree->header->attrType, Global::globalTree->header->attrLenth,
									Global::globalTree->colNum, Comparator::Lt, true, true);
							});
							bool ok = true;
							while(scanner->NextRecord(&tmpRec)){
								uchar* idxBuf = new uchar[primaryIdxLength]{0};
								int pos = 4;
								for(int i = 0; i < primaryIdx->colNum; i++){ // 对于每个主键
									uchar mainColID = primaryIdx->header->indexColID[i];
									int fieldLength = DataType::constantLengthOf(primaryIdx->header->attrType[i], primaryIdx->header->attrLenth[i]);
									if(getBitFromLeft(updateMask, mainColID)){ // 该主键被update修改,从SetHelper中获取值
										if(helperByIndex[mainColID]->value.type == DataType::NONE){
											Global::NullPrimaryKey(T4.pos);
											delete[] idxBuf;
											ok = false;
											break;
										}
										else if(helperByIndex[mainColID]->value.type == DataType::CHAR || helperByIndex[mainColID]->value.type == DataType::VARCHAR){
											memcpy(idxBuf + pos, helperByIndex[mainColID]->value.str.data(), helperByIndex[mainColID]->value.str.length());
										}
										else
											memcpy(idxBuf + pos, helperByIndex[mainColID]->value.bytes, fieldLength);
									}
									else{ // 主键未被修改,从tmpRec中获取值
										// 主键不能是null,不用考虑这种情况
										if(primaryIdx->header->attrType[i] == DataType::VARCHAR && primaryIdx->header->attrLenth[i] > 255){
											uchar* ridAddr = tmpRec.GetData() + table->ColOffset(mainColID);
											RID tmpRID(*(uint*)ridAddr, *(uint*)(ridAddr + 4));
											ushort len = 0;
											Global::dbms->CurrentDatabase()->GetLongVarchar(tmpRID, idxBuf + pos, len);
										}
										else
											memcpy(idxBuf + pos, tmpRec.GetData() + table->ColOffset(mainColID), fieldLength);
									}
									pos += fieldLength;
								}
								tmpRec.FreeMemory();
								if(!ok)
									break;
								if(!primaryCheck.insert(idxBuf).second){
									Global::PrimaryKeyConflict(T4.pos);
									delete[] idxBuf;
									ok = false;
									break;
								}
							}
							delete primaryIdx;
							for(auto check_it = primaryCheck.begin(); check_it != primaryCheck.end(); check_it++)
								delete[] *check_it;
							if(!ok)
								return false;
						}
						scanner->Reset();
						// check foreign masters: 只需检查update后的外键是否在master的主键中即可
						for(int i = 0; i < table->FKMasterNum(); i++){ // 对每个master
							uchar masterName[MAX_TABLE_NAME_LEN + 1] = {0};
							Global::dbms->CurrentDatabase()->GetTableName(table->GetHeader()->fkMaster[i], masterName);
							Table* master = Global::dbms->CurrentDatabase()->OpenTable((char*)masterName);
							assert(master != nullptr);
							bool needCheck = false;
							for(int j = 0; j < MAX_COL_NUM; j++){
								uchar localCol = table->GetHeader()->slaveKeyID[i][j];
								if(localCol == COL_ID_NONE)
									break;
								if(getBitFromLeft(updateMask, localCol)){
									needCheck = true;
									break;
								}
							}
							if(needCheck){
								int masterConstPriIdxLenth = 4;
								for(int j = 0; j < MAX_COL_NUM; j++){
									uchar masterCol = master->GetHeader()->primaryKeyID[j];
									if(masterCol == COL_ID_NONE)
										break;
									masterConstPriIdxLenth += DataType::constantLengthOf(master->GetHeader()->attrType[masterCol], master->GetHeader()->attrLenth[masterCol]);
								}
								uchar idxBuf[masterConstPriIdxLenth] = {0};
								while(scanner->NextRecord(&tmpRec)){
									memset(idxBuf, 0, sizeof(idxBuf));
									int bufPos = 4;
									for(int j = 0; j < MAX_COL_NUM; j++){ // 对于每个slave key
										uchar localCol = table->GetHeader()->slaveKeyID[i][j];
										if(localCol == COL_ID_NONE)
											break;
										int constantLength = DataType::constantLengthOf(table->GetHeader()->attrType[localCol], table->GetHeader()->attrLenth[localCol]);
										if(getBitFromLeft(updateMask, localCol)){ // 这个字段被update更新
											if(helperByIndex[localCol]->value.type == DataType::NONE){
												Global::ForeignKeyNotFound(T4.pos, masterName);
												tmpRec.FreeMemory();
												return false;
											}
											else if(table->GetHeader()->attrType[localCol] == DataType::VARCHAR || table->GetHeader()->attrType[localCol] == DataType::CHAR){
												memcpy(idxBuf + bufPos, helperByIndex[localCol]->value.str.data(), helperByIndex[localCol]->value.str.length());
											}
											else
												memcpy(idxBuf + bufPos, helperByIndex[localCol]->value.bytes, constantLength);
										}
										else{ // 字段未被update更新,使用从表中读出的值
											if(table->GetHeader()->attrType[localCol] == DataType::VARCHAR && table->GetHeader()->attrLenth[localCol] > 255){
												uchar* ridAddr = tmpRec.GetData() + table->ColOffset(localCol);
												RID tmpRID(*(uint*)ridAddr, *(uint*)(ridAddr + 4));
												ushort len = 0;
												Global::dbms->CurrentDatabase()->GetLongVarchar(tmpRID, idxBuf + bufPos, len);
											}
										}
										bufPos += constantLength;
									}
									tmpRec.FreeMemory();
									BplusTree* masterIdx = new BplusTree(Global::dbms->CurrentDatabase()->idx, master->GetPrimaryIndexPage());
									RID tmpRID;
									if(!masterIdx->SafeValueSearch(idxBuf, &tmpRID)){
										delete masterIdx;
										Global::ForeignKeyNotFound(T4.pos, masterName);
										return false;
									}
									delete masterIdx;
								}
							}
							Global::dbms->CurrentDatabase()->CloseTable((char*)masterName);
						}
						scanner->Reset();
						// update
						while(scanner->NextRecord(&tmpRec)){
							uchar tmpBuf[table->GetHeader()->recordLenth] = {0};
							memcpy(tmpBuf, tmpRec.GetData(), sizeof(tmpBuf));
							for(auto set_it = setHelpers.begin(); set_it != setHelpers.end(); set_it++){
								uchar setColType = table->GetHeader()->attrType[set_it->colID];
								if(set_it->value.type == DataType::NONE){ // use val type
									setBitFromLeft(*(uint*)tmpRec.GetData(), set_it->colID);
								}
								else{
									if(setColType == DataType::CHAR){ // use field type
										memcpy(tmpRec.GetData() + table->ColOffset(set_it->colID), set_it->value.str.data(), set_it->value.str.length());
										memset(tmpRec.GetData() + table->ColOffset(set_it->colID) + set_it->value.str.length(), 0, 
											table->GetHeader()->attrLenth[set_it->colID] - set_it->value.str.length());
									}
									else if(setColType == DataType::VARCHAR){ // use field type
										if(table->GetHeader()->attrLenth[set_it->colID] > 255){
											RID tmpRID;
											Global::dbms->CurrentDatabase()->InsertLongVarchar(set_it->value.str.data(), set_it->value.str.length(), &tmpRID);
											*(uint*)(tmpRec.GetData() + table->ColOffset(set_it->colID)) = tmpRID.GetPageNum();
											*(uint*)(tmpRec.GetData() + table->ColOffset(set_it->colID + 4)) = tmpRID.GetSlotNum();
										}
										else{
											memcpy(tmpRec.GetData() + table->ColOffset(set_it->colID), set_it->value.str.data(), set_it->value.str.length());
											memset(tmpRec.GetData() + table->ColOffset(set_it->colID) + set_it->value.str.length(), 0,
												table->GetHeader()->attrLenth[set_it->colID] - set_it->value.str.length());
										}
									}
									else{
										memcpy(tmpRec.GetData() + table->ColOffset(set_it->colID), set_it->value.bytes, 
											DataType::lengthOf(table->GetHeader()->attrType[set_it->colID], table->GetHeader()->attrType[set_it->colID]));
									}
								}
							}
							table->UpdateRecord(*tmpRec.GetRid(), tmpRec.GetData(), 0, 0, table->GetHeader()->recordLenth);
							// 更新所有索引,包括主键索引
							ParsingHelper::UpdateIndexes(table, tmpBuf, tmpRec, updateMask);
							tmpRec.FreeMemory();
						}
						return true;
					};
				}
			| 	SELECT selector FROM IdList WHERE whereClause
				{
					printf("YACC: select tb with condition\n");
					Global::types.push_back($1);
					Global::types.push_back($2);
					Global::types.push_back($4);
					Global::types.push_back($6);
					Global::action = [](std::vector<Type> &typeVec){
						Type &T1 = typeVec[0], &T2 = typeVec[1], &T4 = typeVec[2], &T6 = typeVec[3];
						int selectNum = T2.colList.size();
						if(T4.IDList.size() == 1){ // select from one table
							if(!Global::dbms->CurrentDatabase()){
								Global::NoActiveDb(T1.pos);
								return false;
							}
							if(T4.IDList[0].length() > MAX_TABLE_NAME_LEN){
								Global::TableNameTooLong(T4.pos);
								return false;
							}
							Table* table = Global::dbms->CurrentDatabase()->OpenTable(T4.IDList[0].data());
							if(!table){
								Global::NoSuchTable(T4.pos, T4.IDList[0].data());
								return false;
							}
							// check selector
							std::vector<uchar> wantedCols;
							if(!T2.selectAll){ // not select*
								for(auto col_it = T2.colList.begin(); col_it != T2.colList.end(); col_it++){
									if(col_it->tableName.length() && col_it->tableName != T4.IDList[0]){
										Global::IrrelevantTable(T2.pos, col_it->tableName.data());
										// Global::dbms->CurrentDatabase()->CloseTable(T4.IDList[0].data());
										return false;
									}
									if(col_it->colName.length() > MAX_ATTRI_NAME_LEN){
										Global::FieldNameTooLong(T2.pos);
										return false;
									}
									uchar tmpColID = table->IDofCol(col_it->colName.data());
									if(tmpColID == COL_ID_NONE){
										Global::NoSuchField(T2.pos, col_it->colName.data());
										// Global::dbms->CurrentDatabase()->CloseTable(T4.IDList[0].data());
										return false;
									}
									wantedCols.push_back(tmpColID);
								}
								std::sort(wantedCols.begin(), wantedCols.end()); // sort it
							}
							else{ // select*
								for(int i = 0; i < table->ColNum(); i++)
									wantedCols.push_back(i);
							}
							// check whereClause validity
							std::vector<SelectHelper> helpers; // FIXING: self cmps
							std::vector<SelectHelper> whereHelpersCol[table->ColNum()]; // FIXING
							int cmpUnitsNeeded = 0;
							if(!ParsingHelper::checkWhereClause(helpers, whereHelpersCol, cmpUnitsNeeded, T4.IDList[0], T6.condList, table, T6.pos))
								return false;
							// TODO: use index where possible
							uint whereMask = 0;
							uchar rangeCol = COL_ID_NONE, idxID = INDEX_ID_NONE;
							std::vector<IndexHelper> idxHelpers[table->ColNum()];
							if(ParsingHelper::CanUseIndex(table, whereHelpersCol, idxHelpers, whereMask, rangeCol)){
								idxID = table->BestIndexFor(whereMask, rangeCol);
								if(idxID != INDEX_ID_NONE){
									// TODO
									BplusTree* index = new BplusTree(Global::dbms->CurrentDatabase()->idx, table->GetHeader()->bpTreePage[idxID]);
									int constantIdxLength = DataType::calcConstantLength(index->header->attrType, index->header->attrLenth, index->colNum);
									uchar idxBuf[constantIdxLength] = {0};
									uchar lowupBuf[constantIdxLength] = {0}; // 为了上下界都存在时使用
									int bufPos = 4;
									int cmpColNum = 0;
									for(int i = 0; i < MAX_COL_NUM; i++){ // 对于每一个索引列
										uchar indexedCol = index->header->indexColID[i];
										if(indexedCol == COL_ID_NONE)
											break;
										int constantFieldLenth = DataType::constantLengthOf(index->header->attrType[i], index->header->attrLenth[i]);
										Val *val = nullptr;
										if(getBitFromLeft(whereMask, indexedCol)){
											cmpColNum++;
											val = &idxHelpers[indexedCol][0].eqVal;
										}
										else if(rangeCol == indexedCol){ // cmpColNum在这里不更新
											if(idxHelpers[indexedCol][0].hasLower){
												val = &idxHelpers[indexedCol][0].lowerVal;
												if(idxHelpers[indexedCol][0].hasUpper){
													memcpy(lowupBuf, idxBuf, bufPos);
													Val *lowupVal = &idxHelpers[indexedCol][0].upperVal;
													if(lowupVal->type == DataType::NONE)
														setBitFromLeft(*(uint*)lowupBuf, i);
													else if(lowupVal->type == DataType::CHAR || lowupVal->type == DataType::VARCHAR){
														memcpy(lowupBuf + bufPos, lowupVal->str.data(), lowupVal->str.length());
													}
													else{
														memcpy(lowupBuf + bufPos, lowupVal->bytes, constantFieldLenth);
													}
												}
											}
											else if(idxHelpers[indexedCol][0].hasUpper)
												val = &idxHelpers[indexedCol][0].upperVal;
											else
												break;
										}
										else
											break;
										// copy val to buf
										if(val->type == DataType::NONE)
											setBitFromLeft(*(uint*)idxBuf, i);
										else if(val->type == DataType::CHAR || val->type == DataType::VARCHAR){
											memcpy(idxBuf + bufPos, val->str.data(), val->str.length());
										}
										else{
											memcpy(idxBuf + bufPos, val->bytes, constantFieldLenth);
										}
										bufPos += constantFieldLenth;
									} // end: build idxBuf
									std::vector<RID> results;
									if(rangeCol == COL_ID_NONE) // case 2, 仅有Eq
										index->SafeValueSelect(cmpColNum, idxBuf, Comparator::Eq, nullptr, Comparator::Eq, results);
									else{
										if(idxHelpers[rangeCol][0].hasUpper){
											if(idxHelpers[rangeCol][0].hasLower){ // case 4, 有上下界
												index->SafeValueSelect(cmpColNum, idxBuf, idxHelpers[rangeCol][0].lowerCmp, lowupBuf, idxHelpers[rangeCol][0].upperCmp, results);
											}
											else{ // case 1, 仅有上界
												index->SafeValueSelect(cmpColNum, nullptr, Comparator::Eq, idxBuf, idxHelpers[rangeCol][0].upperCmp, results);
											}
										}
										else{ // case 3, 仅有下界
												index->SafeValueSelect(cmpColNum, idxBuf, idxHelpers[rangeCol][0].lowerCmp, nullptr, Comparator::Eq, results);
										}
									}
									table->PrintSelection(wantedCols, results);
									delete index;
								} // end: idxID != INDEX_ID_NONE
							}
							// 不能使用索引
							if(idxID == INDEX_ID_NONE){
								// build scanner
								Scanner* scanner = ParsingHelper::buildScanner(table, helpers, whereHelpersCol, cmpUnitsNeeded);
								// Print
								scanner->PrintSelection(wantedCols);
								delete scanner;
							}
						}
						else{
							// TODO: multi-table query ?
							if(!Global::dbms->CurrentDatabase()){
								Global::NoActiveDb(T1.pos);
								return false;
							}
							Table* tables[T4.IDList.size()] = {0};
							auto tableNameToIndex = [&](const std::string& name)->int{
								for(int i = 0; i < T4.IDList.size(); i++)
									if(T4.IDList[i] == name)
										return i;
								return -1;
							};
							auto colNameToTable = [&](const std::string& name, uchar& tbID, uchar& colID, int pos)->bool{
								tbID = TB_ID_NONE;
								colID = COL_ID_NONE;
								for(int i = 0; i < T4.IDList.size(); i++){
									uchar tmpColID = tables[i]->IDofCol(name.data());
									if(tmpColID != COL_ID_NONE){
										if(colID != COL_ID_NONE){
											Global::AmbiguousField(pos, name.data());
											return false;
										}
										colID = tmpColID;
										tbID = i;
									}
								}
								if(colID == COL_ID_NONE){
									Global::NoSuchField(pos, name.data());
									return false;
								}
								return true;
							};
							for(int i = 0; i < T4.IDList.size(); i++){
								if(T4.IDList[i].length() > MAX_TABLE_NAME_LEN){
									Global::TableNameTooLong(T4.pos);
									return false;
								}
								tables[i] = Global::dbms->CurrentDatabase()->OpenTable(T4.IDList[i].data());
								if(!tables[i]){
									Global::NoSuchTable(T4.pos, T4.IDList[0].data());
									return false;
								}
							}
							// check selector
							std::vector<uchar> wantedCols[T4.IDList.size()];
							if(!T2.selectAll){
								for(auto col_it = T2.colList.begin(); col_it != T2.colList.end(); col_it++){
									if(col_it->colName.length() > MAX_ATTRI_NAME_LEN){
										Global::FieldNameTooLong(T2.pos);
										return false;
									}
									if(col_it->tableName.length()){
										int tableIndex = tableNameToIndex(col_it->tableName);
										if(tableIndex == -1){
											Global::IrrelevantTable(T2.pos, col_it->tableName.data());
											return false;
										}
										uchar colID = tables[tableIndex]->IDofCol(col_it->colName.data());
										if(colID == COL_ID_NONE){
											Global::NoSuchField(T2.pos, col_it->colName.data());
											return false;
										}
										wantedCols[tableIndex].push_back(colID);
									}
									else{
										uchar colID = COL_ID_NONE, tbID = TB_ID_NONE;
										if(!colNameToTable(col_it->colName, tbID, colID, T2.pos))
											return false;
										wantedCols[tbID].push_back(colID);
									}
								}
								for(int i = 0; i < T4.IDList.size(); i++)
									std::sort(wantedCols[i].begin(), wantedCols[i].end());
							}
							else{ // select*
								for(int i = 0; i < T4.IDList.size(); i++)
									for(int j = 0; j < tables[i]->ColNum(); j++)
										wantedCols[i].push_back(j);
							}
							// check whereClause validity
							// TODO
							MultiScanner multiScanner;
							std::vector<SelectHelper> helpers[T4.IDList.size()];
							std::vector<SelectHelper> whereHelpersCol[T4.IDList.size()][MAX_COL_NUM];
							int cmpUnitsNeeded[T4.IDList.size()] = {0};
							std::vector<WhereInstr> whereClauses[T4.IDList.size()];
							bool canBuildScanner[T4.IDList.size()] = {0};
							// TODO
							for(auto where_it = T6.condList.begin(); where_it != T6.condList.end(); where_it++){
								// TODO: distribute WhereInstr
								auto checkWhereInstr = [&](const Col& col, uchar& tableID, uchar& colID)->bool{
									if(col.colName.length() > MAX_ATTRI_NAME_LEN){
										Global::FieldNameTooLong(T6.pos);
										return false;
									}
									if(col.tableName.length()){
										int tableIndex = tableNameToIndex(col.tableName);
										if(tableIndex == -1){
											Global::IrrelevantTable(T6.pos, col.tableName.data());
											return false;
										}
										tableID = tableIndex;
										colID = tables[tableIndex]->IDofCol(col.colName.data());
										if(colID == COL_ID_NONE){
											Global::NoSuchField(T6.pos, col.colName.data());
											return false;
										}
									}
									else{
										if(!colNameToTable(col.colName, tableID, colID, T6.pos))
											return false;
									}
									return true;
								};
								// 检查左侧
								uchar leftTableID = TB_ID_NONE, leftColID = COL_ID_NONE;
								if(!checkWhereInstr(where_it->column, leftTableID, leftColID))
									return false;
								// 检查右侧
								if(where_it->isExprCol){
									uchar rightTableID = TB_ID_NONE, rightColID = COL_ID_NONE;
									if(!checkWhereInstr(where_it->exprCol, rightTableID, rightColID))
										return false;
									if(leftTableID == rightTableID){
										whereClauses[leftTableID].push_back(*where_it);
										canBuildScanner[leftTableID] = true;
									}
									else
										multiScanner.AddUnit(leftTableID, leftColID, rightTableID, rightColID, where_it->cmp);
								}
								else{
									whereClauses[leftTableID].push_back(*where_it);
									canBuildScanner[leftTableID] = true;
								}
							}
							for(int i = 0; i < T4.IDList.size(); i++){
								if(!ParsingHelper::checkWhereClause(helpers[i], whereHelpersCol[i], cmpUnitsNeeded[i], T4.IDList[i], whereClauses[i], tables[i], T6.pos))
									return false;
							}
							for(int i = 0; i < T4.IDList.size(); i++){
								if(canBuildScanner[i])
									multiScanner.AddScanner(ParsingHelper::buildScanner(tables[i], helpers[i], whereHelpersCol[i], cmpUnitsNeeded[i]));
								else
									multiScanner.AddScanner(tables[i]->GetScanner([](const Record& rec)->bool{return true;}));
							}
							multiScanner.PrintSelection(wantedCols);
							multiScanner.DeleteScanners();
						}
						return true;
					};
				}
			|	SELECT selector FROM IdList
				{
					printf("YACC: select tb no condition\n");
					Global::types.push_back($1);
					Global::types.push_back($2);
					Global::types.push_back($4);
					Global::action = [](std::vector<Type>& typeVec)->bool{
						Type &T1 = typeVec[0], &T2 = typeVec[1], &T4 = typeVec[2];
						int selectNum = T2.colList.size();
						if(T4.IDList.size() == 1){ // select from one table
							if(!Global::dbms->CurrentDatabase()){
								Global::NoActiveDb(T1.pos);
								return false;
							}
							if(T4.IDList[0].length() > MAX_TABLE_NAME_LEN){
								Global::TableNameTooLong(T4.pos);
								return false;
							}
							Table* table = Global::dbms->CurrentDatabase()->OpenTable(T4.IDList[0].data());
							if(!table){
								Global::NoSuchTable(T4.pos, T4.IDList[0].data());
								return false;
							}
							// check selector
							std::vector<uchar> wantedCols;
							if(!T2.selectAll){ // not select*
								for(auto col_it = T2.colList.begin(); col_it != T2.colList.end(); col_it++){
									if(col_it->tableName.length() && col_it->tableName != T4.IDList[0]){
										Global::IrrelevantTable(T2.pos, col_it->tableName.data());
										// Global::dbms->CurrentDatabase()->CloseTable(T4.IDList[0].data());
										return false;
									}
									uchar tmpColID = table->IDofCol(col_it->colName.data());
									if(tmpColID == COL_ID_NONE){
										Global::NoSuchField(T2.pos, col_it->colName.data());
										// Global::dbms->CurrentDatabase()->CloseTable(T4.IDList[0].data());
										return false;
									}
									wantedCols.push_back(tmpColID);
								}
								std::sort(wantedCols.begin(), wantedCols.end()); // sort it
							}
							else{ // select*
								for(int i = 0; i < table->ColNum(); i++)
									wantedCols.push_back(i);
							}
							// build scanner
							Scanner* scanner = table->GetScanner([](const Record& rec)->bool{return true;});
							// Print
							scanner->PrintSelection(wantedCols);
							delete scanner;
						}
						else{
							// TODO: multi-table query ?
						}
						return true;
					};
				}
			;

IdxStmt		:	CREATE INDEX IDENTIFIER ON IDENTIFIER '(' IdList ')'
				{
					printf("YACC: create idx\n");
					Global::types.push_back($1);
					Global::types.push_back($3);
					Global::types.push_back($5);
					Global::types.push_back($7);
					Global::action = [](std::vector<Type>& typeVec){
						Type &T1 = typeVec[0], &T3 = typeVec[1], &T5 = typeVec[2], &T7 = typeVec[3];
						if(Global::dbms->CurrentDatabase() == nullptr){
							Global::NoActiveDb(T1.pos);
							return false;
						}
						if(T3.val.str.length() > MAX_INDEX_NAME_LEN){
							Global::IndexNameTooLong(T3.pos);
							return false;
						}
						if(ParsingHelper::IndexNameReserved(T3.val.str.data())){
							Global::IndexNameReserved(T3.pos, T3.val.str.data());
							return false;
						}
						if(T5.val.str.length() > MAX_TABLE_NAME_LEN){
							Global::TableNameTooLong(T5.pos);
							return false;
						}
						Table* table = Global::dbms->CurrentDatabase()->OpenTable(T5.val.str.data());
						if(table == nullptr){
							Global::NoSuchTable(T5.pos, T5.val.str.data());
							return false;
						}
						std::vector<uchar> idxCols;
						for(auto it = T7.IDList.begin(); it != T7.IDList.end(); it++){
							if(it->length() > MAX_ATTRI_NAME_LEN){
								Global::FieldNameTooLong(T7.pos);
								return false;
							}
							uchar colID = table->IDofCol(it->data());
							if(colID == COL_ID_NONE){
								Global::NoSuchField(T7.pos, it->data());
								return false;
							}
							idxCols.push_back(colID);
						}
						int res = table->CreateIndexOn(idxCols, T3.val.str.data());
						if(res == 2){
							Global::TooManyIndexes(T1.pos);
							return false;
						}
						else if(res == 1){
							Global::IndexNameConflict(T3.pos, T3.val.str.data());
							return false;
						}
						return true;
					};
				}
			|	DROP INDEX IDENTIFIER ON IDENTIFIER
				{
					printf("YACC: drop idx\n");
					Global::types.push_back($1);
					Global::types.push_back($3);
					Global::types.push_back($5);
					Global::action = [](std::vector<Type> &typeVec){
						Type &T1 = typeVec[0], &T3 = typeVec[1], &T5 = typeVec[2];
						if(Global::dbms->CurrentDatabase() == nullptr){
							Global::NoActiveDb(T1.pos);
							return false;
						}
						if(T3.val.str.length() > MAX_INDEX_NAME_LEN){
							Global::IndexNameTooLong(T3.pos);
							return false;
						}
						if(ParsingHelper::IndexNameReserved(T3.val.str.data())){
							Global::IndexNameReserved(T3.pos, T3.val.str.data());
							return false;
						}
						if(T5.val.str.length() > MAX_TABLE_NAME_LEN){
							Global::TableNameTooLong(T5.pos);
							return false;
						}
						if(ParsingHelper::TableNameReserved(T5.val.str.data())){
							Global::TableNameReserved(T5.pos, T5.val.str.data());
							return false;
						}
						Table* table = Global::dbms->CurrentDatabase()->OpenTable(T5.val.str.data());
						if(table == nullptr){
							Global::NoSuchTable(T5.pos, T5.val.str.data());
							return false;
						}
						if(!table->RemoveIndex(T3.val.str.data())){
							Global::NoSuchIndex(T3.pos, T3.val.str.data());
							return false;
						}
						return true;
					};
				}
			|	ALTER TABLE IDENTIFIER ADD INDEX IDENTIFIER '(' IdList ')'
				{
					printf("YACC: alter add idx\n");
					Global::types.push_back($1);
					Global::types.push_back($3);
					Global::types.push_back($6);
					Global::types.push_back($8);
					Global::action = [](std::vector<Type>& typeVec){
						Type &T1 = typeVec[0], &T3 = typeVec[1], &T6 = typeVec[2], &T8 = typeVec[3];
						if(Global::dbms->CurrentDatabase() == nullptr){
							Global::NoActiveDb(T1.pos);
							return false;
						}
						if(T6.val.str.length() > MAX_INDEX_NAME_LEN){
							Global::IndexNameTooLong(T6.pos);
							return false;
						}
						if(ParsingHelper::IndexNameReserved(T6.val.str.data())){
							Global::IndexNameReserved(T6.pos, T6.val.str.data());
							return false;
						}
						if(T3.val.str.length() > MAX_TABLE_NAME_LEN){
							Global::TableNameTooLong(T3.pos);
							return false;
						}
						if(ParsingHelper::TableNameReserved(T3.val.str.data())){
							Global::TableNameReserved(T3.pos, T3.val.str.data());
							return false;
						}
						Table* table = Global::dbms->CurrentDatabase()->OpenTable(T3.val.str.data());
						if(table == nullptr){
							Global::NoSuchTable(T3.pos, T3.val.str.data());
							return false;
						}
						std::vector<uchar> idxCols;
						for(auto it = T8.IDList.begin(); it != T8.IDList.end(); it++){
							if(it->length() > MAX_ATTRI_NAME_LEN){
								Global::FieldNameTooLong(T8.pos);
								return false;
							}
							uchar colID = table->IDofCol(it->data());
							if(colID == COL_ID_NONE){
								Global::NoSuchField(T8.pos, it->data());
								return false;
							}
							idxCols.push_back(colID);
						}
						int res = table->CreateIndexOn(idxCols, T6.val.str.data());
						if(res == 2){
							Global::TooManyIndexes(T1.pos);
							return false;
						}
						else if(res == 1){
							Global::IndexNameConflict(T6.pos, T6.val.str.data());
							return false;
						}
						return true;
					};
				}
			|	ALTER TABLE IDENTIFIER DROP INDEX IDENTIFIER
				{
					printf("YACC: alter drop idx\n");
					Global::types.push_back($1);
					Global::types.push_back($3);
					Global::types.push_back($6);
					Global::action = [](std::vector<Type> &typeVec){
						Type &T1 = typeVec[0], &T3 = typeVec[1], &T6 = typeVec[2];
						if(Global::dbms->CurrentDatabase() == nullptr){
							Global::NoActiveDb(T1.pos);
							return false;
						}
						if(T6.val.str.length() > MAX_INDEX_NAME_LEN){
							Global::IndexNameTooLong(T6.pos);
							return false;
						}
						if(ParsingHelper::IndexNameReserved(T6.val.str.data())){
							Global::IndexNameReserved(T6.pos, T6.val.str.data());
							return false;
						}
						if(T3.val.str.length() > MAX_TABLE_NAME_LEN){
							Global::TableNameTooLong(T3.pos);
							return false;
						}
						if(ParsingHelper::TableNameReserved(T3.val.str.data())){
							Global::TableNameReserved(T3.pos ,T3.val.str.data());
							return false;
						}
						Table* table = Global::dbms->CurrentDatabase()->OpenTable(T3.val.str.data());
						if(table == nullptr){
							Global::NoSuchTable(T3.pos, T3.val.str.data());
							return false;
						}
						if(!table->RemoveIndex(T6.val.str.data())){
							Global::NoSuchIndex(T6.pos, T6.val.str.data());
							return false;
						}
						return true;
					};
				}
			;

AlterStmt	:	ALTER TABLE IDENTIFIER ADD field
				{
					// check: 若field有not null限制,则其必须有default值或目标表为空;没有not null限制时,若有default,则插入default, 否则插入null
					// rebuild table
					printf("YACC: alter add col\n");
				}
			|	ALTER TABLE IDENTIFIER DROP IDENTIFIER
				{
					// check: primary / indexed / slave
					// rebuild table
					printf("YACC: alter drop col\n");
				}
			|	ALTER TABLE IDENTIFIER CHANGE colField
				{
					// check: cannot change primary key / indexed columns / slave columns
					// rebuild table
					// difficulty: type conversion?
					printf("YACC: alter change field\n");
				}
			|	ALTER TABLE IDENTIFIER RENAME TO IDENTIFIER
				{
					// easy
					printf("YACC: alter rename\n");
					Global::types.push_back($1);
					Global::types.push_back($3);
					Global::types.push_back($6);
					Global::action = [](std::vector<Type> &typeVec){
						Type &T1 = typeVec[0], &T3 = typeVec[1], &T6 = typeVec[2];
						if(Global::dbms->CurrentDatabase() == nullptr){
							Global::NoActiveDb(T1.pos);
							return false;
						}
						if(T3.val.str.length() > MAX_TABLE_NAME_LEN){
							Global::TableNameTooLong(T3.pos);
							return false;
						}
						if(ParsingHelper::TableNameReserved(T3.val.str.data())){
							Global::TableNameReserved(T3.pos, T3.val.str.data());
							return false;
						}
						if(T6.val.str.length() > MAX_TABLE_NAME_LEN){
							Global::TableNameTooLong(T6.pos);
							return false;
						}
						if(ParsingHelper::TableNameReserved(T6.val.str.data())){
							Global::TableNameReserved(T6.pos, T6.val.str.data());
							return false;
						}
						if(Global::dbms->CurrentDatabase()->GetTableID(T6.val.str.data()) != TB_ID_NONE){
							Global::TableNameConflict(T6.pos);
							return false;
						}
						if(!Global::dbms->CurrentDatabase()->RenameTable(T3.val.str.data(), T6.val.str.data())){
							Global::NoSuchTable(T3.pos, T3.val.str.data());
							return false;
						}
						return true;
					};
				}
			|	ALTER TABLE IDENTIFIER ADD PRIMARY KEY '(' colList ')'
				{
					// check: any conflict?
					// build: primary index
					printf("YACC: alter add primary\n");
				}
			|	ALTER TABLE IDENTIFIER DROP PRIMARY KEY
				{
					// check: any slaves?
					// clean: primary index
					printf("YACC: alter drop primary\n");
				}
			|	ALTER TABLE IDENTIFIER ADD CONSTRAINT IDENTIFIER FOREIGN KEY '(' colList ')' REFERENCES IDENTIFIER '(' colList ')'
				{
					// check: any conflict?
					printf("YACC: alter add foreign\n");
				}
			|	ALTER TABLE IDENTIFIER DROP FOREIGN KEY IDENTIFIER
				{
					// easy
					printf("YACC: alter drop foreign\n");
				}
			;
			// we do not support:
			// alter table {tablename} add constraint {primarykeyname} primary key ({col}...)
			// alter table {tablename} drop primary key {primarykeyname}

// fieldList 是建表时使用的
fieldList	:	fieldList ',' field
				{
					printf("YACC: fieldList recur\n");
					$$.fieldList = $1.fieldList;
					$$.constraintList = $1.constraintList;
					if($3.constraintList.empty())
						$$.fieldList.push_back($3.field);
					else
						$$.constraintList.push_back($3.constraintList[0]);
				}
			|	field
				{
					printf("YACC: fieldList base\n");
					if($1.constraintList.empty())
						$$.fieldList.push_back($1.field);
					else
						$$.constraintList.push_back($1.constraintList[0]);
				}
			;

// 名字长度已经经过检查
// field.length应该保留varchar的原始长度
field		:	colField
				{
					printf("YACC: col field\n");
					$$.field = $1.field;
				}
			|	consField
				{
					printf("YACC: constraint field\n");
					$$.constraintList = $1.constraintList;
				}

// colField可以变成field用于建表,也可以在alter add里面用于增加列
colField	:	IDENTIFIER type
				{
					printf("YACC: field normal\n");
					if($1.val.str.length() > MAX_ATTRI_NAME_LEN){ // attribute name too long
						Global::FieldNameTooLong($1.pos);
						YYABORT;
					}
					else
						$$.field.name = $1.val.str;
					$$.field.type = $2.val.type;
					$$.field.length = DataType::lengthOf($2.val.type, *(uint*)$2.val.bytes, false); // keep string length
				}
			|	IDENTIFIER type NOT KW_NULL
				{
					printf("YACC: field not null\n");
					if($1.val.str.length() > MAX_ATTRI_NAME_LEN){ // attribute name too long
						Global::FieldNameTooLong($1.pos);
						YYABORT;
					}
					else
						$$.field.name = $1.val.str;
					$$.field.type = $2.val.type;
					$$.field.length = DataType::lengthOf($2.val.type, *(uint*)$2.val.bytes, false); // keep string length
					$$.field.nullable = false;
				}
			|	IDENTIFIER type DEFAULT value
				{
					printf("YACC: field default\n");
					if($1.val.str.length() > MAX_ATTRI_NAME_LEN){ // attribute name too long
						Global::FieldNameTooLong($1.pos);
						YYABORT;
					}
					else
						$$.field.name = $1.val.str;
					$$.field.type = $2.val.type;
					$$.field.length = DataType::lengthOf($2.val.type, *(uint*)$2.val.bytes, false); // keep string length
					$$.field.hasDefault = true;
					if(ParsingHelper::ConvertValue($$.val, $2.val.type, *(uint*)$2.val.bytes, $4.val, true)){
						$$.field.defaultValue = $$.val;
					}
					else{ // wrong
						Global::IncompatibleTypes($4.pos, $2.val.str.data());
						YYABORT;
					}
				}
			|	IDENTIFIER type NOT KW_NULL DEFAULT value
				{
					printf("YACC: not null default\n");
					if($1.val.str.length() > MAX_ATTRI_NAME_LEN){ // attribute name too long
						Global::FieldNameTooLong($1.pos);
						YYABORT;
					}
					else
						$$.field.name = $1.val.str;
					$$.field.type = $2.val.type;
					$$.field.length = DataType::lengthOf($2.val.type, *(uint*)$2.val.bytes, false); // keep string length
					$$.field.hasDefault = true;
					$$.field.nullable = false;
					if(ParsingHelper::ConvertValue($$.val, $2.val.type, *(uint*)$2.val.bytes, $6.val, false)){
						$$.field.defaultValue = $$.val;
					}
					else{ // wrong
						Global::IncompatibleTypes($6.pos, $2.val.str.data());
						YYABORT;
					}
				}
			;

// 主键和外键约束
consField	:	PRIMARY KEY '(' IdList ')'
				{
					printf("YACC: primary key\n");
					Constraint tmp;
					tmp.IDList = $4.IDList;
					tmp.isFK = false;
					$$.constraintList.push_back(tmp);
				}
			|	FOREIGN KEY '(' IdList ')' REFERENCES IDENTIFIER '(' IdList ')' // 建表时创建外键约束,只能建立1对1的映射; 多对多需要使用alter命令
				{
					printf("YACC: foreign\n");
					Constraint tmp;
					tmp.IDList = $4.IDList;
					tmp.masterName = $7.val.str;
					tmp.masterIDList = $9.IDList;
					tmp.isFK = true;
					$$.constraintList.push_back(tmp);
				}
			;

// Type.type : 类型
// Type.val.bytes : 对于INT, DATE, FLOAT来说此字段无用; 对于VARCHAR, CHAR来说表示长度, 对于NUMERIC来说表示两个长度参数, 都将结果以int类型存到val处
// 长度都已经经过检查
type		:	KW_INT
				{
					printf("YACC: type int\n");
					$$.val.type = DataType::INT;
				}
			|	DATE
				{
					printf("YACC: type data\n");
					$$.val.type = DataType::DATE;
				}
			|	FLOAT
				{
					printf("YACC: type float\n");
					$$.val.type = DataType::FLOAT;
				}
			|	CHAR '(' INT_LIT ')'
				{
					printf("YACC: char\n");
					$$.val.type = DataType::CHAR;
					int intVal = 0;
					uchar code = Field::strToInt($3.val.str, intVal);
					if(code != 0 || intVal < 0 || intVal > 255){
						Global::IllegalCharLength($3.pos);
						YYABORT;
					}
					else
						*(int*)$$.val.bytes = intVal;
				}
			|	VARCHAR	'(' INT_LIT ')'
				{
					printf("YACC: varchar\n");
					$$.val.type = DataType::VARCHAR;
					int intVal = 0;
					uchar code = Field::strToInt($3.val.str, intVal);
					if(code != 0 || intVal < 0 || intVal > 65535){
						Global::IllegalVarcharLength($3.pos);
						YYABORT;
					}
					else
						*(int*)$$.val.bytes = intVal;
				}
			|	NUMERIC '(' INT_LIT ',' INT_LIT ')'
				{
					printf("YACC: numeric\n");
					$$.val.type = DataType::NUMERIC;
					int intVal = 0, intVal0 = 0;
					uchar code = Field::strToInt($3.val.str, intVal), code0 = Field::strToInt($5.val.str, intVal0);
					if(code != 0 || intVal < 1 || intVal > 38){
						Global::IllegalNumericPrecision($3.pos);
						YYABORT;
					}
					else if(code0 != 0 || intVal0 < 0 || intVal0 > intVal){
						Global::IllegalNumericScale($5.pos);
						YYABORT;
					}
					*(uint*)$$.val.bytes = (intVal << 8) + intVal0; // p, s are stored in val
				}
			| 	NUMERIC
				{
					printf("YACC: default numeric\n");
					$$.val.type = DataType::NUMERIC;
					*(uint*)$$.val.bytes = 18 << 8; // p, s默认值是18和0
				}
			;

// insert into {tablename} values(valueList), (valueList), (valueList) ...
valueLists	:	'(' valueList ')'
				{
					printf("YACC: valueLists base\n");
					$$.valLists.push_back($2.valList);
				}
			|	valueLists ',' '(' valueList ')'
				{
					printf("YACC: valueLists recur\n");
					$$.valLists = $$.valLists;
					$$.valLists.push_back($4.valList);
				}
			;

valueList	:	value
				{
					printf("YACC: valueList base\n");
					$$.valList.push_back($1.val);
				}
			|	valueList ',' value
				{
					printf("YACC: valueList recur\n");
					$$.valList = $1.valList;
					$$.valList.push_back($3.val);
				}
			;

value		:	INT_LIT
				{
					printf("YACC: int lit\n");
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
							Global::IntConversionFailed($1.pos);
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
					printf("YACC: float lit\n");
					$$.val.type = DataType::FLOAT;
					float floatVal = 0;
					uchar code = Field::strToFloat($1.val.str, floatVal);
					if(code != 0){
						Global::FloatConversionFailed($1.pos);
						YYABORT;
					}
					else
						*(float*)$$.val.bytes = floatVal;
				}
			|	DATE_LIT
				{
					printf("YACC: date lit\n");
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
						Global::DateConversionFailed($1.pos);
						YYABORT;
					}
				}
			| 	STRING_LIT
				{
					printf("YACC: string lit\n");
					if($1.val.str.length() <= 255)
						$$.val.type = DataType::CHAR;
					else if($1.val.str.length() <= 65535)
						$$.val.type = DataType::VARCHAR;
					else{
						Global::StringConversionFailed($1.pos);
						YYABORT;
					}
				}
			|	KW_NULL
				{
					printf("YACC: null lit\n");
					$$.val.type = DataType::NONE; // use NONE to record null lit
				}
			| 	'-' value // minus: for float, int and bigint
				{
					printf("YACC: minus\n");
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
						Global::IllegalMinus($1.pos);
						YYABORT;
					}
					$$.val = $2.val;
				}
			;

whereClause :	whereClause AND whereUnit
				{
					printf("YACC: where recur\n");
					$$.condList = $1.condList;
					$$.condList.push_back($3.condList[0]);
				}
			|	whereUnit
				{
					printf("YACC: where base\n");
					$$.condList = $1.condList;
				}

whereUnit	:	col op expr
				{
					printf("YACC: if op\n");
					WhereInstr tmp;
					tmp.column = $1.column;
					tmp.isExprCol = $3.isExprCol;
					tmp.cmp = $2.cmp;
					if(tmp.isExprCol)
						tmp.exprCol = $3.column;
					else{
						if($3.val.type == DataType::NONE){ // 如果出现col op NULL的情况,直接返回,不可能得到任何符合要求的记录
							Global::ComparisonToNull($3.pos);
							YYABORT;
						}
						tmp.exprVal = $3.val;
					}
					$$.condList.push_back(tmp);
				}
			|	col IS KW_NULL
				{
					printf("YACC: if null\n");
					WhereInstr tmp;
					tmp.column = $1.column;
					tmp.cmp = Comparator::Eq;		   // equals
					tmp.exprVal.type = DataType::NONE; // null
					$$.condList.push_back(tmp);
				}
			|	col IS NOT KW_NULL
				{
					printf("YACC: if not null\n");
					WhereInstr tmp;
					tmp.column = $1.column;
					tmp.cmp = Comparator::NE;		   // not equals
					tmp.exprVal.type = DataType::NONE; // null
					$$.condList.push_back(tmp);
				}
			;

col			:	IDENTIFIER
				{
					printf("YACC: simple col\n");
					if($1.val.str.length() > MAX_ATTRI_NAME_LEN){
						Global::FieldNameTooLong($1.pos);
						YYABORT;
					}
					$$.column.colName = $1.val.str;
				}
			|	IDENTIFIER '.' IDENTIFIER
				{
					printf("YACC: compl col\n");
					if($1.val.str.length() > MAX_TABLE_NAME_LEN){
						Global::TableNameTooLong($1.pos);
						YYABORT;
					}
					$$.column.tableName = $1.val.str;
					if($3.val.str.length() > MAX_ATTRI_NAME_LEN){
						Global::FieldNameTooLong($3.pos);
						YYABORT;
					}
					$$.column.colName = $3.val.str;
				}
			;

op			:	GE
				{
					printf("YACC: GE\n");
					$$.cmp = Comparator::GtEq;
				}
			|	LE
				{
					printf("YACC: LE\n");
					$$.cmp = Comparator::LtEq;
				}
			|	NE
				{
					printf("YACC: NE\n");
					$$.cmp = Comparator::NE;
				}
			|	'>'
				{
					printf("YACC: Gt\n");
					$$.cmp = Comparator::Gt;
				}
			|	'<'
				{
					printf("YACC: Lt\n");
					$$.cmp = Comparator::Lt;
				}
			|	'='
				{
					printf("YACC: eq\n");
					$$.cmp = Comparator::Eq;
				}
			;

expr		:	value
				{
					printf("YACC: value expr\n");
					$$.isExprCol = false;
					$$.val = $1.val;
				}
			|	col
				{
					printf("YACC: col expr\n");
					$$.isExprCol = true;
					$$.column = $1.column;
				}
			;

setClause	:	IDENTIFIER '=' value
				{
					printf("YACC: set\n");
					$$.setList.push_back(SetInstr($1.val.str, $3.val));
				}
			|	setClause ',' IDENTIFIER '=' value
				{
					printf("YACC: multiple set\n");
					$$.setList = $1.setList;
					$$.setList.push_back(SetInstr($3.val.str, $5.val));
				}
			;

selector	:	'*'
				{
					printf("YACC: select *\n");
					$$.selectAll = true;
				}
			|	colList
				{
					printf("YACC: select colList\n");
					$$.colList = $1.colList;
				}
			;

colList		:	col
				{
					printf("YACC: colList base\n");
					$$.colList.push_back($1.column);
				}
			| 	colList ',' col
				{
					printf("YACC: colList recur\n");
					$$.colList = $1.colList;
					$$.colList.push_back($3.column);
				}
			;

IdList		:	IDENTIFIER
				{
					printf("YACC: IdList base\n");
					$$.IDList.push_back($1.val.str);
				}
			|	IdList ',' IDENTIFIER
				{
					printf("YACC: IdList recur\n");
					$$.IDList = $1.IDList;
					$$.IDList.push_back($3.val.str);
				}
			;

%%
void yyerror(const char *s)			//当yacc遇到语法错误时，会回调yyerror函数，并且把错误信息放在参数s中
{
	printf("Error: %s near %d\n", s, Global::pos);					//直接输出错误信息
	Global::errorSign = true;
}