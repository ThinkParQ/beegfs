#ifndef LEFTJOINEQ_H_
#define LEFTJOINEQ_H_

#include <common/Common.h>

#include <cstddef>
#include <utility>

#include <boost/type_traits/decay.hpp>
#include <boost/utility/result_of.hpp>

template<typename Left, typename Right, typename KeyExtract>
class LeftJoinEq
{
   private:
      enum State {
         s_next_left,
         s_first_right,
         s_next_right,
         s_only_left,
      };

      typedef typename boost::decay<
            typename boost::result_of<KeyExtract(typename Left::ElementType&)>::type
         >::type LeftKey;
      typedef typename boost::decay<
            typename boost::result_of<KeyExtract(typename Right::ElementType&)>::type
         >::type RightKey;

   public:
      typedef std::pair<typename Left::ElementType, typename Right::ElementType*> ElementType;

      struct MarkerType
      {
         typename Left::MarkerType leftMark;
         typename Right::MarkerType rightMark;
         State state;

         bool hasRightMark;
         typename Right::MarkerType innerRightMark;
      };

   public:
      LeftJoinEq(Left left, Right right, KeyExtract keyExtract)
         : left(left), right(right), keyExtract(keyExtract), state(s_next_left),
           hasRightMark(false)
      {
         if(!this->right.step() )
            this->state = s_only_left;
      }

      bool step()
      {
         switch(this->state)
         {
            case s_only_left:
               if(!this->left.step() )
                  return false;

               return makeCurrent(false);

            case s_next_left:
               if(!this->left.step() )
               {
                  this->state = s_only_left;
                  return false;
               }

               if(this->hasRightMark &&
                     this->keyExtract(*this->left.get() ) == this->rightKeyAtMark)
                  this->right.restore(this->rightMark);
               else
                  this->hasRightMark = false;

               BEEGFS_FALLTHROUGH;

            case s_first_right: {
               while(rightKey() < leftKey() )
               {
                  if(!this->right.step() )
                  {
                     this->state = s_only_left;
                     return makeCurrent(false);
                  }
               }

               if(leftKey() == rightKey() )
               {
                  this->state = s_next_right;
                  this->hasRightMark = true;
                  this->rightKeyAtMark = rightKey();
                  this->rightMark = this->right.mark();
                  return makeCurrent(true);
               }

               this->state = s_next_left;
               return makeCurrent(false);
            }

            case s_next_right: {
               if(!this->right.step() )
               {
                  this->state = s_next_left;
                  return step();
               }

               if(leftKey() == rightKey() )
               {
                  this->state = s_next_right;
                  return makeCurrent(true);
               }

               this->state = s_next_left;
               return step();
            }
         }

         return false;
      }

      ElementType* get()
      {
         return &current;
      }

      MarkerType mark() const
      {
         MarkerType result = { this->left.mark(), this->right.mark(), this->state,
            this->hasRightMark, this->rightMark };
         return result;
      }

      void restore(MarkerType mark)
      {
         this->hasRightMark = mark.hasRightMark;
         if(mark.hasRightMark)
         {
            this->rightMark = mark.innerRightMark;
            this->right.restore(mark.innerRightMark);
            this->rightKeyAtMark = rightKey();
         }

         this->left.restore(mark.leftMark);
         this->right.restore(mark.rightMark);
         this->state = mark.state;

         if(this->state == s_only_left)
            makeCurrent(false);
         else
            makeCurrent(leftKey() == rightKey() );
      }

   private:
      bool makeCurrent(bool hasRight)
      {
         if(hasRight)
            this->current = ElementType(*this->left.get(), this->right.get() );
         else
            this->current = ElementType(*this->left.get(), nullptr);

         return true;
      }

      LeftKey leftKey() { return this->keyExtract(*this->left.get() ); }
      RightKey rightKey() { return this->keyExtract(*this->right.get() ); }

   private:
      Left left;
      Right right;
      KeyExtract keyExtract;
      ElementType current;
      State state;

      bool hasRightMark;
      RightKey rightKeyAtMark;
      typename Right::MarkerType rightMark;
};

namespace db {

template<typename KeyExtract, typename Left, typename Right>
inline LeftJoinEq<Left, Right, KeyExtract> leftJoinBy(KeyExtract ex, Left left, Right right)
{
   return LeftJoinEq<Left, Right, KeyExtract>(left, right, ex);
}

}

#endif
