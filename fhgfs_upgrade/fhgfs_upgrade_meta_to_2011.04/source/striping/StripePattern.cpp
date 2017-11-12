#include <serialization/Serialization.h>
#include "SimplePattern.h"
#include "Raid0Pattern.h"
#include "StripePattern.h"

/**
 * @return outPattern; outPattern->patternType is STRIPEPATTERN_Invalid on error
 */
StripePattern* StripePattern::createFromBuf(const char* patternStart,
   struct StripePatternHeader& patternHeader)
{
   StripePattern* pattern;
   
   switch(patternHeader.patternType)
   {
      case STRIPEPATTERN_Raid0:
      {
         pattern = new Raid0Pattern(patternHeader.chunkSize);
      } break;
      
      default:
      {
         pattern = new SimplePattern(STRIPEPATTERN_Invalid, 0);
         return pattern;
      } break;
   }

   const char* patternSpecificBuf = patternStart + STRIPEPATTERN_HEADER_LENGTH;
   unsigned patternSpecificBufLen = patternHeader.patternLength - STRIPEPATTERN_HEADER_LENGTH;

   if(!pattern->deserializePattern(patternSpecificBuf, patternSpecificBufLen) )
   {
      pattern->setPatternType(STRIPEPATTERN_Invalid);
      return pattern;
   }

   return pattern;
   
}
         

void StripePattern::serializeHeader(char* buf)
{
   size_t bufPos = 0;
   
   // pattern length
   
   bufPos += Serialization::serializeUInt(&buf[bufPos], getSerialPatternLength() );
   
   // pattern type

   bufPos += Serialization::serializeUInt(&buf[bufPos], patternType);   

   // chunkSize

   bufPos += Serialization::serializeUInt(&buf[bufPos], chunkSize);

}

bool StripePattern::deserializeHeader(const char* buf, size_t bufLen,
   struct StripePatternHeader* outPatternHeader)
{
   size_t bufPos = 0;
   
   // pattern length
   unsigned lenInfoFieldLen;
   if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos,
      &outPatternHeader->patternLength, &lenInfoFieldLen) )
      return false;
      
   bufPos += lenInfoFieldLen;
   
   // pattern type
   unsigned typeInfoFieldLen;
   if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos,
      &outPatternHeader->patternType, &typeInfoFieldLen) )
      return false;
      
   bufPos += typeInfoFieldLen;

   // chunkSize
   unsigned chunkSizeInfoFieldLen;
   if(!Serialization::deserializeUInt(&buf[bufPos], bufLen-bufPos,
      &outPatternHeader->chunkSize, &chunkSizeInfoFieldLen) )
      return false;
      
   bufPos += chunkSizeInfoFieldLen;
   
   
   // check length field
   if(outPatternHeader->patternLength < STRIPEPATTERN_HEADER_LENGTH)
      return false;
      
   // check chunkSize
   if(!outPatternHeader->chunkSize)
      return false;
      
      
   return true;
}








