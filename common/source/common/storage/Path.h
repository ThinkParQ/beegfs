#ifndef COMMON_PATH_H_
#define COMMON_PATH_H_

#include <common/toolkit/serialization/Serialization.h>

#ifdef BEEGFS_DEBUG
#include <common/app/log/LogContext.h>
#endif

#include <string>
#include <cstring>
#include <vector>
#include <stdexcept>
#include <utility>

class Path
{
   public:
      Path() {}

      explicit Path(std::string str) :
         pathStr(std::move(str))
      {
         updateDirSeparators();
      }

      void operator=(const std::string& str)
      {
         pathStr = str;
         updateDirSeparators();
      }

      const std::string& str() const
      {
         return pathStr;
      }

      bool absolute() const
      {
         return !dirSeparators.empty() && dirSeparators.front() == 0;
      }

      /**
       * @returns the first component of the path.
       */
      std::string front() const
      {
         if (dirSeparators.empty())
            return pathStr;
         else if (!absolute())
            return pathStr.substr(0, dirSeparators[0]);
         else if (dirSeparators.size() == 1)
            return pathStr.substr(1);
         else
            return pathStr.substr(1, dirSeparators[1] - 1);
      }

      /**
       * @returns the last element of the path.
       */
      std::string back() const
      {
         if (dirSeparators.empty())
            return pathStr;
         else
            return pathStr.substr(dirSeparators.back() + 1);
      }

      bool empty() const
      {
         return pathStr.empty();
      }

      /**
       * @returns the dirname of the path, i.e. the whole path excluding the last component.
       */
      Path dirname() const
      {
         if (dirSeparators.size() > 0)
            return Path(pathStr.substr(0, dirSeparators.back()));
         else
            return Path(".");
      }

      /**
       * @returns the number of components in the path.
       */
      size_t size() const
      {
         if (pathStr.empty())
            return 0;
         else if (absolute())
            return dirSeparators.size();
         else
            return dirSeparators.size() + 1;
      }

      /*
       * @returns the index'th element of the Path.
       * @throws std::out_of_range if index is out of range (i.e. index >= this->size()).
       */
      std::string operator[](size_t index) const
      {
         if (absolute())
         {
            if (index >= dirSeparators.size())
               throw std::out_of_range("Path element index out of range.");

            const std::string::size_type left = dirSeparators[index] + 1;

            if (index == dirSeparators.size() - 1)
               return pathStr.substr(left);


            const std::string::size_type right = dirSeparators[index + 1] - left;

            return pathStr.substr(left, right);
         }
         else
         {
            if (index > dirSeparators.size())
               throw std::out_of_range("Path element index out of range.");

            if (index == 0)
            {
               if (dirSeparators.empty())
                  return pathStr;
               else
                  return pathStr.substr(0, dirSeparators[0]);
            }

            const std::string::size_type left = dirSeparators[index - 1] + 1;

            if (index == dirSeparators.size())
               return pathStr.substr(left);

            const std::string::size_type right = dirSeparators[index] - left;

            return pathStr.substr(left, right);
         }
      }

      /**
       * Concatenates a path to the existing path.
       * @throws std::invalid_argument if path to be concatenated is absolute.
       */
      Path& operator/=(const Path& rhs)
      {
         if (rhs.absolute())
         {
#ifdef BEEGFS_DEBUG
            LogContext(__func__).logBacktrace();
#endif
            throw std::invalid_argument("Can't concatenate absolute path; "
                  "Base: " + pathStr + " Concatenated path: " + rhs.pathStr + ".");
         }

         if (empty())
         {
            *this = rhs;
            return *this;
         }

         if (!rhs.empty())
         {
            dirSeparators.push_back(pathStr.length());

            for (auto it = rhs.dirSeparators.begin(); it != rhs.dirSeparators.end(); ++it)
               dirSeparators.push_back(*it + pathStr.length() + 1);

            pathStr.reserve(pathStr.length() + 1 + rhs.pathStr.length());
            pathStr.append("/");
            pathStr.append(rhs.pathStr);
         }

         return *this;
      }

      Path& operator/=(const char* rhs)
      {
         if (empty())
         {
            *this = rhs;
            return *this;
         }

         const size_t rhsLen = std::strlen(rhs);
         if (rhsLen != 0)
         {
            if (rhs[0] == '/')
            {
#ifdef BEEGFS_DEBUG
            LogContext(__func__).logBacktrace();
#endif
               throw std::invalid_argument("Can't concatenate absolute path; "
                     "Base: " + pathStr + " Concatenated path: " + rhs + ".");
            }

            dirSeparators.push_back(pathStr.length());

            for (size_t i = 0; i < rhsLen; i++)
               if (rhs[i] == '/')
                  dirSeparators.push_back(pathStr.length() + i + 1);

            pathStr.reserve(pathStr.length() + 1 + rhsLen);
            pathStr.append("/");
            pathStr.append(rhs);
         }

         return *this;
      }

      Path& operator/=(const std::string& rhs)
      {
         return operator/=(rhs.c_str());
      }

      friend Path operator/(const Path& lhs, const Path& rhs)
      {
         Path res(lhs);
         res /= rhs;
         return res;
      }

      friend Path operator/(const Path& lhs, const std::string& rhs)
      {
         Path res(lhs);
         res /= rhs;
         return res;
      }

      friend Path operator/(const std::string& lhs, const Path& rhs)
      {
         Path res(lhs);
         res /= rhs;
         return res;
      }

      friend Path operator/(const Path& lhs, const char* rhs)
      {
         Path res(lhs);
         res /= rhs;
         return res;
      }

      friend Path operator/(const char* lhs, const Path& rhs)
      {
         Path res(lhs);
         res /= rhs;
         return res;
      }

      friend bool operator==(const Path& lhs, const Path& rhs)
      {
         // No need to compare dirSeparators here
         return lhs.pathStr == rhs.pathStr;
      }

      friend bool operator!=(const Path& lhs, const Path& rhs)
      {
         return lhs.pathStr != rhs.pathStr;
      }

      static void serialize(const Path* obj, Serializer& ser)
      {
         ser % obj->pathStr;
      }

      static void serialize(Path* obj, Deserializer& des)
      {
         des % obj->pathStr;
         obj->updateDirSeparators();
      }

      friend std::ostream& operator<<(std::ostream& ostr, const Path& p)
      {
         ostr << p.pathStr;
         return ostr;
      }

   private:
      std::string pathStr;
      std::vector<std::string::size_type> dirSeparators;

      /**
       * Updates the dirSeparators vector and compresses any consecutive directory separators
       * contained in the path string.
       */
      void updateDirSeparators()
      {
         std::string::size_type inputIndex = 0;
         std::string::size_type outputIndex = 0;

         // Compress consecutive slashes
         while (inputIndex < pathStr.size())
         {
            pathStr[outputIndex++] = pathStr[inputIndex++];

            if (pathStr[inputIndex - 1] == '/')
            {
               dirSeparators.push_back(outputIndex - 1);
               while (pathStr[inputIndex] == '/')
                  inputIndex++;
            }
         }

         // Remove trailing slash if there is one
         if (!pathStr.empty() && pathStr[outputIndex - 1] == '/')
         {
            pathStr.resize(outputIndex - 1);
            dirSeparators.pop_back();
         }
         else
         {
            pathStr.resize(outputIndex);
         }
      }
};

#endif /* COMMON_PATH_H_ */
