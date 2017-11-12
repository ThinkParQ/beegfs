#ifndef COMMON_BARRIER_H
#define COMMON_BARRIER_H

#include <common/threading/PThread.h>

class Barrier
{
   public:
      Barrier(unsigned count)
      {
         int res = pthread_barrier_init(&barrier, nullptr, count);
         if (res != 0)
            throw PThreadException(System::getErrString(res));
      }

      ~Barrier()
      {
         int res = pthread_barrier_destroy(&barrier);
         if (res != 0)
            std::terminate();
      }

      Barrier() = delete;
      Barrier(const Barrier&) = delete;
      Barrier(Barrier&&) = delete;
      Barrier& operator=(const Barrier&) = delete;
      Barrier& operator=(Barrier&&) = delete;

   private:
      pthread_barrier_t barrier;

   public:
      void wait()
      {
         pthread_barrier_wait(&barrier);
         // Note: Not checking return val, because it can only fail if the barrier is not properly
         //       initialized, but proper initialization is guaranteed by the constructor.
      }
};

#endif /* COMMON_BARRIER_H */
