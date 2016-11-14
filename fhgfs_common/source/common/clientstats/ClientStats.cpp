#include <common/nodes/OpCounter.h>
#include <common/system/System.h>
#include <common/toolkit/SocketTk.h>
#include "ClientStats.h"
#include "ClientStatsDiff.h"

#include <iostream>


/**
 * Basic parsing and sanity checks of the vector we got from the server
 */
bool ClientStats::parseStatsVector(UInt64Vector* newVec)
{
   LogContext log = LogContext("ClientStats (parse stats)");
   // first element of the vector are the number OPs

   // sanity checks first

   int minVecSize = STATS_VEC_RESERVED_ELEMENTS + 1;
   int vecSize = newVec->size();
   if (vecSize < minVecSize)
   {
      std::string logMessage("Stats vector too small. Server restarted and no stats yet? "
         "length: " + StringTk::intToStr(vecSize) + "; min expected: " +
         StringTk::intToStr(minVecSize) );

      log.logErr(logMessage);
      std::cerr << logMessage << std::endl;

      return false;
   }


   if (!this->firstTransfer)
   {
      uint64_t newNumOps = newVec->at(NODEOPS_POS_NUMOPS);

      if (this->numOps != newNumOps)
      {
         std::string logMessage("Bug: Mismatch of numOps in between transfers (" +
            StringTk::uint64ToStr(this->numOps) + " vs. " + StringTk::uint64ToStr(newNumOps) +
            "). Aborting.");

         log.logErr(logMessage);
         std::cerr << logMessage << std::endl;

         return false;
      }
   }
   else
   {
      // first transfer
      this->numOps = newVec->at(NODEOPS_POS_NUMOPS);
      this->firstTransfer = false;
   }

   // next element tells us if the server has more data
   this->moreDataVal = newVec->at(NODE_OPS_POS_MORE_DATA);

   int layout_version = newVec->at(NODE_OPS_POS_LAYOUT_VERSION);
   if (layout_version != OPCOUNTER_VEC_LAYOUT_VERS)
   {
      std::string logMessage1("Incompatible data mismatch detected. Please update server and "
         "beegfs-utils to the same version!");
      std::string logMessage2("layout version: " + StringTk::intToStr(layout_version) + " vs. "
         + StringTk::intToStr(OPCOUNTER_VEC_LAYOUT_VERS) );

      log.logErr(logMessage1 + logMessage2);
      log.logErr(logMessage2);

      std::cerr << logMessage1 << std::endl;
      std::cerr << logMessage2 << std::endl;
      // hmm, to abort or not to abort? Let's try to continue for now.
   }

   this->statsAdd(newVec); // now the real data parsing

   return true;
}

/**
 * Parse the stats data we got from the server and put the data (op-counter)
 * into our own (per-IP) vectors.
 */
