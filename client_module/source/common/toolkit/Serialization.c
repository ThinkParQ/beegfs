#include "Serialization.h"
#include <os/OsDeps.h>

bool __Serialization_deserializeNestedField(DeserializeCtx* ctx, DeserializeCtx* outCtx)
{
   unsigned length;

   if(!Serialization_deserializeUInt(ctx, &length) )
      return false;

   length -= Serialization_serialLenUInt();

   if (ctx->length < length)
      return false;

   outCtx->data = ctx->data;
   outCtx->length = length;

   ctx->data += length;
   ctx->length -= length;
   return true;
}

/**
 * @return 0 on error (e.g. strLen is greater than bufLen), used buffer size otherwise
 */
void Serialization_serializeStr(SerializeCtx* ctx, unsigned strLen, const char* strStart)
{
   // write length field
   Serialization_serializeUInt(ctx, strLen);

   // write raw string
   Serialization_serializeBlock(ctx, strStart, strLen);

   // write termination char
   Serialization_serializeChar(ctx, 0);
}

/**
 * @return false on error (e.g. strLen is greater than bufLen)
 */
bool Serialization_deserializeStr(DeserializeCtx* ctx,
   unsigned* outStrLen, const char** outStrStart)
{
   // length field
   if(unlikely(!Serialization_deserializeUInt(ctx, outStrLen) ) )
      return false;

   // check length and terminating zero
   if(unlikely( (ctx->length < *outStrLen + 1) || ( (ctx->data)[*outStrLen] != 0) ) )
      return false;

   // string start
   *outStrStart = ctx->data;
   ctx->data += *outStrLen + 1;
   ctx->length -= *outStrLen + 1;

   return true;
}

/**
 * @return 0 on error (e.g. arrLen is greater than bufLen), used buffer size otherwise
 */
void Serialization_serializeCharArray(SerializeCtx* ctx, unsigned arrLen, const char* arrStart)
{
   // elem count info field
   Serialization_serializeUInt(ctx, arrLen);

   // copy raw array data
   Serialization_serializeBlock(ctx, arrStart, arrLen);
}

/**
 * @return false on error (e.g. arrLen is greater than bufLen)
 */
bool Serialization_deserializeCharArray(DeserializeCtx* ctx,
   unsigned* outElemNum, const char** outArrStart, unsigned* outLen)
{
   if(unlikely(!Serialization_deserializeUInt(ctx, outElemNum) ) )
      return false;

   if(ctx->length < *outElemNum)
      return false;

   // array start
   *outArrStart = ctx->data;

   ctx->data += *outElemNum;
   ctx->length -= *outElemNum;

   return true;
}

/**
 * Note: adds padding to achieve 4 byte alignment
 *
 * @return 0 on error (e.g. strLen is greater than bufLen), used buffer size otherwise
 */
void Serialization_serializeStrAlign4(SerializeCtx* ctx, unsigned strLen, const char* strStart)
{
   // write length field
   Serialization_serializeUInt(ctx, strLen);

   // write raw string
   Serialization_serializeBlock(ctx, strStart, strLen);

   // write termination char
   Serialization_serializeChar(ctx, 0);

   // align to 4b length
   if( (strLen + 1) % 4)
      Serialization_serializeBlock(ctx, "\0\0\0", 4 - (strLen + 1) % 4);
}

/**
 * @return false on error (e.g. strLen is greater than bufLen)
 */
bool Serialization_deserializeStrAlign4(DeserializeCtx* ctx,
   unsigned* outStrLen, const char** outStrStart)
{
   const char* const bufAtStart = ctx->data;
   unsigned padding;

   // length field
   if(unlikely(!Serialization_deserializeUInt(ctx, outStrLen) ) )
      return false;

   // check length and terminating zero
   if(unlikely( (ctx->length < *outStrLen + 1) || ( (ctx->data)[*outStrLen] != 0) ) )
      return false;

   // string start
   *outStrStart = ctx->data;
   ctx->data += *outStrLen + 1;
   ctx->length -= *outStrLen + 1;

   padding = (ctx->data - bufAtStart) % 4;
   if(padding != 0)
   {
      if(unlikely(ctx->length < padding) )
         return false;

      ctx->data += 4 - padding;
      ctx->length -= 4 - padding;
   }

   return true;
}

