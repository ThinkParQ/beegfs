#ifndef COMMON_GETTARGETCONSISTENCYSTATERESPSMSG_H
#define COMMON_GETTARGETCONSISTENCYSTATERESPSMSG_H

class GetTargetConsistencyStatesRespMsg : public NetMessageSerdes<GetTargetConsistencyStatesRespMsg>
{
   public:
      GetTargetConsistencyStatesRespMsg(const TargetConsistencyStateVec& states)
         : BaseType(NETMSGTYPE_GetTargetConsistencyStatesResp),
           states(states)
      { }

      GetTargetConsistencyStatesRespMsg() : BaseType(NETMSGTYPE_GetTargetConsistencyStatesResp)
      { }

   protected:
      TargetConsistencyStateVec states;

   public:
      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % obj->states;
      }

      const TargetConsistencyStateVec& getStates() const { return states; }
};

#endif /* COMMON_GETTARGETCONSISTENCYSTATERESPSMSG_H */
