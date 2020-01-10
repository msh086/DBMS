/*
 * testfilesystem.cpp
 *
 *  Created on: 2015年10月6日
 *      Author: lql
 *          QQ: 896849432
 * 各位同学十分抱歉，之前给的样例程序不支持同时打开多个文件
 * 是因为初始化没有做，现在的程序加上了初始化（main函数中的第一行）
 * 已经可以支持多个文件的维护
 *
 * 但是还是建议大家只维护一个文件，因为首先没有必要，我们可以把数据库中
 * 的索引和数据都放在同一个文件中，当我们开启一个数据库时，就关掉上一个
 * 其次，多个文件就要对应多个fileID，在BufPageManager中是利用一个hash函数
 * 将(fileID,pageID)映射为一个整数，但由于我设计的hash函数过于简单，就是fileID和
 * pageID的和，所以不同文件的页很有可能映射为同一个数，增加了hash的碰撞率，影响效率
 *
 * 还有非常重要的一点，BufType b = bpm->allocPage(...)
 * 在利用上述allocPage函数或者getPage函数获得指向申请缓存的指针后，
 * 不要自行进行类似的delete[] b操作，内存的申请和释放都在BufPageManager中做好
 * 如果自行进行类似free(b)或者delete[] b的操作，可能会导致严重错误
 */
#include "MyDB/Table.h"
#include "MyDB/Scanner.h"
#include "MyDB/Header.h"
#include "MyDB/Database.h"
#include "RM/DataType.h"
#include "indexing/IndexHeader.h"
#include "indexing/BplusTree.h"
#include "MyDB/DBMS.h"
#include <iostream>
#include <vector>
#include <algorithm>
#include <ctime>
#include <fstream>

