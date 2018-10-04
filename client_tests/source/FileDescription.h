#ifndef source_FileDescription_h_GaigZp4R6hw0kOUJ0X7Nh
#define source_FileDescription_h_GaigZp4R6hw0kOUJ0X7Nh

// NOTE: file descriptions do IO in blocks auf 4k. *do not* reduce this block size below the
// minimum chunk size of the file system, since we absolutely do not want to trigger cache
// bypasses with IO from file descriptions *unless* explicitly noted.

#include "SafeFD.h"
#include <list>
#include <memory>
#include <random>
#include <vector>

class FilePart {
   public:
      virtual ~FilePart();

      virtual void writeTo(int fd);
      virtual void writeTo(char* block);

      virtual void verifyFrom(int fd);
      virtual void verifyFrom(const char* block);

      virtual size_t partSize() const = 0;

   protected:
      virtual void writeBlock(char* block, size_t offset, size_t length) = 0;
      virtual void verifyBlock(const char* block, size_t offset, size_t length) = 0;

   public:
      void writeTo(SafeFD& fd)
      {
         writeTo(fd.raw() );
      }

      void verifyFrom(SafeFD& fd)
      {
         verifyFrom(fd.raw() );
      }
};

class Data : public FilePart {
   public:
      explicit Data(const char* data);
      explicit Data(std::vector<char> data);

      Data(char c, unsigned count)
         : data(count, c)
      {}

   private:
      std::vector<char> data;

   protected:
      void writeBlock(char* block, size_t offset, size_t length) override;
      void verifyBlock(const char* block, size_t offset, size_t length) override;

   public:
      size_t partSize() const override
      {
         return data.size();
      }
};

class RandomBlock : public FilePart {
   public:
      explicit RandomBlock(unsigned size);

   private:
      unsigned int seed;
      unsigned int size;
      std::tuple<size_t, std::unique_ptr<std::mt19937>> source;

   protected:
      void writeBlock(char* block, size_t offset, size_t length) override;
      void verifyBlock(const char* block, size_t offset, size_t length) override;

   public:
      size_t partSize() const override
      {
         return size;
      }
};

class Hole : public FilePart {
   public:
      explicit Hole(unsigned size)
         : size(size)
      {}

      void writeTo(int fd) override;

   private:
      unsigned size;

   protected:
      void writeBlock(char* block, size_t offset, size_t length) override;
      void verifyBlock(const char* block, size_t offset, size_t length) override;

   public:
      size_t partSize() const override
      {
         return size;
      }
};

class FileDescription {
   private:
      std::vector<std::unique_ptr<FilePart>> parts;

   public:
      void writeTo(int fd);
      void writeTo(void* block);
      void verifyFrom(int fd);
      void verifyFrom(const void* block);

   public:
      template<typename Part>
      FileDescription& operator<<(Part part)
      {
         this->parts.emplace_back(new Part(std::move(part) ) );
         return *this;
      }

      size_t size() const
      {
         size_t result = 0;
         for(auto& part : this->parts)
            result += part->partSize();
         return result;
      }

      void writeTo(SafeFD& fd)
      {
         writeTo(fd.raw() );
      }

      void verifyFrom(SafeFD& fd)
      {
         verifyFrom(fd.raw() );
      }
};

#endif
