BUILD_DIR = ./build/
DEBUG = n
DEBUGARG =
DEPENDENCIES = $(BUILD_DIR)yacc.tab.o $(BUILD_DIR)lex.yy.o $(BUILD_DIR)Global.o $(BUILD_DIR)interface.o $(BUILD_DIR)MyBitMap.o $(BUILD_DIR)FileManager.o $(BUILD_DIR)SimpleUtils.o \
	$(BUILD_DIR)BufPageManager.o $(BUILD_DIR)BplusTree.o $(BUILD_DIR)BplusTreeNode.o $(BUILD_DIR)Database.o $(BUILD_DIR)Scanner.o $(BUILD_DIR)Table.o $(BUILD_DIR)DBMS.o

ifeq ($(DEBUG), y)
	DEBUGARG = -g
endif

main : $(DEPENDENCIES)
	g++ $^ -o main $(DEBUGARG)

.PHONY : run
run : main
	cd ../../run; ../f*/fr*/main
$(BUILD_DIR)yacc.tab.o : yacc.tab.c
	g++ -c yacc.tab.c -o $@ $(DEBUGARG)
$(BUILD_DIR)lex.yy.o : lex.yy.c
	g++ -c lex.yy.c -o $@ $(DEBUGARG)
$(BUILD_DIR)Global.o : Global.h Global.cpp
	g++ -c Global.cpp -o $@ $(DEBUGARG)
$(BUILD_DIR)interface.o : interface.cpp
	g++ -c interface.cpp -o $@ $(DEBUGARG)
lex.yy.c : lex.l
	flex -I lex.l
yacc.tab.c : yacc.y
	bison -d yacc.y
# DB part
$(BUILD_DIR)MyBitMap.o : ../utils/MyBitMap.h ../utils/MyBitMap.cpp
	g++ -c ../utils/MyBitMap.cpp -o $@ $(DEBUGARG)
$(BUILD_DIR)FileManager.o : ../fileio/FileManager.h ../fileio/FileManager.cpp
	g++ -c ../fileio/FileManager.cpp -o $@ $(DEBUGARG)
$(BUILD_DIR)BufPageManager.o : ../bufmanager/BufPageManager.h ../bufmanager/BufPageManager.cpp
	g++ -c ../bufmanager/BufPageManager.cpp -o $@ $(DEBUGARG)
$(BUILD_DIR)SimpleUtils.o : ../RM/SimpleUtils.h ../RM/SimpleUtils.cpp
	g++ -c ../RM/SimpleUtils.cpp -o $@ $(DEBUGARG)
$(BUILD_DIR)BplusTree.o : ../indexing/BplusTree.h ../indexing/BplusTree.cpp
	g++ -c ../indexing/BplusTree.cpp -o $@ $(DEBUGARG)
$(BUILD_DIR)BplusTreeNode.o : ../indexing/BplusTreeNode.h ../indexing/BplusTreeNode.cpp
	g++ -c ../indexing/BplusTreeNode.cpp -o $@ $(DEBUGARG)
$(BUILD_DIR)Database.o : ../MyDB/Database.h ../MyDB/Database.cpp
	g++ -c ../MyDB/Database.cpp -o $@ $(DEBUGARG)
$(BUILD_DIR)Scanner.o : ../MyDB/Scanner.h ../MyDB/Scanner.cpp
	g++ -c ../MyDB/Scanner.cpp -o $@ $(DEBUGARG)
$(BUILD_DIR)Table.o : ../MyDB/Table.h ../MyDB/Table.cpp
	g++ -c ../MyDB/Table.cpp -o $@ $(DEBUGARG)
$(BUILD_DIR)DBMS.o : ../MyDB/DBMS.h ../MyDB/DBMS.cpp
	g++ -c ../MyDB/DBMS.cpp -o $@ $(DEBUGARG)

.PHONY : clean
clean :
	- rm *.tab.*
	- rm *.c
	- rm $(BUILD_DIR)*.o
	- rm main

.PHONY : reset
reset :
	- rm -rf ../../run/*