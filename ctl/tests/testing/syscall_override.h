#include <functional>

#include "../syscall.h"

//! Usage: include and use `beegfs::testing::syscall::scoped_override`
//! to override a syscall for the current scope:
//!
//! beegfs::testing::syscall::scoped_override const llistxattr_override
//!   ( beegfs:testing::syscall::llistxattr
//!   , [] (char const*, char*, size_t)
//!     {
//!       errno = ENAMETOOLONG;
//!       return -1;
//!     }
//!   );
//! assert (llistxattr ("", nullptr, 0) == -1);
//! assert (errno == ENAMETOOLONG);

namespace beegfs
{
   namespace testing
   {
      namespace syscall
      {
         template<typename Signature>
            struct override_hook
         {
            Signature* original;
            std::function<Signature> current;

            void reset()
            {
               current = original;
            }
            void set (std::function<Signature> fun)
            {
               current = std::move (fun);
            }

            override_hook (const char* name)
               : original ( reinterpret_cast<Signature*>
                              (beegfs::syscall::dlsym_next (name))
                          )
               , current (original)
            {}
         };
         struct scoped_override
         {
            std::function<void()> _on_dtor;
            template<typename Override, typename Fun>
               scoped_override (Override& override, Fun fun)
                  : _on_dtor ([&] { override.set (nullptr); })
            {
               override.set (fun);
            }
            ~scoped_override()
            {
               _on_dtor();
            }
         };

         override_hook<ssize_t (const char*, char*, size_t)> llistxattr ("llistxattr");
         override_hook<ssize_t (const char*, const char*, void*, size_t)> lgetxattr ("lgetxattr");
         override_hook<int (const char*, const char*, const void*, size_t, int)> lsetxattr ("lsetxattr");
      }
   }
}

extern "C" ssize_t llistxattr
   (const char* path, char* out_data, size_t out_size)
{
   return beegfs::testing::syscall::llistxattr.current
      (path, out_data, out_size);
}

extern "C" ssize_t lgetxattr
   (const char* path, const char* name, void* value, size_t size)
{
   return beegfs::testing::syscall::lgetxattr.current
      (path, name, value, size);
}

extern "C" int lsetxattr
   (const char* path, const char* name, const void* value, size_t size, int flags)
{
   return beegfs::testing::syscall::lsetxattr.current
      (path, name, value, size, flags);
}
