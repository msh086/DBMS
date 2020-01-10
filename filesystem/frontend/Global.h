#ifndef GLOBAL_H
#define GLOBAL_H
#include<string>
#include<vector>
#include<cstring>
#include<stdio.h>
#include<stdarg.h>
#include "Type.h"
#include "../MyDB/DBMS.h"

struct Error{
	int pos;
	std::string msg;
	Error(int pos, const std::string& str){
		this->pos = pos;
		this->msg = str;
	}
};

class Global{
		static const char* _format(char* dst, const char* formatStr, ...){
			va_list ap;
			va_start(ap, formatStr);
			vsnprintf(dst, 100, formatStr, ap);
			va_end(ap);
			return dst;
			// ref: https://blog.csdn.net/LaineGates/article/details/50571719
		}
	public:
		static std::string buffer; // 接受字符串时的缓冲区
		static std::vector<Error> errors; // 遇到的错误
		static int pos; // 当前位置
		static DBMS* dbms;
		static char formatBuffer[100];
		static bool exitSign;
		static std::vector<Type> types;
		static bool errorSign;
		static bool (*action)(std::vector<Type>& types);
		static std::vector<BplusTree*> trees; // trees和scanners都是用来管理内存的
		static std::vector<Scanner*> scanners;
		static BplusTree* globalTree; // update中,判断update后是否存在主键冲突,需要用到set,比较函数若为Lambda,则不能捕获任何变量

		static void freeMemory(){
			for(auto it = trees.begin(); it != trees.end(); it++)
				delete *it;
			trees.clear();
			for(auto it = scanners.begin(); it != scanners.end(); it++)
				delete *it;
			scanners.clear();
		}

		static void newError(int pos, const char* str){
			errors.push_back(Error(pos, std::string(str)));
		}
		static const char* format(const char* formatStr, ...){
			va_list ap;
			va_start(ap, formatStr);
			vsnprintf(formatBuffer, 100, formatStr, ap);
			va_end(ap);
			return formatBuffer;
		}
		// 预置的错误类型
		// database level
		static void NoActiveDb(int pos){
			newError(pos, "No available database");
		}
		static void DbNameTooLong(int pos){
			newError(pos, format("Database name should be no longer than %d", MAX_DB_NAME_LEN));
		}
		static void DbNameConflict(int pos){
			newError(pos, "Database with the same name already exists");
		}
		static void NoSuchDb(int pos, const char* name){
			newError(pos, format("No database named %s", name));
		}

		// table level
		static void TableNameTooLong(int pos){
			newError(pos, format("Table name should be no longer than %d", MAX_TABLE_NAME_LEN));
		}
		static void NoSuchTable(int pos, const char* name){
			newError(pos, format("No table named %s", name));
		}
		static void IrrelevantTable(int pos, const char* name){
			newError(pos, format("Irrelevant table %s", name));
		}
		static void TableNameConflict(int pos){
			newError(pos, "Table with the same name already exists");
		}
		static void TooManyFields(int pos){
			newError(pos, format("Number of columns in a tables should be not more than %d", MAX_COL_NUM));
		}
		static void FieldNumNotMatch(int pos, int expected, int given){
			newError(pos, format("Incompatible field num, %d expected, %d given", expected, given));
		}
		static void TableNameReserved(int pos, const char* name){
			newError(pos, format("Table name %s is reserved for DBMS", name));
		}

		// field level
		static void NoSuchField(int pos, const char* name){
			newError(pos, format("Unknown field %s", name));
		}
		static void FieldNameTooLong(int pos){
			newError(pos, format("Field name should be no longer than %d", MAX_ATTRI_NAME_LEN));
		}
		static void IncompatibleTypes(int pos, const void* name){
			newError(pos, format("Incompatible type for field %.*s", MAX_ATTRI_NAME_LEN, (const char*)name));
		}
		static void IncomparableFields(int pos, const uchar* left, const uchar* right){
			newError(pos, format("Field %.*s and %.*s are incomparable", MAX_ATTRI_NAME_LEN, left, MAX_ATTRI_NAME_LEN, right));
		}
		static void FieldNameConflict(int pos, const char* name){
			newError(pos, format("Field with name %s already exists", name));
		}

