#ifndef GLOBAL_H
#define GLOBAL_H
#include<string>
#include<vector>

class Global{
	public:
		static std::string buffer; // 接受字符串时的缓冲区
		static std::vector<int> errors; // 遇到的错误
		static int pos; // 当前位置
};

#endif