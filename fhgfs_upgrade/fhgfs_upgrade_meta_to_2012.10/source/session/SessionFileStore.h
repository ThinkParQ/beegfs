#ifndef SESSIONFILESTORE_H_
#define SESSIONFILESTORE_H_

#include <common/toolkit/ObjectReferencer.h>
#include <common/threading/Mutex.h>
#include <common/toolkit/Random.h>
#include <common/Common.h>
#include "SessionFile.h"

typedef ObjectReferencer<SessionFile*> SessionFileReferencer;
typedef std::map<unsigned, SessionFileReferencer*> SessionFileMap;
typedef SessionFileMap::iterator SessionFileMapIter;
typedef SessionFileMap::value_type SessionFileMapVal;

typedef std::list<SessionFile*> SessionFileList;
typedef SessionFileList::iterator SessionFileListIter;


class SessionFileStore
{
   public:
      SessionFileStore()
      {
         // note: randomize to make it more unlikely that after a meta server shutdown the same
         // client instance can open a file and gets a colliding (with the old meta server
         // instance) sessionID
         this->lastSessionID = Random().getNextInt();
      }
      
      unsigned addSession(SessionFile* session);
      SessionFile* addAndReferenceRecoverySession(SessionFile* session);
      SessionFile* referenceSession(unsigned sessionID);
      void releaseSession(SessionFile* session, EntryInfo* entryInfo);
      bool removeSession(unsigned sessionID);
      void removeAllSessions(SessionFileList* outRemovedSessions,
         UIntList* outReferencedSessions);
      
      size_t getSize();
   
   private:
      SessionFileMap sessions;
      unsigned lastSessionID;
      
      Mutex mutex;
      
      SessionFile* removeAndGetSession(unsigned sessionID); // actually not needed now
      
      unsigned generateNewSessionID();

      void performAsyncCleanup(EntryInfo* entryInfo, FileInode* inode, unsigned accessFlags);
};

#endif /*SESSIONFILESTORE_H_*/
