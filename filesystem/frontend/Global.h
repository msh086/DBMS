#ifndef GLOBAL_H
#define GLOBAL_H
#include<string>
#include<vector>
#include<cstring>
#include<stdio.h>
#include<stdarg.h>

class DBMS;

struct Error{
	int pos;
	std::string msg;
	Error(int pos, const std::string& str){
		this->pos = pos;
		this->msg = str;
	}
};

class Global{
		static const char* _format(char* dst, const char* formatStr, ...){
			va_list ap;
			va_start(ap, formatStr);
			vsnprintf(dst, 100, formatStr, ap);
			va_end(ap);
			return dst;
			// ref: https://blog.csdn.net/LaineGates/article/details/50571719
		}
	public:
		static std::string buffer; // 接受字符串时的缓冲区
		static std::vector<Error> errors; // 遇到的错误
		static int pos; // 当前位置
		static DBMS* dbms;
		static char formatBuffer[100];
		static void newError(int pos, const char* str){
			errors.push_back(Error(pos, std::string(str)));
		}
		static const char* format(const char* formatStr, ...){
			va_list ap;
			va_start(ap, formatStr);
			vsnprintf(formatBuffer, 100, formatStr, ap);
			va_end(ap);
			return formatBuffer;
		}
};

#endif