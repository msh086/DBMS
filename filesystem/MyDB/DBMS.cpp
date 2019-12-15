#include "DBMS.h"
DBMS* DBMS::instance = nullptr;
FileManager* DBMS::fm = FileManager::Instance();
BufPageManager* DBMS::bpm = BufPageManager::Instance();