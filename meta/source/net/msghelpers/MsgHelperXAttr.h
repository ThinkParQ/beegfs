#ifndef MSGHELPERXATTR_H_
#define MSGHELPERXATTR_H_

#include <common/storage/StorageErrors.h>

class EntryInfo; // forward declaration
class Socket;

class MsgHelperXAttr
{
   public:
      static std::pair<FhgfsOpsErr, StringVector> listxattr(EntryInfo* entryInfo);
      static std::tuple<FhgfsOpsErr, std::vector<char>, ssize_t> getxattr(EntryInfo* entryInfo,
         const std::string& name, size_t maxSize);
      static FhgfsOpsErr removexattr(EntryInfo* entryInfo, const std::string& name);
      static FhgfsOpsErr setxattr(EntryInfo* entryInfo, const std::string& name,
         const CharVector& value, int flags);

      static const std::string CURRENT_DIR_FILENAME;
      static const ssize_t MAX_VALUE_SIZE;

      class StreamXAttrState
      {
         public:
            StreamXAttrState():
               entryInfo(nullptr)
            {}

            StreamXAttrState(EntryInfo& entryInfo, StringVector names):
               entryInfo(&entryInfo), names(std::move(names))
            {}

            StreamXAttrState(std::string path, StringVector names):
               entryInfo(nullptr), path(std::move(path)), names(std::move(names))
            {}

            static FhgfsOpsErr streamXattrFn(Socket* socket, void* context);

            static FhgfsOpsErr readNextXAttr(Socket* socket, std::string& name, CharVector& value);

         private:
            EntryInfo* entryInfo;
            std::string path;
            StringVector names;

            FhgfsOpsErr streamXattr(Socket* socket) const;
      };
};

#endif /*MSGHELPERXATTR_H_*/
