#ifndef SERIALIZATION_H_
#define SERIALIZATION_H_

#include <common/Common.h>
#include <common/threading/Atomics.h>

#include "Byteswap.h"

#include <boost/scoped_array.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/static_assert.hpp>
#include <boost/type_traits/decay.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/utility/enable_if.hpp>

#define SERIALIZATION_NICLISTELEM_NAME_SIZE  (16)
#define SERIALIZATION_NICLISTELEM_SIZE       (8+SERIALIZATION_NICLISTELEM_NAME_SIZE) /*
                                              8 = 4b ipAddr + 1b nicType + 3b alignment padding */
#define SERIALIZATION_CHUNKINFOLISTELEM_ID_SIZE (96)
#define SERIALIZATION_CHUNKINFOLISTELEM_PATHSTR_SIZE (255)
#define SERIALIZATION_FILEINFOLISTELEM_OWNERNODE_SIZE (255)


class EntryInfo;
class Node;
class Path;
class QuotaData;
class StorageTargetInfo;
struct HighResolutionStats;
struct NicAddress;

template<typename T>
struct ListSerializationHasLength : boost::true_type {};

template<>
struct ListSerializationHasLength<char> : boost::false_type {};

template<typename KeyT, typename ValueT>
struct MapSerializationHasLength : boost::false_type {};

template<typename T>
struct IsSerdesPrimitive {
   typedef typename boost::decay<T>::type value_type;
   enum {
      value =
         boost::is_same<value_type, bool>::value
         || boost::is_same<value_type, char>::value
         || boost::is_same<value_type, uint8_t>::value
         || boost::is_same<value_type, int16_t>::value
         || boost::is_same<value_type, uint16_t>::value
         || boost::is_same<value_type, int32_t>::value
         || boost::is_same<value_type, uint32_t>::value
         || boost::is_same<value_type, int64_t>::value
         || boost::is_same<value_type, uint64_t>::value
   };
};

template<typename T>
struct SerializeAs {};

class Serializer
{
   public:
      Serializer()
         : buffer(NULL), bufferSize(-1), bufferOffset(0)
      {}

      Serializer(char* buffer, unsigned bufferSize)
         : buffer(buffer), bufferSize(bufferSize), bufferOffset(0)
      {}

      Serializer(Serializer&& other)
         : buffer(NULL), bufferSize(-1), bufferOffset(0)
      {
         swap(other);
      }

      Serializer& operator=(Serializer&& other)
      {
         Serializer(std::move(other)).swap(*this);
         return *this;
      }

   private:
      char* buffer;
      unsigned bufferSize;
      unsigned bufferOffset;

      template<typename Value, void (*Fn)(const Value*, Serializer&)>
      struct has_serializer : boost::true_type {};

      Serializer(char* buffer, unsigned bufferSize, unsigned bufferOffset)
         : buffer(buffer), bufferSize(bufferSize), bufferOffset(bufferOffset)
      {
      }

   public:
      bool isReading() const { return false; }
      bool isWriting() const { return true; }

      unsigned size() const
      {
         return this->bufferOffset;
      }

      bool good() const
      {
         return this->bufferSize && this->bufferOffset <= this->bufferSize;
      }

      void swap(Serializer& other)
      {
         std::swap(buffer, other.buffer);
         std::swap(bufferSize, other.bufferSize);
         std::swap(bufferOffset, other.bufferOffset);
      }

      Serializer mark() const
      {
         return {buffer, bufferSize, bufferOffset};
      }

      void putBlock(const void* source, size_t length)
      {
         if(this->buffer
            && likely(
                  this->bufferOffset + length >= this->bufferOffset
                  && this->bufferOffset + length <= this->bufferSize) )
            std::memcpy(this->buffer + this->bufferOffset, source, length);
         else
            this->bufferSize = 0;

         this->bufferOffset += length;
      }

      void skip(unsigned size)
      {
         while(size > 0)
         {
            putBlock("\0\0\0\0\0\0\0\0", std::min<unsigned>(size, 8) );
            size -= std::min<unsigned>(size, 8);
         }
      }

