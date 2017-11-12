#ifndef STATSHELPER_H
#define STATSHELPER_H


#include <components/Database.h>



#define DISPLAY_FLOATING_AVERAGE_ORDER       31 //a odd value is required

namespace statshelper
{
   TabType selectTableTypeAndTimeRange(uint timespanM, uint64_t* outValueTimeRange);
   TabType selectTableType(uint timespanM);

   void floatingAverage(UInt64List *inList, UInt64List *inTimesList, UInt64List *outList,
      UInt64List *outTimesList, unsigned short order);
};

#endif /*STATSHELPER_H*/
