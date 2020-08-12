#ifndef SETFRAGMENT_H_
#define SETFRAGMENT_H_

#include <algorithm>
#include <cerrno>
#include <stdexcept>
#include <string>
#include <vector>

#include <boost/scoped_array.hpp>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

struct FragmentDoesNotExist : public std::runtime_error
{
   FragmentDoesNotExist(const std::string& file)
      : runtime_error(file)
   {}
};

template<typename Data>
class SetFragment {
   public:
      static const unsigned CONFIG_AREA_SIZE = 4096;
      static const size_t BUFFER_SIZE = 4ULL * 1024 * 1024;

   private:
      std::string file;
      int fd;
      size_t itemCount;
      bool sorted;

      std::vector<Data> buffer;
      size_t firstBufferedItem;
      ssize_t firstDirtyItem;
      ssize_t lastDirtyItem;

      typename Data::KeyType lastItemKey;

      SetFragment(const SetFragment&);
      SetFragment& operator=(const SetFragment&);

      size_t readBlock(Data* into, size_t count, size_t from)
      {
         size_t total = 0;
         count *= sizeof(Data);
         from *= sizeof(Data);

         from += CONFIG_AREA_SIZE;

         char* buf = (char*) into;

         while(total < count)
         {
            ssize_t current = ::pread(fd, buf + total, count - total, from + total);
            total += current;

            if (current < 0)
               throw std::runtime_error("read failed: " + std::string(strerror(errno)));

            if (current == 0)
               break;
         }

         return total / sizeof(Data);
      }

      size_t writeBlock(const Data* source, size_t count, size_t from)
      {
         size_t total = 0;
         count *= sizeof(Data);
         from *= sizeof(Data);

         from += CONFIG_AREA_SIZE;

         const char* buf = (const char*) source;

         while(total < count)
         {
            ssize_t current = ::pwrite(fd, buf + total, count - total, from + total);
            total += current;

            if (current < 0)
               throw std::runtime_error("write failed: " + std::string(strerror(errno)));

            if (current == 0)
               break;
         }

         return total / sizeof(Data);
      }

      void flushBuffer()
      {
         if(firstDirtyItem < 0)
            return;

         const Data* first = &buffer[firstDirtyItem - firstBufferedItem];

         writeBlock(first, lastDirtyItem - firstDirtyItem + 1, firstDirtyItem);

         buffer.clear();
         firstDirtyItem = -1;
      }

      void bufferFileRange(size_t begin, size_t end)
      {
         flushBuffer();

         const size_t firstAddress = begin;
         const size_t windowSize = end == size_t(-1) ? end : (end - begin + 1);
         const size_t totalRead = std::min<size_t>(windowSize, BUFFER_SIZE / sizeof(Data) );

         buffer.resize(totalRead);

         size_t read = readBlock(&buffer[0], totalRead, firstAddress);

         firstBufferedItem = begin;
         buffer.resize(read);
      }

   public:
      SetFragment(const std::string& file, bool allowCreate = true)
         : file(file),
           itemCount(0),
           firstBufferedItem(0), firstDirtyItem(-1), lastDirtyItem(0)
      {
         fd = ::open(file.c_str(), O_RDWR | (allowCreate ? O_CREAT : 0), 0660);
         if(fd < 0)
         {
            int eno = errno;
            if(!allowCreate && errno == ENOENT)
               throw FragmentDoesNotExist(file);
            else
               throw std::runtime_error("could not open fragment file " + file + ": " + strerror(eno));
         }

         off_t totalSize = ::lseek(fd, 0, SEEK_END);

         if(totalSize > 0)
         {
            int eno = errno;
            if (totalSize < (off_t) CONFIG_AREA_SIZE)
               throw std::runtime_error("error while opening fragment file " + file + ": " + strerror(eno));
            else
            if ( (totalSize - CONFIG_AREA_SIZE) % sizeof(Data) )
               throw std::runtime_error("error while opening fragment file " + file + ": " + strerror(eno));
         }

         if(totalSize == 0)
         {
            sorted = true;
            itemCount = 0;
         }
         else
         {
            if (::pread(fd, &sorted, sizeof(sorted), 0) != sizeof(sorted)) {
               int eno = errno;
               throw std::runtime_error("error while opening fragment file " + file + ": " + strerror(eno));
            }

            itemCount = (totalSize - CONFIG_AREA_SIZE) / sizeof(Data);
         }

         if(itemCount > 0)
            lastItemKey = (*this)[itemCount - 1].pkey();
      }

