#ifndef MODEHASHDIR_H_
#define MODEHASHDIR_H_

#include <app/App.h>
#include <common/Common.h>
#include <common/nodes/NodeStoreServers.h>
#include <modes/Mode.h>

class ModeHashDir : public Mode
{
   public:

      virtual int execute();

      static void printHelp();

   private:

};

#endif /* MODEHASHDIR_H_ */
