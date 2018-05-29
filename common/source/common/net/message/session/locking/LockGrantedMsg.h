#ifndef LOCKGRANTEDMSG_H_
#define LOCKGRANTEDMSG_H_

#include <common/net/message/AcknowledgeableMsg.h>
#include <common/nodes/NumNodeID.h>


class LockGrantedMsg : public AcknowledgeableMsgSerdes<LockGrantedMsg>
{
   public:

      /**
       * @param lockAckID just a reference, so do not free it as long as you use this object!
       * @param ackID reply ack; just a reference, so do not free it as long as you use this object!
       * @param granterNodeID nodeID of the sender of this msg (=> receiver of the ack); just a
       * reference, so do not free it as long as you use this object!
       */
      LockGrantedMsg(const std::string& lockAckID, const std::string& ackID,
         NumNodeID granterNodeID)
         : BaseType(NETMSGTYPE_LockGranted, ackID.c_str() )
      {
         this->lockAckID = lockAckID.c_str();
         this->lockAckIDLen = lockAckID.length();

         this->granterNodeID = granterNodeID;
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % serdes::rawString(obj->lockAckID, obj->lockAckIDLen, 4);
         obj->serializeAckID(ctx, 4);
         ctx % obj->granterNodeID;
      }


   protected:
      /**
       * Constructor for deserialization only
       */
      LockGrantedMsg() : BaseType(NETMSGTYPE_LockGranted)
      {
      }

   private:
      unsigned lockAckIDLen;
      const char* lockAckID;
      NumNodeID granterNodeID;

   public:

      // getters & setters
      const char* getLockAckID() const
      {
         return lockAckID;
      }

      NumNodeID getGranterNodeID()
      {
         return granterNodeID;
      }

};


#endif /* LOCKGRANTEDMSG_H_ */
