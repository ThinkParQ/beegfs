#include <common/toolkit/serialization/Serialization.h>
#include <common/toolkit/Random.h>
#include <common/toolkit/StringTk.h>
#include "Locking.h"



bool EntryLockDetails::operator==(const EntryLockDetails& other) const
{
   return clientNumID == other.clientNumID
      && clientFD == other.clientFD
      && ownerPID == other.ownerPID
      && lockAckID == other.lockAckID
      && lockTypeFlags == other.lockTypeFlags;
}

bool RangeLockDetails::operator==(const RangeLockDetails& other) const
{
   return clientNumID == other.clientNumID
      && ownerPID == other.ownerPID
      && lockAckID == other.lockAckID
      && lockTypeFlags == other.lockTypeFlags
      && start == other.start
      && end == other.end;
}



void EntryLockDetails::initRandomForSerializationTests()
{
   Random rand;

   this->clientFD = rand.getNextInt();
   this->clientNumID = NumNodeID(rand.getNextInt() );
   StringTk::genRandomAlphaNumericString(this->lockAckID, rand.getNextInRange(0, 30) );
   this->lockTypeFlags = rand.getNextInt();
   this->ownerPID = rand.getNextInt();
}

void RangeLockDetails::initRandomForSerializationTests()
{
   Random rand;

   this->clientNumID = NumNodeID(rand.getNextInt() );
   this->end = rand.getNextInt();
   StringTk::genRandomAlphaNumericString(this->lockAckID, rand.getNextInRange(0, 30) );
   this->lockTypeFlags = rand.getNextInt();
   this->ownerPID = rand.getNextInt();
   this->start = rand.getNextInt();
}
