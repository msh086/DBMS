#ifndef RID_H
#define RID_H
class Table;
class BplusTree;
class Scanner;
class Database;

class RID
{
    private:
        /**
         * PageNum should store the physical page index, starting from 1 and reserving 0 for header page
         * When converting a RID to total slot num, use (RID.PageNum - 1) * Header.slotNum + RID.slotNum
        */
        unsigned int PageNum;
        unsigned int SlotNum;//The No.x record in a page
    public:
        RID():PageNum(0),SlotNum(0){}
        RID(unsigned int PageNum,unsigned int SlotNum):PageNum(PageNum),SlotNum(SlotNum){}
        unsigned int GetPageNum()const{
            return PageNum;
        }
        unsigned int GetSlotNum()const{
            return SlotNum;
        }
        friend class Table;
        friend class BplusTree;
        friend class Scanner;
        friend class Database;
};


#endif // RID_H_INCLUDED
