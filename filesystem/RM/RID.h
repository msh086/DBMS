#ifndef RID_H_INCLUDED
#define RID_H_INCLUDED
class Table;

class RID
{
    private:
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
};


#endif // RID_H_INCLUDED
