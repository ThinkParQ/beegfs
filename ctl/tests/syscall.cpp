#include "syscall.h"

#include <mutex>
#include <exception>
#include <string>
#include <stdexcept>

#include <dlfcn.h>

namespace beegfs::syscall
{
   namespace
   {
      struct check_dlerror
      {
         static void dlerror()
         {
            if (auto error = ::dlerror())
            {
               throw std::runtime_error (error);
            }
         }

         check_dlerror()
            : _lock (_guard)
         {
            dlerror();
         }

         template<typename Ret>
            Ret check (Ret value) const
         {
            dlerror();
            return value;
         }

         //! \note dlerror() is specified to be not threadsafe. In
         //! fact, it uses a single static buffer to print the
         //! error message in. Thus, sequentialize all functions
         //! that interact with dlerror.
         static std::mutex _guard;
         std::lock_guard<std::mutex> const _lock;
      };
      std::mutex check_dlerror::_guard;
   }

   void* dlsym (void* handle, char const* symbol)
   try
   {
      check_dlerror const checker;
      return checker.check (::dlsym (handle, symbol));
   }
   catch (...)
   {
      std::throw_with_nested
         ( std::runtime_error
            ("dlsym (handle, " + std::string (symbol) + ")")
         );
   }

   void* dlsym_next (char const* symbol)
   {
      return dlsym (RTLD_NEXT, symbol);
   }
   void* dlsym_default (char const* symbol)
   {
      return dlsym (RTLD_DEFAULT, symbol);
   }
}
