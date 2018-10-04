#include "RDMASocket.h"

#include <dlfcn.h>

namespace {
   RDMASocket::ImplCallbacks* socket_impl;

   struct IBLibLoader {
      struct dlclose {
         void operator()(void* v) { ::dlclose(v); }
      };

      std::unique_ptr<void, dlclose> iblib;

      IBLibLoader() {
         // RTLD_NOW to resolve all symbols at load time rather than paying as we go
         iblib.reset(dlopen("libbeegfs_ib.so", RTLD_NOW));
         // no ib support if lib can't be loaded. likely because ib libs are not
         // installed.
         if (!iblib)
            return;

         socket_impl = (RDMASocket::ImplCallbacks*) dlsym(iblib.get(), "beegfs_socket_impl");

         if (!socket_impl)
            iblib.reset();
      }
   };

   // load lib on startup, unload at exit.
   IBLibLoader lib_loader;
}

bool RDMASocket::isRDMAAvailable()
{
   return bool(lib_loader.iblib);
}

bool RDMASocket::rdmaDevicesExist()
{
   return socket_impl && socket_impl->rdma_devices_exist();
}

/**
 * Prepare ibverbs for multi-threading.
 * Call this only once in your program.
 *
 * Note: There is no corresponding uninit-method that needs to be called.
 */
void RDMASocket::rdmaForkInitOnce()
{
   if (socket_impl)
      return socket_impl->rdma_fork_init_once();
}

std::unique_ptr<RDMASocket> RDMASocket::create()
{
   if (!socket_impl)
      throw std::logic_error("beegfs_rdma_socket_create called with no rdma support");

   return std::unique_ptr<RDMASocket>(socket_impl->rdma_socket_create());
}
