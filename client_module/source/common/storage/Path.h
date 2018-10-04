#ifndef PATH_H_
#define PATH_H_

#include <common/Common.h>
#include <common/toolkit/StringTk.h>
#include <common/toolkit/list/StrCpyList.h>
#include <common/toolkit/list/StrCpyListIter.h>


struct Path;
typedef struct Path Path;

static inline void Path_init(Path* this);
static inline void Path_initFromString(Path* this, const char* pathStr);
static inline void Path_uninit(Path* this);

static inline void Path_parseStr(Path* this, const char* pathStr);
static inline bool Path_isPathStrAbsolute(const char* pathStr);

// getters & setters
static inline StrCpyList* Path_getPathElems(Path* this);
static inline char* Path_getPathAsStrCopy(Path* this);
static inline bool Path_isAbsolute(Path* this);
static inline void Path_setAbsolute(Path* this, bool absolute);


struct Path
{
   StrCpyList pathElems;
   bool isPathAbsolute;
};

void Path_init(Path* this)
{
   StrCpyList_init(&this->pathElems);

   this->isPathAbsolute = false;
}

void Path_initFromString(Path* this, const char* pathStr)
{
   Path_init(this);

   this->isPathAbsolute = Path_isPathStrAbsolute(pathStr);
   Path_parseStr(this, pathStr);
}

void Path_uninit(Path* this)
{
   StrCpyList_uninit(&this->pathElems);
}

void Path_parseStr(Path* this, const char* pathStr)
{
   StringTk_explode(pathStr, '/', &this->pathElems);
}

bool Path_isPathStrAbsolute(const char* pathStr)
{
   return (strlen(pathStr) && (pathStr[0] == '/') );
}

StrCpyList* Path_getPathElems(Path* this)
{
   return &this->pathElems;
}

/**
 * @return string does not end with a slash; string is kalloced and needs to be kfreed by the caller
 */
char* Path_getPathAsStrCopy(Path* this)
{
   char* pathStr;
   StrCpyListIter iter;
   size_t currentPathPos;
   size_t totalPathLen;

   // count total path length
   totalPathLen = Path_isAbsolute(this) ? 1 : 0;

   if(!StrCpyList_length(&this->pathElems) )
   { // (very unlikely)
      totalPathLen = 1; // for terminating zero
   }

   StrCpyListIter_init(&iter, &this->pathElems);

   for( ; !StrCpyListIter_end(&iter); StrCpyListIter_next(&iter) )
   {
      char* currentPathElem = StrCpyListIter_value(&iter);

      totalPathLen += strlen(currentPathElem) + 1; // +1 for slash or terminating zero
   }


   // alloc path buffer
   pathStr = os_kmalloc(totalPathLen);


   // copy elems to path
   if(Path_isAbsolute(this) )
   {
      pathStr[0] = '/';
      currentPathPos = 1;
   }
   else
      currentPathPos = 0;


   StrCpyListIter_init(&iter, &this->pathElems);

   for( ; !StrCpyListIter_end(&iter); StrCpyListIter_next(&iter) )
   {
      char* currentPathElem = StrCpyListIter_value(&iter);
      size_t currentPathElemLen = strlen(currentPathElem);

      memcpy(&pathStr[currentPathPos], currentPathElem, currentPathElemLen);

      currentPathPos += currentPathElemLen;

      pathStr[currentPathPos] = '/';

      currentPathPos++;
   }

   // zero-terminate the pathStr
   pathStr[totalPathLen-1] = 0;

   return pathStr;
}

bool Path_isAbsolute(Path* this)
{
   return this->isPathAbsolute;
}

void Path_setAbsolute(Path* this, bool absolute)
{
   this->isPathAbsolute = absolute;
}

#endif /*PATH_H_*/
