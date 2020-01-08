//lex.l和yacc.y共同使用的头文件
#ifndef MAIN_H
#define MAIN_H

#include <string>
#include <set>
#include <stdio.h>
#include <vector>
#include <algorithm>
#include "../utils/pagedef.h" // 全局宏定义
#include "Global.h" // lexer的辅助静态类
#include "../RM/DataType.h"
#include "Field.h"
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

/**
 * yacc中调用的一系列静态函数,用于避免频繁的复制粘贴
*/
struct ParsingHelper{
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
	/**
	 * 检查where子句的合法性,并将结果汇总
	 * @param helpers 一个SelectHelper的向量,存储所有'col = col'类型的条件
	 * @param whereHelpersCol 一个SelectHelper向量的数组,为每个字段存储'col = value'类型的条件
	 * @param cmpUnitsNeeded 整数,Scanner中最多需要的CmpUnit个数
	 * @param tbname string类型,表名
	 * @param whereClause Type类型,对应where字句
	 * @param table 执行(update, delete, select)操作的表
	*/
	static bool checkWhereClause(std::vector<SelectHelper>& helpers, std::vector<SelectHelper>* whereHelpersCol, int& cmpUnitsNeeded, std::string& tbname, Type& whereClause, Table* table){
		for(auto where_it = whereClause.condList.begin(); where_it != whereClause.condList.end(); where_it++){
			if(where_it->column.tableName.length() && where_it->column.tableName != tbname){
				Global::IrrelevantTable(whereClause.pos, where_it->column.tableName.data());
				return false;
			}
			if(where_it->isExprCol && where_it->exprCol.tableName.length() && where_it->exprCol.tableName != tbname){
				Global::IrrelevantTable(whereClause.pos, where_it->exprCol.tableName.data());
				return false;
			}
			uchar colIDLeft = table->IDofCol(where_it->column.colName.data());
			if(colIDLeft == COL_ID_NONE){
				Global::NoSuchField(whereClause.pos, where_it->column.colName.data());
				return false;
			}
			uchar colIDRight = COL_ID_NONE;
			if(where_it->isExprCol){
				colIDRight = table->IDofCol(where_it->exprCol.colName.data());
				if(colIDRight == COL_ID_NONE){
					Global::NoSuchField(whereClause.pos, where_it->exprCol.colName.data());
					return false;
				}
			}
			SelectHelper helper;
			helper.leftColID = colIDLeft;
			helper.rightColID = colIDRight;
			if(!where_it->isExprCol){ // col = value, convert value into binary
				if(!ParsingHelper::ConvertValue(helper.val, table->GetHeader()->attrType[colIDLeft], table->GetHeader()->attrLenth[colIDLeft], 
					where_it->exprVal, getBitFromLeft(table->GetHeader()->nullMask, colIDLeft))){
						Global::IncompatibleTypes(whereClause.pos, table->GetHeader()->attrName[colIDLeft]);
						return false;
					}
			}
			else{ // col = col, check type compatility
				if(!DataType::Comparable(table->GetHeader()->attrType[colIDLeft], table->GetHeader()->attrType[colIDRight])){
					Global::IncomparableFields(whereClause.pos, table->GetHeader()->attrName[colIDLeft], table->GetHeader()->attrName[colIDRight]);
					return false;
				}
			}
			helper.cmp = where_it->cmp;
			helper.hasRightCol = where_it->isExprCol;
			// FIXING
			if(helper.hasRightCol)
				helpers.push_back(helper);
			else{
				whereHelpersCol[helper.leftColID].push_back(helper);
				if(whereHelpersCol[helper.leftColID].size() > cmpUnitsNeeded)
					cmpUnitsNeeded = whereHelpersCol[helper.leftColID].size();
			}
		}
		return true;
	}

	// 参数含义与checkWhereClause中的一样
	static Scanner* buildScanner(Table* table, std::vector<SelectHelper>& helpers, std::vector<SelectHelper> *whereHelpersCol, int cmpUnitsNeeded){
		Scanner* scanner = table->GetScanner(nullptr);
		uchar cmps[table->ColNum()] = {0}; // Comparator::Any is 0
		// FIXME: 现在的constantValue没有加上null word, 没有考虑is null/is not null, 没有考虑字段顺序,没有考虑一个字段上的多个约束
		uchar constantValue[DataType::calcConstantLength(table->GetHeader()->attrType, table->GetHeader()->attrLenth, table->ColNum())];
		const uchar* vals[table->ColNum()] = {0};
		bool selection[table->ColNum()] = {0};
		for(int i = 0; i < cmpUnitsNeeded; i++){ // i is the index for condition
			memset(cmps, 0, sizeof(cmps)); // Comparator::Any
			memset(constantValue, 0, sizeof(constantValue));
			memset(vals, 0, sizeof(vals));
			memset(selection, 0, sizeof(selection));
			for(int j = 0; j < table->ColNum(); j++){
				if(whereHelpersCol[j].size() > i){
					selection[j] = true;
					SelectHelper &tmp = whereHelpersCol[j][i];
					cmps[j] = tmp.cmp;
					if(tmp.val.type == DataType::NONE)
						setBitFromLeft(*(uint*)constantValue, j);
					else if(table->GetHeader()->attrType[j] == DataType::CHAR || table->GetHeader()->attrType[j] == DataType::VARCHAR)
						vals[j] = (const uchar*)tmp.val.str.data();
					else
						vals[j] = tmp.val.bytes;
				}
			}
			ParsingHelper::extractFields(table->GetHeader(), table->ColNum(), selection, vals, constantValue, true, false);
			scanner->AddDemand(constantValue, table->ColNum(), cmps);
		}
		for(auto where_it = helpers.begin(); where_it != helpers.end(); where_it++)
			scanner->AddSelfCmp(where_it->leftColID, where_it->rightColID, where_it->cmp);
		return scanner;			
	}