/**
 * Serialization of a NicList.
 *
 * note: 4 byte aligned.
 *
 * @return 0 on error, used buffer size otherwise
 */
void Serialization_serializeNicList(SerializeCtx* ctx, NicAddressList* nicList)
{
   unsigned nicListSize = NicAddressList_length(nicList);
   NicAddressListIter iter;
   unsigned i;

   // elem count info field
   Serialization_serializeUInt(ctx, nicListSize);

   // serialize each element of the nicList
   NicAddressListIter_init(&iter, nicList);

   for(i=0; i < nicListSize; i++, NicAddressListIter_next(&iter) )
   {
      NicAddress* nicAddr = NicAddressListIter_value(&iter);
      const size_t minNameSize = MIN(sizeof(nicAddr->name), SERIALIZATION_NICLISTELEM_NAME_SIZE);

      { // ipAddress
         Serialization_serializeBlock(ctx, &nicAddr->ipAddr.s_addr, sizeof(nicAddr->ipAddr.s_addr) );
      }

      { // name
         Serialization_serializeBlock(ctx, nicAddr->name, minNameSize);
      }

      {  // nicType
         Serialization_serializeBlock(ctx, &nicAddr->nicType, sizeof(uint8_t) );
      }

      {  // 3 bytes padding (for 4 byte alignment)
         Serialization_serializeBlock(ctx, "\0\0", 3);
      }
   }
}

/**
 * Pre-processes a serialized NicList for deserialization via deserializeNicList().
 *
 * @return false on error or inconsistency
 */
bool Serialization_deserializeNicListPreprocess(DeserializeCtx* ctx, RawList* outList)
{
   if(!Serialization_deserializeUInt(ctx, &outList->elemCount) )
      return false;

   outList->data = ctx->data;
   outList->length = outList->elemCount * SERIALIZATION_NICLISTELEM_SIZE;

   if(unlikely(ctx->length < outList->length) )
      return false;

   ctx->data += outList->length;
   ctx->length -= outList->length;

   return true;
}

/**
 * Deserializes a NicList.
 * (requires pre-processing via nicListBufToMsg() )
 */
void Serialization_deserializeNicList(const RawList* inList, NicAddressList* outNicList)
{
   const char* currentNicListPos = inList->data;
   unsigned i;


   for(i=0; i < inList->elemCount; i++)
   {
      NicAddress* nicAddr = os_kmalloc(sizeof(NicAddress) );
      const size_t minNameSize = MIN(sizeof(nicAddr->name), SERIALIZATION_NICLISTELEM_NAME_SIZE);
      memset(nicAddr, 0, sizeof(NicAddress) ); // clear unused fields

      { // ipAddress
         nicAddr->ipAddr.s_addr = get_unaligned((unsigned*)currentNicListPos);
         currentNicListPos += 4;
      }

      { // name
         memcpy(&nicAddr->name, currentNicListPos, minNameSize);
         (nicAddr->name)[minNameSize-1] = 0; // make sure the string is zero-terminated

         currentNicListPos += SERIALIZATION_NICLISTELEM_NAME_SIZE;
      }

      { // nicType
         nicAddr->nicType = (NicAddrType_t) get_unaligned((char*)currentNicListPos);
         currentNicListPos += 1;
      }

      { // 3 bytes padding (for 4 byte alignment)
         currentNicListPos += 3;
      }


      NicAddressList_append(outNicList, nicAddr);
   }

}

/**
 * Pre-processes a serialized NodeList for deserialization via deserializeNodeList().
 *
 * @return false on error or inconsistency
 */
