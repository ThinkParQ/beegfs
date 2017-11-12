#ifndef QUOTADATA_H_
#define QUOTADATA_H_


#include <common/Common.h>
#include <common/toolkit/serialization/Serialization.h>

#include <sys/quota.h>


class QuotaData;                            //forward declaration

typedef std::map<unsigned, QuotaData> QuotaDataMap;
typedef QuotaDataMap::iterator QuotaDataMapIter;
typedef QuotaDataMap::const_iterator QuotaDataMapConstIter;
typedef QuotaDataMap::value_type QuotaDataMapVal;

typedef std::map<uint16_t, QuotaDataMap> QuotaDataMapForTarget;
typedef QuotaDataMapForTarget::iterator QuotaDataMapForTargetIter;
typedef QuotaDataMapForTarget::const_iterator QuotaDataMapForTargetConstIter;
typedef QuotaDataMapForTarget::value_type QuotaDataMapForTargetMapVal;

typedef std::list<QuotaData> QuotaDataList;
typedef QuotaDataList::iterator QuotaDataListIter;
typedef QuotaDataList::const_iterator QuotaDataListConstIter;


#define QUOTADATAMAPFORTARGET_ALL_TARGETS_ID          0


/**
 * enum for the different quota types
 */
enum QuotaDataType
{
   QuotaDataType_NONE = 0,
   QuotaDataType_USER = 1,
   QuotaDataType_GROUP = 2
};


/**
 * enum for the type of quota limit
 */
enum QuotaLimitType
{
   QuotaLimitType_NONE=0,
   QuotaLimitType_SIZE=1,
   QuotaLimitType_INODE=2
};


/**
 * enum for the different quota errors (return error codes)
 */
enum QuotaExceededErrorType
{
   QuotaExceededErrorType_NOT_EXCEEDED = 0,
   QuotaExceededErrorType_USER_SIZE_EXCEEDED = 1,
   QuotaExceededErrorType_USER_INODE_EXCEEDED = 2,
   QuotaExceededErrorType_GROUP_SIZE_EXCEEDED = 3,
   QuotaExceededErrorType_GROUP_INODE_EXCEEDED = 4
};


/**
 * struct with all quota related informations of a user session, extended with info about enabled
 * quota enforcement
 */
struct SessionQuotaInfo
{
      /**
       * Constructor, struct with all quota related informations of a user session
       *
       * @param useQuota true, if the client has quota enabled (part of the session)
       * @param enforceQuota true, if the server has quota enforcement enabled (part of server conf)
       * @param uid the UID of the session (part of the session)
       * @param gid the GID of the session (part of the session)
       */
      SessionQuotaInfo(bool useQuota, bool enforceQuota, unsigned uid, unsigned gid) :
         useQuota(useQuota), enforceQuota(enforceQuota), uid(uid), gid(gid)
      { /* all inits done in initlializer list */ }

      bool useQuota;       // fhgfs-client has quota enabled
      bool enforceQuota;   // fhgfs-storage has quota enforcement enabled
      unsigned uid;        // user id
      unsigned gid;        // group id
};


class QuotaData
{
   public:
      QuotaData(unsigned id, QuotaDataType type)
      {
         this->id = id;
         this->type = type;
         this->size = 0;
         this->inodes = 0;
         this->valid = true;
      }

      /*
      * For deserialization only
      */
      QuotaData()
      {

      }

      static bool saveQuotaDataMapForTargetToFile(const QuotaDataMapForTarget& map,
         const std::string& path);
      static bool loadQuotaDataMapForTargetFromFile(QuotaDataMapForTarget& outMap,
         const std::string& path);

      static void quotaDataMapToString(const QuotaDataMap& map, QuotaDataType quotaDataType,
         std::ostringstream* outStream);
      static void quotaDataMapForTargetToString(const QuotaDataMapForTarget& map,
         QuotaDataType quotaDataType, std::ostringstream* outStream);
      static void quotaDataListToString(const QuotaDataList& list, std::ostringstream* outStream);


   protected:


   private:
      unsigned id;         // UID or GID
      int type;            // enum QuotaDataType
      uint64_t size;       // defined size limit or used size (bytes)
      uint64_t inodes;     // defined inodes limit or used inodes (counter)
      bool valid;          // true, if this QuotaData are valid

   public:
      //getter and setter

      template<typename This, typename Ctx>
      static void serialize(This obj, Ctx& ctx)
      {
         ctx
            % obj->size
            % obj->inodes
            % obj->id
            % obj->type
            % obj->valid;
      }

      unsigned getID() const
      {
         return id;
      }

      void setID(unsigned id)
      {
         this->id = id;
      }

      uint64_t getSize() const
      {
         return this->size;
      }

