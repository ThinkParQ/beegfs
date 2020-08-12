#ifndef DISKLIST_H_
#define DISKLIST_H_

#include <common/threading/Mutex.h>
#include <common/toolkit/serialization/Serialization.h>

#include <cerrno>
#include <stdexcept>
#include <string>
#include <mutex>
#include <vector>

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

struct DiskListDoesNotExist : public std::runtime_error
{
   DiskListDoesNotExist(const std::string& file)
      : runtime_error(file)
   {}
};

template<typename Data>
class DiskList;

template<typename Data>
class DiskListCursor
{
   friend class DiskList<Data>;

   public:
      bool step()
      {
         uint64_t itemLen;

         {
            ssize_t readRes = ::pread(fd, &itemLen, sizeof(itemLen), offset);

            if (readRes == 0)
               return false;

            if (readRes < (ssize_t) sizeof(itemLen))
               throw std::runtime_error("could not read from disk list file: " + std::string(strerror(errno)));
         }

         itemLen = LE_TO_HOST_64(itemLen);

         std::unique_ptr<char[]> itemBuf(new char[itemLen]);

         ssize_t readRes = ::pread(fd, itemBuf.get(), itemLen, offset + sizeof(itemLen));

         if (readRes < (ssize_t) itemLen)
            throw std::runtime_error("could not read from disk list file: " + std::string(strerror(errno)));

         Deserializer des(itemBuf.get(), itemLen);

         des % item;
         if (!des.good())
            throw std::runtime_error("could not read from disk list file: " + std::string(strerror(errno)));

         offset += sizeof(itemLen) + itemLen;
         return true;
      }

      Data* get()
      {
         return &item;
      }

   private:
      int fd;
      size_t offset;
      Data item;

      DiskListCursor(int fd):
         fd(fd), offset(0)
      {
      }
};

template<typename Data>
class DiskList {
   private:
      std::string file;
      int fd;
      uint64_t itemCount;
      Mutex mtx;

      DiskList(const DiskList&);
      DiskList& operator=(const DiskList&);

   public:
      DiskList(const std::string& file, bool allowCreate = true) :
         file(file), itemCount(0)
      {
         fd = ::open(file.c_str(), O_RDWR | (allowCreate ? O_CREAT : 0), 0660);
         if(fd < 0)
         {
            int eno = errno;
            if(!allowCreate && errno == ENOENT)
               throw DiskListDoesNotExist(file);
            else
               throw std::runtime_error("could not open disk list file " + file + ": " + strerror(eno));
         }
      }

      ~DiskList()
      {
         if(fd >= 0)
            close(fd);
      }

      const std::string filename() const { return file; }

      void append(const Data& data)
      {
         std::lock_guard<Mutex> lock(mtx);

         boost::scoped_array<char> buffer;

         ssize_t size = serializeIntoNewBuffer(data, buffer);
         if (size < 0)
            throw std::runtime_error("error serializing disk list item");

         uint64_t bufSize = HOST_TO_LE_64(size);
         if (::write(fd, &bufSize, sizeof(bufSize)) < (ssize_t) sizeof(bufSize))
            throw std::runtime_error("error writing disk list item: " + std::string(strerror(errno)));

         if (::write(fd, buffer.get(), size) < size)
            throw std::runtime_error("error writing disk list item: " + std::string(strerror(errno)));
      }

      DiskListCursor<Data> cursor()
      {
         return DiskListCursor<Data>(fd);
      }

      void clear()
      {
         if (fd >= 0)
         {
            if (ftruncate(fd, 0) < 0)
               throw std::runtime_error("error clearing disk list: " + std::string(strerror(errno)));
         }
      }
};

#endif