bool Serialization_deserializeNodeListPreprocess(DeserializeCtx* ctx, RawList* outList)
{
   unsigned i;

   if(!Serialization_deserializeUInt(ctx, &outList->elemCount) )
      return false;

   // store start pos for later deserialize()

   outList->data = ctx->data;

   for(i=0; i < outList->elemCount; i++)
   {
      {// nodeID
         unsigned nodeIDLen = 0;
         const char* nodeID = NULL;

         if(unlikely(!Serialization_deserializeStr(ctx, &nodeIDLen, &nodeID) ) )
            return false;
      }

      {// nicList (4b aligned)
         RawList nicList;

         if(unlikely(!Serialization_deserializeNicListPreprocess(ctx, &nicList) ) )
            return false;
      }

      {// nodeNumID
         NumNodeID nodeNumID = {0};

         if(unlikely(!NumNodeID_deserialize(ctx, &nodeNumID) ) )
            return false;
      }

      {// portUDP
         uint16_t portUDP = 0;

         if(unlikely(!Serialization_deserializeUShort(ctx, &portUDP) ) )
            return false;
      }

      {// portTCP
         uint16_t portTCP = 0;

         if(unlikely(!Serialization_deserializeUShort(ctx, &portTCP) ) )
            return false;
      }

      {// nodeType
         char nodeType = 0;

         if(unlikely(!Serialization_deserializeChar(ctx, &nodeType) ) )
            return false;
      }
   }

   return true;
}

/**
 * Deserializes a NodeList.
 * (requires pre-processing)
 *
 * Note: Nodes will be constructed and need to be deleted later
 */
void Serialization_deserializeNodeList(App* app, const RawList* inList, NodeList* outNodeList)
{
   unsigned i;

   DeserializeCtx ctx = {
      .data = inList->data,
      .length = -1,
   };

   for(i=0; i < inList->elemCount; i++)
   {
      NicAddressList nicList;

      const char* nodeID = NULL;
      NumNodeID nodeNumID = (NumNodeID){0};
      uint16_t portUDP = 0;
      uint16_t portTCP = 0;
      char nodeType = 0;


      NicAddressList_init(&nicList);


      {// nodeID
         unsigned nodeIDLen = 0;

         Serialization_deserializeStr(&ctx, &nodeIDLen, &nodeID);
      }

      {// nicList (4b aligned)
         RawList rawNicList;

         Serialization_deserializeNicListPreprocess(&ctx, &rawNicList);
         Serialization_deserializeNicList(&rawNicList, &nicList);
      }

      // nodeNumID
      NumNodeID_deserialize(&ctx, &nodeNumID);

      // portUDP
      Serialization_deserializeUShort(&ctx, &portUDP);

      // portTCP
      Serialization_deserializeUShort(&ctx, &portTCP);

      // nodeType
      Serialization_deserializeChar(&ctx, &nodeType);

      {// construct node
         Node* node;
         App_lockNicList(app);
         node = Node_construct(app, nodeID, nodeNumID, portUDP, portTCP, &nicList,
            nodeType == NODETYPE_Meta || nodeType == NODETYPE_Storage? App_getLocalRDMANicListLocked(app) : NULL);
         App_unlockNicList(app);

         Node_setNodeType(node, (NodeType)nodeType);

         // append node to outList
         NodeList_append(outNodeList, node);
      }

      // cleanup
      ListTk_kfreeNicAddressListElems(&nicList);
      NicAddressList_uninit(&nicList);
   }

}

/**
 * Serialization of a StrCpyList.
 *
 * Note: We keep the serialiazation format compatible with string vec serialization
 */
void Serialization_serializeStrCpyList(SerializeCtx* ctx, StrCpyList* list)
{
   SerializeCtx startCtx = *ctx;
   StrCpyListIter iter;

   size_t listSize = StrCpyList_length(list);
   size_t i;

   // totalBufLen info field
   Serialization_serializeUInt(ctx, 0);

   // elem count info field
   Serialization_serializeUInt(ctx, listSize);

   // store each element of the list as a raw zero-terminated string
   StrCpyListIter_init(&iter, list);

   for(i=0; i < listSize; i++, StrCpyListIter_next(&iter) )
   {
      char* currentElem = StrCpyListIter_value(&iter);
      size_t serialElemLen = strlen(currentElem) + 1; // +1 for the terminating zero

      Serialization_serializeBlock(ctx, currentElem, serialElemLen);
   }

   // fix totalBufLen info field
   Serialization_serializeUInt(&startCtx, ctx->length - startCtx.length);
}

