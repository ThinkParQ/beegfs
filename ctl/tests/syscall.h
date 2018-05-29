#pragma once

//! \todo This should be in /fhgfs_common/source/.
//! \todo There already is a file like this in /testing/.

namespace beegfs
{
   namespace syscall
   {
      void* dlsym_next (char const* symbol);
      void* dlsym_default (char const* symbol);
      void* dlsym (void* handle, char const* symbol);
   }
}
