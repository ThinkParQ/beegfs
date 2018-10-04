#include "Serialization.h"

template<typename Collection>
static void serializeStringCollection(Serializer& ser, const Collection& value)
{
   Serializer atStart = ser.mark();
   uint32_t sizeAtStart = ser.size();
   uint32_t elements = 0; // list<T> is slow for size()

   ser
      % uint32_t(0)  // total length field
      % uint32_t(0); // number of elements

   for(typename Collection::const_iterator it = value.begin(), end = value.end(); it != end; ++it)
   {
      ser.putBlock(it->c_str(), it->size() );
      ser % char(0);
      elements += 1;
   }

   atStart
      % uint32_t(ser.size() - sizeAtStart)
      % elements;
}

template<typename Collection>
static void deserializeStringCollection(Deserializer& des, Collection& value)
{
   unsigned sizeAtStart = des.size();

   uint32_t totalLen;
   uint32_t size;

   des
      % totalLen
      % size;

   if(unlikely(!des.good() ) )
      return;

   value.clear();

   uint32_t lengthRemaining = totalLen - (des.size() - sizeAtStart);
   const char* rawStringArray = des.currentPosition();

   if(lengthRemaining)
   {
      des.skip(lengthRemaining);

      if(unlikely(!des.good() ) )
         return;

      if(unlikely(rawStringArray[lengthRemaining - 1]) )
      {
         des.setBad();
         return;
      }
   }

   while(size > 0 && lengthRemaining > 0)
   {
      std::string item(rawStringArray);
      lengthRemaining -= item.size() + 1;
      rawStringArray += item.size() + 1;
      value.insert(value.end(), item);
      size -= 1;
   }

   if(unlikely(size > 0 || lengthRemaining > 0) )
      des.setBad();
}


Serializer& operator%(Serializer& ser, const std::list<std::string>& list)
{
   serializeStringCollection(ser, list);
   return ser;
}

Deserializer& operator%(Deserializer& des, std::list<std::string>& list)
{
   deserializeStringCollection(des, list);
   return des;
}

Serializer& operator%(Serializer& ser, const std::vector<std::string>& vec)
{
   serializeStringCollection(ser, vec);
   return ser;
}

Deserializer& operator%(Deserializer& des, std::vector<std::string>& vec)
{
   deserializeStringCollection(des, vec);
   return des;
}

Serializer& operator%(Serializer& ser, const std::set<std::string>& set)
{
   serializeStringCollection(ser, set);
   return ser;
}

Deserializer& operator%(Deserializer& des, std::set<std::string>& set)
{
   deserializeStringCollection(des, set);
   return des;
}
