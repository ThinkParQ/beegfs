#ifndef CTL_MODESETSTATE
#define CTL_MODESETSTATE

#include <modes/Mode.h>

class ModeSetState : public Mode
{
   public:
      ModeSetState() {}

      virtual int execute();

      static void printHelp();

   private:
      NodeType nodeType;

      int doSet(uint16_t targetID, TargetConsistencyState state);
};

#endif /* CTL_MODESETSTATE */
