#ifndef META_COMPONENTS_BUDDYRESYNCER_SYNCCANDIDATE_H
#define META_COMPONENTS_BUDDYRESYNCER_SYNCCANDIDATE_H

#include <common/toolkit/serialization/Serialization.h>
#include <common/storage/mirroring/SyncCandidateStore.h>
#include <common/threading/Barrier.h>

#include <string>

enum class MetaSyncDirType
{
   InodesHashDir,
   DentriesHashDir,
   ContentDir,
};
GCC_COMPAT_ENUM_CLASS_OPEQNEQ(MetaSyncDirType)

class MetaSyncCandidateDir
{
   public:
      MetaSyncCandidateDir(const std::string& relativePath, MetaSyncDirType type):
         relPath(relativePath), type(type)
      {}

      MetaSyncCandidateDir() = default;

   private:
      std::string relPath;
      MetaSyncDirType type;

   public:
      const std::string& getRelativePath() const { return relPath; }
      MetaSyncDirType getType() const { return type; }
};

enum class MetaSyncFileType
{
   Inode,
   Dentry,
   Directory,
};
GCC_COMPAT_ENUM_CLASS_OPEQNEQ(MetaSyncFileType)

template<>
struct SerializeAs<MetaSyncFileType> {
   typedef uint8_t type;
};

class MetaSyncCandidateFile
{
   public:
      struct Element
      {
         std::string path;
         MetaSyncFileType type;
         bool isDeletion;
      };

      MetaSyncCandidateFile(): barrier(nullptr) {}

      MetaSyncCandidateFile(MetaSyncCandidateFile&& src):
         barrier(nullptr)
      {
         swap(src);
      }

      MetaSyncCandidateFile& operator=(MetaSyncCandidateFile&& other)
      {
         MetaSyncCandidateFile(std::move(other)).swap(*this);
         return *this;
      }

      void swap(MetaSyncCandidateFile& other)
      {
         paths.swap(other.paths);
         std::swap(barrier, other.barrier);
      }

      void signal()
      {
         barrier->wait();
      }

      friend void swap(MetaSyncCandidateFile& a, MetaSyncCandidateFile& b)
      {
         a.swap(b);
      }

   private:
      std::vector<Element> paths;
      Barrier* barrier;

   public:
      const std::vector<Element>& getElements() const { return paths; }
      std::vector<Element> releaseElements() { return std::move(paths); }

      void addModification(std::string path, MetaSyncFileType type)
      {
         paths.push_back(Element{std::move(path), type, false});
      }

      void addDeletion(std::string path, MetaSyncFileType type)
      {
         paths.push_back(Element{std::move(path), type, true});
      }

      void prepareSignal(Barrier& barrier)
      {
         this->barrier = &barrier;
      }
};

typedef SyncCandidateStore<MetaSyncCandidateDir, MetaSyncCandidateFile> MetaSyncCandidateStore;

#endif /* META_COMPONENTS_BUDDYRESYNCER_SYNCCANDIDATE_H */
