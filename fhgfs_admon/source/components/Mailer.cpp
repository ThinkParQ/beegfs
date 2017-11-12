#include <app/config/RuntimeConfig.h>
#include <program/Program.h>
#include "Mailer.h"



/**
 * Adds a node to the list of broken nodes.
 *
 * @param nodeNumID The nodeNumID of the broken node.
 * @param nodeID THe ID/hostname/name of the broken node.
 * @param nodeType The NodeType of the broken node.
 * @return True on success, if the node is in the list or the node type is not supported false is
 *         returned.
 */
bool Mailer::addDownNode(NumNodeID nodeNumID, std::string nodeID, NodeType nodeType)
{
   std::string nodeTypeStr = "";
   DownNodeList *list = NULL;

   if(nodeType == NODETYPE_Meta)
   {
      list = &(this->downNodes.downMetaNodes);
      nodeTypeStr = "meta";
   }
   else if(nodeType == NODETYPE_Storage)
   {
      list = &(this->downNodes.downStorageNodes);
      nodeTypeStr = "storage";
   }
   else
   {
      return false;
   }

   for(DownNodeListIter iter = list->begin(); iter != list->end(); iter++)
   {
      if ( (*iter).nodeNumID == nodeNumID)
         return false;
   }

   DownNode downNode;
   downNode.nodeNumID = nodeNumID;
   downNode.nodeID = nodeID;
   downNode.downSince = time(NULL);
   downNode.eMailSent = false;
   list->push_back(downNode);

   // fire up a script if it is set in runtimeConfig
   RuntimeConfig *runtimeCfg = Program::getApp()->getRuntimeConfig();
   std::string scriptPath = runtimeCfg->getScriptPath();

   if(!scriptPath.empty() )
   {
      ExternalJobRunner *jobRunner = Program::getApp()->getJobRunner();

      Mutex mutex;
      SafeMutexLock mutexLock(&mutex);

      Job *job = jobRunner->addJob(scriptPath + " " + nodeTypeStr + " " + nodeID, &mutex);

      while (!job->finished)
         job->jobFinishedCond.wait(&mutex);

      mutexLock.unlock();

      jobRunner->delJob(job->id);
   }

   return true;
}

/**
 * The run methode of the mailer thread.
 */
void Mailer::run()
{
   unsigned intervalMS = Program::getApp()->getConfig()->getMailCheckIntervalTimeSec() * 1000;

   try
   {
      log.log(Log_DEBUG, "Component started.");
      registerSignalHandler();
      while (!getSelfTerminate() )
      {
         RuntimeConfig *runtimeCfg = Program::getApp()->getRuntimeConfig();
         if(runtimeCfg->getMailEnabled() )
         {
            notifyBackUpNodes();
            notifyDownNodes();
         }

         // do nothing but wait for the time of intervalMS
         if(PThread::waitForSelfTerminateOrder(intervalMS) )
         {
            break;
         }
      }
      log.log(Log_DEBUG, "Component stopped.");

   }
   catch (std::exception& e)
   {
      Program::getApp()->handleComponentException(e);
   }
}

/**
 * Notify the administrator/user by email about nodes which are broken.
 */
void Mailer::notifyDownNodes()
{
   RuntimeConfig *runtimeCfg = Program::getApp()->getRuntimeConfig();
   int minDownTimeSec = runtimeCfg->getMailMinDownTimeSec();
   int resendMailTimeMin = runtimeCfg->getMailResendMailTimeMin();
   bool notifiedNewDownNodes = false;
   bool mailSend = false;

   std::string msg = "Some Nodes in the BeeGFS System monitored by " +
      Program::getApp()->getLocalNode().getTypedNodeID() +" seem to be dead!\r\n\r\n";

   msg += "A list of these hosts follows:\r\n\r\nMeta Nodes:\r\n----------------\r\n\r\n";

   int64_t nowTime = time (NULL);
   for(DownNodeListIter iter = downNodes.downMetaNodes.begin();
      iter != downNodes.downMetaNodes.end(); iter++)
   {
      if ( (nowTime - (*iter).downSince) > minDownTimeSec)
      {
         msg += Mailer::getHumanReadableNodeID(*iter) +" / Down since " +
            StringTk::timespanToStr(nowTime - (*iter).downSince)+".\r\n";
         mailSend = true;

         if (!(*iter).eMailSent)
         {
            notifiedNewDownNodes = true;
            (*iter).eMailSent = true;
         }
      }
   }

   msg += "\r\n\r\n\r\nStorage Nodes:\r\n----------------\r\n\r\n";

   for(DownNodeListIter iter=downNodes.downStorageNodes.begin();
      iter!=downNodes.downStorageNodes.end(); iter++)
   {
      if ( (nowTime - (*iter).downSince) > minDownTimeSec)
      {
         msg += Mailer::getHumanReadableNodeID(*iter) +" / Down since " +
            StringTk::timespanToStr(nowTime - (*iter).downSince)+".\r\n";
         mailSend = true;

         if (!(*iter).eMailSent)
         {
            notifiedNewDownNodes = true;
            (*iter).eMailSent = true;
         }
      }
   }

   if (notifiedNewDownNodes ||
      (mailSend && ((nowTime - downNodes.lastMail) > (resendMailTimeMin * 60) ) ) )
   {
      log.log(Log_SPAM, "Sending eMail caused by down nodes.");
      std::string subject = "BeeGFS - Nodes Down";
      sendMail(subject, msg);

      downNodes.lastMail = nowTime;
   }
}

