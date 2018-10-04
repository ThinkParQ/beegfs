#ifndef LOCKING_H_
#define LOCKING_H_

#include <common/Common.h>
#include <common/nodes/NumNodeID.h>
#include <common/storage/StorageDefinitions.h>


/**
 * The file lock type, e.g. append lock or entry lock (=>flock) for LockEntryNotificationWork.
 */
enum LockEntryNotifyType
{
   LockEntryNotifyType_APPEND = 0,
   LockEntryNotifyType_FLOCK,
};

enum RangeOverlapType
{
   RangeOverlapType_NONE=0,
   RangeOverlapType_EQUALS=1, // ranges are equal
   RangeOverlapType_CONTAINS=2, // first range wraps around second range
   RangeOverlapType_ISCONTAINED=3, // first range is contained within second range
   RangeOverlapType_STARTOVERLAP=4, // second range overlaps start of first range
   RangeOverlapType_ENDOVERLAP=5 // second range overlaps end of first range
};


/**
 * Entry-locks are treated per FD, e.g. a single process will block itself when it holds an
 * exclusive lock and tries to get another exclusive lock via another file descriptor. That's why
 * we compare clientID and clientFD. (This is different for range-locks.)
 */
struct EntryLockDetails
{
   /**
    * @param lockTypeFlags ENTRYLOCKTYPE_...
    */
   EntryLockDetails(NumNodeID clientNumID, int64_t clientFD, int ownerPID,
      const std::string& lockAckID, int lockTypeFlags):
      clientNumID(clientNumID), clientFD(clientFD), ownerPID(ownerPID), lockAckID(lockAckID),
      lockTypeFlags(lockTypeFlags)
   { }

   /**
    * Constructor for unset lock (empty clientID; other fields not init'ed).
    */
   EntryLockDetails() {}


   NumNodeID clientNumID;
   int64_t clientFD; // unique handle on the corresponding client
   int32_t ownerPID; // pid on client (just informative, because shared on fork() )
   std::string lockAckID; /* ID for ack message when log is granted (and to identify duplicate
      requests, so it must be a globally unique ID) */
   int32_t lockTypeFlags; // ENTRYLOCKTYPE_...

   template<typename This, typename Ctx>
   static void serialize(This obj, Ctx& ctx)
   {
      ctx
         % obj->clientNumID
         % obj->clientFD
         % obj->ownerPID
         % serdes::stringAlign4(obj->lockAckID)
         % obj->lockTypeFlags;
   }

   bool operator==(const EntryLockDetails& other) const;

   bool operator!=(const EntryLockDetails& other) const { return !(*this == other); }

   void initRandomForSerializationTests();

   bool isSet() const
   {
      return bool(clientNumID);
   }

   bool allowsWaiting() const
   {
      return !(lockTypeFlags & ENTRYLOCKTYPE_NOWAIT);
   }

   bool isUnlock() const
   {
      return (lockTypeFlags & ENTRYLOCKTYPE_UNLOCK);
   }

   void setUnlock()
   {
      lockTypeFlags |= ENTRYLOCKTYPE_UNLOCK;
      lockTypeFlags &= ~(ENTRYLOCKTYPE_EXCLUSIVE | ENTRYLOCKTYPE_SHARED | ENTRYLOCKTYPE_CANCEL);
   }

   bool isExclusive() const
   {
      return (lockTypeFlags & ENTRYLOCKTYPE_EXCLUSIVE);
   }

   bool isShared() const
   {
      return (lockTypeFlags & ENTRYLOCKTYPE_SHARED);
   }

   bool isCancel() const
   {
      return (lockTypeFlags & ENTRYLOCKTYPE_CANCEL);
   }

   NumNodeID getClientNumID()
   {
      return clientNumID;
   }

   /**
    * Compares clientID and clientFD.
    */
   bool equalsHandle(const EntryLockDetails& other) const
   {
      // note: if you make changes here, you (probably) also need to change the MapComparator

      return (clientFD == other.clientFD) && (clientNumID == other.clientNumID);
   }