      uint64_t getInodes() const
      {
         return this->inodes;
      }

      /**
       * sets the size, sets the inodes count and sets the valid flag on true
       *
       * @param size the size counter, the defined size limit or the used size
       * @param inodes the inodes counter, the defined inodes limit or the used inodes
       */
      void setQuotaData(uint64_t size, uint64_t inodes)
      {
         this->size = size;
         this->inodes = inodes;
         this->valid = true;
      }

      QuotaDataType getType() const
      {
         return (QuotaDataType)this->type;
      }

      void setType(QuotaDataType type)
      {
         this->type = type;
      }

      bool isValid() const
      {
         return this->valid;
      }

      void setIsValid(bool isValid)
      {
         this->valid = isValid;
      }


      //public inliner

      /**
       * Merges the given QuotaData into this object, but only if the id and the type is the same,
       * it also checks if both QuotaData counter are valid
       *
       * @param second the quota data to merge
       */
      bool mergeQuotaDataCounter(const QuotaData* second)
      {
         if( (this->id != second->id) || (this->type != second->type) ||
            (!this->valid) || (!second->valid) )
            return false;

         this->size += second->size;
         this->inodes += second->inodes;

         return true;
      }

      /**
       * Merges the given values into the this object
       *
       * @param size the size counter, the defined size limit or the used size
       * @param inodes the inodes counter, the defined inodes limit or the used inodes
       */
      void forceMergeQuotaDataCounter(uint64_t size, uint64_t inodes)
      {
         this->size += size;
         this->inodes += inodes;
      }

      /**
       * add a QuotaData object to a QuotaDataList if the list doesn't contain a QuotaData object
       * with the same ID
       *
       * @param quota the QuotaData object which should be added to the given list
       * @param quotaList the list where the given QuotaData should be added
       * @return true if quota was added successful, false if the list contains the ID and the
       *         given quota was ignored
       */
      static bool uniqueIDAddQuotaDataToList(QuotaData quota, QuotaDataList* quotaList)
      {
         for(QuotaDataListIter iter = quotaList->begin(); iter != quotaList->end(); iter++)
            if(iter->getID() == quota.getID())
               return false;

         quotaList->push_back(quota);
         return true;
      }

      static std::string QuotaLimitTypeToString(QuotaLimitType type)
      {
         if(type == QuotaLimitType_SIZE)
            return "QuotaLimitType_SIZE";
         else
         if(type == QuotaLimitType_INODE)
            return "QuotaLimitType_INODE";
         else
            return "QuotaLimitType_UNDEFINED";
      }

      static std::string QuotaDataTypeToString(QuotaDataType type)
      {
         if(type == QuotaDataType_USER)
            return "QuotaDataType_USER";
         else
         if(type == QuotaDataType_GROUP)
            return "QuotaDataType_GROUP";
         else
         if(type == QuotaDataType_NONE)
            return "QuotaDataType_NONE";
         else
            return "QuotaDataType_UNDEFINED";
       }

      static std::string QuotaExceededErrorTypeToString(QuotaExceededErrorType type)
      {
         if(type == QuotaExceededErrorType_NOT_EXCEEDED)
            return "Quota not exceeded.";
         else
         if(type == QuotaExceededErrorType_USER_SIZE_EXCEEDED)
            return "User size quota exceeded.";
         else
         if(type == QuotaExceededErrorType_USER_INODE_EXCEEDED)
            return "User inode quota exceeded.";
         else
         if(type == QuotaExceededErrorType_GROUP_SIZE_EXCEEDED)
            return "Group size quota exceeded.";
         else
         if(type == QuotaExceededErrorType_GROUP_INODE_EXCEEDED)
            return "Group inode quota exceeded.";
         else
            return "Unknown Type";
      }
};

template<>
struct ListSerializationHasLength<QuotaData> : boost::false_type {};

// maps serialize same as lists, deserialize with the element ID as key
inline Serializer& operator%(Serializer& ser, const QuotaDataMap& map)
{
   ser % uint32_t(map.size() );
   for(QuotaDataMapConstIter it = map.begin(), end = map.end(); it != end; ++it)
      ser % it->second;

   return ser;
}

inline Deserializer& operator%(Deserializer& des, QuotaDataMap& map)
{
   uint32_t elemCount;

   des % elemCount;
   if(!des.good() )
      return des;

   map.clear();
   while(map.size() != elemCount)
   {
      QuotaData element;
      des % element;
      if(!des.good() )
         break;

      map.insert(std::make_pair(element.getID(), element) );
   }

   return des;
}

template<>
struct MapSerializationHasLength<uint16_t, QuotaDataMap> : boost::true_type {};

#endif /* QUOTADATA_H_ */
