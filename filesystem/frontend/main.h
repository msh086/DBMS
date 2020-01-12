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
#include "../MyDB/MultiScanner.h"

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
	static bool checkWhereClause(std::vector<SelectHelper>& helpers, std::vector<SelectHelper>* whereHelpersCol, int& cmpUnitsNeeded, std::string& tbname, std::vector<WhereInstr>& whereClause, Table* table, int pos){
		for(auto where_it = whereClause.begin(); where_it != whereClause.end(); where_it++){
			if(where_it->column.tableName.length() && where_it->column.tableName != tbname){
				Global::IrrelevantTable(pos, where_it->column.tableName.data());
				return false;
			}
			if(where_it->isExprCol && where_it->exprCol.tableName.length() && where_it->exprCol.tableName != tbname){
				Global::IrrelevantTable(pos, where_it->exprCol.tableName.data());
				return false;
			}
			uchar colIDLeft = table->IDofCol(where_it->column.colName.data());
			if(colIDLeft == COL_ID_NONE){
				Global::NoSuchField(pos, where_it->column.colName.data());
				return false;
			}
			uchar colIDRight = COL_ID_NONE;
			if(where_it->isExprCol){
				colIDRight = table->IDofCol(where_it->exprCol.colName.data());
				if(colIDRight == COL_ID_NONE){
					Global::NoSuchField(pos, where_it->exprCol.colName.data());
					return false;
				}
			}
			SelectHelper helper;
			helper.leftColID = colIDLeft;
			helper.rightColID = colIDRight;
			if(!where_it->isExprCol){ // col = value, convert value into binary
				if(!ParsingHelper::ConvertValue(helper.val, table->GetHeader()->attrType[colIDLeft], table->GetHeader()->attrLenth[colIDLeft], 
					where_it->exprVal, getBitFromLeft(table->GetHeader()->nullMask, colIDLeft))){
						Global::IncompatibleTypes(pos, table->GetHeader()->attrName[colIDLeft]);
						return false;
					}
			}
			else{ // col = col, check type compatility
				if(!DataType::Comparable(table->GetHeader()->attrType[colIDLeft], table->GetHeader()->attrType[colIDRight])){
					Global::IncomparableFields(pos, table->GetHeader()->attrName[colIDLeft], table->GetHeader()->attrName[colIDRight]);
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
						else{
							helper.hasUpper = true;
							helper.upperVal = it->val;
							helper.upperCmp = it->cmp;
						}
						// 然后检查
						if(helper.hasLower){
							if(condCmp(helper.lowerVal, helper.upperVal, it->leftColID, Comparator::GtEq)){
								if(helper.lowerCmp == Comparator::GtEq && helper.upperCmp == Comparator::LtEq && condCmp(helper.lowerVal, helper.upperVal, it->leftColID, Comparator::Eq)){
									helper.hasLower = helper.hasUpper = false;
									helper.isEq = true;
									helper.eqVal = helper.lowerVal;
								}
								else
									return false;
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
		for(int i = 0; i < cmpUnitsNeeded; i++){ // i is the index for condition
			memset(cmps, 0, sizeof(cmps)); // Comparator::Any
			memset(constantValue, 0, sizeof(constantValue));
			for(int j = 0; j < table->ColNum(); j++){
				if(whereHelpersCol[j].size() > i){
					SelectHelper &tmp = whereHelpersCol[j][i];
					cmps[j] = tmp.cmp;
					if(tmp.val.type == DataType::NONE)
						setBitFromLeft(*(uint*)constantValue, j);
					else if(table->GetHeader()->attrType[j] == DataType::CHAR || table->GetHeader()->attrType[j] == DataType::VARCHAR){
						int len = strnlen(tmp.val.str.data(), table->GetHeader()->attrLenth[j]);
						memcpy(constantValue + table->ColOffset(j), tmp.val.str.data(), len);
					}
					else
						memcpy(constantValue + table->ColOffset(j), tmp.val.bytes, DataType::lengthOf(table->GetHeader()->attrType[j], table->GetHeader()->attrLenth[j]));
				}
			}
			scanner->AddDemand(constantValue, table->ColNum(), cmps);
		}
		for(auto where_it = helpers.begin(); where_it != helpers.end(); where_it++)
			scanner->AddSelfCmp(where_it->leftColID, where_it->rightColID, where_it->cmp);
		return scanner;			
	}

	static std::string AnonymousFK(uchar masterTable, const uchar* slaveCols){
		std::string ans;
		ans.append("FK_");
		auto IDtoChar = [](uchar id)->uchar{
			if(id < 10)
				return id + '0';
			else
				return 'A' + id - 10;
		};
		ans.push_back(IDtoChar(masterTable));
		ans.push_back('_');
		uint fkMask = 0;
		for(int i = 0; i < MAX_COL_NUM; i++){
			uchar tmpCol = slaveCols[i];
			if(tmpCol == COL_ID_NONE)
				break;
			setBitFromLeft(fkMask, tmpCol);
		}
		uchar* bytes = (uchar*)&fkMask;
		for(int i = 0; i < 4; i++){
			ans.push_back(IDtoChar(bytes[i] >> 4));
			ans.push_back(IDtoChar(bytes[i] % 4));
		}
		return ans;
	}

	static void InsertIndexes(Table* table, const Record& rec){
		uchar tmpBuf[table->GetHeader()->recordLenth] = {0};
		for(int i = 0; i < table->IdxNum(); i++){
			memset(tmpBuf, 0, sizeof(tmpBuf));
			BplusTree* tree = new BplusTree(Global::dbms->CurrentDatabase()->idx, table->GetHeader()->bpTreePage[i]);
			BplusTree::getIndexFromRecord(tree->header, table, rec.GetData(), tmpBuf);
			tree->SafeInsert(tmpBuf, *rec.GetRid());
			delete tree;
		}
		// ! 不要忘了主键索引
		if(table->GetHeader()->primaryIndexPage){
			memset(tmpBuf, 0, sizeof(tmpBuf));
			BplusTree* tree = new BplusTree(Global::dbms->CurrentDatabase()->idx, table->GetHeader()->primaryIndexPage);
			BplusTree::getIndexFromRecord(tree->header, table, rec.GetData(), tmpBuf);
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
				BplusTree::getIndexFromRecord(tree->header, table, oldRec, tmpBuf);
				tree->SafeRemove(tmpBuf, *newRec.GetRid());
				memset(tmpBuf, 0, sizeof(tmpBuf));
				BplusTree::getIndexFromRecord(tree->header, table, newRec.GetData(), tmpBuf);
				tree->SafeInsert(tmpBuf, *newRec.GetRid());
				delete tree;
			}
		}
		// 主键索引
		if(table->GetHeader()->primaryKeyID[0] != COL_ID_NONE){
			bool hasChange = false;
			for(int i = 0; i < MAX_COL_NUM; i++){
				uchar col = table->GetHeader()->primaryKeyID[i];
				if(col == COL_ID_NONE)
					break;
				if(getBitFromLeft(updateMask, col)){
					hasChange = true;
					break;
				}
			}
			if(hasChange){
				memset(tmpBuf, 0, sizeof(tmpBuf));
				BplusTree* tree = new BplusTree(Global::dbms->CurrentDatabase()->idx, table->GetHeader()->primaryIndexPage);
				BplusTree::getIndexFromRecord(tree->header, table, oldRec, tmpBuf);
				tree->SafeRemove(tmpBuf, *newRec.GetRid());
				memset(tmpBuf, 0, sizeof(tmpBuf));
				BplusTree::getIndexFromRecord(tree->header, table, newRec.GetData(), tmpBuf);
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