#ifndef MODFICATIONEVENT_H_
#define MODFICATIONEVENT_H_

#include <database/EntryID.h>

namespace db {

struct ModificationEvent
{
   EntryID id;

   typedef EntryID KeyType;
   EntryID pkey() const { return id; }
};

}

#endif
