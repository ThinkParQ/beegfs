#include <common/toolkit/StringTk.h>
#include <common/Common.h>


void StringTk_explode(const char* s, char delimiter, StrCpyList* outList)
{
   ssize_t sLen = strlen(s);
   ssize_t lastPos = (strlen(s) && (s[0] == delimiter) ) ? 0 : -1;
   ssize_t currentPos;
   char* findRes;
   ssize_t newElemLen;

   // note: starts at pos 1 because an occurence at pos 0 would lead to an empty string,
   // which would be ignored anyways
   // note: i think the initialization of currentPos here is useless
   for(currentPos = 1; ; )
   {
      findRes = strchr(&s[lastPos+1], delimiter);

      if(!findRes)
      {
         ssize_t restLen = sLen - (lastPos+1);

         if(restLen)
         { // add rest
            char* newElem = StringTk_subStr(&s[lastPos+1], restLen);

            StrCpyList_append(outList, newElem);
            kfree(newElem);
         }

         return;
      }

      currentPos = findRes - s;

      // add substring to outList (leave the delimiter positions out)
      newElemLen = currentPos-lastPos-1; // -1=last delimiter
      if(newElemLen)
      {
         char* newElem = StringTk_subStr(&s[lastPos+1], newElemLen);

         StrCpyList_append(outList, newElem);
         kfree(newElem);
      }

      lastPos = currentPos;
   }
}


bool StringTk_strToBool(const char* s)
{
   if( !StringTk_hasLength(s) ||
       !strcmp(s, "1") ||
       !strcasecmp(s, "y") ||
       !strcasecmp(s, "on") ||
       !strcasecmp(s, "yes") ||
       !strcasecmp(s, "true") )
      return true;

   return false;
}


/**
 * @return string is kmalloc'ed and has to be kfree'd by the caller
 */
char* StringTk_trimCopy(const char* s)
{
   int lastRight;
   int firstLeft;
   int sLen;

   sLen = strlen(s);

   firstLeft = 0;
   while( (firstLeft < sLen) &&
      (s[firstLeft]==' ' || s[firstLeft]=='\n' || s[firstLeft]=='\r' || s[firstLeft]=='\t') )
      firstLeft++;

   if(firstLeft == sLen)
      return StringTk_strDup(""); // the string is empty or contains only space chars

   // the string contains at least one non-space char

   lastRight = sLen - 1;
   while(s[lastRight]==' ' || s[lastRight]=='\n' || s[lastRight]=='\r' || s[lastRight]=='\t')
      lastRight--;

   return StringTk_subStr(&s[firstLeft], lastRight - firstLeft + 1);
}

/**
 * Note: Provided, because old kernels don't provide kasprintf().
 *
 * @return will be kalloced and needs to be kfree'd by the caller
 */
char* StringTk_kasprintf(const char *fmt, ...)
{
   char* printStr;
   int printLen;
   char tmpChar; // (old kernels don't seem to be able to handle vsnprintf(NULL, 0, ...) )
   va_list ap;
   va_list apCopy; // kasprintf uses this

   va_start(ap, fmt);

   va_copy(apCopy, ap);

   printLen = vsnprintf(&tmpChar, 1, fmt, apCopy);

   va_end(apCopy);

   printStr = os_kmalloc(printLen + 1);
   if(likely(printStr) )
   {
      vsnprintf(printStr, printLen + 1, fmt, ap);
   }

   va_end(ap);

   return printStr;
}