/**
 * Pre-processes a serialized StrCpyList.
 *
 * @return false on error or inconsistency
 */
bool Serialization_deserializeStrCpyListPreprocess(DeserializeCtx* ctx, RawList* outList)
{
   DeserializeCtx outCtx;

   if(!__Serialization_deserializeNestedField(ctx, &outCtx) )
      return false;

   // elem count field
   if(unlikely(!Serialization_deserializeUInt(&outCtx, &outList->elemCount) ) )
      return false;

   if(unlikely(outList->elemCount && ( outCtx.data[outCtx.length - 1] != 0) ) )
      return false;

   outList->data = outCtx.data;
   outList->length = outCtx.length;

   return true;
}

/**
 * Deserializes a StrCpyList.
 * (requires pre-processing)
 *
 * @return false on error or inconsistency
 */
bool Serialization_deserializeStrCpyList(const RawList* inList, StrCpyList* outList)
{
   const char* listStart = inList->data;
   unsigned listBufLen = inList->length;
   unsigned i;

   // read each list element as a raw zero-terminated string
   // and make sure that we do not read beyond the specified end position

   for(i = 0; (i < inList->elemCount) && (listBufLen > 0); i++)
   {
      size_t elemLen = strlen(listStart);

      StrCpyList_append(outList, listStart);

      listStart += elemLen + 1; // +1 for the terminating zero
      listBufLen -= elemLen + 1; // +1 for the terminating zero
   }

   return i == inList->elemCount;
}

/**
 * Serialization of a StrCpyVec.
 *
 * Note: We keep the serialiazation format compatible with string list serialization
 */
void Serialization_serializeStrCpyVec(SerializeCtx* ctx, StrCpyVec* vec)
{
   Serialization_serializeStrCpyList(ctx, &vec->strCpyList);
}

/**
 * Pre-processes a serialized StrCpyVec.
 *
 * Note: We keep the serialization format compatible to the string list format.
 *
 * @return false on error or inconsistency
 */
bool Serialization_deserializeStrCpyVecPreprocess(DeserializeCtx* ctx, RawList* outList)
{
   // serialization format is 100% compatible to the list version, so we can just use the list
      // version code here...

   return Serialization_deserializeStrCpyListPreprocess(ctx, outList);
}

/**
 * Deserializes a StrCpyVec.
 * (requires pre-processing)
 *
 * @return false on error or inconsistency
 */
bool Serialization_deserializeStrCpyVec(const RawList* inList, StrCpyVec* outVec)
{
   const char* listStart = inList->data;
   unsigned listBufLen = inList->length;
   unsigned i;

   // read each list element as a raw zero-terminated string
   // and make sure that we do not read beyond the specified end position

   for(i = 0; (i < inList->elemCount) && (listBufLen > 0); i++)
   {
      size_t elemLen = strlen(listStart);

      StrCpyVec_append(outVec, listStart);

      listStart += elemLen + 1; // +1 for the terminating zero
      listBufLen -= elemLen + 1; // +1 for the terminating zero
   }

   return i == inList->elemCount;
}

/**
 * Pre-processes a serialized UInt8List().
 *
 * @return false on error or inconsistency
 */
bool Serialization_deserializeUInt8ListPreprocess(DeserializeCtx* ctx, RawList* outList)
{
   DeserializeCtx outCtx;

   if(!__Serialization_deserializeNestedField(ctx, &outCtx) )
      return false;

   // elem count field
   if(unlikely(!Serialization_deserializeUInt(&outCtx, &outList->elemCount) ) )
      return false;

   if(outCtx.length != outList->elemCount * Serialization_serialLenUInt8() )
      return false;

   outList->data = outCtx.data;
   outList->length = outCtx.length;

   return true;
}

/**
 * Deserializes a UInt8List.
 * (requires pre-processing)
 *
 * @return false on error or inconsistency
 */
bool Serialization_deserializeUInt8List(const RawList* inList, UInt8List* outList)
{
   DeserializeCtx ctx = { inList->data, inList->length };
   unsigned i;

   // read each list element
   for(i=0; i < inList->elemCount; i++)
   {
      uint8_t value;

      if(!Serialization_deserializeUInt8(&ctx, &value) )
         return false;

      UInt8List_append(outList, value);
   }

   return true;
}

