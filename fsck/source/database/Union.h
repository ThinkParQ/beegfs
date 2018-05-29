#ifndef UNION_H_
#define UNION_H_

#include <utility>

template<typename Left, typename Right, typename KeyExtract>
class Union
{
   public:
      typedef typename Left::ElementType ElementType;

      struct MarkerType
      {
         typename Left::MarkerType leftMark;
         typename Right::MarkerType rightMark;
         bool leftEnded;
         bool rightEnded;
         bool nextStepLeft;
      };

   public:
      Union(Left left, Right right, KeyExtract keyExtract):
         left(left), right(right), keyExtract(keyExtract), leftEnded(false), nextStepLeft(true),
         currentAtLeft(false)
      {
         this->rightEnded = !this->right.step();
      }

      bool step()
      {
         if(this->leftEnded && this->rightEnded)
            return false;

         if(this->leftEnded)
         {
            this->rightEnded = !this->right.step();
            return resetCurrent();
         }

         if(this->rightEnded)
         {
            this->leftEnded = !this->left.step();
            return resetCurrent();
         }

         if(this->nextStepLeft)
            this->leftEnded = !this->left.step();
         else
            this->rightEnded = !this->right.step();

         return resetCurrent();
      }

      ElementType* get()
      {
         if(this->currentAtLeft)
            return this->left.get();
         else
            return this->right.get();
      }

      MarkerType mark() const
      {
         MarkerType result = { this->left.mark(), this->right.mark(), this->leftEnded,
            this->rightEnded, this->nextStepLeft };
         return result;
      }

      void restore(MarkerType mark)
      {
         this->leftEnded = mark.leftEnded;
         this->rightEnded = mark.rightEnded;
         this->nextStepLeft = mark.nextStepLeft;

         if(!this->leftEnded)
            this->left.restore(mark.leftMark);

         if(!this->rightEnded)
            this->right.restore(mark.rightMark);

         resetCurrent();
      }

   private:
      bool resetCurrent()
      {
         if(this->leftEnded && this->rightEnded)
            return false;

         if(this->leftEnded)
            this->currentAtLeft = false;
         else
         if(this->rightEnded)
            this->currentAtLeft = true;
         else
         if(this->keyExtract(*this->left.get() ) < this->keyExtract(*this->right.get() ) )
         {
            this->currentAtLeft = true;
            this->nextStepLeft = true;
         }
         else
         {
            this->currentAtLeft = false;
            this->nextStepLeft = false;
         }

         return true;
      }

   private:
      Left left;
      Right right;
      KeyExtract keyExtract;
      bool leftEnded;
      bool rightEnded;
      bool nextStepLeft;
      bool currentAtLeft;
};

namespace db {

template<typename KeyExtract, typename Left, typename Right>
inline Union<Left, Right, KeyExtract> unionBy(KeyExtract keyExtract, Left left, Right right)
{
   return Union<Left, Right, KeyExtract>(left, right, keyExtract);
}

}

#endif
