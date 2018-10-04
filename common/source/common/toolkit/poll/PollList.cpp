#include "PollList.h"

void PollList::add(Pollable* pollable)
{
   pollMap.insert(PollMapVal(pollable->getFD(), pollable) );
}

void PollList::remove(Pollable* pollable)
{
   removeByFD(pollable->getFD() );
}

void PollList::removeByFD(int fd)
{
   pollMap.erase(fd);
}

Pollable* PollList::getPollableByFD(int fd)
{
   PollMapIter iter = pollMap.find(fd);

   if(iter == pollMap.end() )
      return NULL;

   return iter->second;
}