/**
 * Pre-processes a serialized UInt8Vec().
 *
 * @return false on error or inconsistency
 */
bool Serialization_deserializeUInt8VecPreprocess(DeserializeCtx* ctx, RawList* outList)
{
   // serialization format is 100% compatible to the list version, so we can just use the list
      // version code here...

   return Serialization_deserializeUInt8ListPreprocess(ctx, outList);
}

/**
 * Deserializes a UInt8Vec.
 * (requires pre-processing)
 *
 * @return false on error or inconsistency
 */
bool Serialization_deserializeUInt8Vec(const RawList* inList, UInt8Vec* outVec)
{
   DeserializeCtx ctx = { inList->data, inList->length };
   unsigned i;

   // read each list element
   for(i=0; i < inList->elemCount; i++)
   {
      uint8_t value;

      if(!Serialization_deserializeUInt8(&ctx, &value) )
         return false;

      UInt8Vec_append(outVec, value);
   }

   return true;
}


/**
 * Serialization of a UInt16List.
 *
 * Note: We keep the serialization format compatible with int vec serialization
 */
void Serialization_serializeUInt16List(SerializeCtx* ctx, UInt16List* list)
{
   SerializeCtx startCtx = *ctx;
   UInt16ListIter iter;
   unsigned i;

   unsigned listSize = UInt16List_length(list);

   // totalBufLen info field
   Serialization_serializeUInt(ctx, 0);

   // elem count info field
   Serialization_serializeUInt(ctx, listSize);

   // store each element of the list
   UInt16ListIter_init(&iter, list);

   for(i=0; i < listSize; i++, UInt16ListIter_next(&iter) )
   {
      uint16_t currentValue = UInt16ListIter_value(&iter);

      Serialization_serializeUShort(ctx, currentValue);
   }

   // fix totalBufLen info field
   Serialization_serializeUInt(&startCtx, ctx->length - startCtx.length);
}

/**
 * Pre-processes a serialized UInt16List().
 *
 * @return false on error or inconsistency
 */
bool Serialization_deserializeUInt16ListPreprocess(DeserializeCtx* ctx, RawList* outList)
{
   DeserializeCtx outCtx;

   if(!__Serialization_deserializeNestedField(ctx, &outCtx) )
      return false;

   // elem count field
   if(unlikely(!Serialization_deserializeUInt(&outCtx, &outList->elemCount) ) )
      return false;

   if(outCtx.length != outList->elemCount * Serialization_serialLenUShort() )
      return false;

   outList->data = outCtx.data;
   outList->length = outCtx.length;

   return true;
}

/**
 * Deserializes a UInt16List.
 * (requires pre-processing)
 *
 * @return false on error or inconsistency
 */
bool Serialization_deserializeUInt16List(const RawList* inList, UInt16List* outList)
{
   DeserializeCtx ctx = { inList->data, inList->length };
   unsigned i;

   // read each list element
   for(i=0; i < inList->elemCount; i++)
   {
      uint16_t value;

      if(!Serialization_deserializeUShort(&ctx, &value) )
         return false;

      UInt16List_append(outList, value);
   }

   return true;
}

/**
 * Note: We keep the serialiazation format compatible with string list serialization
 */
void Serialization_serializeUInt16Vec(SerializeCtx* ctx, UInt16Vec* vec)
{
   // UInt16Vecs are derived from UInt16Lists, so we can just do a cast here and use the list
      // version code...

   Serialization_serializeUInt16List(ctx, (UInt16List*)vec);
}

/**
 * Pre-processes a serialized UInt16Vec().
 *
 * @return false on error or inconsistency
 */
bool Serialization_deserializeUInt16VecPreprocess(DeserializeCtx* ctx, RawList* outList)
{
   // serialization format is 100% compatible to the list version, so we can just use the list
      // version code here...

   return Serialization_deserializeUInt16ListPreprocess(ctx, outList);
}

