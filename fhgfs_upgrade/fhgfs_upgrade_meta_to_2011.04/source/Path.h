#ifndef PATH_H_
#define PATH_H_

#include "StringTk.h"
#include "Common.h"


class Path
{
   public:
      Path()
      {
         this->isPathAbsolute = false;
         this->isPathStrValid = false;
      }

      Path(const std::string pathStr)
      {
         this->isPathAbsolute = isPathStrAbsolute(pathStr);
         parseStr(pathStr);

         this->isPathStrValid = false;
      }

      Path(Path& parentPath, Path& childPath)
      {
         this->isPathAbsolute = parentPath.isPathAbsolute;
         this->isPathStrValid = false;

         pathElems.insert(pathElems.end(),
            parentPath.pathElems.begin(), parentPath.pathElems.end() );
         pathElems.insert(pathElems.end(),
            childPath.pathElems.begin(), childPath.pathElems.end() );
      }

      Path(Path& parentPath, const std::string childPathStr)
      {
         Path childPath(childPathStr);

         this->isPathAbsolute = parentPath.isPathAbsolute;
         this->isPathStrValid = false;

         pathElems.insert(pathElems.end(),
            parentPath.pathElems.begin(), parentPath.pathElems.end() );
         pathElems.insert(pathElems.end(),
            childPath.pathElems.begin(), childPath.pathElems.end() );
      }


      void parseStr(const std::string pathStr)
      {
         StringTk::explode(pathStr, '/', &pathElems);
      }

      /**
       * Call this to allow usage of getPathAsStrConst() later (for thread-safety)
       */
      void initPathStr()
      {
         if(!isPathStrValid)
            updatePathStr();
      }



   private:
      StringList pathElems;
      bool isPathAbsolute;

      std::string pathStr;
      bool isPathStrValid;

      // inliners
      static bool isPathStrAbsolute(std::string pathStr)
      {
         return (pathStr.length() && (pathStr[0] == '/') );
      }

      void updatePathStr()
      {
         StringListIter iter = pathElems.begin();
         size_t pathSize = pathElems.size();

         pathStr.clear();

         if(isPathAbsolute)
            pathStr += "/";

         for(size_t i=0; i < pathSize; i++, iter++)
         {
            if(i)
               pathStr += "/";

            pathStr += *iter;
         }


         isPathStrValid = true;
      }


   public:
      // inliners

      bool operator == (const Path& other) const
      {
         if(pathElems.size() != other.pathElems.size() )
            return false;

         if(isPathAbsolute != other.isPathAbsolute)
            return false;

         StringListConstIter iterThis = pathElems.begin();
         StringListConstIter iterOther = other.pathElems.begin();
         for( ; iterThis != pathElems.end(); iterThis++, iterOther++)
         {
            if(iterThis->compare(*iterOther) )
               return false;
         }

         return true;
      }

      bool operator != (const Path& other) const
      {
         return !(*this == other);
      }

      void appendElem(std::string newElem)
      {
         pathElems.push_back(newElem);
         isPathStrValid = false;
      }


      // getters & setters

      size_t getNumPathElems()
      {
         return pathElems.size();
      }

      bool getIsEmpty()
      {
         return pathElems.empty();
      }


      const StringList* getPathElems() const
      {
         return &pathElems;
      }

      /**
       * Note: Only use this if you are planning to update the path elements.
       */
      StringList* getPathElemsNonConst()
      {
         isPathStrValid = false;
         return &pathElems;
      }

      std::string getLastElem() const
      {
         StringListConstIter iter = --(pathElems.end() );
         return *iter;
      }


      /**
       * @return string does not end with a slash
       */
      std::string getPathAsStr()
      {
         if(!isPathStrValid)
            updatePathStr();

         return pathStr;
      }

      /**
       * Note: Make sure that the internal string was initialized when you use this (typically by
       * calling initPathStr() )
       *
       * @return string does not end with a slash
       */
      std::string getPathAsStrConst() const
      {
         if(unlikely(!isPathStrValid) )
         {
            std::cerr << "BUG(?): getPathAsStrConst called with uninitialized path string" <<
               std::endl;

            return "<undef>";
         }

         return pathStr;
      }

      bool isAbsolute() const
      {
         return isPathAbsolute;
      }

      void setAbsolute(bool isAbsolute)
      {
         isPathAbsolute = isAbsolute;
         isPathStrValid = false;
      }

};


#endif /*PATH_H_*/