	/**
	 * 抽取header中部分字段,构造记录,用于index的比较和where字句的scanner构造
	 * colNum表示原始记录的字段数量
	 * select表示哪些字段被抽取,是colNum大小的数组
	 * valList是被抽取字段的值,是colNum大小的数组
	 * dst是构造出的记录的存储地址
	 * isConstant = true表示从常量记录中抽取;反之则表示从物理记录中抽取
	 * compact = true表示压缩字段,即不会为未被选中的字段预留空间;反之则会预留空间
	 * NOTE: 请在调用该函数前处理好null word并写入到dst前4个字节内
	*/
	static void extractFields(const BaseHeader* header, int colNum, bool* selection, const uchar** valList, uchar* dst, bool isConstant, bool compact){
		int bufPos = 4;
		for(int i = 0; i < colNum; i++){
			uchar colType = header->attrType[i];
			uchar colLength = header->attrLenth[i];
			int fieldLength = isConstant ? DataType::constantLengthOf(colType, colLength) : DataType::lengthOf(colType, colLength);
			if(selection[i]){
				if(!getBitFromLeft(*(uint*)dst, i)) // 非null时才赋值常量值
					if(isConstant && (colType == DataType::VARCHAR || colType == DataType::CHAR)){ // 如果是常量字符串,需要在后面补上'\0'
						int len = strnlen((const char*)valList[i], colLength);
						memcpy(dst + bufPos, valList[i], len);
						memset(dst + bufPos + len, 0, fieldLength - len);
					}
					else
						memcpy(dst + bufPos, valList[i], fieldLength);
				bufPos += fieldLength;
			}
			else if(!compact) // 在compact模式下,未被选中的字段不占用空间
				bufPos += fieldLength;
		}
	}

	/**
	 * 给出一个表和它的一个索引的header,从一条完整记录中抽取索引需要的字段
	 * 
	*/
	static void getIndexFromRecord(const IndexHeader* idxHeader, Table* table, const uchar* record, uchar* dst){
		bool selection[MAX_COL_NUM] = {0};
		const uchar* vals[MAX_COL_NUM] = {0};
		for(int i = 0; i < MAX_COL_NUM; i++){
			uchar targetCol = idxHeader->indexColID[i];
			if(targetCol == COL_ID_NONE)
				break;
			selection[targetCol] = true;
			vals[targetCol] = record + table->ColOffset(targetCol);
		}
		extractFields(table->GetHeader(), table->ColNum(), selection, vals, dst, false, true);
	}

	static void InsertIndexes(Table* table, const Record& rec){
		uchar tmpBuf[table->GetHeader()->recordLenth] = {0};
		for(int i = 0; i < table->IdxNum(); i++){
			memset(tmpBuf, 0, sizeof(tmpBuf));
			BplusTree* tree = new BplusTree(Global::dbms->CurrentDatabase()->idx, table->GetHeader()->bpTreePage[i]);
			getIndexFromRecord(tree->header, table, rec.GetData(), tmpBuf);
			tree->SafeInsert(tmpBuf, *rec.GetRid());
			delete tree;
		}
	}

	static void UpdateIndexes(Table* table, const uchar* oldRec, const Record& newRec, uint updateMask){
		uchar tmpBuf[table->GetHeader()->recordLenth] = {0};
		for(int i = 0; i < table->IdxNum(); i++){
			bool hasChange = false;
			for(int j = 0; j < MAX_COL_NUM; j++){
				uchar col = table->GetHeader()->indexID[i][j];
				if(col == COL_ID_NONE)
					break;
				if(getBitFromLeft(updateMask, col)){
					hasChange = true;
					break;
				}
			}
			if(hasChange){
				memset(tmpBuf, 0, sizeof(tmpBuf));
				BplusTree* tree = new BplusTree(Global::dbms->CurrentDatabase()->idx, table->GetHeader()->bpTreePage[i]);
				getIndexFromRecord(tree->header, table, oldRec, tmpBuf);
				tree->SafeRemove(tmpBuf, *newRec.GetRid());
				memset(tmpBuf, 0, sizeof(tmpBuf));
				getIndexFromRecord(tree->header, table, newRec.GetData(), tmpBuf);
				tree->SafeInsert(tmpBuf, *newRec.GetRid());
				delete tree;
			}
		}
	}

	static bool IndexNameReserved(const char* name){
		return identical(name, PRIMARY_RESERVED_IDX_NAME, MAX_INDEX_NAME_LEN);
	}

	static bool TableNameReserved(const char* name){
		return identical(name, DB_RESERVED_TABLE_NAME, MAX_TABLE_NAME_LEN) || identical(name, IDX_RESERVED_TABLE_NAME, MAX_TABLE_NAME_LEN)
			|| identical(name, VARCHAR_RESERVED_TABLE_NAME, MAX_TABLE_NAME_LEN);
	}
};

struct Debugger{
	static bool debug(std::vector<Type> &typeVec){}
};

#define YYSTYPE Type//把YYSTYPE(即yylval变量)重定义为struct Type类型，这样lex就能向yacc返回更多的数据了

#endif
