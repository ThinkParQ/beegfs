#ifndef HBMGRNOTIFICATION_H_
#define HBMGRNOTIFICATION_H_

#include <common/nodes/Node.h>
#include <common/Common.h>


/*
 * This file contains an abstract base class for async notifications and also the various tiny
 * sub-classes.
 */


enum HbMgrNotificationType {
   HbMgrNotificationType_NODEADDED,
   HbMgrNotificationType_NODEREMOVED,
   HbMgrNotificationType_TARGETADDED,
   HbMgrNotificationType_REFRESHCAPACITYPOOLS,
   HbMgrNotificationType_REFRESHTARGETSTATES,
   HbMgrNotificationType_BUDDYGROUPADDED,
   HbMgrNotificationType_PUBLISHCAPACITIES,
   HbMgrNotificationType_REFRESHSTORAGEPOOLS,
   HbMgrNotificationType_REFRESHNODE
};



/**
 * Base class for async notifications
 */
class HbMgrNotification
{
   public:
      virtual ~HbMgrNotification() {}

      virtual void processNotification() = 0;


   protected:
      HbMgrNotification(HbMgrNotificationType notificationType) :
         notificationType(notificationType) {}

      HbMgrNotificationType notificationType;


   public:
      // getters & setters

      HbMgrNotificationType getNotificationType()
      {
         return notificationType;
      }
};


class HbMgrNotificationNodeAdded : public HbMgrNotification
{
   public:
      HbMgrNotificationNodeAdded(std::string nodeID, NumNodeID nodeNumID, NodeType nodeType) :
         HbMgrNotification(HbMgrNotificationType_NODEADDED),
         nodeID(nodeID), nodeNumID(nodeNumID), nodeType(nodeType)
      {}

      void processNotification();


   private:
      std::string nodeID;
      NumNodeID nodeNumID;
      NodeType nodeType;
};


class HbMgrNotificationNodeRemoved : public HbMgrNotification
{
   public:
      HbMgrNotificationNodeRemoved(NumNodeID nodeNumID, NodeType nodeType) :
         HbMgrNotification(HbMgrNotificationType_NODEREMOVED),
         nodeNumID(nodeNumID), nodeType(nodeType)
      {}

      void processNotification();


   private:
      NumNodeID nodeNumID;
      NodeType nodeType;
};


class HbMgrNotificationTargetAdded : public HbMgrNotification
{
   public:
      HbMgrNotificationTargetAdded(uint16_t targetID, NumNodeID nodeID,
                                   StoragePoolId storagePoolId):
            HbMgrNotification(HbMgrNotificationType_TARGETADDED),
            targetID(targetID), nodeID(nodeID), storagePoolId(storagePoolId)
      {}

      void processNotification();

   private:
      uint16_t targetID;
      NumNodeID nodeID;
      StoragePoolId storagePoolId;
};

class HbMgrNotificationRefreshCapacityPools : public HbMgrNotification
{
   public:
      HbMgrNotificationRefreshCapacityPools() :
         HbMgrNotification(HbMgrNotificationType_REFRESHCAPACITYPOOLS)
      {}

      void processNotification();
};

class HbMgrNotificationRefreshTargetStates : public HbMgrNotification
{
   public:
      HbMgrNotificationRefreshTargetStates() :
         HbMgrNotification(HbMgrNotificationType_REFRESHTARGETSTATES)
      {}

      void processNotification();
};

class HbMgrNotificationPublishCapacities : public HbMgrNotification
{
   public:
      HbMgrNotificationPublishCapacities() :
         HbMgrNotification(HbMgrNotificationType_PUBLISHCAPACITIES)
      {}

      void processNotification();
};

class HbMgrNotificationMirrorBuddyGroupAdded: public HbMgrNotification
{
   public:
      HbMgrNotificationMirrorBuddyGroupAdded(NodeType nodeType, uint16_t buddyGroupID,
         uint16_t primaryTargetID, uint16_t secondaryTargetID) :
         HbMgrNotification(HbMgrNotificationType_BUDDYGROUPADDED),
         nodeType(nodeType), buddyGroupID(buddyGroupID),
         primaryTargetID(primaryTargetID), secondaryTargetID(secondaryTargetID)
      {
      }

      void processNotification();

   private:
      NodeType nodeType;
      uint16_t buddyGroupID;
      uint16_t primaryTargetID;
      uint16_t secondaryTargetID;
};

class HbMgrNotificationRefreshStoragePools: public HbMgrNotification
{
   public:
      HbMgrNotificationRefreshStoragePools():
         HbMgrNotification(HbMgrNotificationType_REFRESHSTORAGEPOOLS) {}

      void processNotification();
};

class HbMgrNotificationRefreshNode : public HbMgrNotification
{
   public:
      HbMgrNotificationRefreshNode(std::string nodeID, NumNodeID nodeNumID, NodeType nodeType) :
         HbMgrNotification(HbMgrNotificationType_REFRESHNODE),
         nodeID(nodeID), nodeNumID(nodeNumID), nodeType(nodeType)
      {}

      void processNotification();


   private:
      std::string nodeID;
      NumNodeID nodeNumID;
      NodeType nodeType;
};

#endif /* HBMGRNOTIFICATION_H_ */