      template<typename Value>
      typename boost::enable_if_c<
            IsSerdesPrimitive<Value>::value,
            Serializer&>::type
         operator%(Value value)
      {
         putIntegral(value);
         return *this;
      }

      template<typename Value>
      typename boost::enable_if_c<
            !IsSerdesPrimitive<Value>::value
               && has_serializer<Value, &Value::serialize>::value,
            Serializer&>::type
         operator%(const Value& value)
      {
         Value::serialize(&value, *this);
         return *this;
      }

      template<typename Value, typename = typename SerializeAs<Value>::type>
      Serializer& operator%(const Value& value);

   public:
      template<typename T>
      Serializer& operator%(const std::list<T>& value)
      {
         putCollection<ListSerializationHasLength<T>::value>(value.begin(), value.end());
         return *this;
      }

      template<typename T>
      Serializer& operator%(const std::vector<T>& value)
      {
         putCollection<ListSerializationHasLength<T>::value>(value.begin(), value.end());
         return *this;
      }

      template<typename T, typename Compare>
      Serializer& operator%(const std::set<T, Compare>& value)
      {
         putCollection<ListSerializationHasLength<T>::value>(value.begin(), value.end());
         return *this;
      }

      template<typename KeyT, typename ValueT, typename CompT, typename AllocT>
      Serializer& operator%(const std::map<KeyT, ValueT, CompT, AllocT>& map)
      {
         putCollection<MapSerializationHasLength<KeyT, ValueT>::value>(map.begin(), map.end());
         return *this;
      }

      template<typename T1, typename T2>
      Serializer& operator%(const std::pair<T1, T2>& pair)
      {
         return *this
            % pair.first
            % pair.second;
      }

      template<typename... ElemsT>
      Serializer& operator%(const std::tuple<ElemsT...>& tuple)
      {
         putTuple(tuple, std::integral_constant<size_t, sizeof...(ElemsT)>());
         return *this;
      }

   private:
      void putIntegral(bool value) { putRaw<uint8_t>(value ? 1 : 0); }
      void putIntegral(char value) { putRaw(value); }
      void putIntegral(uint8_t value) { putRaw(value); }
      void putIntegral(int16_t value) { putRaw(HOST_TO_LE_16(value) ); }
      void putIntegral(uint16_t value) { putRaw(HOST_TO_LE_16(value) ); }
      void putIntegral(int32_t value) { putRaw(HOST_TO_LE_32(value) ); }
      void putIntegral(uint32_t value) { putRaw(HOST_TO_LE_32(value) ); }
      void putIntegral(int64_t value) { putRaw(HOST_TO_LE_64(value) ); }
      void putIntegral(uint64_t value) { putRaw(HOST_TO_LE_64(value) ); }

      template<typename Raw>
      void putRaw(Raw value)
      {
         putBlock(&value, sizeof(value) );
      }

      template<bool HasLengthField, typename Iter>
      void putCollection(const Iter begin, const Iter end)
      {
         Serializer atStart = mark();
         unsigned offsetAtStart = this->bufferOffset;
         uint32_t elements = 0; // list<T> is slow for size()

         if (HasLengthField)
            *this % uint32_t(0);

         *this % uint32_t(0); // number of elements

         for(Iter it = begin; it != end; ++it)
         {
            *this % *it;
            elements += 1;
         }

         if (HasLengthField)
            atStart % uint32_t(this->bufferOffset - offsetAtStart);

         atStart % elements;
      }

      template<typename... ElemsT, size_t Idx>
      void putTuple(const std::tuple<ElemsT...>& tuple, std::integral_constant<size_t, Idx>)
      {
         *this % std::get<sizeof...(ElemsT) - Idx>(tuple);
         putTuple(tuple, std::integral_constant<size_t, Idx - 1>());
      }

      template<typename... ElemsT>
      void putTuple(const std::tuple<ElemsT...>& tuple, std::integral_constant<size_t, 0>)
      {
      }
};

inline void swap(Serializer& a, Serializer& b)
{
   a.swap(b);
}

class Deserializer
{
   public:
      Deserializer(const char* buffer, unsigned bufferSize)
         : buffer(buffer), bufferSize(bufferSize), bufferOffset(0)
      {}