using namespace std;
int main() {
	// char filename[30] = "../../testfile.txt";
	// std::ifstream fin;
	// fin.open(filename);
	// if(fin.fail())
	// 	return 1;
	// char buffer[200] = "";
	// char c;
	// while(char c = fin.get()){
	// 	if(c == EOF)
	// 		break;
	// 	printf("%c", c);
	// }
	// // while(fin.getline(buffer, 200)){
	// // 	printf("line with length of %d: %.*s\n", strnlen(buffer, 200), 200, buffer);
	// // 	memset(buffer, 0, 200);
	// // }
	// fin.close();
	// return 0;


	DBMS::Instance()->Init();
	DBMS::Instance()->CreateDatabase("db");
	Database* db = DBMS::Instance()->UseDatabase("db");
	Header* header = new Header();
	header->recordLenth = PAGE_SIZE;
	header->slotNum = (uint)PAGE_SIZE / header->recordLenth;
	memcpy(header->attrName[0], "name", 5);
	db->CreateTable("index test", header, nullptr);
	Table* tb = db->OpenTable("index test");

	IndexHeader * ih = new IndexHeader();
	ih->attrType[0] = DataType::INT;
	BplusTree* tree = new BplusTree(tb, ih);

	int eleCount = 5; // internal cap = 682, external cap = 511. About 30 full nodes
	std::vector<int> vec, pages;
	for(int i = 0; i < eleCount; i++)
		vec.push_back(i);
	int pageCount = 5;
	for(int i = 0; i < pageCount; i++)
		pages.push_back(i);

	ull seed = (ull)time(0);
	printf("seed: %llu\n", seed);
	srand(seed); // 1578574308
	std::random_shuffle(vec.begin(), vec.end());
	printf("Insertion sequence\n");
	for(int i = 0; i < eleCount; i++)
		printf("%d ", vec.at(i));
	printf("\n");
	uint buf[2] = {0};
	for(int i = 0; i < eleCount; i++){
		buf[1] = vec.at(i);
		std::random_shuffle(pages.begin(), pages.end());
		for(auto page_it = pages.begin(); page_it != pages.end(); page_it++){
			tree->SafeInsert((uchar*)buf, RID(0, *page_it));
			//tree->DebugPrint();
			//printf("\n");
		}
	}
	tree->DebugPrint();
	printf("\n");
	printf("Start deletion\n");
	std::random_shuffle(vec.begin(), vec.end());
	printf("Deletion sequence:\n");
	for(int i = 0; i < eleCount; i++)
		printf("%d ", vec.at(i));
	printf("\n");
	for(int i = 0; i < eleCount; i++){
		buf[1] = vec.at(i);
		std::random_shuffle(pages.begin(), pages.end());
		for(auto page_it = pages.begin(); page_it != pages.end(); page_it++){
			tree->SafeRemove((uchar*)buf, RID(0, *page_it));
			printf("Remove key %d with RID %d\n", vec.at(i), *page_it);
			tree->DebugPrint();
			printf("\n");
		}
	}

	printf("errorSign: %d\n", BplusTree::errorSign);

	db->CloseTable("index test");
	db->DeleteTable("index test");
	return 0;

	// uchar dst[17] = {0}; // bin dst
	// uchar digits[38] = {0};
	// memset(dst, 0, 17);
	// DataType::floatToBin(true, "123456", "654321", dst, 12, 6); // p - s <= l <= p, r <= p - l -> ok
	// DataType::binToDigits(dst + 1, digits, 12);
	// DataType::floatToBin(true, "123456", "654321", dst, 5, 0); // l > p -> overflow
	// DataType::binToDigits(dst + 1, digits, 5);
	// DataType::floatToBin(true, "123456", "654321", dst, 15, 8); // l < p - s, r <= s -> ok
	// DataType::binToDigits(dst + 1, digits, 15);
	// DataType::floatToBin(true, "123456", "654321", dst, 15, 3); // l < p - s, r > s -> round
	// DataType::binToDigits(dst + 1, digits, 15);
	// DataType::floatToBin(true, "123456", "654321", dst, 8, 4); // p - s <= l <= p, r > p - l -> round
	// DataType::binToDigits(dst + 1, digits, 8);
	// return 0;

/*
	Database* db = new Database();
	Header * header = new Header();
	header->recordLenth = 10;
	header->slotNum = (uint)PAGE_SIZE / header->recordLenth;
	memcpy(header->attrName[0], "name", MAX_TABLE_NAME_LEN);
	header->attrType[0] = DataType::CHAR;
	header->attrLenth[0] = 10;
	db->CreateTable("tb", header, reinterpret_cast<const uchar*>( "0123456789123"));
	
	Table* table = db->OpenTable("tb");
	table->DebugPrint();
	uchar buf[10] = "record ";
	RID* rid = new RID();
	for(int i = 0; i < 10; i++){
		buf[7] = '0' + i;
		table->InsertRecord(buf, rid);
		printf("The rid of inserted record: %u, %u\n", rid->GetPageNum(), rid->GetSlotNum());
	}
	delete rid;
	db->CloseTable("tb");
	table = db->OpenTable("tb");
	table->DebugPrint();
	Scanner* scanner = table->GetScanner([](const Record& rec){
			uchar* data = rec.GetData();
			return data[7] >= '3' && data[7] <= '8';
		});
	Record* rec = new Record();
	while(scanner->NextRecord(rec) != nullptr){
		printf("record page=%d, slot=%d, content=%.*s\n", rec->GetRid()->GetPageNum(), rec->GetRid()->GetSlotNum(), 10, rec->GetData());
		rec->GetData()[8] = '?';
		table->UpdateRecord(*rec);
		table->DeleteRecord(*rec->GetRid());
	}
	db->CloseTable("tb");
	printf(">>> Updated table<<<\n");
	table = db->OpenTable("tb");
	delete scanner;
	scanner = table->GetScanner([](const Record& rec){return true;});
	while(scanner->NextRecord(rec) != nullptr){
		printf("record page=%d, slot=%d, content=%.*s\n", rec->GetRid()->GetPageNum(), rec->GetRid()->GetSlotNum(), 10, rec->GetData());
	}
	db->DeleteTable("tb");
	*/


/*
	MyBitMap::initConst();   //新加的初始化
	FileManager* fm = new FileManager();
	BufPageManager* bpm = new BufPageManager(fm);
	fm->createFile("testfile.txt"); //新建文件
	fm->createFile("testfile2.txt");
	int fileID, f2;
	fm->openFile("testfile.txt", fileID); //打开文件，fileID是返回的文件id
        fm->openFile("testfile2.txt", f2);
	for (int pageID = 0; pageID < 1000; ++ pageID) {
		int index;
		//为pageID获取一个缓存页
		BufType b = bpm->allocPage(fileID, pageID, index, false);
		//注意，在allocPage或者getPage后，千万不要进行delete[] b这样的操作
		//内存的分配和管理都在BufPageManager中做好，不需要关心，如果自行释放会导致问题
		b[0] = pageID; //对缓存页进行写操作
		b[1] = fileID;
		bpm->markDirty(index); //标记脏页
		//在重新调用allocPage获取另一个页的数据时并没有将原先b指向的内存释放掉
		//因为内存管理都在BufPageManager中做好了
		b = bpm->allocPage(f2, pageID, index, false);
		b[0] = pageID;
		b[1] = f2;
		bpm->markDirty(index);
	}
	for (int pageID = 0; pageID < 1000; ++ pageID) {
		int index;
		//为pageID获取一个缓存页
		BufType b = bpm->getPage(fileID, pageID, index);
		//注意，在allocPage或者getPage后，千万不要进行delete[] b这样的操作
		//内存的分配和管理都在BufPageManager中做好，不需要关心，如果自行释放会导致问题
		cout << b[0] << ":" << b[1] << endl; 		//读取缓存页中第一个整数
		bpm->access(index); //标记访问
		b = bpm->getPage(f2, pageID, index);
		cout << b[0] << ":" << b[1] << endl;
		bpm->access(index);
	}
*/

	//程序结束前可以调用BufPageManager的某个函数将缓存中的内容写回
	//具体的函数大家可以看看ppt或者程序的注释
	return 0;
}
