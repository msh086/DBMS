main : yacc.tab.o lex.yy.o Global.o interface.o
	g++ yacc.tab.o lex.yy.o Global.o interface.o -o main
yacc.tab.o : yacc.tab.c
	g++ -c yacc.tab.c
lex.yy.o : lex.yy.c
	g++ -c lex.yy.c
Global.o : Global.h Global.cpp
	g++ -c Global.cpp
interface.o : interface.cpp
	g++ -c interface.cpp
lex.yy.c : lex.l
	flex -I lex.l
yacc.tab.c : yacc.y
	bison -d yacc.y

.PHONY : clean
clean :
	- rm *.o
	- rm *.tab.*
	- rm *.c
	- rm *.cc