#ifndef FSCKCONTDIR_H_
#define FSCKCONTDIR_H_

#include <common/Common.h>

class FsckContDir;

typedef std::list<FsckContDir> FsckContDirList;
typedef FsckContDirList::iterator FsckContDirListIter;

class FsckContDir
{
   public:
      size_t serialize(char* outBuf);
      bool deserialize(const char* buf, size_t bufLen, unsigned* outLen);
      unsigned serialLen(void);

   private:
      std::string id;
      uint16_t saveNodeID;

   public:
      FsckContDir(std::string id, uint16_t saveNodeID) : id(id), saveNodeID(saveNodeID) { };

      //only for deserialization
      FsckContDir() {}

      std::string getID() const
      {
         return this->id;
      }

      uint16_t getSaveNodeID() const
      {
         return saveNodeID;
      }

      bool operator< (const FsckContDir& other)
      {
         if (id < other.id)
            return true;
         else
            return false;
      }

      bool operator== (const FsckContDir& other)
      {
         if (id.compare(other.id) != 0)
            return false;
         else
         if (saveNodeID != other.saveNodeID)
            return false;
         else
            return true;
      }

      bool operator!= (const FsckContDir& other)
      {
         return !(operator==( other ) );
      }

      void update(std::string id, uint16_t saveNodeID)
       {
          this->id = id;
          this->saveNodeID = saveNodeID;
       }
};

#endif /* FSCKCONTDIR_H_ */
