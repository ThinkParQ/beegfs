#ifndef MODE_H_
#define MODE_H_

#include <common/nodes/Node.h>
#include <common/nodes/NodeStoreClients.h>
#include <common/nodes/NodeStoreServers.h>
#include <common/Common.h>
#include <modes/modehelpers/ModeHelper.h>


class Mode
{
   public:
      virtual ~Mode() {}

      /**
       * Executes the mode inside the calling thread.
       *
       * @return APPCODE_...
       */
      virtual int execute() = 0;

   protected:
      Mode(bool skipInit = false);

   private:
      void initializeCommonObjects();

};

#endif /*MODE_H_*/