/**
 * Deserializes a UInt16Vec.
 * (requires pre-processing)
 *
 * @return false on error or inconsistency
 */
bool Serialization_deserializeUInt16Vec(const RawList* inList, UInt16Vec* outVec)
{
   DeserializeCtx ctx = { inList->data, inList->length };
   unsigned i;

   // read each list element
   for(i=0; i < inList->elemCount; i++)
   {
      uint16_t value;

      if(!Serialization_deserializeUShort(&ctx, &value) )
         return false;

      UInt16Vec_append(outVec, value);
   }

   return true;
}

/**
 * Serialization of a Int64CpyList.
 *
 * Note: We keep the serialization format compatible with int vec serialization
 */
void Serialization_serializeInt64CpyList(SerializeCtx* ctx, Int64CpyList* list)
{
   SerializeCtx startCtx = *ctx;
   Int64CpyListIter iter;
   unsigned i;

   unsigned listSize = Int64CpyList_length(list);

   // totalBufLen info field
   Serialization_serializeUInt(ctx, 0);

   // elem count info field
   Serialization_serializeUInt(ctx, listSize);

   // store each element of the list
   Int64CpyListIter_init(&iter, list);

   for(i=0; i < listSize; i++, Int64CpyListIter_next(&iter) )
   {
      int64_t currentValue = Int64CpyListIter_value(&iter);

      Serialization_serializeInt64(ctx, currentValue);
   }

   // fix totalBufLen info field
   Serialization_serializeUInt(&startCtx, ctx->length - startCtx.length);
}

/**
 * Pre-processes a serialized Int64CpyList().
 *
 * @return false on error or inconsistency
 */
bool Serialization_deserializeInt64CpyListPreprocess(DeserializeCtx* ctx, RawList* outList)
{
   DeserializeCtx outCtx;

   if(!__Serialization_deserializeNestedField(ctx, &outCtx) )
      return false;

   // elem count field
   if(unlikely(!Serialization_deserializeUInt(&outCtx, &outList->elemCount) ) )
      return false;

   if(outCtx.length != (outList->elemCount * Serialization_serialLenInt64() ) )
      return false;

   outList->data = outCtx.data;
   outList->length = outCtx.length;

   return true;
}

/**
 * Deserializes a Int64CpyList.
 * (requires pre-processing)
 *
 * @return false on error or inconsistency
 */
bool Serialization_deserializeInt64CpyList(const RawList* inList, Int64CpyList* outList)
{
   DeserializeCtx ctx = { inList->data, inList->length };
   unsigned i;

   // read each list element
   for(i=0; i < inList->elemCount; i++)
   {
      int64_t value;

      if(!Serialization_deserializeInt64(&ctx, &value) )
         return false;

      Int64CpyList_append(outList, value);
   }

   return true;
}

/**
 * Note: We keep the serialiazation format compatible with string list serialization
 */
void Serialization_serializeInt64CpyVec(SerializeCtx* ctx, Int64CpyVec* vec)
{
   // Int64CpyVecs are derived from Int64CpyLists, so we can just do a cast here and use the list
      // version code...

   Serialization_serializeInt64CpyList(ctx, (Int64CpyList*)vec);
}

/**
 * Pre-processes a serialized Int64CpyVec().
 *
 * @return false on error or inconsistency
 */
bool Serialization_deserializeInt64CpyVecPreprocess(DeserializeCtx* ctx, RawList* outList)
{
   // serialization format is 100% compatible to the list version, so we can just use the list
      // version code here...

   return Serialization_deserializeInt64CpyListPreprocess(ctx, outList);
}

/**
 * Deserializes a Int64CpyVec.
 * (requires pre-processing)
 *
 * @return false on error or inconsistency
 */
bool Serialization_deserializeInt64CpyVec(const RawList* inList, Int64CpyVec* outVec)
{
   DeserializeCtx ctx = { inList->data, inList->length };
   unsigned i;

   // read each list element
   for(i=0; i < inList->elemCount; i++)
   {
      int64_t value;

      if(!Serialization_deserializeInt64(&ctx, &value) )
         return false;

      Int64CpyVec_append(outVec, value);
   }

   return true;
}