   std::string toString() const
   {
      std::ostringstream outStream;
      outStream <<
         "clientNumID: " << clientNumID << "; " <<
         "clientFD: "    << clientFD << "; " <<
         "ownerPID: "    << ownerPID << "; " <<
         "lockAckID: "   << lockAckID << "; " <<
         "lockTypeFlags: ";

      // append lockTypeFlags
      if(isUnlock() )
         outStream << "u";
      if(isShared() )
         outStream << "s";
      if(isExclusive() )
         outStream << "x";
      if(isCancel() )
         outStream << "c";
      if(allowsWaiting() )
         outStream << "w";

      return outStream.str();
   }

   struct MapComparator
   {
      /**
       * Order by file handle.
       *
       * @return true if a is smaller than b
       */
      bool operator() (const EntryLockDetails& a, const EntryLockDetails& b) const
      {
         return (a.clientFD < b.clientFD) ||
           ( (a.clientFD == b.clientFD) && (a.clientNumID < b.clientNumID) );
      }
   };
};


typedef std::set<EntryLockDetails, EntryLockDetails::MapComparator> EntryLockDetailsSet;
typedef EntryLockDetailsSet::iterator EntryLockDetailsSetIter;
typedef EntryLockDetailsSet::const_iterator EntryLockDetailsSetCIter;

typedef std::list<EntryLockDetails> EntryLockDetailsList;
typedef EntryLockDetailsList::iterator EntryLockDetailsListIter;
typedef EntryLockDetailsList::const_iterator EntryLockDetailsListCIter;


/**
 * A simple container for flock/append entry lock queue pointers.
 *
 * We have this to easily pass the different flock and append queues to the corresponding generic
 * lock management methods.
 */
class EntryLockQueuesContainer
{
   public:
      EntryLockQueuesContainer(EntryLockDetails* exclLock, EntryLockDetailsSet* sharedLocks,
      EntryLockDetailsList* waitersExclLock, EntryLockDetailsList* waitersSharedLock,
      StringSet* waitersLockIDs, LockEntryNotifyType lockType = LockEntryNotifyType_FLOCK) :
         exclLock(exclLock), sharedLocks(sharedLocks), waitersExclLock(waitersExclLock),
         waitersSharedLock(waitersSharedLock), waitersLockIDs(waitersLockIDs), lockType(lockType)
      {
         // (all inits done in initializer list)
      }

      EntryLockDetails* exclLock; // current exclusiveTID lock
      EntryLockDetailsSet* sharedLocks; // current shared locks (key is lock, value is dummy)
      EntryLockDetailsList* waitersExclLock; // queue (append new to end, pop from top)
      EntryLockDetailsList* waitersSharedLock; // queue (append new to end, pop from top)
      StringSet* waitersLockIDs; // currently enqueued lockIDs (for fast duplicate check)

      LockEntryNotifyType lockType; // to be passed to LockingNotifier
};


/**
 * A simple container for append entry lock queue pointers
 *
 * This contains dummy queues for shared locks, because append does not use shared locks, but the
 * generic lock management methods expect them to exist (which could be optimized out if necessary).
 */
class AppendLockQueuesContainer : public EntryLockQueuesContainer
{
   public:
      AppendLockQueuesContainer(EntryLockDetails* exclLock, EntryLockDetailsList* waitersExclLock,
      StringSet* waitersLockIDsFLock) :
         EntryLockQueuesContainer(exclLock, &sharedLocksDummy, waitersExclLock,
            &waitersSharedLockDummy, waitersLockIDsFLock, LockEntryNotifyType_APPEND)
      {
         // (all inits done in initializer list)
      }

   private:
      EntryLockDetailsSet sharedLocksDummy; // dummy, because append uses exclusive only
      EntryLockDetailsList waitersSharedLockDummy; // dummy, because append uses exclusive only
};


/**
 * Range-locks are treated per-process, e.g. independent of different file descriptors or threads,
 * a process cannot block itself with two exclusive locks. That's why we only compare clientID and
 * ownerPID here. (This is different for entry-locks.)
 */
