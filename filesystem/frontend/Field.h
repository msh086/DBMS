#ifndef FIELD_H
#define FIELD_H
#include "../utils/pagedef.h"
#include "../RM/DataType.h"
#include <string>
#include <stdexcept>

struct Field{
	char name[MAX_ATTRI_NAME_LEN] = "";
    uchar type = DataType::NONE;
	ushort length = 0;
	bool nullable = true;
	bool hasDefault = false;
	uchar* defaultValue = nullptr;
	~Field(){
		if(defaultValue)
			delete[] defaultValue;
	}
    const static int INVALID_ARG = 1, OUT_OF_RANGE = 2, OTHER = 3;
    static uchar strToInt(const std::string& str, int& dst){
        
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

    static uchar strToLL(const std::string& str, ll& dst){
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

    static uchar strToFloat(const std::string& str, float dst){
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
};

#endif