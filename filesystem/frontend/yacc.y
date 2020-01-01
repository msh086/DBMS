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
					// TODO: load table from file
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
						for(auto it = T5.constraintList.begin(); it != T5.constraintList.end(); it++){
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
									if(master->GetHeader()->primaryKeyID[primaryPos++] != masterColIndex){
										Global::MasterFieldNotPrimary(T5.pos);
										return false;
									}
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
					// Global::action = Debugger::debug;
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
						RID tmpRID(1, 0);
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
						// check primary key constraint and store generated index records for later insertion
						std::vector<std::string> idxRecords;
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
						// chekc primary key constraint
						uchar buf[idxRecLength];
						BplusTree* primaryIdx = nullptr;
						if(primaryColCount){
							uint idxPage = table->GetPrimaryIndexPage();
							assert(idxPage);
							primaryIdx = new BplusTree(Global::dbms->CurrentDatabase()->idx, idxPage);
							Global::dbms->CurrentDatabase()->TakeCareOfTree(primaryIdx);
							const uchar* vals[primaryColCount] = {0};
							for(auto list_it = TT.valLists.begin(); list_it != TT.valLists.end(); list_it++){ // for every insertion
								memset(vals, 0, sizeof(vals));
								memset(buf, 0, sizeof(buf));
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
								RID tmpRID;
								if(primaryIdx->SafeValueSearch(buf, &tmpRID)){
									Global::PrimaryKeyConflict(T5.pos);
									return false;
								}
								idxRecords.push_back(std::string((char*)buf, sizeof(buf)));
							}
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
							table->InsertRecord(tmpRec.GetData(), &tmpRID);
							// TODO: update other indexes beside primary index
							if(primaryIdx){
								primaryIdx->SafeInsert((const uchar*)idxRecords[idxRecPos++].data(), tmpRID);
							}
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
						if(!ParsingHelper::checkWhereClause(helpers, whereHelpersCol, cmpUnitsNeeded, T3.val.str, T5, table))
							return false;
						// TODO: use index where possible
						// build scanner
						Scanner* scanner = ParsingHelper::buildScanner(table, helpers, whereHelpersCol, cmpUnitsNeeded);
						// delete
						// TODO: update index
						Record tmpRec;
						while(scanner->NextRecord(&tmpRec)){
							table->DeleteRecord(*tmpRec.GetRid());
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
								// Global::dbms->CurrentDatabase()->CloseTable(T2.val.str.data());
								return false;
							}
							uchar setColID = table->IDofCol(set_it->colName.data());
							if(setColID == COL_ID_NONE){
								Global::NoSuchField(T4.pos, set_it->colName.data());
								// Global::dbms->CurrentDatabase()->CloseTable(T2.val.str.data());
								return false;
							}
							SetHelper helper;
							if(!ParsingHelper::ConvertValue(helper.value, table->GetHeader()->attrType[setColID], table->GetHeader()->attrLenth[setColID], set_it->value, getBitFromLeft(table->GetHeader()->nullMask, setColID))){
								Global::IncompatibleTypes(T4.pos, table->GetHeader()->attrName[setColID]);
								// Global::dbms->CurrentDatabase()->CloseTable(T2.val.str.data());
								return false;
							}
							// TODO: check foreign constraints
							helper.colID = setColID;
							setHelpers.push_back(helper);
						}
						// check whereClause validity
						std::vector<SelectHelper> helpers;
						std::vector<SelectHelper> whereHelpersCol[table->ColNum()]; // FIXING
						int cmpUnitsNeeded = 0;
						if(!ParsingHelper::checkWhereClause(helpers, whereHelpersCol, cmpUnitsNeeded, T2.val.str, T6, table))
							return false;
						// TODO: use index where possible
						// build scanner
						Scanner* scanner = ParsingHelper::buildScanner(table, helpers, whereHelpersCol, cmpUnitsNeeded);
						// update
						Record tmpRec;
						while(scanner->NextRecord(&tmpRec)){
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
									}
									else{
										memcpy(tmpRec.GetData() + table->ColOffset(set_it->colID), set_it->value.bytes, 
											DataType::lengthOf(table->GetHeader()->attrType[set_it->colID], table->GetHeader()->attrType[set_it->colID]));
									}
								}
								table->UpdateRecord(*tmpRec.GetRid(), tmpRec.GetData(), 0, 0, table->GetHeader()->recordLenth);
								tmpRec.FreeMemory();
							}
						}
						delete scanner;
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
							if(!ParsingHelper::checkWhereClause(helpers, whereHelpersCol, cmpUnitsNeeded, T4.IDList[0], T6, table))
								return false;
							// TODO: use index where possible
							// build scanner
							Scanner* scanner = ParsingHelper::buildScanner(table, helpers, whereHelpersCol, cmpUnitsNeeded);
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
				}
			|	DROP INDEX IDENTIFIER
				{
					printf("YACC: drop idx\n");
				}
			|	ALTER TABLE IDENTIFIER ADD INDEX IDENTIFIER '(' IdList ')'
				{
					printf("YACC: alter add idx\n");
				}
			|	ALTER TABLE IDENTIFIER DROP INDEX IDENTIFIER
				{
					printf("YACC: alter drop idx\n");
				}
			;

AlterStmt	:	ALTER TABLE IDENTIFIER ADD field
				{
					printf("YACC: alter add col\n");
				}
			|	ALTER TABLE IDENTIFIER DROP IDENTIFIER
				{
					printf("YACC: alter drop col\n");
				}
			|	ALTER TABLE IDENTIFIER CHANGE IDENTIFIER field
				{
					printf("YACC: alter change field\n");
				}
			|	ALTER TABLE IDENTIFIER RENAME TO IDENTIFIER
				{
					printf("YACC: alter rename\n");
				}
			|	ALTER TABLE IDENTIFIER ADD PRIMARY KEY '(' colList ')'
				{
					printf("YACC: alter add primary\n");
				}
			|	ALTER TABLE IDENTIFIER DROP PRIMARY KEY
				{
					printf("YACC: alter drop primary\n");
				}
			|	ALTER TABLE IDENTIFIER ADD CONSTRAINT IDENTIFIER FOREIGN KEY '(' colList ')' REFERENCES IDENTIFIER '(' colList ')'
				{
					printf("YACC: alter add foreign\n");
				}
			|	ALTER TABLE IDENTIFIER DROP FOREIGN KEY IDENTIFIER
				{
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

// field可以组成fieldList用于建表,也可以在alter add里面用于增加列
// 名字长度已经经过检查
// field.length应该保留varchar的原始长度
field		:	IDENTIFIER type
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
			|	PRIMARY KEY '(' IdList ')'
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