struct RangeLockDetails
{
   RangeLockDetails(NumNodeID clientNumID, int ownerPID, const std::string& lockAckID,
      int lockTypeFlags, uint64_t start, uint64_t end) :
      clientNumID(clientNumID), ownerPID(ownerPID), lockAckID(lockAckID),
      lockTypeFlags(lockTypeFlags), start(start), end(end)
   { }

   /**
    * Constructor for unset lock (empty clientID; other fields not init'ed).
    */
   RangeLockDetails() {}

   NumNodeID clientNumID;
   int32_t ownerPID; // pid on client
   std::string lockAckID; /* ID for ack message when log is granted (and to identify duplicate
      requests, so it must be globally unique) */
   int32_t lockTypeFlags; // ENTRYLOCKTYPE_...

   uint64_t start;
   uint64_t end; // inclusive end

   template<typename This, typename Ctx>
   static void serialize(This obj, Ctx& ctx)
   {
      ctx
         % obj->clientNumID
         % obj->ownerPID
         % serdes::stringAlign4(obj->lockAckID)
         % obj->lockTypeFlags
         % obj->start
         % obj->end;
   }

   bool operator==(const RangeLockDetails& other) const;

   bool operator!=(const RangeLockDetails& other) const { return !(*this == other); }

   void initRandomForSerializationTests();

   bool isSet() const
   {
      return bool(clientNumID);
   }

   bool allowsWaiting() const
   {
      return !(lockTypeFlags & ENTRYLOCKTYPE_NOWAIT);
   }

   bool isUnlock() const
   {
      return (lockTypeFlags & ENTRYLOCKTYPE_UNLOCK);
   }

   void setUnlock()
   {
      lockTypeFlags |= ENTRYLOCKTYPE_UNLOCK;
      lockTypeFlags &= ~(ENTRYLOCKTYPE_EXCLUSIVE | ENTRYLOCKTYPE_SHARED | ENTRYLOCKTYPE_CANCEL);
   }

   bool isExclusive() const
   {
      return (lockTypeFlags & ENTRYLOCKTYPE_EXCLUSIVE);
   }

   bool isShared() const
   {
      return (lockTypeFlags & ENTRYLOCKTYPE_SHARED);
   }

   bool isCancel() const
   {
      return (lockTypeFlags & ENTRYLOCKTYPE_CANCEL);
   }

   /**
    * Compares clientID and ownerPID.
    */
   bool equalsHandle(const RangeLockDetails& other) const
   {
      // note: if you make changes here, you (probably) also need to change the MapComparators

      return (ownerPID == other.ownerPID) && (clientNumID == other.clientNumID);
   }

   /**
    * Check if ranges overlap or directly extend each other.
    * Note: this just checks ranges, not owner (which is good, because we rely on that at some
    *    points in the code)
    */
   bool isMergeable(const RangeLockDetails& other) const
   {
      // check if other region ends before this one or starts after this one
      // (+1 is because we are not only looking for overlaps, but also for extensions)
      if( ( (other.end+1) < start) || (other.start > (end+1) ) )
         return false;

      // other range overlaps or is directly extending this range
      return true;
   }

   /**
    * Check if ranges have common values.
    */
   bool overlaps(const RangeLockDetails& other) const
   {
      // check if other region ends before this one or starts after this one
      if( (other.end < start) || (other.start > end) )
         return false;

      // other range not before or after this one => overlap
      return true;
   }

   RangeOverlapType overlapsEx(const RangeLockDetails& other) const
   {
      if( (other.end < start) || (other.start > end) )
         return RangeOverlapType_NONE; // other region ends before this one or starts after this one

      if( (other.start == start) && (other.end == end) )
         return RangeOverlapType_EQUALS;

      if( (start <= other.start) && (end >= other.end) )
         return RangeOverlapType_CONTAINS; // this range contains other range

      if( (start >= other.start) && (end <= other.end) )
         return RangeOverlapType_ISCONTAINED; // this range is contained in other range

      if(start < other.start)
         return RangeOverlapType_STARTOVERLAP; // other range overlaps at start of this range

      return RangeOverlapType_ENDOVERLAP; // other range overlaps at end of this range
   }