      Deserializer(Deserializer&& other)
         : buffer(NULL), bufferSize(-1), bufferOffset(0)
      {
         swap(other);
      }

      Deserializer& operator=(Deserializer&& other)
      {
         Deserializer(std::move(other)).swap(*this);
         return *this;
      }

   private:
      const char* buffer;
      unsigned bufferSize;
      unsigned bufferOffset;

      template<typename Value, void (*Fn)(Value*, Deserializer&)>
      struct has_deserializer : boost::true_type {};

      Deserializer(char* buffer, unsigned bufferSize, unsigned bufferOffset)
         : buffer(buffer), bufferSize(bufferSize), bufferOffset(bufferOffset)
      {
      }

   public:
      bool isReading() const { return true; }
      bool isWriting() const { return false; }

      const char* currentPosition() const
      {
         return this->buffer + this->bufferOffset;
      }

      unsigned size() const
      {
         return this->bufferOffset;
      }

      bool good() const
      {
         return this->bufferSize && this->bufferOffset <= this->bufferSize;
      }

      void swap(Deserializer& other)
      {
         std::swap(buffer, other.buffer);
         std::swap(bufferSize, other.bufferSize);
         std::swap(bufferOffset, other.bufferOffset);
      }

      void setBad()
      {
         this->bufferSize = 0;
      }

      void getBlock(void* dest, size_t length)
      {
         if(likely(this->bufferOffset + length >= this->bufferOffset
               && this->bufferOffset + length <= this->bufferSize) )
            std::memcpy(dest, this->buffer + this->bufferOffset, length);
         else
            setBad();

         this->bufferOffset += length;
      }

      template<typename Value>
      typename boost::enable_if_c<
            IsSerdesPrimitive<Value>::value,
            Deserializer&>::type
         operator%(Value& value)
      {
         getIntegral(value);
         return *this;
      }

      template<typename Value>
      typename boost::enable_if_c<
            !IsSerdesPrimitive<Value>::value
               && has_deserializer<Value, &Value::serialize>::value,
            Deserializer&>::type
         operator%(Value& value)
      {
         Value::serialize(&value, *this);
         return *this;
      }

      template<typename Value, typename = typename SerializeAs<Value>::type>
      Deserializer& operator%(Value& value);

      void skip(size_t length)
      {
         if(unlikely(this->bufferOffset + length < this->bufferOffset
               || this->bufferOffset + length > this->bufferSize) )
            setBad();

         this->bufferOffset += length;
      }

   public:
      template<typename T>
      Deserializer& operator%(std::list<T>& value)
      {
         getCollection(value);
         return *this;
      }

      template<typename T>
      Deserializer& operator%(std::vector<T>& value)
      {
         getCollection(value);
         return *this;
      }

      template<typename T, typename Compare>
      Deserializer& operator%(std::set<T, Compare>& value)
      {
         getCollection(value);
         return *this;
      }

      template<typename KeyT, typename ValueT, typename CompT, typename AllocT>
      Deserializer& operator%(std::map<KeyT, ValueT, CompT, AllocT>& map)
      {
         getMap(map);
         return *this;
      }

      template<typename T1, typename T2>
      Deserializer& operator%(std::pair<T1, T2>& pair)
      {
         return *this
            % pair.first
            % pair.second;
      }

      template<typename... ElemsT>
      Deserializer& operator%(std::tuple<ElemsT...>& tuple)
      {
         getTuple(tuple, std::integral_constant<size_t, sizeof...(ElemsT)>());
         return *this;
      }

   private:
      void getIntegral(bool& value) { value = getRaw<uint8_t>(); }
      void getIntegral(char& value) { value = getRaw<char>(); }
      void getIntegral(uint8_t& value) { value = getRaw<uint8_t>(); }
      void getIntegral(uint16_t& value) { value = LE_TO_HOST_16(getRaw<uint16_t>() ); }
      void getIntegral(uint32_t& value) { value = LE_TO_HOST_32(getRaw<uint32_t>() ); }
      void getIntegral(uint64_t& value) { value = LE_TO_HOST_64(getRaw<uint64_t>() ); }

      void getIntegral(int16_t& value)
      {
         value = bitcast<int16_t>(LE_TO_HOST_16(getRaw<uint16_t>() ) );
      }