		// constraints
		static void MultiplePrimaryKey(int pos){
			newError(pos, "Multiple primary key declaration");
		}
		static void TooManySlaveTime(int pos){
			newError(pos, format("A table can reference other tables in foreign key constraint no more than %d times", MAX_REF_SLAVE_TIME));
		}
		static void NoDefaultValue(int pos, const uchar* name){
			newError(pos, format("No default value for field %.*s", MAX_ATTRI_NAME_LEN, name));
		}
		static void MasterFieldNotPrimary(int pos){
			newError(pos, "No corresponding primary key in master table");
		}
		static void DuplicateFieldInConstraint(int pos){
			newError(pos, "A field cannot appear more than once in a constraint");
		}
		static void ForeignKeyNotFound(int pos, const uchar* name){
			newError(pos, format("Key combination not found in foreign key master table %s", name));
		}
		static void PrimaryKeyConflict(int pos){
			newError(pos, "Primary key conflict");
		}
		static void NullPrimaryKey(int pos){
			newError(pos, "Primary key cannot be null");
		}
		static void MasterTypeNotMatch(int pos){
			newError(pos, "Fields in slave table should have identical types as primary keys in master table");
		}
		static void MasterFieldNumNotMatch(int pos){
			newError(pos, "Slave field num not equal to master primary field num");
		}

		// type
		static void IllegalCharLength(int pos){
			newError(pos, "Length for type CHAR should be between 0 and 255");
		}
		static void IllegalVarcharLength(int pos){
			newError(pos, "Length for type VARCHAR should be between 0 and 65535");
		}
		static void IllegalNumericPrecision(int pos){
			newError(pos, "NUMERIC precision should be between 1 and 38");
		}
		static void IllegalNumericScale(int pos){
			newError(pos, "NUMERIC scale should be between 0 and NUMERIC precision");
		}

		// literal
		static void IntConversionFailed(int pos){
			newError(pos, "Value cannot be converted to INT");
		}
		static void FloatConversionFailed(int pos){
			newError(pos, "Value cannot be converted to FLOAT");
		}
		static void DateConversionFailed(int pos){
			newError(pos, "Wrong format for DATE type");
		}
		static void StringConversionFailed(int pos){
			newError(pos, "String is too long, it should be no longer than 65535 bytes");
		}
		static void IllegalMinus(int pos){
			newError(pos, "Can only apply '-' to INT, BIGINT and FLOAT type");
		}
		static void NotCharacter(int pos){
			newError(pos, "Not a character");
		}

		// index
		static void TooManyIndexes(int pos){
			newError(pos, format("A table can have no more than %d indexes", MAX_INDEX_NUM));
		}
		static void IndexNameTooLong(int pos){
			newError(pos, format("Index name should be no longer than %d", MAX_INDEX_NAME_LEN));
		}
		static void IndexNameConflict(int pos, const char* name){
			newError(pos, format("Index with name %s already exists", name));
		}
		static void IndexNameReserved(int pos, const char* name){
			newError(pos, format("Index name %s is reserved for DBMS", name));
		}
		static void NoSuchIndex(int pos, const char* name){
			newError(pos, format("No index named %s", name));
		}
		
		// others
		static void ComparisonToNull(int pos){
			newError(pos, "'field op null' will match nothing");
		}
		static void FileNotFound(int pos, const char* name){
			newError(pos, format("File named %s not found", name));
		}
		static void MultipleSetForField(int pos){
			newError(pos, "Cannot assign a field twice");
		}
		static void AmbiguousField(int pos, const char* name){
			newError(pos, format("Field %s has multiple candidates", name));
		}
};

#endif