      ~SetFragment()
      {
         if(fd >= 0)
         {
            flush();
            close(fd);
         }
      }

      const std::string filename() const { return file; }
      size_t size() const { return itemCount; }

      void append(const Data& data)
      {
         if(itemCount > firstBufferedItem + buffer.size() )
         {
            flushBuffer();
            firstBufferedItem = itemCount;
         }

         if(firstDirtyItem < 0)
            firstDirtyItem = itemCount;

         lastDirtyItem = itemCount;

         if(itemCount > 0 && sorted)
            sorted = lastItemKey < data.pkey();

         buffer.push_back(data);
         itemCount++;
         lastItemKey = data.pkey();

         if(buffer.size() * sizeof(Data) >= BUFFER_SIZE)
            flushBuffer();
      }

      Data& operator[](size_t offset)
      {
         if(offset < firstBufferedItem || offset >= firstBufferedItem + buffer.size() )
            bufferFileRange(offset == 0 ? 0 : offset - 1, -1);

         return buffer[offset - firstBufferedItem];
      }

      void flush()
      {
         if(firstDirtyItem < 0)
            return;

         flushBuffer();
         buffer = std::vector<Data>();

         if (::pwrite(fd, &sorted, sizeof(sorted), 0) < 0) {
            int eno = errno;
            throw std::runtime_error("error in flush of " + file + ": " + strerror(eno));
         }

         // truncate to CONFIG_AREA_SIZE (for reopen)
         if (itemCount == 0 && ::ftruncate(fd, CONFIG_AREA_SIZE) < 0) {
            int eno = errno;
            throw std::runtime_error("error in flush of " + file + ": " + strerror(eno));
         }
      }

      void sort()
      {
         flush();

         if(sorted)
            return;

         boost::scoped_array<Data> data(new Data[size()]);

         if(readBlock(data.get(), size(), 0) < size() )
            throw 42;

         struct ops
         {
            static bool compare(const Data& l, const Data& r)
            {
               return l.pkey() < r.pkey();
            }
         };

         std::sort(data.get(), data.get() + size(), ops::compare);

         writeBlock(data.get(), size(), 0);

         sorted = true;

         flush();
      }

      void drop()
      {
         flush();
         if (::unlink(file.c_str()) < 0) {
            int eno = errno;
            throw std::runtime_error("could not unlink fragment file " + file + ": " + strerror(eno));
         }

         close(fd);
         fd = -1;
      }

      void rename(const std::string& to)
      {
         if (::rename(file.c_str(), to.c_str()) < 0) {
            int eno = errno;
            throw std::runtime_error("could not rename fragment file " + file + ": " + strerror(eno));
         }

         file = to;
      }

      template<typename Key, typename Fn>
      std::pair<bool, Data> getByKeyProjection(const Key& key, Fn fn)
      {
         if(size() == 0)
            return std::make_pair(false, Data() );

         size_t first = 0;
         size_t last = size() - 1;

         while(first != last)
         {
            size_t midIdx = first + (last - first) / 2;

            bufferFileRange(midIdx, midIdx + 1);
            Data& mid = (*this)[midIdx];

            if (fn(mid.pkey() ) < key)
               first = midIdx + 1;
            else
               last = midIdx;
         }

         if(fn( (*this)[first].pkey() ) == key)
            return std::make_pair(true, (*this)[first]);
         else
            return std::make_pair(false, Data() );
      }

      std::pair<bool, Data> getByKey(const typename Data::KeyType& key)
      {
         struct ops
         {
            static typename Data::KeyType key(const typename Data::KeyType& key) { return key; }
         };

         return getByKeyProjection(key, ops::key);
      }
};

#endif