bool ClientStats::statsAdd(UInt64Vector* serverVec)
{
   LogContext log = LogContext("ClientStats (add stats)");

   UInt64VectorIter iter = serverVec->begin() + NODE_OPS_POS_FIRSTDATAELEMENT;

   // numOps + numMetaElements; numMeta = FirstCounter - 1; but no -1 as we start to count with 0
   uint64_t elementsPerIP = this->numOps + OPCOUNTERVECTOR_POS_FIRSTCOUNTER;

   do
   {
      uint64_t ip = *iter; // first element of the vector

      // update the cookieIP for each vector we got and last value is then the real cookie
      this->cookieIP = ip;

      UInt64Vector* currentIPOpVec = this->getVectorFromMap(&this->currentVectorMap, ip);

      if (!currentIPOpVec)
      {
         // IP not found yet. We need to add it.
         newStatsData(iter, iter + elementsPerIP, elementsPerIP);

         UInt64Vector* testVec = this->getVectorFromMap(&this->currentVectorMap, ip);
         if (testVec->empty() )
         {
            struct in_addr in_addr = { (in_addr_t)ip };

            std::string logMessage("BUG: Vector in map is unexpectedly empty!: " +
               Socket::ipaddrToStr(&in_addr));

            log.logErr(logMessage);
            std::cerr << logMessage << std::endl; // print the IP#
         }

         // adding stats for this IP done, next IP please
         iter += elementsPerIP;

         continue;
      }

      if (currentIPOpVec->empty())
      {
         struct in_addr in_addr = { (in_addr_t)ip };

         std::string logMessage("BUG: Empty vector found for IP " + Socket::ipaddrToStr(&in_addr));

         log.logErr(logMessage);
         std::cerr << logMessage << std::endl; // print the IP#

         return false;
      }

      if (currentIPOpVec->front() != ip)
      {
         struct in_addr in_addr = { (in_addr_t)ip };
         struct in_addr current_addr = { (in_addr_t)currentIPOpVec->front() };

         std::string logMessage("BUG: IP mismatch! Expected: " + Socket::ipaddrToStr(&in_addr) +
            " found: " + Socket::ipaddrToStr(&current_addr) );

         log.logErr(logMessage);
         std::cerr << logMessage << std::endl;

         return false;
      }

      // walk over IP and other meta-fields on-to first counter (remember: 0-field = IP)
      iter += OPCOUNTERVECTOR_POS_FIRSTCOUNTER;

      uint64_t currentSize = currentIPOpVec->size();
      if (currentSize < elementsPerIP)
      {
         /* the saved vector is smaller than the new vector, possibly due to different FhGFS
          * versions on different servers */
         currentIPOpVec->resize(elementsPerIP, 0); // resize the saved vector and init with zero
      }

      for (uint64_t ia = OPCOUNTERVECTOR_POS_FIRSTCOUNTER; ia < elementsPerIP; ia++)
      {

         if (iter == serverVec->end())
         {
            std::string logMessage("BUG: Vector smaller than expected.");

            log.logErr(logMessage);
            std::cerr << logMessage << std::endl;
            return false;
         }

         currentIPOpVec->at(ia) += *iter; // add values from different servers

         iter++;
      }

      /* Note on termination condition:
       * iter != serverVec->end() -> should be sufficient, but we have another sanity check
       * if something should go wrong:
       * iter - serverVec->begin() -> the current index (position)
       * iter - serverVec->begin() + elementsPerIP - 1 -> the index where it would end at the next
       * round */
   } while (iter != serverVec->end() &&
      iter - serverVec->begin() + elementsPerIP - 1 < serverVec->size());

   if ((int) serverVec->size() != (iter - serverVec->begin()))
   {
      std::string logMessage1("Unexpected: Iterator not at vector end: " +
         StringTk::intToStr(iter - serverVec->begin() ) +
         " vs. " +
         StringTk::intToStr(serverVec->size() ) + ".");
      std::string logMessage2("Server vs. beegfs-utils version mismatch?");

      log.logErr(logMessage1 + logMessage2);
      std::cerr << logMessage1 << std::endl;
      std::cerr << logMessage2 << std::endl;
   }

   return true;
}

/**
 * Create a new vector and add the data from the server
 *
 * @param vecStart - vector iterator start
 * @param vecEnd   - vector iterator stop
 * @param nElem    - just for verification of the number of elements
 */
void ClientStats::newStatsData(UInt64VectorIter vecStart, UInt64VectorIter vecEnd,
   uint64_t nElem)
{
   UInt64VectorIter iter = vecStart;
   UInt64Vector* IPOpVec = new UInt64Vector;

   while (iter != vecEnd)
   {
      IPOpVec->push_back(*iter);
      iter++;
   }

   if (IPOpVec->size() != nElem)
   {
      LogContext log = LogContext("ClientStats (new stats)");

      std::string logMessage("BUG: Unexpected vector length: " +
         StringTk::intToStr(IPOpVec->size() ) + "!=" + StringTk::intToStr(nElem) );

      log.logErr(logMessage);
      std::cerr << logMessage << std::endl;

      delete(IPOpVec);
      return;
   }

   this->addToCurrentVectorMap(IPOpVec);
}

/**
 * Add the given vector to our currentVectorMap. First element of the vector is the IP
 */
bool ClientStats::addToCurrentVectorMap(UInt64Vector* vec)
{
   uint64_t node = vec->at(OPCOUNTERVECTOR_POS_IP);

   if (this->currentVectorMap.find(node) != this->currentVectorMap.end() )
   {
      LogContext log = LogContext("ClientStats (add to vector)");
      std::string logMessage("BUG: Found existing IP in the map!");

      log.logErr(logMessage);
      std::cerr << logMessage << std::endl;
      return false;
   }

   this->currentVectorMap.insert(ClientNodeOpVectorMap::value_type(node, vec));

   return true;
}

