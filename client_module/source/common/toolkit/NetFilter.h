#ifndef NETFILTER_H_
#define NETFILTER_H_

#include <app/config/Config.h>
#include <common/toolkit/Serialization.h>
#include <common/toolkit/SocketTk.h>
#include <common/Common.h>
#include <linux/in.h>


struct NetFilterEntry;
typedef struct NetFilterEntry NetFilterEntry;

struct NetFilter;
typedef struct NetFilter NetFilter;


static inline __must_check bool NetFilter_init(NetFilter* this, const char* filename);
static inline NetFilter* NetFilter_construct(const char* filename);
static inline void NetFilter_uninit(NetFilter* this);
static inline void NetFilter_destruct(NetFilter* this);

// inliners
static inline bool NetFilter_isAllowed(NetFilter* this, struct in_addr ipAddr);
static inline bool NetFilter_isContained(NetFilter* this, struct in_addr ipAddr);
static inline bool __NetFilter_prepareArray(NetFilter* this, const char* filename);

// getters & setters
static inline size_t NetFilter_getNumFilterEntries(NetFilter* this);


struct NetFilterEntry
{
   uint32_t netAddressShifted; // shifted by number of significant bits
      // Note: this address is in host(!) byte order to enable correct shift operator usage
   int shiftBitsNum; // for right shift
};

struct NetFilter
{
   NetFilterEntry* filterArray;
   size_t filterArrayLen;
};


/**
 * @param filename path to filter file.
 */
bool NetFilter_init(NetFilter* this, const char* filename)
{
   return __NetFilter_prepareArray(this, filename);
}

NetFilter* NetFilter_construct(const char* filename)
{
   NetFilter* this = kmalloc(sizeof(*this), GFP_NOFS);

   if(!this ||
      !NetFilter_init(this, filename) )
   {
      kfree(this);
      return NULL;
   }

   return this;
}

void NetFilter_uninit(NetFilter* this)
{
   SAFE_KFREE(this->filterArray);
}

void NetFilter_destruct(NetFilter* this)
{
   NetFilter_uninit(this);

   kfree(this);
}

/**
 * Note: Empty filter will always return allowed (i.e. "true").
 * Note: Will always allow the loopback address.
 */
bool NetFilter_isAllowed(NetFilter* this, struct in_addr ipAddr)
{
   if(!this->filterArrayLen)
      return true;

   return NetFilter_isContained(this, ipAddr);
}

/**
 * Note: Always implicitly contains the loopback address.
 */
bool NetFilter_isContained(NetFilter* this, struct in_addr ipAddr)
{
   uint32_t ipHostOrder;
   size_t i;

   // note: stored addresses are in host byte order to enable correct shift operator usage
   ipHostOrder = ntohl(ipAddr.s_addr);

   if(ipHostOrder == INADDR_LOOPBACK) // (inaddr_loopback is in host byte order)
      return true;

   for(i = 0; i < this->filterArrayLen; i++)
   {
      const uint32_t ipHostOrderShifted = ipHostOrder >> this->filterArray[i].shiftBitsNum;

      if(this->filterArray[i].netAddressShifted == ipHostOrderShifted)
      { // address match
         return true;
      }
   }

   // no match found
   return false;
}

/**
 * @param filename path to filter file.
 * @return false if a specified file could not be opened.
 */
bool __NetFilter_prepareArray(NetFilter* this, const char* filename)
{
   bool loadRes = true;
   StrCpyList filterList;

   this->filterArray = NULL;
   this->filterArrayLen = 0;

   if(!StringTk_hasLength(filename) )
   { // no file specified => no filter entries
      return true;
   }


   StrCpyList_init(&filterList);

   loadRes = Config_loadStringListFile(filename, &filterList);
   if(!loadRes)
   { // file error
      printk_fhgfs(KERN_WARNING,
         "Unable to load configured net filter file: %s\n", filename);
      return false;
   }

   if(StrCpyList_length(&filterList) )
   { // file did contain some lines
      StrCpyListIter iter;
      size_t i;

      this->filterArrayLen = StrCpyList_length(&filterList);
      this->filterArray = kmalloc(this->filterArrayLen * sizeof(NetFilterEntry), GFP_NOFS);
      if(!this->filterArray)
         goto err_alloc;

      StrCpyListIter_init(&iter, &filterList);
      i = 0;

      for( ; !StrCpyListIter_end(&iter); StrCpyListIter_next(&iter) )
      {
         char* line = StrCpyListIter_value(&iter);
         uint32_t entryAddr;

         char* findRes = strchr(line, '/');
         if(!findRes)
         { // slash character missing => ignore this faulty line (and reduce filter length)
            this->filterArrayLen--;
            continue;
         }

         *findRes = 0;

         // note: we store addresses in host byte order to enable correct shift operator usage
         entryAddr = ntohl(SocketTk_in_aton(line).s_addr);
         this->filterArray[i].shiftBitsNum = 32 - StringTk_strToUInt(findRes + 1);
         this->filterArray[i].netAddressShifted = entryAddr >> this->filterArray[i].shiftBitsNum;

         i++; // must be increased here because of the faulty line handling
      }
   }

   StrCpyList_uninit(&filterList);
   return true;

err_alloc:
   StrCpyList_uninit(&filterList);
   return false;
}

size_t NetFilter_getNumFilterEntries(NetFilter* this)
{
   return this->filterArrayLen;
}


#endif /* NETFILTER_H_ */
