/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

#ifndef YY_YY_YACC_TAB_H_INCLUDED
# define YY_YY_YACC_TAB_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    DATABASE = 258,
    DATABASES = 259,
    TABLE = 260,
    TABLES = 261,
    SHOW = 262,
    CREATE = 263,
    DROP = 264,
    USE = 265,
    PRIMARY = 266,
    KEY = 267,
    NOT = 268,
    NULL = 269,
    INSERT = 270,
    INTO = 271,
    VALUES = 272,
    DELETE = 273,
    FROM = 274,
    WHERE = 275,
    UPDATE = 276,
    SET = 277,
    SELECT = 278,
    IS = 279,
    INT = 280,
    VARCHAR = 281,
    CHAR = 282,
    DEFAULT = 283,
    CONSTRAINT = 284,
    CHANGE = 285,
    ALTER = 286,
    ADD = 287,
    RENAME = 288,
    DESC = 289,
    INDEX = 290,
    AND = 291,
    DATE = 292,
    FLOAT = 293,
    FOREIGN = 294,
    REFERENCES = 295,
    NUMERIC = 296,
    DECIMAL = 297,
    ON = 298,
    INT_LIT = 299,
    STRING_LIT = 300,
    FLOAT_LIT = 301,
    DATE_LIT = 302,
    IDENTIFIER = 303,
    GE = 304,
    LE = 305,
    NE = 306
  };
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef int YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;

int yyparse (void);

#endif /* !YY_YY_YACC_TAB_H_INCLUDED  */
