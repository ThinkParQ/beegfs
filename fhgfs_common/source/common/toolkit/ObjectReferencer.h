#ifndef OBJECTREFERENCER_H_
#define OBJECTREFERENCER_H_

#include <common/app/log/LogContext.h>

template <class T>
class ObjectReferencer
{
   public:
      /**
       * @param ownReferencedObject if set to true, this object owns the referencedObject
       * (so it will delete() the referencedObject when itself is being deleted)
       */
      ObjectReferencer(T referencedObject, bool ownReferencedObject=true)
      {
         this->referencedObject = referencedObject;
         this->ownReferencedObject = ownReferencedObject;

         this->refCount = 0;
      }

      ~ObjectReferencer()
      {
         if(ownReferencedObject)
            delete referencedObject;
      }


   private:
      int refCount;
      bool ownReferencedObject;

      T referencedObject;

   public:
      // inliners

      T reference()
      {
         refCount++;
         return referencedObject;
      }

      int release()
      {
         #ifdef DEBUG_REFCOUNT
            if(!refCount)
            {
               LogContext log("ObjectReferencer::release");
               log.logErr("Bug: refCount is 0 and release() was called");
               log.logBacktrace();

               return 0;
            }
            else
               return --refCount;
         #else
            return(--refCount);
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
         return refCount;
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


#endif /*OBJECTREFERENCER_H_*/
