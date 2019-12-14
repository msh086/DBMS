#ifndef PARSERUTILS_H
#define PARSERUTILS_H
#include "../utils/pagedef.h"
#include <string>
#include <stdexcept>
#include "Field.h"

const uchar INVALID_ARG = 1, OUT_OF_RANGE = 2, OTHER = 3;

uchar strToInt(const std::string& str, int& dst){
	
	try{
		dst = std::stoi(str);
		return 0;
	}
	catch(const std::invalid_argument& ia){
		return INVALID_ARG;
	}
	catch(const std::out_of_range& oor){
		return OUT_OF_RANGE;
	}
	catch(const std::exception& e){
		return OTHER;
	}
}

uchar strToLL(const std::string& str, ll& dst){
	try{
		dst = std::stoll(str);
		return 0;
	}
	catch(const std::invalid_argument& ia){
		return INVALID_ARG;
	}
	catch(const std::out_of_range& oor){
		return OUT_OF_RANGE;
	}
	catch(const std::exception& e){
		return OTHER;
	}
}

uchar strToFloat(const std::string& str, float dst){
	try{
		dst = std::stof(str);
		return 0;
	}
	catch(const std::invalid_argument& ia){
		return INVALID_ARG;
	}
	catch(const std::out_of_range& oor){
		return OUT_OF_RANGE;
	}
	catch(const std::exception& e){
		return OTHER;
	}
}

#endif