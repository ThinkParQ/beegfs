#ifndef NETFILTER_H_
#define NETFILTER_H_

#include <common/Common.h>
#include <common/app/config/ICommonConfig.h>
#include <common/toolkit/StringTk.h>
#include <common/toolkit/serialization/Serialization.h>

class NetFilter
{
   private:
      struct NetFilterEntry
      {
         in_addr_t netAddressShifted; // shifted by number of significant bits
            // Note: this address is in host(!) byte order to enable correct shift operator usage
         int shiftBitsNum; // for right shift
      };


   public:
      /**
       * @param filename path to filter file
       */
      NetFilter(std::string filename)
      {
         prepareArray(filename);
      }

   private:
      boost::scoped_array<NetFilterEntry> filterArray;
      size_t filterArrayLen;

      // inliners
      void prepareArray(std::string filename)
      {
         this->filterArray.reset();
         this->filterArrayLen = 0;

         if(filename.empty() )
            return; // no file specified => no filter entries


         StringList filterList;

         ICommonConfig::loadStringListFile(filename.c_str(), filterList);

         if(!filterList.empty() )
         { // file did contain some lines

            this->filterArrayLen = filterList.size();
            this->filterArray.reset(new NetFilterEntry[filterArrayLen]);

            StringListIter iter = filterList.begin();
            size_t i = 0;

            for( ; iter != filterList.end(); iter++)
            {
               StringList entryParts;

               StringTk::explode(*iter, '/', &entryParts);

               if(entryParts.size() != 2)
               { // ignore this faulty line (and reduce filter length)
                  this->filterArrayLen--;
                  continue;
               }

               // note: we store addresses in host byte order to enable correct shift operator usage
               StringListIter entryPartsIter = entryParts.begin();
               in_addr_t entryAddr =
                  ntohl(inet_addr(entryPartsIter->c_str() ) );

               entryPartsIter++;
               unsigned significantBitsNum = StringTk::strToUInt(*entryPartsIter);

               (this->filterArray)[i].shiftBitsNum = 32 - significantBitsNum;
               (this->filterArray)[i].netAddressShifted =
                  entryAddr >> ( (this->filterArray)[i].shiftBitsNum);

               i++; // must be increased here because of the faulty line handling
            }

         }

      }


   public:
      // inliners

      /**
       * Note: Will always allow the loopback address.
       */
      bool isAllowed(in_addr_t ipAddr) const
      {
         if(!this->filterArrayLen)
            return true;

         return isContained(ipAddr);
      }

      /**
       * Note: Always implicitly contains the loopback address.
       */
      bool isContained(in_addr_t ipAddr) const
      {
         const in_addr_t ipHostOrder = ntohl(ipAddr);

         if(ipHostOrder == INADDR_LOOPBACK) // (inaddr_loopback is in host byte order)
            return true;

         for(size_t i = 0; i < this->filterArrayLen; i++)
         {
            if (filterArray[i].shiftBitsNum == 32)
               return true;

            // note: stored addresses are in host byte order to enable correct shift operator usage
            const in_addr_t ipHostOrderShifted =
               ipHostOrder >> ( (this->filterArray)[i].shiftBitsNum);

            if( (this->filterArray)[i].netAddressShifted == ipHostOrderShifted)
            { // address match
               return true;
            }
         }

         // no match found
         return false;
      }


      // setters & getters
      size_t getNumFilterEntries() const
      {
         return filterArrayLen;
      }
};

#endif /* NETFILTER_H_ */
