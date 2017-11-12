#ifndef META_METAFILEHANDLE_H
#define META_METAFILEHANDLE_H

#include <storage/DirInode.h>
#include <storage/FileInode.h>

class MetaStore;

// this class is used to transport referenced file inode out of and back into the meta store.
// since file inodes must keep their parent directory alive, a handle to an inode also requires a
// handle to a directory to function properly.
class MetaFileHandle
{
   friend class MetaStore;

   public:
      typedef void (MetaFileHandle::*bool_type)();

      MetaFileHandle():
         inode(nullptr), parent(nullptr)
      {}

      MetaFileHandle(FileInode* inode, DirInode* parent):
         inode(inode), parent(parent)
      {}

      MetaFileHandle(const MetaFileHandle&) = delete;
      MetaFileHandle& operator=(const MetaFileHandle&) = delete;

      MetaFileHandle(MetaFileHandle&& other):
         inode(nullptr), parent(nullptr)
      {
         swap(other);
      }

      MetaFileHandle& operator=(MetaFileHandle&& other)
      {
         MetaFileHandle(std::move(other)).swap(*this);
         return *this;
      }

      FileInode* operator->() const { return inode; }
      FileInode& operator*() const { return *inode; }

      FileInode* get() const { return inode; }

      operator bool_type() const { return inode ? &MetaFileHandle::bool_value : 0; }

      void swap(MetaFileHandle& other)
      {
         std::swap(inode, other.inode);
         std::swap(parent, other.parent);
      }

      friend void swap(MetaFileHandle& a, MetaFileHandle& b)
      {
         a.swap(b);
      }

   private:
      FileInode* inode;
      DirInode* parent;

      void bool_value() {}
};

#endif
