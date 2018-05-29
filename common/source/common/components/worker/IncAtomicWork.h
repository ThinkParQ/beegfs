#ifndef INCATOMICWORK_H
#define INCATOMICWORK_H

#include <common/app/log/LogContext.h>
#include <common/components/worker/Work.h>
#include <common/threading/Atomics.h>

template <class TemplateType>
class IncAtomicWork : public Work
{
   public:
      IncAtomicWork(Atomic<TemplateType>* atomicValue)
      {
         this->atomicValue = atomicValue;
      }

      virtual ~IncAtomicWork() { };

      void process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen)
      {
         LOG_DEBUG("IncAtomicWork", Log_DEBUG,
            "Processing IncAtomicWork");

         // increment counter
         this->atomicValue->increase();

         LOG_DEBUG("IncAtomicWork", Log_DEBUG,
            "Processed IncAtomicWork");
      }

   private:
      Atomic<TemplateType>* atomicValue;
};

#endif /* INCATOMICWORK_H */
