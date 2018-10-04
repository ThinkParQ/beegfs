#include <common/net/message/storage/quota/SetExceededQuotaRespMsg.h>
#include <common/toolkit/MessagingTk.h>

#include "SetExceededQuotaWork.h"

void SetExceededQuotaWork::process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen)
{
   LogContext log("SetQuotaMsg incoming");

   SetExceededQuotaMsg msg(storagePoolId, idType, limitType);
   prepareMessage(this->messageNumber, &msg);

   SetExceededQuotaRespMsg* respMsgCast;

   const auto respMsg = MessagingTk::requestResponse(storageNode, msg,
         NETMSGTYPE_SetExceededQuotaResp);
   if (!respMsg)
   {
      log.logErr("Failed to communicate with node: " + storageNode.getTypedNodeID() );

      *this->result = 0;
      this->counter->incCount();

      return;
   }

   respMsgCast = (SetExceededQuotaRespMsg*)respMsg.get();
   int resultValue = respMsgCast->getValue();

   if(resultValue != 0)
   {
      log.logErr("Failed to set exceeded quota (" + QuotaData::QuotaLimitTypeToString(
         this->limitType) + ", "+ QuotaData::QuotaDataTypeToString(this->idType) +
         ") on storage server: " + storageNode.getNodeIDWithTypeStr());

      *this->result = 0;
   }
   else
      *this->result = 1;

   this->counter->incCount();
}

/*
 * type dependent message preparations
 *
 * @param messageNumber the sequence number of the message
 * @param msg the message to send
 */
void SetExceededQuotaWork::prepareMessage(int messageNumber, SetExceededQuotaMsg* msg)
{
   int counter = 0;
   int startThisMessage = (messageNumber * SETEXCEEDEDQUOTAMSG_MAX_ID_COUNT);
   int startNextMessage = ((messageNumber + 1) * SETEXCEEDEDQUOTAMSG_MAX_ID_COUNT);

   for(UIntListIter iter = this->idList->begin(); iter != this->idList->end(); iter++)
   {
      if(counter < startThisMessage)
      {
         counter++;
         continue;
      }
      else
      if(counter >= startNextMessage)
         return;

      counter++;
      msg->getExceededQuotaIDs()->push_back(*iter);
   }
}