/**
 * Print the client operation stats per IP/userID.
 */
void ClientStats::printStats(uint64_t elapsedSecs)
{
   ClientStatsDiff statsDiff(&this->currentVectorMap, &this->oldVectorMap);
   UInt64VectorList* diffVectorList = statsDiff.getDiffVectorList();

   UInt64VectorListIter listIter = diffVectorList->begin();

   std::cout << std::endl; // blank line

   std::cout << "====== " << elapsedSecs << " s ======" << std::endl;

   UInt64Vector* sumVec = statsDiff.getSumVector();
   // sumVec size might be zero, if there was no diff-vector
   if( (sumVec->size() >= OPCOUNTERVECTOR_POS_FIRSTCOUNTER + 1) &&
       checkPrintIPStats(sumVec) &&
       this->cfgOptions->filter.empty() )
   {
      std::cout << "Sum:            ";
      this->printCounters(sumVec, elapsedSecs);
   }

   // Loop over the List of vectors, so over IPs
   unsigned lineCounter = 0;
   while( (listIter != diffVectorList->end() ) &&
          (lineCounter < this->cfgOptions->maxLines) )
   {
      UInt64Vector* statsVec = *listIter;

      bool printThisIP = checkPrintIPStats(statsVec);
      if (!printThisIP)
      {
         listIter++; // No need to print this IP, continue with next vector
         continue;
      }

      UInt64VectorIter vectorIter = statsVec->begin(); // reset to the beginning
      uint64_t ip = *vectorIter; // IP address or userID, depending on stats mode

      if(!this->cfgOptions->perUser)
      { // print IP in first column
         struct in_addr in_addr = { (in_addr_t)ip };
         std::string hostname;

         if(this->cfgOptions->showNames)
            hostname = SocketTk::getHostnameFromIP(&in_addr, false, false);

         // with showNames set, hostname might be empty here if lookup failed
         if(hostname.empty() )
            hostname = Socket::ipaddrToStr(&in_addr); // print the IP address

         if(!this->cfgOptions->filter.empty() && (this->cfgOptions->filter != hostname) )
            goto skip_line;

         std::cout << std::setw(16) << std::setiosflags(std::ios::left); // width formatting

         std::cout << hostname; // print hostname
      }
      else
      { // print userID in first column

         std::string username;

         if(this->cfgOptions->showNames && ( (int)ip == NETMSG_DEFAULT_USERID) )
            username = "<unknown>";

         if(this->cfgOptions->showNames && username.empty() )
            username = System::getUserNameFromUID(ip);

         // note: parse to signed int, because NETMSG_DEFAULT_USERID(~0 => -1) looks better this way
         if(username.empty() )
            username = StringTk::intToStr(ip); // print numeric userID

         if(!this->cfgOptions->filter.empty() && (this->cfgOptions->filter != username) )
            goto skip_line;

         std::cout << std::setw(16) << std::setiosflags(std::ios::left); // width formatting

         std::cout << username; // print username
      }

      std::cout << std::setw(1); // set formatting back to minimal width

      printCounters(statsVec, elapsedSecs);

      lineCounter++;

   skip_line:
      listIter++; // nextIP
   }
}

/**
 * Just print the real vector counters to stdout
 */
void ClientStats::printCounters(UInt64Vector* statsVec, uint64_t elapsedSecs)
{
   if (statsVec->size() < OPCOUNTERVECTOR_POS_FIRSTCOUNTER + 1)
      return; // nothing to print

   UInt64VectorIter vectorIter = statsVec->begin();
   vectorIter += OPCOUNTERVECTOR_POS_IP + 1; // walk over IP

   int opNum = 0;

   while (vectorIter != statsVec->end()) // loop over the op-counters
   {
      double value;
      bool isRWBytesOp = getIsRWBytesOp(opNum);
      std::string unitPrefix = valueToCfgUnit(opNum, *vectorIter, &value);
      std::string perSecStr;

      if (!this->cfgOptions->perInterval && elapsedSecs && isRWBytesOp)
      {
         value /= this->cfgOptions->intervalSecs; // value per second
         perSecStr = "/s";
      }

      if ( (value > 0) || this->cfgOptions->allStats) // in dynamic mode we only print values > 0
      {
         printf("  "); // space between elements

         if (!unitPrefix.empty() )
            printf("%.3lf ", value);
         else
            printf("%.0lf ", value);

         printf("[%s%s%s]", unitPrefix.c_str(), opNumToStr(opNum).c_str(), perSecStr.c_str() );
      }

      vectorIter++;
      opNum++;
   }

   std::cout << std::endl;
}

