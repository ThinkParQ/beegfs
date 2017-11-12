#ifndef GETNODECAPACITYPOOLSMSG_H_
#define GETNODECAPACITYPOOLSMSG_H_

#include <common/Common.h>
#include "../SimpleIntMsg.h"


enum CapacityPoolQueryType
{
   CapacityPoolQuery_META = 0,
   CapacityPoolQuery_STORAGE = 1,
   CapacityPoolQuery_METABUDDIES = 2,
   CapacityPoolQuery_STORAGEBUDDIES = 3
};


class GetNodeCapacityPoolsMsg : public SimpleIntMsg
{
   public:
      GetNodeCapacityPoolsMsg(CapacityPoolQueryType poolType) :
         SimpleIntMsg(NETMSGTYPE_GetNodeCapacityPools, poolType)
      {
      }

      /**
       * For deserialization only.
       */
      GetNodeCapacityPoolsMsg() : SimpleIntMsg(NETMSGTYPE_GetNodeCapacityPools)
      {
      }

      static std::string queryTypeToStr(CapacityPoolQueryType t)
      {
         switch (t)
         {
            case CapacityPoolQuery_META:
               return "Meta";
            case CapacityPoolQuery_STORAGE:
               return "Storage";
            case CapacityPoolQuery_METABUDDIES:
               return "Meta buddies";
            case CapacityPoolQuery_STORAGEBUDDIES:
               return "Storage buddies";
            default:
               return "<unknown>";
         }
      }

      // getters & setters

      CapacityPoolQueryType getCapacityPoolQueryType()
      {
         return (CapacityPoolQueryType)getValue();
      }
};

#endif /* GETNODECAPACITYPOOLSMSG_H_ */
