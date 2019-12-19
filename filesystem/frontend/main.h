//lex.l和yacc.y共同使用的头文件
#ifndef MAIN_HPP
#define MAIN_HPP

#include <string>
#include <set>
#include <stdio.h>
#include <vector>
#include "../utils/pagedef.h" // 全局宏定义
#include "Global.h" // lexer的辅助静态类
#include "../RM/DataType.h"
#include "Field.h"
#include "../MyDB/DBMS.h"
#include "../RM/SimpleUtils.h"

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
		Type &TT = typeVec[0], &T1 = typeVec[1], &T3 = typeVec[2], &T5 = typeVec[3];
		if(Global::dbms->CurrentDatabase() == nullptr){
			Global::newError(T1.pos, "No available database");
			return false;
		}
		if(T3.val.str.length() > MAX_TABLE_NAME_LEN){
			Global::newError(T3.pos, Global::format("Table name should be no longer than %d", MAX_TABLE_NAME_LEN));
			return false;
		}
		Table* table = Global::dbms->CurrentDatabase()->OpenTable(T3.val.str.data());
		if(!table){
			Global::newError(T3.pos, Global::format("No table named %s", T3.val.str.data()));
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
					Global::newError(T5.pos, Global::format("Incompatible field num, %d expected, %d given", arg_pos, list_it->size()));
					Global::dbms->CurrentDatabase()->CloseTable(T3.val.str.data()); // remember to close table before exit
					return false;
				}
				if(!Type::ConvertValue(TT.val, header->attrType[arg_pos], header->attrLenth[arg_pos], *arg_it, getBitFromLeft(header->nullMask, arg_pos))){
					Global::newError(T5.pos, Global::format("Incompatible type for field %s.%d", header->attrName[arg_pos], MAX_ATTRI_NAME_LEN));
					Global::dbms->CurrentDatabase()->CloseTable(T3.val.str.data());
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
					Global::newError(T5.pos, Global::format("No default value for field %s.%d", header->attrName[arg_pos], MAX_ATTRI_NAME_LEN));
					Global::dbms->CurrentDatabase()->CloseTable(T3.val.str.data());
					return false;
				}
				else{
					if(getBitFromLeft(*tmpRec.GetData(), arg_pos)){ // default value is null
						TT.val.type = DataType::NONE;
					}
					else{
						memcpy(TT.val.bytes, tmpRec.GetData() + table->ColOffset(arg_pos), DataType::lengthOf(header->attrType[arg_pos], header->attrLenth[arg_pos]));
						// NOTE: default value is long varchar -> insert a COPY of the long varchar
					}
					TT.valList.push_back(TT.val);
				}
			}
			TT.valLists.push_back(TT.valList);
			TT.valList.clear();
		}
		// TODO: primary / foreign key constraint
		// TODO: maintain index
		// now there is no error in input, start insertion
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
				else
					memcpy(tmpRec.GetData() + table->ColOffset(arg_pos), arg_it->bytes, DataType::lengthOf(header->attrType[arg_pos], header->attrLenth[arg_pos]));
				arg_pos++;
			}
			table->InsertRecord(tmpRec.GetData(), &tmpRID);
		}
		Global::dbms->CurrentDatabase()->CloseTable(T3.val.str.data());
		return true;
	}
};


#define YYSTYPE Type//把YYSTYPE(即yylval变量)重定义为struct Type类型，这样lex就能向yacc返回更多的数据了

#endif
