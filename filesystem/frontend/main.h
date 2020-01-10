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

	// TODO: null value?
	/**
	 * @param table 索引的原始表
	 * @param whereHelpersCol 类型为std::vector<SelectHelper>[table->ColNum()]
	 * @param idxHelpers 类型为std::vector<IndexHelper>[table->ColNum()],类似java的Optional
	 * @param whereMask where子句约束的字段中,所有约束条件为Eq的字段集合
	 * @param rangeCol where字句约束的字段中,约束条件为range的字段ID,没有则为COL_ID_NONE
	 * @return 是否可能可以使用索引,返回值为true iff 至多只有一个字段是range约束;没有相互冲突的约束,导致某一字段的可接受集为空;没有除了Eq和range之外的约束类型(e.g. a > 0 and a < 2 a != 1)
	*/
	static bool CanUseIndex(Table* table, std::vector<SelectHelper>* whereHelpersCol, std::vector<IndexHelper>* idxHelpers, uint& whereMask, uchar& rangeCol){
		whereMask = 0;
		rangeCol = COL_ID_NONE;
		for(int i = 0; i < table->ColNum(); i++){ // 对于每一列
			if(whereHelpersCol[i].size() == 0)
				continue;
			std::sort(whereHelpersCol[i].begin(), whereHelpersCol[i].end(), [](const SelectHelper& left, const SelectHelper& right)->bool{
				auto calcValue = [](uchar cmp)->uchar{
					if(cmp == Comparator::Eq)
						return 0;
					if(cmp == Comparator::Gt || Comparator::GtEq)
						return 1;
					if(cmp == Comparator::Lt || Comparator::LtEq)
						return 2;
					if(cmp == Comparator::NE)
						return 3;
					assert(false);
				};
				return calcValue(left.cmp) < calcValue(right.cmp);
			}); // 所有SelectHelper被根据Eq < Gt/GtEq < Lt/LtEq < NE排序
			IndexHelper helper;
			auto condCmp = [table](const Val& leftVal, const Val& rightVal, uchar colID, uchar cmp)->bool{
				const uchar* left = nullptr, *right = nullptr;
				bool nullLeft = leftVal.type == DataType::NONE, nullRight = rightVal.type == DataType::NONE;
				uchar colType = table->GetHeader()->attrType[colID];
				if(colType == DataType::CHAR || colType == DataType::VARCHAR){
					left = (const uchar*)leftVal.str.data();
					right = (const uchar*)rightVal.str.data();
				}
				else{
					left = leftVal.bytes;
					right = rightVal.bytes;
				}
				return DataType::compare(left, right, colType, table->GetHeader()->attrLenth[colID], cmp, nullLeft, nullRight, true, true);
			};
			for(auto it = whereHelpersCol[i].begin(); it != whereHelpersCol[i].end(); it++){ // 对于该字段的每一个条件
				if(it->cmp == Comparator::Eq){
					if(helper.isEq){
						if(!condCmp(helper.eqVal, it->val, it->leftColID, Comparator::Eq))
							return false;
					}
					helper.isEq = true;
					helper.eqVal = it->val;
				}
				else if(it->cmp == Comparator::Gt || it->cmp == Comparator::GtEq){
					if(helper.isEq){
						// 唯一合法的两种情况: 1. >=: newValue <= oldValue; 2. >: newValue < oldValue
						if(!condCmp(helper.eqVal, it->val, it->leftColID, it->cmp))
							return false;
					}
					else if(helper.hasLower){
						if(it->cmp == Comparator::GtEq){
							if(condCmp(it->val, helper.lowerVal, it->leftColID, Comparator::GtEq)){ // newValue >= oldValue, newCmp is GtEq
								helper.lowerCmp = Comparator::GtEq;
								helper.lowerVal = it->val;
							}
						}
						else{
							if(condCmp(it->val, helper.lowerVal, it->leftColID, Comparator::Gt)){ // newValue > oldValue, newCmp is Gt
								helper.lowerCmp = Comparator::Gt;
								helper.lowerVal = it->val;
							}
						}
					}
					else{
						helper.hasLower = true;
						helper.lowerCmp = it->cmp;
						helper.lowerVal = it->val;
					}
				}
				else if(it->cmp == Comparator::Lt || it->cmp == Comparator::LtEq){
					if(helper.isEq){
						// 唯一合法的两种情况: 1. <=: newValue >= oldValue; 2. <: newValue > oldValue
						if(!condCmp(helper.eqVal, it->val, it->leftColID, it->cmp))
							return false;
					}
					else{ // 之前就是区间比较
						// 首先检查upperVal,合并
						if(helper.hasUpper){
							if(it->cmp == Comparator::LtEq){
								if(condCmp(it->val, helper.upperVal, it->leftColID, Comparator::Lt)){ // newValue < oldValue, newCmp is <=
									helper.upperCmp = Comparator::LtEq;
									helper.upperVal = it->val;
								}
								// newValue >= oldValue, newCmp is <= : use old cmp and val
							}
							else{
								if(condCmp(it->val, helper.upperVal, it->leftColID, Comparator::LtEq)){ // newValue <= oldValue, newCmp is <
									helper.upperCmp = Comparator::Lt;
									helper.upperVal = it->val;
								}
								// newValue > oldValue, newCmp is < : use old cmp and val
							}
						}
						// 然后检查
						if(helper.hasLower){
							if(condCmp(helper.lowerVal, it->val, it->leftColID, Comparator::GtEq)){
								if(helper.lowerCmp == Comparator::GtEq && it->cmp == Comparator::LtEq && condCmp(helper.lowerVal, it->val, it->leftColID, Comparator::Eq)){
									helper.hasLower = helper.hasUpper = false;
									helper.isEq = true;
									helper.eqVal = it->val;
									// compare with upperVal
								}
								else
									return false;
							}
							// compare with upperVal
						}
						else{
							if(!helper.isEq){ // 没有在之前阶段被合并
								helper.hasUpper = true;
								helper.upperVal = it->val;
								helper.upperCmp = it->cmp;
							}
						}
					}
				}
				else if(it->cmp == Comparator::NE){
					if(helper.isEq){
						if(condCmp(helper.eqVal, it->val, it->leftColID, Comparator::Eq))
							return false;
					}
					else if(it->val.type != DataType::NONE){ // just ignore 'is not null' for range comparison
						bool safe = false;
						if(helper.hasLower){
							if(condCmp(it->val, helper.lowerVal, it->leftColID, Comparator::LtEq)){
								safe = true;
								if(helper.lowerCmp == Comparator::GtEq && condCmp(it->val, helper.lowerVal, it->leftColID, Comparator::Eq))
									helper.lowerCmp = Comparator::Gt;
							}
						}
						if(!safe && helper.hasUpper){
							if(condCmp(it->val, helper.upperVal, it->leftColID, Comparator::GtEq)){
								safe = true;
								if(helper.upperCmp == Comparator::LtEq && condCmp(it->val, helper.upperVal, it->leftColID, Comparator::Eq))
									helper.upperCmp = Comparator::Lt;
							}
						}
						if(!safe)
							return false;
					}
				}
			} // end: for every condition
			if(helper.isEq)
				setBitFromLeft(whereMask, i);
			else{
				if(rangeCol != COL_ID_NONE) // TODO: 使用结果简化scanner条件
					return false;
				else
					rangeCol = i;
			}
			idxHelpers[i].push_back(helper);
		} // end: for every field
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
	static bool debug(std::vector<Type> &typeVec){
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
		}
		return true;
	}
};

#define YYSTYPE Type//把YYSTYPE(即yylval变量)重定义为struct Type类型，这样lex就能向yacc返回更多的数据了

#endif
