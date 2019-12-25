#ifndef TYPE_H
#define TYPE_H
#include <string>
#include <vector>
#include "Field.h"

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
};

#endif