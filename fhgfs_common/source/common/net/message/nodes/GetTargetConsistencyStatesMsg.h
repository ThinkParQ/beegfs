#ifndef COMMON_GETTARGETCONSISTENCYSTATESMSG_H
#define COMMON_GETTARGETCONSISTENCYSTATESMSG_H

class GetTargetConsistencyStatesMsg : public NetMessageSerdes<GetTargetConsistencyStatesMsg>
{
   public:
      GetTargetConsistencyStatesMsg(const UInt16Vector& targetIDs)
         : BaseType(NETMSGTYPE_GetTargetConsistencyStates),
           targetIDs(targetIDs)
      { }

      GetTargetConsistencyStatesMsg() : BaseType(NETMSGTYPE_GetTargetConsistencyStates)
      { }

   protected:
      UInt16Vector targetIDs;

   public:
      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % obj->targetIDs;
      }
};

#endif /* COMMON_GETTARGETCONSISTENCYSTATESMSG_H */
