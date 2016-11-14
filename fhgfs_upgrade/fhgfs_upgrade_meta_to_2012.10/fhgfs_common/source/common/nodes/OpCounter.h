/*
 * OpCounters - counter file system operations
 *
 */

#ifndef OPCOUNTER_H_
#define OPCOUNTER_H_

#include <string>
#include <common/Common.h>
#include <common/threading/Atomics.h>
#include <common/nodes/Node.h>
#include <common/app/log/LogContext.h>
#include <common/nodes/OpCounterTypes.h>

// ID to human readable name mapping
typedef struct OpCounterStrings
{
      int type;
      std::string opStr;
} OpCounterStrings;


/**
 * class to store filesystem operation counters
 * NOTE: OpCounterLastEnum is NOT supposed to be used at all, it just defines the last number
 *       The first vector element is supposed to be OPSUM.
 *       Valid operation counter numbers are therefore from: 1 ... (OpCounterLastEnum - 1)
 *       Usually that should be automatically given, if increaseOpCounter() is called using
 *       enum defined values.
 */
class OpCounter
{
   public:
      /**
       * create object OpCounter, valid nodeType is either NODETYPE_Meta or NODETYPE_Storage
       */
      OpCounter(enum NodeType nodeType)
      {
         switch (nodeType)
         {
            case NODETYPE_Meta:
            {
               this->numCounter = MetaOpCounter_OpCounterLastEnum;
               this->sumElement = MetaOpCounter_OPSUM;
            }
               break;
            case NODETYPE_Storage:
            {
               this->numCounter = StorageOpCounter_OpCounterLastEnum;
               this->sumElement = StorageOpCounter_OPSUM;
            }
               break;
            default:
            {
               LogContext log(__func__);
               log.logErr("Invalid Nodetype " + StringTk::int64ToStr(nodeType));

               // We won't use opcounters then at all
               this->numCounter = 0;

               return;
            }
               break;
         }

         // Really 'this->numCounters', but only if numCounter is defined as OpCounterLastEnum
         //  as this also solves resize(max + 1), as OpCounterLastEnum is not a valid Op
         OpCounters.resize(this->numCounter);
      }

   private:
      std::vector<AtomicUInt64> OpCounters;
      int sumElement; // the vector element that has the sum of all counters
      int numCounter; // OpCounterLastEnum - 1, so the number of operaration counters

   public:

      /**
       * Increase counter by one op
       */
      bool increaseOpCounter(int opType)
      {
         // Also check for == as it is not an operation, but 'OpCounterLastEnum'
         if (unlikely(opType >= this->numCounter))
         {
            LogContext log(__func__);
            log.logErr("Invalid operation type given: " + StringTk::int64ToStr(opType));
#ifdef FHGFS_DEBUG
            abort();
#endif
            return false;
         }

#ifdef FHGFS_DEBUG
         if (unlikely(opType == this->sumElement))
         {
            LogContext log(__func__);
            log.logErr("opType == sumElement ( " + StringTk::int64ToStr(opType) + ")!");
            abort();
         }
#endif
         OpCounters[opType].increase();
         OpCounters[this->sumElement].increase();

         return true;
      }

      /**
       * Increase the read/write opCounters by one op, but also increase the corresponding
       * op-counter-bytes values
       */
      void increaseOpBytes(int opType, uint64_t numBytes)
      {
         bool incOpRes = increaseOpCounter(opType);
         if (unlikely(incOpRes == false))
            return; // something entirely wrong

         // hopefully the compiler will remove this as we are inlining
         if (unlikely(opType != StorageOpCounter_WRITEOPS && opType != StorageOpCounter_READOPS))
         {
            LogContext log(__func__);
            log.logErr("Invalid operation type given: " + StringTk::int64ToStr(opType));

#ifdef FHGFS_DEBUG
            abort();
#endif
            return;
         }

         // hopefully the compiler will remove the 'if' as we are inlining
         if (opType == StorageOpCounter_WRITEOPS)
            OpCounters[StorageOpCounter_WRITEBYTES].increase(numBytes);
         else
            OpCounters[StorageOpCounter_READBYTES].increase(numBytes);
      }

      /**
       * Read current counter value - NOTE: without any lock protection, so just the current value
       */
      uint64_t getOpCounter(int operation)
      {
         return OpCounters[operation].read();
      }

      /**
       * Just return how many counters we have
       */
      uint64_t getNumCounter()
      {
         return this->numCounter;
      }

      /**
       * Add our counters to an existing vector
       */
      bool addCountersToVec(UInt64Vector *vec)
      {
         std::vector<AtomicUInt64>::iterator iter;
         for (iter = OpCounters.begin(); iter != OpCounters.end(); iter++)
         {
            vec->push_back(iter->read());
         }

         return true;
      }
};


#endif /* OPCOUNTER_H_ */
