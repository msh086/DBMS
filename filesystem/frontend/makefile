BUILD_DIR = ./build/
TARGET = $(BUILD_DIR)$@

main : yacc.tab.o lex.yy.o Global.o interface.o MyBitMap.o FileManager.o BufPageManager.o SimpleUtils.o BplusTree.o BplusTreeNode.o Database.o Scanner.o Table.o DBMS.o
	g++ $(BUILD_DIR)yacc.tab.o $(BUILD_DIR)lex.yy.o $(BUILD_DIR)Global.o $(BUILD_DIR)interface.o $(BUILD_DIR)MyBitMap.o $(BUILD_DIR)FileManager.o $(BUILD_DIR)SimpleUtils.o \
	$(BUILD_DIR)BufPageManager.o $(BUILD_DIR)BplusTree.o $(BUILD_DIR)BplusTreeNode.o $(BUILD_DIR)Database.o $(BUILD_DIR)Scanner.o $(BUILD_DIR)Table.o $(BUILD_DIR)DBMS.o -o main
yacc.tab.o : yacc.tab.c
	g++ -c yacc.tab.c -o $(TARGET)
lex.yy.o : lex.yy.c
	g++ -c lex.yy.c -o $(TARGET)
Global.o : Global.h Global.cpp
	g++ -c Global.cpp -o $(TARGET)
interface.o : interface.cpp
	g++ -c interface.cpp -o $(TARGET)
lex.yy.c : lex.l
	flex -I lex.l
yacc.tab.c : yacc.y
	bison -d yacc.y
# DB part
MyBitMap.o : ../utils/MyBitMap.h ../utils/MyBitMap.cpp
	g++ -c ../utils/MyBitMap.cpp -o $(TARGET)
FileManager.o : ../fileio/FileManager.h ../fileio/FileManager.cpp
	g++ -c ../fileio/FileManager.cpp -o FileManager.o -o $(TARGET)
BufPageManager.o : ../bufmanager/BufPageManager.h ../bufmanager/BufPageManager.cpp
	g++ -c ../bufmanager/BufPageManager.cpp -o $(TARGET)
SimpleUtils.o : ../RM/SimpleUtils.h ../RM/SimpleUtils.cpp
	g++ -c ../RM/SimpleUtils.cpp -o $(TARGET)
BplusTree.o : ../indexing/BplusTree.h ../indexing/BplusTree.cpp
	g++ -c ../indexing/BplusTree.cpp -o BplusTree.o -o $(TARGET)
BplusTreeNode.o : ../indexing/BplusTreeNode.h ../indexing/BplusTreeNode.cpp
	g++ -c ../indexing/BplusTreeNode.cpp -o BplusTreeNode.o -o $(TARGET)
Database.o : ../MyDB/Database.h ../MyDB/Database.cpp
	g++ -c ../MyDB/Database.cpp -o Database.o -o $(TARGET)
Scanner.o : ../MyDB/Scanner.h ../MyDB/Scanner.cpp
	g++ -c ../MyDB/Scanner.cpp -o Scanner.o -o $(TARGET)
Table.o : ../MyDB/Table.h ../MyDB/Table.cpp
	g++ -c ../MyDB/Table.cpp -o Table.o -o $(TARGET)
DBMS.o : ../MyDB/DBMS.h ../MyDB/DBMS.cpp
	g++ -c ../MyDB/DBMS.cpp -o DBMS.o -o $(TARGET)

.PHONY : clean
clean :
	- rm *.tab.*
	- rm *.c
	- rm $(BUILD_DIR)/*.o