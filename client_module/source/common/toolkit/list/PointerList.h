#ifndef POINTERLIST_H_
#define POINTERLIST_H_

#include <common/Common.h>

struct PointerListElem;
typedef struct PointerListElem PointerListElem;
struct PointerList;
typedef struct PointerList PointerList;

static inline void PointerList_init(PointerList* this);
static inline void PointerList_uninit(PointerList* this);

static inline void PointerList_addHead(PointerList* this, void* valuePointer);
static inline void PointerList_addTail(PointerList* this, void* valuePointer);
static inline void PointerList_append(PointerList* this, void* valuePointer);
static inline void PointerList_removeHead(PointerList* this);
static inline void PointerList_removeTail(PointerList* this);
static inline void PointerList_removeElem(PointerList* this, PointerListElem* elem);
static inline void PointerList_moveToHead(PointerList* this, PointerListElem* elem);
static inline void PointerList_moveToTail(PointerList* this, PointerListElem* elem);
static inline size_t PointerList_length(const PointerList* this);
static inline void PointerList_clear(PointerList* this);

static inline PointerListElem* PointerList_getTail(PointerList* this);
static inline PointerListElem* PointerList_getHead(PointerList* this);


struct PointerListElem
{
   void* valuePointer;

   struct PointerListElem* prev;
   struct PointerListElem* next;
};

struct PointerList
{
   struct PointerListElem* head;
   struct PointerListElem* tail;

   size_t length;
};

void PointerList_init(PointerList* this)
{
   this->head = NULL;
   this->tail = NULL;

   this->length = 0;
}

void PointerList_uninit(PointerList* this)
{
   PointerList_clear(this);
}

static inline void __PointerList_addHead(PointerList* this, PointerListElem* elem)
{
   if(this->length)
   { // elements exist => replace head
      elem->next = this->head;
      this->head->prev = elem;
      this->head = elem;
   }
   else
   { // no elements exist yet
      this->head = elem;
      this->tail = elem;

      elem->next = NULL;
   }

   elem->prev = NULL;
   this->length++;
}

void PointerList_addHead(PointerList* this, void* valuePointer)
{
   PointerListElem* elem = (PointerListElem*)os_kmalloc(
      sizeof(PointerListElem) );
   elem->valuePointer = valuePointer;
   __PointerList_addHead(this, elem);
}

static inline void __PointerList_addTail(PointerList* this, PointerListElem* elem)
{
   if(this->length)
   { // elements exist => replace tail
      elem->prev = this->tail;
      this->tail->next = elem;
      this->tail = elem;
   }
   else
   { // no elements exist yet
      this->head = elem;
      this->tail = elem;

      elem->prev = NULL;
   }

   elem->next = NULL;
   this->length++;
}

void PointerList_addTail(PointerList* this, void* valuePointer)
{
   PointerListElem* elem = (PointerListElem*)os_kmalloc(sizeof(PointerListElem) );
   elem->valuePointer = valuePointer;
   __PointerList_addTail(this, elem);
}

void PointerList_append(PointerList* this, void* valuePointer)
{
   PointerList_addTail(this, valuePointer);
}

static inline void __PointerList_removeHead(PointerList* this, bool freeElem)
{
   #ifdef BEEGFS_DEBUG
      if(!this->length)
      {
         BEEGFS_BUG_ON(true, "Attempt to remove head from empty list");
         return;
      }
   #endif

   if(this->length == 1)
   { // removing the last element
      if (freeElem)
         kfree(this->head);
      this->head = NULL;
      this->tail = NULL;
      this->length = 0;
   }
   else
   { // there are more elements in the list
      PointerListElem* newHead = this->head->next;

      if (freeElem)
         kfree(this->head);

      this->head = newHead;
      this->head->prev = NULL;

      this->length--;
   }

}

void PointerList_removeHead(PointerList* this)
{
   __PointerList_removeHead(this, true);
}

static inline void __PointerList_removeTail(PointerList* this, bool freeElem)
{
   #ifdef BEEGFS_DEBUG
      if(!this->length)
      {
         BEEGFS_BUG_ON(true, "Attempt to remove tail from empty list");
         return;
      }
   #endif

   if(this->length == 1)
   { // removing the last element
      if (freeElem)
         kfree(this->tail);
      this->head = NULL;
      this->tail = NULL;
      this->length = 0;
   }
   else
   { // there are more elements in the list
      PointerListElem* newTail = this->tail->prev;

      if (freeElem)
         kfree(this->tail);

      this->tail = newTail;
      this->tail->next = NULL;

      this->length--;
   }
}

void PointerList_removeTail(PointerList* this)
{
   __PointerList_removeTail(this, true);
}

static inline void __PointerList_removeElem(PointerList* this, PointerListElem* elem, bool freeElem)
{
   if(elem == this->head)
      __PointerList_removeHead(this, freeElem);
   else
   if(elem == this->tail)
      __PointerList_removeTail(this, freeElem);
   else
   {
      // not head and not tail, so this elem is somewhere in the middle

      PointerListElem* prev = elem->prev;
      PointerListElem* next = elem->next;

      prev->next = next;
      next->prev = prev;

      if (freeElem)
         kfree(elem);

      this->length--;
   }
}

void PointerList_removeElem(PointerList* this, PointerListElem* elem)
{
   __PointerList_removeElem(this, elem, true);
}

size_t PointerList_length(const PointerList* this)
{
   return this->length;
}

void PointerList_clear(PointerList* this)
{
   // free all elems
   PointerListElem* elem = this->head;
   while(elem)
   {
      PointerListElem* next = elem->next;
      kfree(elem);
      elem = next;
   }

   // reset attributes
   this->head = NULL;
   this->tail = NULL;

   this->length = 0;
}

PointerListElem* PointerList_getTail(PointerList* this)
{
   return this->tail;
}

PointerListElem* PointerList_getHead(PointerList* this)
{
   return this->head;
}

void PointerList_moveToHead(PointerList* this, PointerListElem *elem)
{
   __PointerList_removeElem(this, elem, false);
   __PointerList_addHead(this, elem);
}

void PointerList_moveToTail(PointerList* this, PointerListElem *elem)
{
   __PointerList_removeElem(this, elem, false);
   __PointerList_addTail(this, elem);
}

#endif /*POINTERLIST_H_*/
