#ifndef STRCPYLIST_H_
#define STRCPYLIST_H_

#include "StringList.h"

struct StrCpyList;
typedef struct StrCpyList StrCpyList;

static inline void StrCpyList_init(StrCpyList* this);
static inline void StrCpyList_uninit(StrCpyList* this);
static inline void StrCpyList_addHead(StrCpyList* this, const char* valuePointer);
static inline void StrCpyList_append(StrCpyList* this, const char* valuePointer);
static inline size_t StrCpyList_length(StrCpyList* this);
static inline void StrCpyList_clear(StrCpyList* this);

struct StrCpyList
{
   struct StringList stringList;
};


void StrCpyList_init(StrCpyList* this)
{
   StringList_init( (StringList*)this);
}

void StrCpyList_uninit(StrCpyList* this)
{
   struct PointerListElem* elem = ( (PointerList*)this)->head;
   while(elem)
   {
      struct PointerListElem* next = elem->next;
      kfree(elem->valuePointer);
      elem = next;
   }


   StringList_uninit( (StringList*)this);
}

void StrCpyList_addHead(StrCpyList* this, const char* valuePointer)
{
   size_t valueLen = strlen(valuePointer)+1;
   char* valueCopy = (char*)os_kmalloc(valueLen);
   memcpy(valueCopy, valuePointer, valueLen);

   StringList_addHead( (StringList*)this, valueCopy);
}

void StrCpyList_append(StrCpyList* this, const char* valuePointer)
{
   size_t valueLen = strlen(valuePointer)+1;
   char* valueCopy = (char*)os_kmalloc(valueLen);
   memcpy(valueCopy, valuePointer, valueLen);

   StringList_append( (StringList*)this, valueCopy);
}

size_t StrCpyList_length(StrCpyList* this)
{
   return StringList_length( (StringList*)this);
}

void StrCpyList_clear(StrCpyList* this)
{
   struct PointerListElem* elem = ( (PointerList*)this)->head;
   while(elem)
   {
      struct PointerListElem* next = elem->next;
      kfree(elem->valuePointer);
      elem = next;
   }

   StringList_clear( (StringList*)this);
}

#endif /*STRCPYLIST_H_*/
