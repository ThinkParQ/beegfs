#include "PollList.h"

PollList::PollList()
{
   // note: it's good to have arrayLen very small here so it will be realloced later again
      // (=> better for NUMA)

   this->arrayOutdated = false;
   this->arrayLen = 1;
   this->pollArray.reset(new struct pollfd[arrayLen]);
}

void PollList::add(Pollable* pollable)
{
   //LOG_DEBUG("PollList::add", 4, "Adding new FD: " +
   //   StringTk::intToStr(pollable->getFD() ) );

   size_t mapSize = pollMap.size();

   if(!arrayOutdated && (mapSize < arrayLen) )
   { // quick add without setting arrayOutdated
      // note: yes, we're using mapSize before insertion here (of course)
      pollArray[mapSize].fd = pollable->getFD();
      pollArray[mapSize].events = POLLIN;
      pollArray[mapSize].revents = 0;
   }
   else
   {
      arrayOutdated = true;
   }

   pollMap.insert(PollMapVal(pollable->getFD(), pollable) );
}

void PollList::remove(Pollable* pollable)
{
   removeByFD(pollable->getFD() );
}

void PollList::removeByFD(int fd)
{
   pollMap.erase(fd);
   arrayOutdated = true;
}

void PollList::getPollArray(struct pollfd** pollArrayOut, unsigned* outLen)
{
   if(arrayOutdated)
      updatePollArray();

   *pollArrayOut = pollArray.get();
   *outLen = pollMap.size();
}

Pollable* PollList::getPollableByFD(int fd)
{
   PollMapIter iter = pollMap.find(fd);

   if(iter == pollMap.end() )
      return NULL;

   return iter->second;
}

void PollList::updatePollArray()
{
   unsigned mapSize = pollMap.size();

   // resize array if required (grow only, shrinking doesn't make much sense because dropped conns
      // are probably gonne come back sooner or later)

   if(mapSize > arrayLen)
   { // need to enlarge the array
      arrayLen = (mapSize << 1); // twice as much as we need to avoid frequent realloc
      pollArray.reset(new struct pollfd[arrayLen]);
   }

   // copy from map to array

   PollMapIter iter = pollMap.begin();

   for(unsigned i = 0; i < mapSize; i++, iter++)
   {
      pollArray[i].fd = iter->first;
      pollArray[i].events = POLLIN;
      pollArray[i].revents = 0;
   }

   arrayOutdated = false;
}