      void getIntegral(int32_t& value)
      {
         value = bitcast<int32_t>(LE_TO_HOST_32(getRaw<uint32_t>() ) );
      }

      void getIntegral(int64_t& value)
      {
         value = bitcast<int64_t>(LE_TO_HOST_64(getRaw<uint64_t>() ) );
      }

      template<typename Raw>
      Raw getRaw()
      {
         Raw value = 0;
         getBlock(&value, sizeof(value) );
         return value;
      }

      template<typename Out, typename In>
      static Out bitcast(In in)
      {
         BOOST_STATIC_ASSERT(sizeof(In) == sizeof(Out) );
         Out out;
         memcpy(&out, &in, sizeof(in) );
         return out;
      }

   private:
      template<typename Collection>
      void getCollection(Collection& value)
      {
         unsigned offsetAtStart = this->bufferOffset;

         uint32_t totalLen;
         uint32_t size;

         if(ListSerializationHasLength<typename Collection::value_type>::value)
            *this % totalLen;

         *this % size;

         if(unlikely(!good() ) )
            return;

         value.clear();

         while(size > 0)
         {
            typename Collection::value_type item;

            *this % item;
            if (unlikely(!good()))
               return;
            value.insert(value.end(), item);

            size -= 1;
         }

         if(unlikely(!good() ) )
            return;

         if(!ListSerializationHasLength<typename Collection::value_type>::value)
            return;

         if(unlikely(totalLen != this->bufferOffset - offsetAtStart) )
            setBad();
      }

      template<typename KeyT, typename ValueT, typename CompT, typename AllocT>
      void getMap(std::map<KeyT, ValueT, CompT, AllocT>& value)
      {
         unsigned offsetAtStart = this->bufferOffset;

         uint32_t totalLen;
         uint32_t size;

         if (MapSerializationHasLength<KeyT, ValueT>::value)
            *this % totalLen;

         *this % size;

         if (unlikely(!good()))
            return;

         value.clear();

         while (size > 0)
         {
            KeyT key;

            *this % key;
            if (unlikely(!good()))
               return;
            *this % value[key];
            if (unlikely(!good()))
               return;

            size -= 1;
         }

         if (unlikely(!good()))
            return;

         if (!MapSerializationHasLength<KeyT, ValueT>::value)
            return;

         if (unlikely(totalLen != this->bufferOffset - offsetAtStart))
            setBad();
      }

      template<size_t Idx, typename... ElemsT>
      void getTuple(std::tuple<ElemsT...>& tuple, std::integral_constant<size_t, Idx>)
      {
         *this % std::get<sizeof...(ElemsT) - Idx>(tuple);
         getTuple(tuple, std::integral_constant<size_t, Idx - 1>());
      }

      template<typename... ElemsT>
      void getTuple(std::tuple<ElemsT...>& tuple, std::integral_constant<size_t, 0>)
      {
      }
};

inline void swap(Deserializer& a, Deserializer& b)
{
   a.swap(b);
}

template<typename Direction>
class PadFieldTo
{
   public:
      PadFieldTo(Direction& target, unsigned padding)
         : target(target), offsetAtStart(target.size() ), padding(padding)
      {}

      ~PadFieldTo()
      {
         unsigned rangeSize = target.size() - this->offsetAtStart;
         if(rangeSize % this->padding)
            target.skip(this->padding - rangeSize % this->padding);
      }

   private:
      Direction& target;
      size_t offsetAtStart;
      unsigned padding;

      PadFieldTo(const PadFieldTo&);
      PadFieldTo& operator=(const PadFieldTo&);
};



namespace serdes {

template<typename Value, typename Raw>
struct AsSer
{
   const Value* value;

   AsSer(const Value& value) : value(&value) {}

   friend Serializer& operator%(Serializer& ser, AsSer value)
   {
      return ser % Raw(*value.value);
   }
};

template<typename Value, typename Raw>
struct AsDes
{
   Value* value;

   AsDes(Value& value) : value(&value) {}