/**
 * Find out whether the given operation is a read/write bytes field.
 *
 * @return true if this is opNum is a read or write bytes field.
 */
bool ClientStats::getIsRWBytesOp(int opNum) const
{
   if(cfgOptions->nodeType == NODETYPE_Storage)
   { // find out is this is a read/write bytes op field
      if( (opNum == StorageOpCounter_READBYTES) ||
            (opNum == StorageOpCounter_WRITEBYTES) )
         return true;
   }

   return false;
}

/*
 * Convert the value from bytes to a human readable value, e.g. MiB
 *
 * Note: So far this is only supposed to be done for read/write bytes and not for the
 * the other op counters.
 *
 * @param OpNum     The opCounter type
 * @param inValue   The value we are supposed to convert
 * @param outValue  The converted value
 * @return          The unit we converted the outValue to, empty ("") if outValue == inValue
 */
std::string ClientStats::valueToCfgUnit(int opNum, uint64_t inValue, double* outValue)
{
   *outValue = inValue;

   if(!getIsRWBytesOp(opNum) )
      return "";

   if (this->cfgOptions->unit == "MiB")
   {
      *outValue /= (double) (1024 * 1024);
      return "Mi";
   }

   return "";
}

/**
 * Check if we need to print the stats for the given vector
 */
 bool ClientStats::checkPrintIPStats(UInt64Vector* statsVec)
{
   UInt64VectorIter vectorIter = statsVec->begin();
   vectorIter += OPCOUNTERVECTOR_POS_FIRSTCOUNTER; // on-to first counter

   bool doPrint = false;

   // this loop is to check if any value is != 0. Vector values are the op-counters
   while (vectorIter != statsVec->end())
   {
      uint64_t value = *vectorIter;
      if (value > 0)
      {
         doPrint = true;
         break;
      }

      vectorIter++;
   }

   return doPrint;
}

/**
 * Find and return the vector corresponding to an IP or NULL if none
 */
UInt64Vector *ClientStats::getVectorFromMap(ClientNodeOpVectorMap *vectorMap, uint64_t IP)
{
   ClientNodeOpVectorMapIter iter = vectorMap->find(IP);
   if (iter == vectorMap->end() )
      return NULL; // node not found

   return iter->second;
}

/**
 * Remove all entries in the given vectorMap
 * @param vectorMap  - the ClientNodeOpVectorMap to operate on
 * @param doDelete   - delete the vectors in the map or not
 */
bool ClientStats::emptyVectorMap(ClientNodeOpVectorMap *vectorMap, bool doDelete)
{
   if (doDelete)
   {
      // Delete the vectors in the map
      for (ClientNodeOpVectorMapIter iter = vectorMap->begin(); iter != vectorMap->end(); iter++)
      {
         UInt64Vector* vector = iter->second;
         SAFE_DELETE(vector);
      }
   }

   vectorMap->clear();

   return true;
}

/**
 * Move entries from the currentVectorMap to the oldVectorMap and clear currentVectorMap.
 */
void ClientStats::currentToOldVectorMap(void)
{
   this->emptyVectorMap(&this->oldVectorMap, true); // first delete everything in the old map

   this->oldVectorMap.swap(this->currentVectorMap);
}

/**
 * Convert the given operation number into its correspondig meta or storage operation string
 * (human readable name)
 */
std::string ClientStats::opNumToStr(int opNum)
{
   if(cfgOptions->nodeType == NODETYPE_Meta)
      return metaOpNumToStr.opNumToStr(opNum);
   else
   if(cfgOptions->nodeType == NODETYPE_Storage)
         return storageOpNumToStr.opNumToStr(opNum);
   else
      return "<" + StringTk::intToStr(opNum) + ">"; // should never happen
}
