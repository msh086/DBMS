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



struct Col{
	std::string tableName;
	std::string colName;
	void Reset(){
		tableName.clear();
		colName.clear();
	}
};

struct SetInstr{
	std::string colName;
	Val value;
	SetInstr(const std::string& colName, const Val& value){
		this->colName = colName;
		this->value = value;
	}
	void Reset(){
		colName.clear();
		value.Reset();
	}
};

struct SetHelper{
	uchar colID;
	Val value;
};

struct WhereInstr{
	Col column;
	Col exprCol;
	Val exprVal;
	uchar cmp;
	bool isExprCol = false;
	void Reset(){
		column.Reset();
		exprCol.Reset();
		exprVal.Reset();
		cmp = Comparator::Any;
		isExprCol = false;
	}
};

struct SelectHelper{
	uchar leftColID;
	uchar rightColID;
	Val val;
	uchar cmp = DataType::NONE;
	bool hasRightCol = false;
};

struct Constraint{
	std::vector<std::string> IDList;
	bool isFK = false;
	void Reset(){
		IDList.clear();
		isFK = false;
	}
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
	// column
	Col column;
	// select ... from ... where col op {col | expr}, expr := {col | value}
	bool isExprCol = false;
	// update ... set ...
	std::vector<SetInstr> setList;
	// select col, col ...
	std::vector<Col> colList;
	bool selectAll = false; // select *
	// ID list, could be list of tablenames or columnName
	std::vector<std::string> IDList;
	// ... where x op x and x op x and ...
	std::vector<WhereInstr> condList;
	// constraint type
	std::vector<Constraint> constraintList;
	// 偷懒的方法:用来保证只有parse到最顶层时才会执行sql.同时使代码不至于都挤到一起
	std::vector<Type> typeBuf;
	bool (*action)(std::vector<Type>& typeVec) = nullptr;

	/**
	 * Do nothing
	 * Uncomment statements in this function when you suspect yacc is reusing objects
	*/
	void Reset(){
		// pos = 0;
		// fieldList.clear();
		// valLists.clear();
		// valList.clear();
		// val.Reset();
		// cmp = Comparator::Any;
		// column.Reset();
		// isExprCol = false;
		// setList.clear();
		// colList.clear();
		// selectAll = false;
		// IDList.clear();
		// condList.clear();
		// constraintList.clear();
		// typeBuf.clear();
		// action = nullptr;
	}
	/**
	 * 将 @param value 的值转换为 @param type 类型,并存储到dst内
	 * 返回转换是否成功
	 * @param length 应为原始长度,不是内存长度
	 * NOTE: dst.type会被赋值为value.type,即目标Val的类型是源Val类型,而非字段类型
	*/
	static bool ConvertValue(Val& dst, uchar type, ushort length, const Val& value, bool nullable){
		dst.type = value.type;
		// decide subtype
		if(value.type == DataType::NONE){ // value is null
			if(nullable)
				return true;
			else
				return false;
		}
		if(value.type == DataType::INT && type == DataType::BIGINT){
			*(ll*)dst.bytes = *(int*)value.bytes;
		}
		else if(value.type == DataType::INT && type == DataType::FLOAT){
			*(float*)dst.bytes = *(int*)value.bytes;
		}
		else if(value.type == DataType::FLOAT && type == DataType::NUMERIC){
			int byteNum = DataType::lengthOf(DataType::NUMERIC, length);
			DataType::floatToBin(*(const float*)value.bytes < 0, value.str.data(), value.str.find('.'), dst.bytes, length >> 8, length & 0xff);
		}
		else if(value.type == DataType::INT && type == DataType::NUMERIC){
			int byteNum = DataType::lengthOf(DataType::NUMERIC, length);
			DataType::floatToBin(*(const int*)value.bytes < 0, value.str.data(), value.str.length(), dst.bytes, length >> 8, length & 0xff);
		}
		else if(value.type == DataType::BIGINT && type == DataType::NUMERIC){
			int byteNum = DataType::lengthOf(DataType::NUMERIC, length);
			DataType::floatToBin(*(ll*)value.bytes < 0, value.str.data(), value.str.length(), dst.bytes, length >> 8, length & 0xff);
		}
		else if(value.type == DataType::CHAR && type == DataType::VARCHAR && value.str.length() <= length){
			dst.str = value.str;
		}
		else if(value.type == DataType::CHAR && type == DataType::CHAR && value.str.length() <= length){
			dst.str = value.str;
		}
		else if(value.type == DataType::VARCHAR && type == DataType::VARCHAR && value.str.length() <= length){
			dst.str = value.str;
		}
		else if(value.type == type){
			if(type == DataType::INT){
				*(int*)dst.bytes = *(int*)value.bytes;
			}
			else if(type == DataType::BIGINT){
				*(ll*)dst.bytes = *(ll*)value.bytes;
			}
			else if(type == DataType::FLOAT){
				*(float*)dst.bytes = *(float*)value.bytes;
			}
			else if(type == DataType::DATE){
				memcpy(dst.bytes, value.bytes, 3);
			}
			else
				return false;
			// a value will only be parsed as float, not numeric
			// varchar and char are solved previously, char and varchar with incompatible length will reach here
		}
		else
			return false;
		return true;
	}

