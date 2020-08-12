#ifndef SESSIONLOCALFILESTORE_H_
#define SESSIONLOCALFILESTORE_H_

#include <common/toolkit/ObjectReferencer.h>
#include <common/threading/Mutex.h>
#include <common/Common.h>
#include "SessionLocalFile.h"



class SessionLocalFileStore
{
   public:
      SessionLocalFileStore() {}

      std::shared_ptr<SessionLocalFile> addAndReferenceSession(
         std::unique_ptr<SessionLocalFile> session);
      std::shared_ptr<SessionLocalFile> referenceSession(const std::string& fileHandleID,
         uint16_t targetID, bool isMirrorSession) const;
      std::shared_ptr<SessionLocalFile::Handle> removeSession(
         const std::string& fileHandleID, uint16_t targetID, bool isMirrorSession);
      size_t removeAllSessions();
      void removeAllMirrorSessions(uint16_t targetID);
      void deleteAllSessions();
      void mergeSessionLocalFiles(SessionLocalFileStore*
         sessionLocalFileStore);

      size_t getSize() const;

      void serializeForTarget(Serializer& ser, uint16_t targetID);
      void deserializeForTarget(Deserializer& des, uint16_t targetID);

   private:
      struct Key
      {
         std::string fileHandleID;
         uint16_t targetID; // really targetID (not buddy group ID for mirrors, because both buddies
                            // can be attached to the same server)
         bool isMirrored; // true if this is a session for a mirror chunk file (has an influence on
                          // the map key to avoid conflicts with the original session in rotated
                          // mirrors mode).

         template<typename This, typename Ctx>
         static void serialize(This obj, Ctx& ctx)
         {
            ctx
               % obj->fileHandleID
               % obj->targetID
               % obj->isMirrored;
         }

         friend bool operator<(const Key& a, const Key& b)
         {
            return std::tie(a.fileHandleID, a.targetID, a.isMirrored)
                 < std::tie(b.fileHandleID, b.targetID, b.isMirrored);
         }
      };

      std::map<Key, std::shared_ptr<SessionLocalFile>> sessions;

      mutable Mutex mutex;
};

#endif /*SESSIONLOCALFILESTORE_H_*/