   friend Deserializer& operator%(Deserializer& des, AsDes value)
   {
      Raw raw;

      des % raw;
      *value.value = Value(raw);
      return des;
   }
};

template<typename Raw, typename Value>
inline AsSer<Value, Raw> as(const Value& value)
{
   return AsSer<Value, Raw>(value);
}

template<typename Raw, typename Value>
inline AsDes<Value, Raw> as(Value& value)
{
   return AsDes<Value, Raw>(value);
}



struct RawStringSer
{
   const char* data;
   unsigned size;
   unsigned align;

   RawStringSer(const char* data, unsigned size, unsigned align)
      : data(data), size(size), align(align)
   {}

   friend Serializer& operator%(Serializer& ser, const RawStringSer& str)
   {
      PadFieldTo<Serializer> field(ser, str.align);
      ser % uint32_t(str.size);
      ser.putBlock(str.data, str.size);
      ser % char(0);
      return ser;
   }
};

struct RawStringDes
{
   const char** data;
   unsigned* size;
   unsigned align;

   RawStringDes(const char*& data, unsigned& size, unsigned align)
      : data(&data), size(&size), align(align)
   {}

   friend Deserializer& operator%(Deserializer& des, const RawStringDes& str)
   {
      PadFieldTo<Deserializer> field(des, str.align);
      uint32_t length;

      des % length;
      if(unlikely(!des.good() ) )
         return des;

      *str.size = length;
      *str.data = des.currentPosition();

      des.skip(length + 1);
      if(unlikely(!des.good() ) )
         return des;

      if( (*str.data)[length] != 0)
         des.setBad();

      return des;
   }
};

inline RawStringSer rawString(const char* source, const unsigned& length, unsigned align = 1)
{
   return RawStringSer(source, length, align);
}

inline RawStringDes rawString(const char*& source, unsigned& length, unsigned align = 1)
{
   return RawStringDes(source, length, align);
}



struct StdString
{
   std::string* data;
   unsigned align;

   StdString(std::string& data, unsigned align) : data(&data), align(align) {}

   friend Deserializer& operator%(Deserializer& des, const StdString& value)
   {
      const char* str;
      unsigned len;

      // silence compiler warning. len will always be properly initialized if raw string read
      // succeeds (i.e. des is good afterwards)
      str = NULL;
      len = 0;

      des % RawStringDes(str, len, value.align);
      if(likely(des.good() ) )
         *value.data = std::string(str, str + len);
      else
         value.data->clear();

      return des;
   }
};

inline StdString stringAlign4(std::string& str)
{
   return StdString(str, 4);
}

inline RawStringSer stringAlign4(const std::string& str)
{
   return RawStringSer(str.c_str(), str.size(), 4);
}



template<typename Object>
struct BackedPtrSer
{
   const Object* source;

   BackedPtrSer(const Object& source)
      : source(&source)
   {}

   friend Serializer& operator%(Serializer& ser, const BackedPtrSer& value)
   {
      return ser % *value.source;
   }
};

template<typename Object, typename Back = typename boost::remove_cv<Object>::type>
struct BackedPtrDes
{
   Object** ptr;
   Back* backing;

   BackedPtrDes(Object*& ptr, Back& backing)
      : ptr(&ptr), backing(&backing)
   {}

   static Object* get(Object& backing)
   {
      return &backing;
   }

   template<typename T>
   static Object* get(std::unique_ptr<T>& backing)
   {
      return backing.get();
   }

   friend Deserializer& operator%(Deserializer& des, BackedPtrDes value)
   {
      des % *value.backing;
      *value.ptr = BackedPtrDes::get(*value.backing);
      return des;
   }
};

template<typename Source, typename Backing>
inline BackedPtrSer<Source> backedPtr(Source* const& source, const Backing& backing)
{
   return BackedPtrSer<Source>(*source);
}

template<typename Source, typename Backing>
inline BackedPtrSer<Source> backedPtr(const Source* const& source, const Backing& backing)
{
   return BackedPtrSer<Source>(*source);
}

template<typename Object>
inline BackedPtrDes<Object> backedPtr(Object*& ptr, Object& backing)
{
   return BackedPtrDes<Object>(ptr, backing);
}

template<typename Object>
inline BackedPtrDes<const Object> backedPtr(const Object*& ptr, Object& backing)
{
   return BackedPtrDes<const Object>(ptr, backing);
}

template<typename Object>
inline BackedPtrDes<Object, std::unique_ptr<Object>>
   backedPtr(Object*& ptr, std::unique_ptr<Object>& backing)
{
   return BackedPtrDes<Object, std::unique_ptr<Object>>(ptr, backing);
}



struct RawBlockSer
{
   const char* source;
   uint32_t size;

