#include "StatsAdmon.h"

#include <common/clientstats/ClientStatsDiff.h>
#include <common/clientstats/ClientStatsHelper.h>



void StatsAdmon::addStatsToXml(TiXmlElement *rootElement, bool doUserStats)
{
   ClientStatsDiff statsDiff(&this->currentVectorMap, &this->oldVectorMap );
   UInt64VectorList* diffVectorList = statsDiff.getDiffVectorList();

   UInt64VectorListIter listIter = diffVectorList->begin();

   UInt64Vector* sumVec = statsDiff.getSumVector();
   // sumVec size might be zero, if there was no diff-vector
   if (sumVec->size() >= OPCOUNTERVECTOR_POS_FIRSTCOUNTER + 1 && this->checkPrintIPStats(sumVec))
   {
      TiXmlElement *sumElement = new TiXmlElement("sum");
      rootElement->LinkEndChild(sumElement);

      this->addCountersToElement(sumVec, sumElement);
   }

   TiXmlElement *hostsElement = new TiXmlElement("hosts");
   rootElement->LinkEndChild(hostsElement);

   // Loop over the List of vectors, so over IPs
   unsigned lineCounter = 0;
   while (listIter != diffVectorList->end() && lineCounter < this->cfgOptions->maxLines)
   {
      UInt64Vector* statsVec = *listIter;

      bool printThisIP = this->checkPrintIPStats(statsVec);
      if (!printThisIP)
      {
         listIter++; // No need to print this IP, continue with next vector
         continue;
      }

      UInt64VectorIter vectorIter = statsVec->begin(); // reset to the beginning
      uint64_t ip = *vectorIter;

      TiXmlElement *hostElement = new TiXmlElement("host");

      if(doUserStats)
      {
         hostElement->SetAttribute("ip", StringTk::intToStr(ip) );
         hostsElement->LinkEndChild(hostElement);
      }
      else
      {
         struct in_addr in_addr = { (in_addr_t)ip };
         hostElement->SetAttribute("ip", Socket::ipaddrToStr(&in_addr) );
         hostsElement->LinkEndChild(hostElement);
      }

      this->addCountersToElement(statsVec, hostElement);


      listIter++; // nextIP
      lineCounter++;
   }
}

void StatsAdmon::addCountersToElement(UInt64Vector* statsVec, TiXmlElement *xmlParent)
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

      if (!this->cfgOptions->perInterval && isRWBytesOp)
      {
         value /= this->cfgOptions->intervalSecs; // value per second
         perSecStr = "B/s";
      }

      if ( (value > 0) || this->cfgOptions->allStats) // in dynamic mode we only print values > 0
      {
         char buffer[30];
         if ( unitPrefix.empty() || (value == 0) )
            sprintf(buffer, "%.0lf", value);
         else
            sprintf(buffer, "%.3lf %s%s", value, unitPrefix.c_str(), perSecStr.c_str());

         TiXmlElement *opElement = new TiXmlElement(opNumToStr(opNum) );
         xmlParent->LinkEndChild(opElement);
         opElement->SetAttribute("id", StringTk::uintToStr(opNum) );
         opElement->LinkEndChild(new TiXmlText(buffer) );
      }

      vectorIter++;
      opNum++;
   }
}
