#ifndef NETFILTER_H_
#define NETFILTER_H_

#include <common/Common.h>
#include <common/app/config/ICommonConfig.h>


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
      NetFilter(ICommonConfig* cfg) throw(InvalidConfigException)
      {
         prepareArray(cfg);
      }

      ~NetFilter()
      {
         SAFE_FREE(filterArray);
      }

   private:
      NetFilterEntry* filterArray;
      size_t filterArrayLen;

      // inliners
      void prepareArray(ICommonConfig* cfg) throw(InvalidConfigException)
      {
         this->filterArray = NULL;
         this->filterArrayLen = 0;

         std::string filename = cfg->getConnNetFilterFile();
         if(filename.empty() )
            return; // no file specified => no filter entries


         StringList filterList;

         ICommonConfig::loadStringListFile(filename.c_str(), filterList);

         if(!filterList.empty() )
         { // file did contain some lines

            this->filterArrayLen = filterList.size();
            this->filterArray =
               (NetFilterEntry*)malloc(this->filterArrayLen * sizeof(NetFilterEntry) );

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
                  Serialization::ntohlTrans(inet_addr(entryPartsIter->c_str() ) );

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

         const in_addr_t ipHostOrder = Serialization::ntohlTrans(ipAddr);

         if(ipHostOrder == INADDR_LOOPBACK) // (inaddr_loopback is in host byte order)
            return true;

         for(size_t i = 0; i < this->filterArrayLen; i++)
         {
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
