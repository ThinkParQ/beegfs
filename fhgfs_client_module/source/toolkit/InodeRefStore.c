#include "InodeRefStore.h"


int __InodeRefStore_keyComparator(const void* key1, const void* key2)
{
   if(key1 < key2)
      return -1;
   else
   if(key1 > key2)
      return 1;
   else
      return 0;
}
