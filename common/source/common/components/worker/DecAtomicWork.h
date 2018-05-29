#ifndef DECATOMICWORK_H
#define DECATOMICWORK_H

#include <common/app/log/LogContext.h>
#include <common/components/worker/Work.h>
#include <common/threading/Atomics.h>

template <class TemplateType>
class DecAtomicWork : public Work
{
   public:
   DecAtomicWork(Atomic<TemplateType>* atomicValue)
      {
         this->atomicValue = atomicValue;
      }

      virtual ~DecAtomicWork() { };

      void process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen)
      {
         LOG_DEBUG("DecAtomicWork", Log_DEBUG,
            "Processing DecAtomicWork");

         // increment counter
         this->atomicValue->decrease();

         LOG_DEBUG("DecAtomicWork", Log_DEBUG,
            "Processed DecAtomicWork");
      }

   private:
      Atomic<TemplateType>* atomicValue;
};

#endif /* DECATOMICWORK_H */
