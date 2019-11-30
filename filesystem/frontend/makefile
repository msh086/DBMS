main : lex.yy.o yacc.tab.o
	g++ lex.yy.o yacc.tab.o -o main
lex.yy.o : lex.yy.cc
	g++ -c lex.yy.cc
yacc.tab.o : yacc.tab.cpp
	g++ -c yacc.tab.cpp
lex.yy.cc : lex.l
	flext -+ lex.l
yacc.tab.cpp : yacc.ypp
	bison -d yacc.ypp

.PHONY : clean
clean :
	rm *.o