/**
 * Notify the administrator/user by email about nodes which are working again.
 */
void Mailer::notifyBackUpNodes()
{
   bool sendMail = false;

   std::string msg = "Some nodes in the BeeGFS System monitored by " +
      Program::getApp()->getLocalNode().getID() + " seem to be up again after a downtime!\r\n\r\n";
   msg += "A list of these hosts follows:\r\n\r\nMeta Nodes:\r\n----------------\r\n\r\n";

   int64_t nowTime = time (NULL);

   //actively poll for nodes which are reachable again
   NodeStoreMetaEx *metaNodeStore = Program::getApp()->getMetaNodes();
   DownNodeListIter iter=downNodes.downMetaNodes.begin();

   while (iter!=downNodes.downMetaNodes.end() )
   {
      auto node = std::static_pointer_cast<MetaNodeEx>(
            metaNodeStore->referenceNode( (*iter).nodeNumID) );
      if (node == NULL) // node seems to be removed from store
      {
            iter = downNodes.downMetaNodes.erase(iter);
      }
      else
      {
         if (node->getContent().isResponding)
         {
            msg += Mailer::getHumanReadableNodeID(*iter) + " - Was down for " +
               StringTk::timespanToStr(nowTime - (*iter).downSince)+".\r\n";
            iter = downNodes.downMetaNodes.erase(iter);
            sendMail = true;
            node->upAgain();
         }
         else
         {
            iter++;
         }
      }
   }

   msg += "\r\n\r\n\r\nStorage Nodes:\r\n----------------\r\n\r\n";

    NodeStoreStorageEx *storageNodeStore = Program::getApp()->getStorageNodes();
   iter = downNodes.downStorageNodes.begin();

    while (iter != downNodes.downStorageNodes.end() )
   {
      auto node = std::static_pointer_cast<StorageNodeEx>(
            storageNodeStore->referenceNode( (*iter).nodeNumID) );

      if (node->getContent().isResponding)
      {
         msg += Mailer::getHumanReadableNodeID( (*iter) ) +" - Was down for " +
            StringTk::timespanToStr(nowTime - (*iter).downSince)+".\r\n";
         iter = downNodes.downStorageNodes.erase(iter);
         sendMail = true;
         node->upAgain();
      }
      else
      {
         iter++;
      }
   }

    if (sendMail)
    {
       log.log(Log_SPAM,"Sending eMail caused by nodes which are up again.");
       std::string subject = "BeeGFS - Nodes Up again";
       Mailer::sendMail(subject, msg);
    }
}

/**
 * Sends an email.
 *
 * @param subject The subject of the email.
 * @param msg The message of the email.
 * @return 0 on success else -1
 */
