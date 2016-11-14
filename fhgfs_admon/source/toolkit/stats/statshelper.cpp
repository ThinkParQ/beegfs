#include "statshelper.h"


namespace statshelper
{
   TabType selectTableTypeAndTimeRange(uint timespanM, uint64_t* outValueTimeRange)
   {
      if (timespanM > 7200) // more than 5 days -> daily
      {
         *outValueTimeRange = 57600000; //16 hours before and after the time value
         return TABTYPE_Daily;
      }
      else
      if (timespanM > 1440) // more than 1 day
      {
         *outValueTimeRange = 2520000; //42 minutes before and after the time value
         return TABTYPE_Hourly;
      }
      else
      {
         *outValueTimeRange = 2000; //2 seconds before and after the time value
         return TABTYPE_Normal;
      }
   }

   TabType selectTableType(uint timespanM)
   {
      uint64_t value;

      return statshelper::selectTableTypeAndTimeRange(timespanM, &value);
   }

   /**
    * calculates a floating average with slightly changed time values
    *
    * @param order count of values which are used for the calculation of a average value, this
    * value must be a odd value
    */
   void floatingAverage(UInt64List *inList, UInt64List *inTimesList, UInt64List *outList,
      UInt64List *outTimesList, unsigned short order)
   {
      int halfOrder = order/2;

      // if order is smaller than 2 or if inList smaller than order it doesn't make any sense
      if ((order < 2) || (inList->size() < order))
      {
         return;
      }

      uint64_t sum = 0;
      UInt64ListIter addValueIter = inList->begin();
      UInt64ListIter subValueIter = inList->begin();
      UInt64ListIter timesIter = inTimesList->begin();

      // calculate the values which doesn't have the full order at the beginning of the chart
      for (int valueCount = 0; valueCount < order; addValueIter++, timesIter++)
      {
         //sum values for the first average value
         while(valueCount < halfOrder)
         {
            sum += *addValueIter;
            valueCount++;
            addValueIter++;
         }

         sum += *addValueIter;
         valueCount++;

         outList->push_back(sum / valueCount);
         outTimesList->push_back(*timesIter);
      }

      // calculate the values with the the full order
      for(; addValueIter != inList->end(); addValueIter++, subValueIter++, timesIter++)
      {
         sum += *addValueIter;
         sum -= *subValueIter;

         outList->push_back(sum / order);
         outTimesList->push_back(*timesIter);
      }

      // calculate the values which doesn't have the full order at the end of the chart
      for(int valueCount = order; timesIter != inTimesList->end(); subValueIter++, timesIter++)
      {
         sum -= *subValueIter;
         valueCount--;

         outList->push_back(sum / valueCount);
         outTimesList->push_back(*timesIter);
      }
   }
};
