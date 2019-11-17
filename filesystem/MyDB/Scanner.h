#ifndef SCANNER_H
#define SCANNER_H
#include "../RM/Record.h"
#include "Table.h"
/**
 * A scanner provides a read-only interface for getting records that meet certain requirements
 * A table can have multiple scanners
*/
class Scanner{
    Table* table = nullptr;
    bool (*demand)(const Record& record) = nullptr;

    RID* rid = new RID(1, 0);
    Record* record = nullptr;

    // A scanner can only be instantiated from a table
    Scanner(Table* table, bool (*demand)(const Record& record)){
        this->table = table;
        this->demand = demand;
    }
    public:
        /**
         * Return the next record that meets requirements 'demand', or nullptr if none is found
         * No memory is dynamically allocated
        */
        Record* NextRecord(Record* rec){
            while(table->NextRecord(*rid)){
                record = table->GetRecord(*rid, rec); // Memory needs to be released
                if(demand(*record))
                    return record;
                record = nullptr;
            }
            return nullptr;
        }

        ~Scanner(){
            delete rid;
        }
        friend class Table;
};

Scanner* Table::GetScanner(bool (*demand)(const Record& record)){
    return new Scanner(this, demand);
}

#endif