#ifndef MODEGENERICDEBUG_H_
#define MODEGENERICDEBUG_H_

#include <common/Common.h>
#include <common/nodes/NodeStore.h>
#include "Mode.h"


class ModeGenericDebug : public Mode
{
   public:
      ModeGenericDebug()
      {
      }

      virtual int execute();

      static void printHelp();


   private:

      bool communicate(NodeStore* nodes, std::string nodeID, std::string cmdStr,
         std::string* outCmdRespStr);
};

#endif /* MODEGENERICDEBUG_H_ */
