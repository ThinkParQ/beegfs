#ifndef META_INCOMPLETEINODE_H
#define META_INCOMPLETEINODE_H

#include <common/storage/StorageErrors.h>

class IncompleteInode
{
   public:
      IncompleteInode() : fd(-1), hasContent(false) {}

      explicit IncompleteInode(int fd) : fd(fd), hasContent(false) {}

      ~IncompleteInode();

      IncompleteInode(const IncompleteInode&) = delete;
      IncompleteInode& operator=(const IncompleteInode&) = delete;

      IncompleteInode(IncompleteInode&& other)
         : fd(-1), hasContent(false)
      {
         swap(other);
      }

      IncompleteInode& operator=(IncompleteInode&& other)
      {
         IncompleteInode(std::move(other)).swap(*this);
         return *this;
      }

      void swap(IncompleteInode& other)
      {
         std::swap(fd, other.fd);
         std::swap(hasContent, other.hasContent);
         std::swap(xattrsSet, other.xattrsSet);
      }

      friend void swap(IncompleteInode& a, IncompleteInode& b)
      {
         a.swap(b);
      }

      FhgfsOpsErr setXattr(const char* name, const void* value, size_t size);

      FhgfsOpsErr setContent(const void* value, size_t size);

      FhgfsOpsErr clearUnsetXAttrs();

   private:
      int fd;
      bool hasContent;
      std::set<std::string> xattrsSet;

      std::string fileName() const;
};

#endif
