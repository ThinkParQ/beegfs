#ifndef LISTTK_H_
#define LISTTK_H_

#include <common/Common.h>

#include <iterator>

class ListTk
{
   public:


   private:
      ListTk() {}


   public:
      // inliners

      /**
       * @param outPos zero-based list position if found, -1 otherwise
       */
      static bool listContains(const std::string& searchStr, const StringList* list,
         ssize_t* outPos)
      {
         (*outPos) = 0;

         for(StringListConstIter iter=list->begin(); iter != list->end(); iter++)
         {
            if(!searchStr.compare(*iter) )
               return true;

            (*outPos)++;
         }

         (*outPos) = -1;

         return false;
      }

      /**
       * @param outPos zero-based list position if found, -1 otherwise
       */
      template<typename T>
      static bool listContains(T& searchValue, std::list<T>& list, ssize_t& outPos)
      {
         outPos = 0;

         for(StringListIter iter=list.begin(); iter != list.end(); iter++)
         {
            if (*iter == searchValue)
               return true;

            outPos++;
         }

         outPos = -1;

         return false;
      }

      template<typename T>
      static bool listsEqual(std::list<T>* listA, std::list<T>* listB)
      {
         // check sizes
         if ( listA->size() != listB->size() )
            return false;

         // sort both lists and compare elements
         listA->sort();
         listB->sort();

         typename std::list<T>::iterator iterA = listA->begin();
         typename std::list<T>::iterator iterB = listB->begin();

         while ( iterA != listA->end() )
         {
            T objectA = *iterA;
            T objectB = *iterB;

            if ( objectA != objectB )
               return false;

            iterA++;
            iterB++;
         }

         return true;
      }

      /*
       * remove all elements from removeList in list
       */
      template<typename T>
      static void removeFromList(std::list<T>& list, std::list<T>& removeList)
      {
         typename std::list<T>::iterator iter;
         for (iter = removeList.begin(); iter != removeList.end(); iter++)
         {
            list.remove(*iter);
         }
      }

      /* This function is meant to replace advance (http://www.cplusplus.com/reference/std/iterator
       * /advance/) from the STL in many cases. The problem with std::advance is, that if your list
       * contains x elements, you can still call advance(iter, x+y) without any problem. After
       * advancing to the lists end, it will just start at the beginning again.
       * This version of advance will stop at the end of the list and just set iter to list::end
       *
       * @param list the list used to check if the end is reached (should be the list, from which
       * iter was created)
       * @param iter the list's iterator
       * @param count how many elements to proceed
       * @return the actual element count, that was advanced
       */
      template<typename T, typename InputIterator>
      static unsigned advance(std::list<T>& list, InputIterator& iter, unsigned count)
      {
         for (unsigned i=0; i<count; i++)
         {
            if (iter == list.end())
               return i;

            iter++;
         }

         return count;
      }

      /*
       * erase an element from a list by position
       */
      template<typename T>
      static bool erase(std::list<T>& list, size_t pos)
      {
         // check size
         if ( pos >= list.size())
            return false;

         typename std::list<T>::iterator iter = list.begin();
         std::advance(iter, pos);

         list.erase(iter);

         return true;
      }
};


#endif /*LISTTK_H_*/
