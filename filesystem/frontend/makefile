main : yacc.tab.o lex.yy.o Global.o
	g++ yacc.tab.o lex.yy.o Global.o -o main
yacc.tab.o : yacc.tab.c
	g++ -c yacc.tab.c
lex.yy.o : lex.yy.c
	g++ -c lex.yy.c
Global.o : Global.h Global.cpp
	g++ -c Global.cpp
lex.yy.c : lex.l
	flex lex.l
yacc.tab.c : yacc.y
	bison -d yacc.y

.PHONY : clean
clean :
	- rm *.o
	- rm *.tab.*
	- rm *.c
	- rm *.cc