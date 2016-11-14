#ifndef GETNODEINFOMSGEX_H_
#define GETNODEINFOMSGEX_H_

#include <common/net/message/admon/GetNodeInfoMsg.h>
#include <common/app/log/LogContext.h>
#include <app/App.h>


class GetNodeInfoMsgEx : public GetNodeInfoMsg
{
   public:
      virtual bool processIncoming(ResponseContext& ctx);

   private:
      void getMemInfo(int *memTotal, int *memFree);
      void getCPUInfo(std::string *cpuName, int *cpuSpeed, int *cpuCount);

};

#endif /*GETNODEINFOMSGEX_H_*/
