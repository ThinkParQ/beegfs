#ifndef STORAGESTREAMLISTENERV2_H_
#define STORAGESTREAMLISTENERV2_H_

#include <app/App.h>
#include <common/components/streamlistenerv2/StreamListenerV2.h>
#include <program/Program.h>


/**
 * Other than common StreamListenerV2, this class can handle mutliple work queues through an
 * overridden getWorkQueue() method.
 */
class StorageStreamListenerV2 : public StreamListenerV2
{
   public:
      StorageStreamListenerV2(std::string listenerID, AbstractApp* app):
         StreamListenerV2(listenerID, app, NULL)
      {
         // nothing to be done here
      }

      virtual ~StorageStreamListenerV2() {}


   protected:
      // getters & setters

     virtual MultiWorkQueue* getWorkQueue(uint16_t targetID) const
     {
        return Program::getApp()->getWorkQueue(targetID);
     }
};

#endif /* STORAGESTREAMLISTENERV2_H_ */
