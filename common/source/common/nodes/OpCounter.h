#ifndef OPCOUNTER_H_
#define OPCOUNTER_H_

#include <common/toolkit/StringTk.h>
#include <common/Common.h>
#include <common/threading/Atomics.h>
#include <common/nodes/Node.h>
#include <common/app/log/LogContext.h>
#include <common/nodes/OpCounterTypes.h>


#define OPCOUNTER_SUM_ELEM_INDEX   0


typedef std::vector<AtomicUInt64> AtomicUInt64Vector;
typedef AtomicUInt64Vector::iterator AtomicUInt64VectorIter;


/**
 * Class to store filesystem operation counters.
 *
 * Note: OpCounterLastEnum is NOT supposed to be used at all, it just defines the last number.
 *       The first vector element is supposed to be OPSUM.
 *       Valid operation counter numbers are therefore from: 1 ... (OpCounterLastEnum - 1)
 *       Usually that should be automatically given, if increaseOpCounter() is called using
 *       enum defined values.
 *
 * Note: This class always uses the element at position 0(==OPCOUNTER_SUM_INDEX) as special element
 * that represents the sum of all individual other operations.
 */
class OpCounter
{
   public:


   protected:
      /**
       * @param numOps OpCounterLastEnum, i.e. the number of operation counters including sum
       * element.
       */
      OpCounter(int numOps)
      {
         this->numCounter = numOps;

         opCounters.resize(this->numCounter);
      }


   private:
      AtomicUInt64Vector opCounters;
      int numCounter; // OpCounterLastEnum, so the number of operaration counters


   public:

      /**
       * Increase counter for given opType by one and increase total ops sum.
       */
      bool increaseOpCounter(int opType)
      {
         // Also check for == as OpCounterLastEnum is not an operation
         if (unlikely(opType >= this->numCounter))
         {
            LogContext log(__func__);
            log.logErr("Invalid operation type given: " + StringTk::int64ToStr(opType));
            log.logBacktrace();

            return false;
         }

         #ifdef BEEGFS_DEBUG
         if (unlikely(opType == OPCOUNTER_SUM_ELEM_INDEX))
         {
            LogContext log(__func__);
            log.logErr("opType == sumElement ( " + StringTk::int64ToStr(opType) + ")!");
            log.logBacktrace();

            return false;
         }
         #endif // BEEGFS_DEBUG

         opCounters[opType].increase();
         opCounters[OPCOUNTER_SUM_ELEM_INDEX].increase();

         return true;
      }

      /**
       * Increase counters of given read/write op by one, increase total ops sum, and increase the
       * corresponding op-counter-bytes value by given number of bytes.
       *
       * @param opType only StorageOpCounter_WRITEOPS and _READOPS allowed as value.
       */
      void increaseStorageOpBytes(int opType, uint64_t numBytes)
      {
         bool incOpRes = increaseOpCounter(opType);
         if (unlikely(incOpRes == false))
            return; // something entirely wrong

         if(opType == StorageOpCounter_READOPS)
            opCounters[StorageOpCounter_READBYTES].increase(numBytes);
         else
         if(opType == StorageOpCounter_WRITEOPS)
            opCounters[StorageOpCounter_WRITEBYTES].increase(numBytes);
         else
         { // invalid opType given (should never happen)
            LogContext log(__func__);
            log.logErr("Invalid operation type given: " + StringTk::int64ToStr(opType) );
            log.logBacktrace();

            return;
         }
      }

      /**
       * Read current counter value.
       */
      uint64_t getOpCounter(int operation)
      {
         return opCounters[operation].read();
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
      void addCountersToVec(UInt64Vector *vec)
      {
         for(AtomicUInt64VectorIter iter = opCounters.begin(); iter != opCounters.end(); iter++)
         {
            vec->push_back(iter->read() );
         }
      }
};

class MetaOpCounter : public OpCounter
{
   public:
      MetaOpCounter() : OpCounter(MetaOpCounter_OpCounterLastEnum) {}
};

class StorageOpCounter : public OpCounter
{
   public:
      StorageOpCounter() : OpCounter(StorageOpCounter_OpCounterLastEnum) {}
};


#endif /* OPCOUNTER_H_ */