   RawBlockSer(const char* source, uint32_t size)
      : source(source), size(size)
   {}

   friend Serializer& operator%(Serializer& ser, const RawBlockSer& block)
   {
      ser.putBlock(block.source, block.size);
      return ser;
   }
};

struct RawBlockDes
{
   const char** dest;
   const uint32_t* size;

   RawBlockDes(const char*& block, const uint32_t& size)
      : dest(&block), size(&size)
   {}

   friend Deserializer& operator%(Deserializer& des, const RawBlockDes& block)
   {
      *block.dest = des.currentPosition();
      des.skip(*block.size);
      return des;
   }
};

inline RawBlockSer rawBlock(const char*const& block, const uint32_t& size)
{
   return RawBlockSer(block, size);
}

inline RawBlockDes rawBlock(const char*& block, uint32_t& size)
{
   return RawBlockDes(block, size);
}



template<typename Value, typename As>
struct AtomicAsSer
{
   Atomic<Value> const* value;

   AtomicAsSer(const Atomic<Value>& value)
      : value(&value)
   {}

   friend Serializer& operator%(Serializer& ser, const AtomicAsSer& value)
   {
      return ser % as<As>(value.value->read() );
   }
};

template<typename Value, typename As>
struct AtomicAsDes
{
   Atomic<Value>* value;

   AtomicAsDes(Atomic<Value>& value)
      : value(&value)
   {}

   friend Deserializer& operator%(Deserializer& des, const AtomicAsDes& value)
   {
      As raw;
      des % raw;
      value.value->set(raw);
      return des;
   }
};

template<typename As, typename Value>
inline AtomicAsSer<Value, As> atomicAs(const Atomic<Value>& value)
{
   return AtomicAsSer<Value, As>(value);
}

template<typename As, typename Value>
inline AtomicAsDes<Value, As> atomicAs(Atomic<Value>& value)
{
   return AtomicAsDes<Value, As>(value);
}



template<typename Base, typename Derived>
Base& base(Derived* value)
{
   return *value;
}

template<typename Base, typename Derived>
const Base& base(const Derived* value)
{
   return *value;
}

}

inline Serializer& operator%(Serializer& ser, const std::string& value)
{
   return ser % serdes::rawString(value.c_str(), value.size() );
}

inline Deserializer& operator%(Deserializer& des, std::string& value)
{
   return des % serdes::StdString(value, 1);
}



template<typename Value, typename>
Serializer& Serializer::operator%(const Value& value)
{
   return *this % serdes::as<typename SerializeAs<Value>::type>(value);
}

template<typename Value, typename>
Deserializer& Deserializer::operator%(Value& value)
{
   return *this % serdes::as<typename SerializeAs<Value>::type>(value);
}



Serializer& operator%(Serializer& ser, const std::list<std::string>& list);
Deserializer& operator%(Deserializer& des, std::list<std::string>& list);

Serializer& operator%(Serializer& ser, const std::vector<std::string>& vec);
Deserializer& operator%(Deserializer& des, std::vector<std::string>& vec);

Serializer& operator%(Serializer& ser, const std::set<std::string>& set);
Deserializer& operator%(Deserializer& des, std::set<std::string>& set);



template<typename Object>
inline ssize_t serializeIntoNewBuffer(const Object& object, boost::scoped_array<char>& result)
{
   Serializer ser;
   ser % object;

   ssize_t bufferLength = ser.size();

   boost::scoped_array<char> buffer(new (std::nothrow) char[bufferLength]);
   if (!buffer)
      return -1;

   ser = Serializer(buffer.get(), bufferLength);
   ser % object;

   if (!ser.good())
      return -1;

   buffer.swap(result);
   return ser.size();
}

#endif /*SERIALIZATION_H_*/
