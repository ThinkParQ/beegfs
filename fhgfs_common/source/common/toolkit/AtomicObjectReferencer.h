#ifndef ATOMICOBJECTREFERENCER_H_
#define ATOMICOBJECTREFERENCER_H_

#include <common/app/log/LogContext.h>

template <class T>
class AtomicObjectReferencer
{
   public:
      /**
       * @param ownReferencedObject if set to true, this object owns the referencedObject
       * (so it will delete() the referencedObject when itself is being deleted)
       */
      AtomicObjectReferencer(T referencedObject, bool ownReferencedObject=true)
      {
         this->referencedObject = referencedObject;
         this->ownReferencedObject = ownReferencedObject;
      }

      ~AtomicObjectReferencer()
      {
         if(ownReferencedObject)
            delete referencedObject;
      }


   private:
      AtomicSizeT refCount;
      bool ownReferencedObject;

      T referencedObject;

   public:
      // inliners

      T reference()
      {
         refCount.increase();
         return referencedObject;
      }

      int release()
      {
         #ifdef DEBUG_REFCOUNT
            if(refCount.read() == 0)
            {
               LogContext log("ObjectReferencer::release");
               log.logErr("Bug: refCount is 0 and release() was called.");
               log.logBacktrace();

               return 0;
            }
            else
               return refCount.decrease() - 1; // .decrease() returns the old value
         #else
            return refCount.decrease() - 1; // .decrease() returns the old value
         #endif // DEBUG_REFCOUNT
      }

      /**
       * Be careful: This does not change the reference count!
       */
      T getReferencedObject()
      {
         return referencedObject;
      }

      int getRefCount()
      {
         return refCount.read();
      }

      void setOwnReferencedObject(bool enable)
      {
         this->ownReferencedObject = enable;
      }

      bool getOwnReferencedObject()
      {
         return ownReferencedObject;
      }

};


#endif /*ATOMICOBJECTREFERENCER_H_*/
