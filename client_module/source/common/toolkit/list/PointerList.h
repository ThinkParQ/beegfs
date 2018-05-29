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
static inline size_t PointerList_length(const PointerList* this);
static inline void* PointerList_getHeadValue(PointerList* this);
static inline void* PointerList_getTailValue(PointerList* this);
static inline void PointerList_clear(PointerList* this);

static inline PointerListElem* PointerList_getHead(PointerList* this);
static inline PointerListElem* PointerList_getTail(PointerList* this);
static inline PointerListElem* PointerList_getNext(PointerListElem* elem);
static inline PointerListElem* PointerList_getPrev(PointerListElem* elem);


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

void PointerList_addHead(PointerList* this, void* valuePointer)
{
   PointerListElem* elem = (PointerListElem*)os_kmalloc(
      sizeof(PointerListElem) );

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
   elem->valuePointer = valuePointer;
   this->length++;
}

void PointerList_addTail(PointerList* this, void* valuePointer)
{
   PointerListElem* elem = (PointerListElem*)os_kmalloc(sizeof(PointerListElem) );

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
   elem->valuePointer = valuePointer;
   this->length++;
}

void PointerList_append(PointerList* this, void* valuePointer)
{
   PointerList_addTail(this, valuePointer);
}

void PointerList_removeHead(PointerList* this)
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
      kfree(this->head);
      this->head = NULL;
      this->tail = NULL;
      this->length = 0;
   }
   else
   { // there are more elements in the list
      PointerListElem* newHead = this->head->next;

      kfree(this->head);

      this->head = newHead;
      this->head->prev = NULL;

      this->length--;
   }

}

void PointerList_removeTail(PointerList* this)
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
      kfree(this->tail);
      this->head = NULL;
      this->tail = NULL;
      this->length = 0;
   }
   else
   { // there are more elements in the list
      PointerListElem* newTail = this->tail->prev;

      kfree(this->tail);

      this->tail = newTail;
      this->tail->next = NULL;

      this->length--;
   }
}

void PointerList_removeElem(PointerList* this, PointerListElem* elem)
{
   if(elem == this->head)
      PointerList_removeHead(this);
   else
   if(elem == this->tail)
      PointerList_removeTail(this);
   else
   {
      // not head and not tail, so this elem is somewhere in the middle

      PointerListElem* prev = elem->prev;
      PointerListElem* next = elem->next;

      prev->next = next;
      next->prev = prev;

      kfree(elem);

      this->length--;
   }
}


size_t PointerList_length(const PointerList* this)
{
   return this->length;
}


void* PointerList_getHeadValue(PointerList* this)
{
   return this->head ? this->head->valuePointer : NULL;
}

void* PointerList_getTailValue(PointerList* this)
{
   return this->tail ? this->tail->valuePointer : NULL;
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

PointerListElem* PointerList_getHead(PointerList* this)
{
   return this->head;
}

PointerListElem* PointerList_getTail(PointerList* this)
{
   return this->tail;
}

PointerListElem* PointerList_getNext(PointerListElem* elem)
{
   return elem->next;
}

PointerListElem* PointerList_getPrev(PointerListElem* elem)
{
   return elem->prev;
}


#endif /*POINTERLIST_H_*/