	static bool debug(std::vector<Type> &typeVec){
		Type &T2 = typeVec[0], &T4 = typeVec[1], &T6 = typeVec[2];
		int selectNum = T2.colList.size();
		if(T4.IDList.size() == 1){ // select from one table
			if(!Global::dbms->CurrentDatabase()){
				Global::newError(T4.pos, "No available database");
				return false;
			}
			if(T4.IDList[0].length() > MAX_TABLE_NAME_LEN){
				Global::newError(T4.pos, Global::format("Table name should be no longer than %d", MAX_TABLE_NAME_LEN));
				return false;
			}
			Table* table = Global::dbms->CurrentDatabase()->OpenTable(T4.IDList[0].data());
			if(!table){
				Global::newError(T4.pos, Global::format("No table named %d", T4.IDList[0].data()));
				return false;
			}
			// check selector
			std::vector<uchar> wantedCols;
			if(!T2.selectAll){ // not select*
				for(auto col_it = T2.colList.begin(); col_it != T2.colList.end(); col_it++){
					if(col_it->tableName.length() && col_it->tableName != T4.IDList[0]){
						Global::newError(T2.pos, Global::format("Irrelevant table %s", col_it->tableName.data()));
						Global::dbms->CurrentDatabase()->CloseTable(T4.IDList[0].data());
						return false;
					}
					uchar tmpColID = table->IDofCol(col_it->colName.data());
					if(tmpColID == COL_ID_NONE){
						Global::newError(T2.pos, Global::format("Unknown field %s", col_it->colName.data()));
						Global::dbms->CurrentDatabase()->CloseTable(T4.IDList[0].data());
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
			std::vector<SelectHelper> helpers;
			for(auto where_it = T6.condList.begin(); where_it != T6.condList.end(); where_it++){
				if(where_it->column.tableName.length() && where_it->column.tableName != T4.IDList[0]){
					Global::newError(T6.pos, Global::format("Irrelevant table %s", where_it->column.tableName.data()));
					Global::dbms->CurrentDatabase()->CloseTable(T4.IDList[0].data());
					return false;
				}
				if(where_it->isExprCol && where_it->exprCol.tableName.length() && where_it->exprCol.tableName != T4.IDList[0]){
					Global::newError(T6.pos, Global::format("Irrelevant table %s", where_it->exprCol.tableName.data()));
					Global::dbms->CurrentDatabase()->CloseTable(T4.IDList[0].data());
					return false;
				}
				uchar colIDLeft = table->IDofCol(where_it->column.colName.data());
				if(colIDLeft == COL_ID_NONE){
					Global::newError(T6.pos, Global::format("Unknown field %s", where_it->column.colName.data()));
					Global::dbms->CurrentDatabase()->CloseTable(T4.IDList[0].data());
					return false;
				}
				uchar colIDRight = COL_ID_NONE;
				if(where_it->isExprCol){
					colIDRight = table->IDofCol(where_it->exprCol.colName.data());
					if(colIDRight == COL_ID_NONE){
						Global::newError(T6.pos, Global::format("Unknown field %s", where_it->exprCol.colName.data()));
						Global::dbms->CurrentDatabase()->CloseTable(T4.IDList[0].data());
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
							Global::dbms->CurrentDatabase()->CloseTable(T4.IDList[0].data());
							return false;
						}
				}
				else{ // col = col, check type compatility
					if(!DataType::Comparable(table->GetHeader()->attrType[colIDLeft], table->GetHeader()->attrType[colIDRight])){
						Global::newError(T6.pos, Global::format("Field %.*s and %.*s are incomparable", MAX_ATTRI_NAME_LEN, table->GetHeader()->attrName[colIDLeft], 
							MAX_ATTRI_NAME_LEN, table->GetHeader()->attrName[colIDRight]));
						Global::dbms->CurrentDatabase()->CloseTable(T4.IDList[0].data());
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
			// Print
			scanner->PrintSelection(wantedCols);
			delete scanner;
		}
		else{
			// TODO: multi-table query ?
		}
		return true;
	}
};

#define YYSTYPE Type//把YYSTYPE(即yylval变量)重定义为struct Type类型，这样lex就能向yacc返回更多的数据了

#endif
