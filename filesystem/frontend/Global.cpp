#include "Global.h"
#include "../utils/pagedef.h"
std::string Global::buffer;
std::vector<Error> Global::errors;
int Global::pos = 0;
DBMS* Global::dbms = nullptr;
char Global::formatBuffer[100] = "";