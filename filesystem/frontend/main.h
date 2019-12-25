//lex.l和yacc.y共同使用的头文件
#ifndef MAIN_HPP
#define MAIN_HPP

#include <string>
#include <set>
#include <stdio.h>
#include <vector>
#include <algorithm>
#include "../utils/pagedef.h" // 全局宏定义
#include "Global.h" // lexer的辅助静态类
#include "../RM/DataType.h"
#include "Field.h"
#include "../MyDB/DBMS.h"
#include "../RM/SimpleUtils.h"
#include "../indexing/BplusTree.h"

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



struct Debugger{
	static bool debug(std::vector<Type> &typeVec){
		Type &T1 = typeVec[0], &T2 = typeVec[1], &T4 = typeVec[2], &T6 = typeVec[3];
		if(!Global::dbms->CurrentDatabase()){
			Global::newError(T1.pos, "No available database");
			return false;
		}
		if(T2.val.str.length() > MAX_TABLE_NAME_LEN){
			Global::newError(T2.pos, Global::format("Table name should be no longer than %d", MAX_TABLE_NAME_LEN));
			return false;
		}
		Table* table = Global::dbms->CurrentDatabase()->OpenTable(T2.val.str.data());
		if(!table){
			Global::newError(T2.pos, Global::format("No table named %s", T2.val.str.data()));
			return false;
		}
		// check setClause validity
		std::vector<SetHelper> setHelpers;
		for(auto set_it = T4.setList.begin(); set_it != T4.setList.end(); set_it++){
			if(set_it->colName.length() > MAX_ATTRI_NAME_LEN){
				Global::newError(T2.pos, Global::format("Field name should be no longer than %d", MAX_ATTRI_NAME_LEN));
				Global::dbms->CurrentDatabase()->CloseTable(T2.val.str.data());
				return false;
			}
			uchar setColID = table->IDofCol(set_it->colName.data());
			if(setColID == COL_ID_NONE){
				Global::newError(T4.pos, Global::format("Unknown field %s", set_it->colName.data()));
				Global::dbms->CurrentDatabase()->CloseTable(T2.val.str.data());
				return false;
			}
			SetHelper helper;
			if(!Type::ConvertValue(helper.value, table->GetHeader()->attrType[setColID], table->GetHeader()->attrLenth[setColID], set_it->value, getBitFromLeft(table->GetHeader()->nullMask, setColID))){
				Global::newError(T4.pos, Global::format("Incompatible type for field %.*s", MAX_ATTRI_NAME_LEN, table->GetHeader()->attrName[setColID]));
				Global::dbms->CurrentDatabase()->CloseTable(T2.val.str.data());
				return false;
			}
			// TODO: check foreign constraints
			helper.colID = setColID;
			setHelpers.push_back(helper);
		}
		// check whereClause validity
		std::vector<SelectHelper> helpers;
		for(auto where_it = T6.condList.begin(); where_it != T6.condList.end(); where_it++){
			if(where_it->column.tableName.length() && where_it->column.tableName != T2.val.str){
				Global::newError(T6.pos, Global::format("Irrelevant table %s", where_it->column.tableName.data()));
				Global::dbms->CurrentDatabase()->CloseTable(T2.val.str.data());
				return false;
			}
			if(where_it->isExprCol && where_it->exprCol.tableName.length() && where_it->exprCol.tableName != T2.val.str){
				Global::newError(T6.pos, Global::format("Irrelevant table %s", where_it->exprCol.tableName.data()));
				Global::dbms->CurrentDatabase()->CloseTable(T2.val.str.data());
				return false;
			}
			uchar colIDLeft = table->IDofCol(where_it->column.colName.data());
			if(colIDLeft == COL_ID_NONE){
				Global::newError(T6.pos, Global::format("Unknown field %s", where_it->column.colName.data()));
				Global::dbms->CurrentDatabase()->CloseTable(T2.val.str.data());
				return false;
			}
			uchar colIDRight = COL_ID_NONE;
			if(where_it->isExprCol){
				colIDRight = table->IDofCol(where_it->exprCol.colName.data());
				if(colIDRight == COL_ID_NONE){
					Global::newError(T6.pos, Global::format("Unknown field %s", where_it->exprCol.colName.data()));
					Global::dbms->CurrentDatabase()->CloseTable(T2.val.str.data());
					return false;
				}
			}
			SelectHelper helper;
			helper.leftColID = colIDLeft;
			helper.rightColID = colIDRight;
			helper.cmp = where_it->cmp;
			if(!where_it->isExprCol){ // col = value, convert value into binary
				if(!Type::ConvertValue(helper.val, table->GetHeader()->attrType[colIDLeft], table->GetHeader()->attrLenth[colIDLeft], 
					where_it->exprVal, getBitFromLeft(table->GetHeader()->nullMask, colIDLeft))){
						Global::newError(T6.pos, Global::format("Incompatible type for field %.*s", MAX_ATTRI_NAME_LEN, table->GetHeader()->attrName[colIDLeft]));
						Global::dbms->CurrentDatabase()->CloseTable(T2.val.str.data());
						return false;
					}
			}
			else{ // col = col, check type compatility
				if(!DataType::Comparable(table->GetHeader()->attrType[colIDLeft], table->GetHeader()->attrType[colIDRight])){
					Global::newError(T6.pos, Global::format("Field %.*s and %.*s are incomparable", MAX_ATTRI_NAME_LEN, table->GetHeader()->attrName[colIDLeft], 
						MAX_ATTRI_NAME_LEN, table->GetHeader()->attrName[colIDRight]));
					Global::dbms->CurrentDatabase()->CloseTable(T2.val.str.data());
					return false;
				}
			}
			helper.hasRightCol = where_it->isExprCol;
			helpers.push_back(helper);
		}
		// build scanner
		Scanner* scanner = table->GetScanner(nullptr);
		uchar cmps[table->ColNum()] = {0}; // Comparator::Any is 0
		std::string constantValue;
		for(auto helper_it = helpers.begin(); helper_it != helpers.end(); helper_it++){
			if(!helper_it->hasRightCol){ // 'col = value' style
				uchar leftColType = table->GetHeader()->attrType[helper_it->leftColID];
				ushort leftColLength = table->GetHeader()->attrLenth[helper_it->leftColID];
				if(leftColType == DataType::CHAR || leftColType == DataType::VARCHAR){
					constantValue.append(helper_it->val.str);
					constantValue.append(leftColLength - helper_it->val.str.length(), 0); // padding
				}
				else
					constantValue.append((char*)helper_it->val.bytes, DataType::lengthOf(leftColType, leftColLength));
				cmps[helper_it->leftColID] = helper_it->cmp;
			}
			else{ // 'col = col' style
				scanner->AddSelfCmp(helper_it->leftColID, helper_it->rightColID, helper_it->cmp);
			}
		}
		scanner->SetDemand((uchar*)constantValue.data(), table->ColNum(), cmps);
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
		Global::dbms->CurrentDatabase()->CloseTable(T2.val.str.data());
		return true;
	}
};

#define YYSTYPE Type//把YYSTYPE(即yylval变量)重定义为struct Type类型，这样lex就能向yacc返回更多的数据了

#endif