int Mailer::sendMail(const std::string& subject, const std::string& msg)
{
   RuntimeConfig *runtimeCfg = Program::getApp()->getRuntimeConfig();

   const std::string& smtpServerName = runtimeCfg->getMailSmtpServer();
   const std::string& sender = runtimeCfg->getMailSender();
   const std::string& recipient = runtimeCfg->getMailRecipient();

   if (sender.empty() )
   {
      log.log(Log_ERR, "Unable to deliver mail due to missing sender address.");
      return -1;
   }
   if (recipient.empty() )
   {
      log.log(Log_ERR, "Unable to deliver mail due to missing recipient address.");
      return -1;
   }
   if (subject.empty() )
   {
      log.log(Log_ERR, "Won't send due to empty subject.");
      return -1;
   }
   if (msg.empty() )
   {
      log.log(Log_ERR, "Won't send due to empty message body.");
      return -1;
   }

   if(runtimeCfg->getMailSmtpSendType() == SmtpSendType_SOCKET)
   {
      if (smtpServerName.empty() )
      {
         log.log(Log_ERR, "Unable to deliver mail due to missing SMTP server.");
         return -1;
      }

      sendMailSocket(smtpServerName, sender, recipient, subject, msg);
   }
   else
   {
      sendMailSystem(sender, recipient, subject, msg);
   }

   return 0;
}

/**
 * Uses a socket to send an email.
 *
 * @param smtpServerName The smtp server to send the email.
 * @param sender The sender email address to send the email.
 * @param recipient The recipient email address to send the email.
 * @param subject The subject of the email.
 * @param msg The message of the email.
 * @return 0 on success else -1
 */
int Mailer::sendMailSocket(const std::string& smtpServerName, const std::string& sender,
   const std::string& recipient, const std::string& subject, const std::string& msg)
{
   LogContext log("Mail-socket");

   size_t startPoint = sender.find_last_of("@");
   std::string domain = sender.substr(startPoint+1);

   struct addrinfo hint;
   struct addrinfo* addressList;

   memset(&hint, 0, sizeof(struct addrinfo) );
   hint.ai_flags    = AI_CANONNAME;
   hint.ai_family   = PF_INET;
   hint.ai_socktype = SOCK_STREAM;

   int getRes = getaddrinfo(smtpServerName.c_str(), NULL, &hint,
      &addressList);
   if(getRes)
   {
      log.log(Log_DEBUG, "Unable to deliver mail (error while getting address "
         "info for SMTP Server " + smtpServerName + ").");
      return -1;
   }

   int sock = socket(AF_INET, SOCK_STREAM, 0);
   if(sock < 0)
   {
      log.log(Log_SPAM, "Could not create local socket");
      return -1;
   }

   struct sockaddr_in* inetAddr = (struct sockaddr_in*)(addressList->ai_addr);
   inetAddr->sin_port=htons(25);
   inetAddr->sin_family=AF_INET;

   if ( connect(sock, (struct sockaddr*) inetAddr, sizeof(*inetAddr)) < 0 )
   {
      log.log(Log_SPAM, "Unable to deliver mail (Could not connect to SMTP "
         "Server " + smtpServerName + ")");
      return -1;
   }

   int BUF_SIZE = 256;
   char buf[BUF_SIZE];
   std::string ehlo = "EHLO " + domain + "\r\n";
   std::string mailfrom = "MAIL FROM:<" + sender + ">\r\n";
   std::string mailto = "RCPT TO:<" + recipient + ">\r\n";

   struct tm *datetime;

   time_t t;
   time(&t);
   datetime = localtime(&t);

   char timeStr[32];
   sprintf(timeStr, "%s, %.2d %s %.4d %.2d:%.2d:%.2d",
      (intToWeekday(datetime->tm_wday)).c_str(), datetime->tm_mday,
      (intToMonth(datetime->tm_mon)).c_str(), datetime->tm_year+1900,
      datetime->tm_hour, datetime->tm_min, datetime->tm_sec);

   std::string dateStr = "Date: "+std::string(timeStr)+"\r\n";

   std::string data = "DATA\r\n";
   std::string from = "From: <" + sender + ">\r\n";
   std::string to = "To: <" + recipient + ">\r\n";
   std::string subjectStr = "Subject: " + subject + "\r\n";
   std::string message = msg + "\r\n";
   std::string dot = ".\r\n";
   std::string quit = "QUIT\r\n";

   int receivedBytes = 0;

   do
   {
      receivedBytes = recv(sock, buf, BUF_SIZE, 0);
      if(receivedBytes < 0)
      {
         log.log(Log_DEBUG, "Unable to deliver mail (Error in communication "
            "with SMTP Server " + smtpServerName + ")");

         freeaddrinfo(addressList);
         return -1;
      }
   } while (receivedBytes == BUF_SIZE);

   buf[receivedBytes] = '\0';
   send(sock, ehlo.c_str(), strlen(ehlo.c_str()), 0);

   do
   {
      receivedBytes = recv(sock, buf, BUF_SIZE, 0);
      if(receivedBytes < 0)
      {
         log.log(Log_DEBUG,"Unable to deliver mail (Error in communication "
            "with SMTP Server " + smtpServerName+")");
         freeaddrinfo(addressList);
         return -1;
      }
   } while (receivedBytes == BUF_SIZE);
   buf[receivedBytes] = '\0';

   send(sock, mailfrom.c_str(), strlen(mailfrom.c_str()), 0);

   do
   {
      receivedBytes = recv(sock, buf, 256, 0);
      if(receivedBytes < 0)
      {
         log.log(Log_DEBUG, "Unable to deliver mail (Error in communication "
            "with SMTP Server " + smtpServerName + ")");
         freeaddrinfo(addressList);
         return -1;
      }
   } while (receivedBytes == BUF_SIZE);
   buf[receivedBytes] = '\0';

   send(sock, mailto.c_str(), strlen(mailto.c_str()), 0);

   do
   {
      receivedBytes = recv(sock, buf, 256, 0);
      if(receivedBytes < 0)
      {
         log.log(Log_DEBUG, "Unable to deliver mail (Error in communication "
            "with SMTP Server " + smtpServerName + ")");
         freeaddrinfo(addressList);
         return -1;
      }
   } while (receivedBytes == BUF_SIZE);

   buf[receivedBytes] = '\0';

   send(sock, data.c_str(), strlen(data.c_str()), 0);
   do
   {
      receivedBytes = recv(sock, buf, 256, 0);
      if(receivedBytes < 0)
      {
         log.log(Log_DEBUG, "Unable to deliver mail (Error in communication "
            "with SMTP Server " + smtpServerName + ")");
         freeaddrinfo(addressList);
         return -1;
      }
   } while (receivedBytes == BUF_SIZE);

   buf[receivedBytes] = '\0';

   send(sock, from.c_str(), strlen(from.c_str()), 0);
   send(sock, to.c_str(), strlen(to.c_str()), 0);
   send(sock, dateStr.c_str(), strlen(dateStr.c_str()), 0);
   send(sock, subjectStr.c_str(), strlen(subjectStr.c_str()), 0);
   send(sock, message.c_str(), strlen(message.c_str()), 0);
   send(sock, dot.c_str(), strlen(dot.c_str()), 0);
   send(sock, quit.c_str(), strlen(quit.c_str()), 0);

   freeaddrinfo(addressList);

   return 0;
}

