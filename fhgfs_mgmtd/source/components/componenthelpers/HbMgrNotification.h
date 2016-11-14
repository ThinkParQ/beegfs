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
   HbMgrNotificationType_PUBLISHCAPACITIES
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

      void propagateAddedNode(Node& node);
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

      void propagateRemovedNode();
};


class HbMgrNotificationTargetAdded : public HbMgrNotification
{
   public:
      HbMgrNotificationTargetAdded(uint16_t targetID, NumNodeID nodeID) :
         HbMgrNotification(HbMgrNotificationType_TARGETADDED),
         targetID(targetID), nodeID(nodeID)
      {}

      void processNotification();


   private:
      uint16_t targetID;
      NumNodeID nodeID;

      void propagateAddedTarget();
};

class HbMgrNotificationRefreshCapacityPools : public HbMgrNotification
{
   public:
      HbMgrNotificationRefreshCapacityPools() :
         HbMgrNotification(HbMgrNotificationType_REFRESHCAPACITYPOOLS)
      {}

      void processNotification();


   private:
      void propagateRefreshCapacityPools();
};

class HbMgrNotificationRefreshTargetStates : public HbMgrNotification
{
   public:
      HbMgrNotificationRefreshTargetStates() :
         HbMgrNotification(HbMgrNotificationType_REFRESHTARGETSTATES)
      {}

      void processNotification();


   private:
      void propagateRefreshTargetStates();
};

class HbMgrNotificationPublishCapacities : public HbMgrNotification
{
   public:
      HbMgrNotificationPublishCapacities() :
         HbMgrNotification(HbMgrNotificationType_PUBLISHCAPACITIES)
      {}

      void processNotification();

   private:
      void propagatePublishCapacities();
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

      void propagateAddedMirrorBuddyGroup();
};

#endif /* HBMGRNOTIFICATION_H_ */
