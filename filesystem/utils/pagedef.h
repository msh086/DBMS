#ifndef PAGE_DEF
#define PAGE_DEF
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
/*
 * 一个页面中的字节数
 */
#define PAGE_SIZE 8192
/*
 * 一个页面中的整数个数
 */
#define PAGE_INT_NUM 2048
/*
 * 页面字节数以2为底的指数
 */
#define PAGE_SIZE_IDX 13
#define MAX_FMT_INT_NUM 128
//#define BUF_PAGE_NUM 65536
#define MAX_FILE_NUM 128
#define MAX_TYPE_NUM 256
/*
 * 缓存中页面个数上限
 */
#define CAP 60000
/*
 * hash算法的模
 */
#define MOD 60000
#define IN_DEBUG 0
#define DEBUG_DELETE 0
#define DEBUG_ERASE 1
#define DEBUG_NEXT 1
/*
 * 一个表中列的上限
 */
#define MAX_COL_NUM 31
/*
 * 数据库中表的个数上限
 */
#define MAX_TB_NUM 31

//below macros are added by msh
/**
 * Max string length for an attribute
*/
#define MAX_ATTRI_NAME_LEN 64
/**
 * Max string length for a database name
*/
#define MAX_DB_NAME_LEN 64
/**
 * Max string length for a table name
*/
#define MAX_TABLE_NAME_LEN 64
/**
 * Max times a table can be referrence as a foreign key source
 * P.S. In fact, MAX_FOREIGN_TIME is always no greater than MAX_TB_NUM - 1
*/
#define MAX_FOREIGN_TIME 30
/**
 * Length of fragments to break varchar into, total length = 502 + 10 = 512 B
*/
#define VARCHAR_RECORD_LEN 512

#define VARCHAR_FRAG_LEN (VARCHAR_RECORD_LEN - 10)

#define DBMS_RESERVED_TABLE_NAME "ALL_DB"
#define DB_RESERVED_TABLE_NAME "ALL_TB"
#define IDX_RESERVED_TABLE_NAME "BPTREE"
#define VARCHAR_RESERVED_TABLE_NAME "VARCHAR"

#define DEBUG // If this macro is set, debug methods are available

#define RELEASE 1
typedef unsigned int* BufType;
typedef unsigned int uint;
typedef unsigned short ushort;
// use uchar instead of char when manipulating bytes for efficiency and safety pls
// use char only when dealing with strings
typedef unsigned char uchar; 
typedef unsigned long long ull;
typedef long long ll;
typedef double db;
typedef int INT;
typedef int(cf)(uchar*, uchar*);
// what the hell are these ?
// int current = 0;
// int tt = 0;
#endif
