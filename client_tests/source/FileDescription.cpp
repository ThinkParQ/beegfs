#include "FileDescription.h"
#include "Config.h"
#include <array>
#include <cstring>
#include <cppunit/extensions/HelperMacros.h>
#include <unistd.h>
#include <algorithm>
#include <random>
#include <sys/uio.h>

FilePart::~FilePart()
{
}

void FilePart::writeTo(int fd)
{
   auto size = this->partSize();

   if(size == 0)
      return;

   std::array<char, 4096> buffer;
   size_t offset = 0;

   while(offset < size)
   {
      ssize_t writeRes;
      auto part = std::min(buffer.size(), size);

      writeBlock(&buffer[0], offset, part);

      ::iovec vec{buffer.data(), part};
      if (Config::getUseVectorFunctions())
         writeRes = ::writev(fd, &vec, 1);
      else
         writeRes = ::write(fd, &buffer[0], part);

      CPPUNIT_ASSERT(writeRes > 0);
      offset += writeRes;
   }
}

void FilePart::writeTo(char* block)
{
   auto size = this->partSize();

   if(size == 0)
      return;

   size_t offset = 0;

   while(offset < size)
   {
      auto part = std::min<size_t>(size, 4096);
      writeBlock(block + offset, offset, part);
      offset += part;
   }
}

void FilePart::verifyFrom(int fd)
{
   auto size = this->partSize();

   if(size == 0)
      return;

   std::array<char, 4096> buffer;
   size_t offset = 0;

   while(offset < size)
   {
      ::iovec iov{buffer.data(), std::min(buffer.size(), size)};
      ssize_t readRes;

      if(Config::getUseVectorFunctions())
         readRes = ::readv(fd, &iov, 1);
      else
         readRes = ::read(fd, &buffer[0], std::min(buffer.size(), size) );

      CPPUNIT_ASSERT(readRes > 0);
      verifyBlock(&buffer[0], offset, readRes);
      offset += readRes;
   }

   CPPUNIT_ASSERT(offset == size);
}

void FilePart::verifyFrom(const char* block)
{
   auto size = this->partSize();

   if(size == 0)
      return;

   size_t offset = 0;

   while(offset < size)
   {
      auto part = std::min<size_t>(size, 4096);
      verifyBlock(block + offset, offset, part);
      offset += part;
   }

   CPPUNIT_ASSERT(offset == size);
}

Data::Data(const char* data)
{
   this->data.insert(this->data.end(), data, data + std::strlen(data) );
}

Data::Data(std::vector<char> data)
   : data(data)
{
}

void Data::verifyBlock(const char* block, size_t offset, size_t length)
{
   CPPUNIT_ASSERT(memcmp(&this->data[offset], &block[0], length) == 0);
}

void Data::writeBlock(char* block, size_t offset, size_t length)
{
   memcpy(block, &this->data[0] + offset, length);
}

RandomBlock::RandomBlock(unsigned size)
   : size(size), source(0, nullptr)
{
   std::random_device random;

   this->seed = random();
}

void RandomBlock::writeBlock(char* block, size_t offset, size_t length)
{
   if(std::get<0>(this->source) != offset || !std::get<1>(this->source) )
   {
      this->source = std::make_tuple(offset,
         std::unique_ptr<std::mt19937>(new std::mt19937(this->seed) ) );
      std::get<1>(this->source)->discard(offset);
   }

   auto& randomPos = std::get<0>(this->source);
   auto& generator = *std::get<1>(this->source);

   while(length > 0)
   {
      ( (unsigned char*) block)[randomPos - offset] = (unsigned char) generator();
      randomPos++;
      length--;
   }
}

void RandomBlock::verifyBlock(const char* block, size_t offset, size_t length)
{
   if(std::get<0>(this->source) != offset || !std::get<1>(this->source) )
   {
      this->source = std::make_tuple(offset,
         std::unique_ptr<std::mt19937>(new std::mt19937(this->seed) ) );
      std::get<1>(this->source)->discard(offset);
   }

   auto& randomPos = std::get<0>(this->source);
   auto& generator = *std::get<1>(this->source);

   while(length > 0)
   {
      unsigned char current = ( (const unsigned char*) block)[randomPos - offset];
      CPPUNIT_ASSERT(current == (unsigned char) generator() );
      randomPos++;
      length--;
   }
}

void Hole::writeTo(int fd)
{
   CPPUNIT_ASSERT(::lseek(fd, this->size, SEEK_CUR) >= 0);
}

void Hole::writeBlock(char* block, size_t, size_t length)
{
   memset(block, 0, length);
}

void Hole::verifyBlock(const char* block, size_t, size_t length)
{
   CPPUNIT_ASSERT( (size_t) std::count(block, block + length, 0) == length);
}


void FileDescription::writeTo(int fd)
{
   for(auto& part : this->parts)
      part->writeTo(fd);
}

void FileDescription::writeTo(void* block)
{
   char* data = (char*) block;

   for(auto& part : this->parts)
   {
      part->writeTo(data);
      data += part->partSize();
   }
}

void FileDescription::verifyFrom(int fd)
{
   for(auto& part : this->parts)
      part->verifyFrom(fd);

   char buffer;
   CPPUNIT_ASSERT(::read(fd, &buffer, 1) == 0);
}

void FileDescription::verifyFrom(const void* block)
{
   const char* data = (const char*) block;

   for(auto& part : this->parts)
   {
      part->verifyFrom(data);
      data += part->partSize();
   }

   CPPUNIT_ASSERT(data == (const char*) block + this->size() );
}