   /**
    * Trim the part of this lock that overlaps with trimmer.
    * The caller must make sure that the resulting region has "length>0" and that this is a
    * real one-sided overlap (none of the regions contains the other except if start or end match).
    */
   void trim(const RangeLockDetails& trimmer)
   {
      if(trimmer.end < end)
         start = trimmer.end+1; // trim on start-side
      else
         end = trimmer.start-1; // trim on end-side
   }

   /**
    * Split this region by the splitter region.
    * The result for this will be the remaining left side of splitter, outEndRegion will be set to
    * the remaining right side of splitter.
    * The caller must make sure that both resulting regions have "length>0", so splitter must be
    * completely contained in this and start/end of this/splitter may not match.
    */
   void split(const RangeLockDetails& splitter, RangeLockDetails& outEndRegion)
   {
      // right side remainder
      outEndRegion = *this;
      outEndRegion.start = splitter.end+1;

      // trim left side remainder
      end = splitter.start-1;
   }

   /**
    * Extends this region by the dimension of other region.
    * Caller must make sure that the regions actually do overlap.
    */
   void merge(const RangeLockDetails& other)
   {
      start = BEEGFS_MIN(start, other.start);
      end = BEEGFS_MAX(end, other.end);
   }

   std::string toString() const
   {
      std::ostringstream outStream;
      outStream <<
         "clientNumID: " << clientNumID << "; " <<
         "PID: " << ownerPID << "; " <<
         "lockAckID: " << lockAckID << "; " <<
         "range: " << start << " - " << end << "; " <<
         "lockTypeFlags: ";

      // append lockTypeFlags
      if(isUnlock() )
         outStream << "u";
      if(isShared() )
         outStream << "s";
      if(isExclusive() )
         outStream << "x";
      if(isCancel() )
         outStream << "c";
      if(allowsWaiting() )
         outStream << "w";

      return outStream.str();
   }

   struct MapComparatorShared
   {
      /**
       * Order by file handle, then range start (to make overlap detection for same handle easy).
       *
       * @return true if a is smaller than b
       */
      bool operator() (const RangeLockDetails& a, const RangeLockDetails& b) const
      {
         if(a.ownerPID < b.ownerPID)
            return true;
         if(a.ownerPID > b.ownerPID)
            return false;

         // equal ownerPID

         if(a.clientNumID < b.clientNumID)
            return true;
         if(a.clientNumID > b.clientNumID)
            return false;

         // equal clientID

         if(a.start < b.start)
            return true;

         return false;
      }
   };

   struct MapComparatorExcl
   {
      /**
       * Order by range start (to make general overlap detection easy).
       *
       * @return true if a is smaller than b
       */
      bool operator() (const RangeLockDetails& a, const RangeLockDetails& b) const
      {
         // note: this only works because we cannot have range overlaps for exclusive locks
         return (a.start < b.start);
      }
   };

};


typedef std::set<RangeLockDetails, RangeLockDetails::MapComparatorShared> RangeLockSharedSet;
typedef RangeLockSharedSet::iterator RangeLockSharedSetIter;
typedef RangeLockSharedSet::const_iterator RangeLockSharedSetCIter;

typedef std::set<RangeLockDetails, RangeLockDetails::MapComparatorExcl> RangeLockExclSet;
typedef RangeLockExclSet::iterator RangeLockExclSetIter;
typedef RangeLockExclSet::const_iterator RangeLockExclSetCIter;

typedef std::list<RangeLockDetails> RangeLockDetailsList;
typedef RangeLockDetailsList::iterator RangeLockDetailsListIter;
typedef RangeLockDetailsList::const_iterator RangeLockDetailsListCIter;


#endif /* LOCKING_H_ */
