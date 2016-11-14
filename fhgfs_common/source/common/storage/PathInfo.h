/*
 * class PathInfo - required information to find an inode or chunk files
 *
 * NOTE: If you change this file, do not forget to adjust the client side PathInfo.h
 */

#ifndef PATHINFO_H_
#define PATHINFO_H_

#include <common/Common.h>
#include <common/toolkit/serialization/Serialization.h>

#define PATHINFO_FEATURE_ORIG         1 /* inidicate chunks are stored with origParentUID and
                                           and origParentEntryID, i.e. 2014.01 style layout */
#define PATHINFO_FEATURE_ORIG_UNKNOWN 2 /* indicates _FEATURE_ORIG is unknown and needs to be
                                           requested from the meta-inode */

class PathInfo; // forward declaration


typedef std::list<PathInfo> PathInfoList;
typedef PathInfoList::iterator PathInfoListIter;
typedef PathInfoList::const_iterator PathInfoListConstIter;


/**
 * Information about a file/directory
 */
class PathInfo
{
   public:
      PathInfo()
         : flags(0), origParentUID(0)
      {
      }

      PathInfo(unsigned origParentUID, std::string origParentEntryID, int flags) :
         flags(flags), origParentUID(origParentUID), origParentEntryID(origParentEntryID)
      {
         // everything is set in initializer list
      }


   private:
      int32_t flags;

      /* orig values do not change on file updates, such as rename or chown and are used
       * to get the chunk file path */
      uint32_t origParentUID;
      std::string origParentEntryID;

   public:
      // inliners

      void set(unsigned origParentUID, std::string origParentEntryID, int flags)
      {
         this->origParentUID     = origParentUID;
         this->origParentEntryID = origParentEntryID;
         this->flags             = flags;
      }

      void set(PathInfo *newPathInfo)
      {
         this->origParentUID     = newPathInfo->getOrigUID();
         this->origParentEntryID = newPathInfo->getOrigParentEntryID();
         this->flags             = newPathInfo->getFlags();
      }

      int getFlags() const
      {
         return this->flags;
      }

      void setFlags(int flags)
      {
         this->flags = flags;
      }

      unsigned getOrigUID() const
      {
         return this->origParentUID;
      }

      void setOrigUID(unsigned origParentUID)
      {
         this->origParentUID = origParentUID;
      }

      std::string getOrigParentEntryID() const
      {
         return this->origParentEntryID;
      }

      void setOrigParentEntryID(std::string origParentEntryID)
      {
         this->origParentEntryID = origParentEntryID;
      }

      /**
       * Returns true for new 2014.01 style layout with timestamp and user dirs, false for 2012.10
       * style layout with only hashdirs.
       */
      bool hasOrigFeature() const
      {
         return this->flags & PATHINFO_FEATURE_ORIG ? true : false;
      }

      bool operator==(const PathInfo& other) const
      {
         // consider this to be equal if ALL static parameters are equal
         if (origParentUID != other.origParentUID)
            return false;

         if (origParentEntryID != other.origParentEntryID)
            return false;

         if (flags != other.flags)
            return false;

         return true;
      }

      bool operator!=(const PathInfo& other) const
      {
         return !(operator==( other ) );
      }

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx % obj->flags;

         if (obj->flags & PATHINFO_FEATURE_ORIG)
         {
            ctx
               % obj->origParentUID
               % serdes::stringAlign4(obj->origParentEntryID);
         }
      }
};

#endif /* PATHINFO_H_ */
