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
// TODO: 查询条件的优化,如col = col <==> col is not null

struct Constraint{
	std::vector<std::string> IDList;
	// for fk constraints
	std::string masterName;
	std::vector<std::string> masterIDList;
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
};

#endif