/**
 * Uses sendmail of the system to send an email.
 *
 * @param smtpServerName The smtp server to send the email.
 * @param sender The sender email address to send the email.
 * @param recipient The recipient email address to send the email.
 * @param subject The subject of the email.
 * @param msg The message of the email.
 * @return 0 on success else -1
 */
int Mailer::sendMailSystem(const std::string& sender, const std::string& recipient,
   const std::string& subject, const std::string& msg)
{
   LogContext log("Mail-sendmail");

   App* app = Program::getApp();
   ExternalJobRunner *jobRunner = app->getJobRunner();
   RuntimeConfig* runCfg = app->getRuntimeConfig();

   std::string cmd = "echo \""; // echo to pipe the message to sendmail and " for message start
   cmd.append("Subject: ").append(subject).append("\n\n"); // add the subject of the mail
   cmd.append(msg).append("\" |"); // add message body and end of message body and pipe for echo
   cmd.append(runCfg->getMailSendmailPath() ); // the path to the sendmail binary
   cmd.append(" -f ").append(runCfg->getMailSender() ); // the sender mail address
   cmd.append(" \"").append(runCfg->getMailRecipient() ).append("\""); // the list of recipients

   Mutex mutex;
   SafeMutexLock mutexLock(&mutex);

   Job *job = jobRunner->addJob(cmd, &mutex);

   while (!job->finished)
      job->jobFinishedCond.wait(&mutex);

   mutexLock.unlock();

   if (job->returnCode != 0)
   {
      log.log(Log_ERR, "Unable to deliver mail. Subject: " + subject + " ; Sender: " + sender +
         " ; Recipient: " + recipient);
      log.log(Log_DEBUG, "Unable to deliver mail. Command: " + cmd);
      jobRunner->delJob(job->id);
      return -1;
   }

   jobRunner->delJob(job->id);

   return 0;
}
