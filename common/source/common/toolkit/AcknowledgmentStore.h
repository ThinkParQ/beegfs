#ifndef ACKNOWLEDGMENTSTORE_H_
#define ACKNOWLEDGMENTSTORE_H_

#include <common/Common.h>
#include <common/nodes/Node.h>
#include <common/threading/Condition.h>


class WaitAckNotification
{
   public:
      Mutex waitAcksMutex; // this mutex also syncs access to the waitMap/receivedMap during the
         // wait phase (which is between registration and deregistration)
      Condition waitAcksCompleteCond; // in case all WaitAcks have been received
};


class WaitAck
{
   public:
      /**
       * @param privateData any private data that helps the caller to identify to ack later
       */
      WaitAck(std::string ackID, void* privateData) :
         ackID(ackID), privateData(privateData)
      { /* all inits done in initializer list */ }

      std::string ackID;
      void* privateData; // caller's private data
};


typedef std::map<std::string, WaitAck> WaitAckMap; // keys are ackIDs
typedef WaitAckMap::iterator WaitAckMapIter;
typedef WaitAckMap::value_type WaitAckMapVal;


class AckStoreEntry
{
   public:
      std::string ackID;

      WaitAckMap* waitMap; // ack will be removed from this map if it is received
      WaitAckMap* receivedMap; // ack will be added to this map if it is received

      WaitAckNotification* notifier;
};


typedef std::map<std::string, AckStoreEntry> AckStoreMap;
typedef AckStoreMap::iterator AckStoreMapIter;
typedef AckStoreMap::value_type AckStoreMapVal;



class AcknowledgmentStore
{
   public:
      ~AcknowledgmentStore()
      {
         storeMap.clear();
      }

      void registerWaitAcks(WaitAckMap* waitAcks, WaitAckMap* receivedAcks,
         WaitAckNotification* notifier);
      void unregisterWaitAcks(WaitAckMap* waitAcks);
      void receivedAck(std::string ackID);
      bool waitForAckCompletion(WaitAckMap* waitAcks, WaitAckNotification* notifier, int timeoutMS);


   private:
      AckStoreMap storeMap;
      Mutex mutex;


   public:
      // inliners


};

#endif /*ACKNOWLEDGMENTSTORE_H_*/
