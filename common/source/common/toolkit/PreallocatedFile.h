#ifndef COMMON_PREALLOCATEDFILE_H_
#define COMMON_PREALLOCATEDFILE_H_

#include <common/toolkit/FDHandle.h>
#include <common/toolkit/serialization/Serialization.h>

#include <boost/optional.hpp>
#include <system_error>
#include <type_traits>

#include <unistd.h>
#include <fcntl.h>

namespace detail {
template<typename T>
struct PreallocatedFileDefaultSize
{
   static constexpr size_t value =
         std::enable_if<
            std::is_trivial<T>::value,
            std::integral_constant<size_t, sizeof(T)>>::type::value;
};
}

/**
 * Preallocates a file on disk for guaranteed writes. The type T must be serializable and all
 * serialized representations of T must fit into a buffer of size Size. When the file is created
 * it is filled with Size+1 zeroes, where the first byte is used as a tag and the remaining Size
 * bytes are used for user data. If the file was never written, the tag byte will be 0 and read()
 * will return none. If it was written, the tag byte will be 1 and read() will return a value or
 * throw.
 *
 * Preallocation nominally guarantees (on Linux, anyway) that no write operation within the
 * preallocated bounds will fail with any error other than EIO. It is recommended to use this class
 * only for files on local disks since network filesystems may return EIO without being fatally
 * broken.
 *
 * The buffer used for reading/writing will be placed on the stack, thus the user must ensure that
 * the stack can hold at least Size+1 bytes of data.
 */
template<typename T, size_t Size = detail::PreallocatedFileDefaultSize<T>::value>
class PreallocatedFile
{
   static_assert(Size <= std::numeric_limits<off_t>::max(), "Size too large");

   public:
      /**
       * Opens a file and preallocates space for guaranteed writes. If the file exists, it is
       * truncated to Size+1. If the file does not exist, it is created with mode and then truncated
       * to Size+1.
       *
       * @param path path of the file
       * @param mode mode to use when creating path
       */
      PreallocatedFile(const std::string& path, mode_t mode)
      {
         fd = FDHandle(open(path.c_str(), O_CREAT | O_RDWR, mode));
         if (!fd.valid())
            throw std::system_error(errno, std::system_category(), path);

         const int fallocateRes = posix_fallocate(*fd, 0, Size + 1);
         if (fallocateRes != 0)
            throw std::system_error(fallocateRes, std::system_category(), path);
      }

      /**
       * Serializes value and writes the result to the file at offset 1. The serialized value must
       * not exceed Size bytes, otherwise an exception is thrown. The file is *not* fsynced, even
       * if the write operation was successful.
       *
       * @param value value to write to the file
       *
       * @throws std::runtime_error if value does not fit into Size bytes after serialization
       * @throws std::system_error if the write operation fails
       */
      void write(const T& value)
      {
         char buf[Size + 1] = {};
         buf[0] = 1;
         Serializer ser(buf + 1, Size);

         ser % value;
         if (!ser.good())
            throw std::runtime_error("value too large for buffer");

         if (pwrite(*fd, buf, Size + 1, 0) != Size + 1)
            throw std::system_error(errno, std::system_category());
      }

      /**
       * Read the byte at offset 0, and if the byte is not 0, also read Size bytes from the file at
       * offset 1 and deserializes the result. If deserialization fails, an exception is thrown.
       *
       * @returns the deserialized value for the file contents up to Size bytes
       *
       * @throws std::system_error if the file contains less than Size+1 bytes
       * @throws std::runtime_error if the file contents could not be deserialized
       */
      boost::optional<T> read() const
      {
         char buf[Size + 1];

         if (pread(*fd, buf, Size + 1, 0) != Size + 1)
            throw std::system_error(errno, std::system_category());

         if (buf[0] == 0)
            return boost::none;

         Deserializer des(buf + 1, Size);
         T result;

         des % result;
         if (!des.good())
            throw std::runtime_error("deserialization failed");

         return result;
      }

   private:
      FDHandle fd;
};

#endif
