#include <program/Program.h>
#include <toolkit/bashtk.h>
#include <toolkit/install/setuphelper.h>
#include <toolkit/management/FileEntry.h>
#include <toolkit/management/managementhelper.h>
#include <toolkit/security/Auth.h>
#include <toolkit/stats/StatsAdmon.h>
#include <toolkit/stats/statshelper.h>
#include <toolkit/webtk.h>

#include "xmlhelper.h"

#include <common/toolkit/ZipIterator.h>



/*
 * serve the admon gui over HTTP
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 */
void xmlhelper::admonGui(struct mg_connection *conn,
   const struct mg_request_info *request_info)
{
   webtk::serveFile(conn, request_info, ADMON_GUI_PATH);
}

/*
 * generic error page that can be displayed whenever an unspecific error occurs
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param errorMessage The error message to display
 */
void xmlhelper::sendError(struct mg_connection *conn, const struct mg_request_info *request_info,
   std::string errorMessage)
{
   Logger::getLogger()->log(Log_ERR, __func__, "Requested URI: " +
      std::string(request_info->uri) + " from client IP: " + webtk::getIP(request_info) +
      ". Error: " + errorMessage);

   TiXmlDocument doc;
   TiXmlElement *rootElement = new TiXmlElement("data");
   doc.LinkEndChild(rootElement);
   TiXmlElement *errorElement = new TiXmlElement("error");
   rootElement->LinkEndChild(errorElement);

   TiXmlText *errorElementText = new TiXmlText(errorMessage);
   errorElement->LinkEndChild(errorElementText);

   webtk::writeDoc(conn, request_info, doc);
}

/*
 * get a nonce to use for the challenge-response-authentication
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void xmlhelper::getNonce(struct mg_connection *conn,
   const struct mg_request_info *request_info, const char* postBuf, size_t postBufLen)
{
#ifdef MONGOOSE_DEBUG
   webtk::debugMongoose(conn,request_info, postBuf, postBufLen);
#endif

   int64_t nonce = 0;
   int nonceID = SecurityTk::createNonce(&nonce);

   TiXmlDocument doc;
   TiXmlElement *rootElement = new TiXmlElement("data");
   doc.LinkEndChild(rootElement);

   TiXmlElement *idElement = new TiXmlElement("id");
   TiXmlText *idElementText = new TiXmlText(StringTk::intToStr(nonceID));
   idElement->LinkEndChild(idElementText);
   rootElement->LinkEndChild(idElement);

   TiXmlElement *nonceElement = new TiXmlElement("nonce");
   TiXmlText *nonceElementText = new TiXmlText(StringTk::int64ToStr(nonce));
   nonceElement->LinkEndChild(nonceElementText);
   rootElement->LinkEndChild(nonceElement);

   webtk::writeDoc(conn, request_info, doc);
}

/*
 * Does the authentification
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void xmlhelper::authInfo(struct mg_connection *conn,
   const struct mg_request_info *request_info, const char* postBuf, size_t postBufLen)
{
#ifdef MONGOOSE_DEBUG
   webtk::debugMongoose(conn,request_info, postBuf, postBufLen);
#endif

   std::string nonceID = webtk::getVar(conn, "nonceID", "", postBuf, postBufLen);
   if (nonceID.empty())
   {
      xmlhelper::sendError(conn, request_info, "nonceID empty");

      Logger::getLogger()->log(Log_ERR, __func__, "Authentication failed. Missing nonce "
         "in request.");
      return;
   }

   StringList passwords;
   StringList cryptedPasswords;
   Auth::init();
   passwords.push_back(Auth::getPassword("information"));
   passwords.push_back(Auth::getPassword("admin"));

   SecurityTk::cryptWithNonce(&passwords, &cryptedPasswords, StringTk::strToInt(nonceID));

   TiXmlDocument doc;
   TiXmlElement *rootElement = new TiXmlElement("data");
   doc.LinkEndChild(rootElement);

   StringListIter iter = cryptedPasswords.begin();
   if (iter != cryptedPasswords.end())
   {
      TiXmlElement *element = new TiXmlElement("info");
      TiXmlText *elementText = new TiXmlText(*iter);
      element->LinkEndChild(elementText);
      rootElement->LinkEndChild(element);
      iter++;
   }

   if (iter != cryptedPasswords.end())
   {
      TiXmlElement *element = new TiXmlElement("admin");
      TiXmlText *elementText = new TiXmlText(*iter);
      element->LinkEndChild(elementText);
      rootElement->LinkEndChild(element);
      iter++;
   }

   webtk::writeDoc(conn, request_info, doc);
}

/*
 * Changes the password of the admin or info user
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void xmlhelper::changePassword(struct mg_connection *conn,
   const struct mg_request_info *request_info, const char* postBuf, size_t postBufLen)
{
#ifdef MONGOOSE_DEBUG
   webtk::debugMongoose(conn,request_info, postBuf, postBufLen);
#endif

   Logger* log = Logger::getLogger();
   std::string retVal = "false";

   try
   {
      Database *db = Program::getApp()->getDB();
      std::string action = webtk::getVar(conn, "action", "", postBuf, postBufLen);
      std::string nonceID = webtk::getVar(conn, "nonceID", "-1", postBuf, postBufLen);

      std::string secret = webtk::getVar(conn, "secret", "", postBuf, postBufLen);

      TiXmlDocument doc;
      TiXmlElement *rootElement = new TiXmlElement("data");
      doc.LinkEndChild(rootElement);

      TiXmlElement *successElement = new TiXmlElement("success");
      rootElement->LinkEndChild(successElement);

      if (action.compare("enableInfo") == 0)
      {
         Auth::init();
         if (SecurityTk::checkReply(secret, Auth::getPassword("admin"),
            StringTk::strToInt(nonceID)))
         {
            db->setDisabled("information", false);
            Auth::init();

            log->log(Log_WARNING, __func__, "Enable passwordless login for information user.");
            retVal = "true";
         }
         else
         {
            log->log(Log_WARNING, __func__, "Failed to enable passwordless login for information "
               "user.");
            retVal = "false";
         }
      }
      else
      if (action.compare("disableInfo") == 0)
      {
         Auth::init();
         if (SecurityTk::checkReply(secret, Auth::getPassword("admin"),
            StringTk::strToInt(nonceID) ) )
         {
            db->setDisabled("information", true);
            Auth::init();

            log->log(Log_WARNING, __func__, "Disable passwordless login for information user.");
            retVal = "true";
         }
         else
         {
            log->log(Log_WARNING, __func__, "Failed to disable passwordless login for information "
               "user.");
            retVal = "false";
         }
      }
      else
      {
         std::string user = webtk::getVar(conn, "user", postBuf, postBufLen);
         if ( (user.compare("information") == 0) || (user.compare("admin") == 0) )
         {
            std::string newPw = webtk::getVar(conn, "newPw", postBuf, postBufLen);
            if (newPw.compare("") != 0)
            {
               Auth::init();
               if (SecurityTk::checkReply(secret, Auth::getPassword("admin"),
                  StringTk::strToInt(nonceID) ) )
               {
                  db->setPassword(user, newPw);
                  Auth::init();

                  retVal = "true";
                  log->log(Log_WARNING, __func__, "New password set successful for user: " + user);
               }
               else
               {
                  retVal = "false";
                  log->log(Log_WARNING, __func__, "Failed to set new password because the old "
                     "password is wrong. User: " + user);
               }
            }
            else
            {
               log->log(Log_WARNING, __func__, "Failed to set new password because the new "
                  "password is empty. User: " + user);
               retVal = "false";
            }
         }
         else
         {
            log->log(Log_WARNING, __func__, "Failed to set new password because the user is "
               "unknown. User: " + user);
            retVal = "false";
         }
      }

      TiXmlText *successElementText = new TiXmlText(retVal);
      successElement->LinkEndChild(successElementText);

      webtk::writeDoc(conn, request_info, doc);
   }
   catch (HttpVarException &e)
   {
      xmlhelper::sendError(conn, request_info, e.what());
   }
}

/*
 * Checks the architecture and the linux distro of the remote hosts
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void xmlhelper::checkDistriArch(struct mg_connection *conn,
   const struct mg_request_info *request_info, const char* postBuf, size_t postBufLen)
{
#ifdef MONGOOSE_DEBUG
   webtk::debugMongoose(conn,request_info, postBuf, postBufLen);
#endif

   TiXmlDocument doc;
   TiXmlElement *rootElement = new TiXmlElement("data");
   doc.LinkEndChild(rootElement);

   int checkRes = setuphelper::checkDistriArch();
   if(checkRes)
   {
      Logger::getLogger()->log(Log_ERR, __func__, "Failed to check distri "
         "architecture.");
   }

   TiXmlElement *element = new TiXmlElement("finished");
   TiXmlText *elementText = new TiXmlText(checkRes ? "false" : "true");
   element->LinkEndChild(elementText);
   rootElement->LinkEndChild(element);

   webtk::writeDoc(conn, request_info, doc);
}

/*
 * Checks the reachabiliy of a remote host by ssh
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void xmlhelper::checkSSH(struct mg_connection *conn,
   const struct mg_request_info *request_info, const char* postBuf, size_t postBufLen)
{
#ifdef MONGOOSE_DEBUG
   webtk::debugMongoose(conn,request_info, postBuf, postBufLen);
#endif

   try
   {
      TiXmlDocument doc;
      TiXmlElement *rootElement = new TiXmlElement("data");
      doc.LinkEndChild(rootElement);

      StringList nodeList;
      webtk::getVarList(conn, "node", &nodeList, postBuf, postBufLen);

      StringList failedNodes;
      setuphelper::checkSSH(&nodeList, &failedNodes);

      std::string nodes;

      for (StringListIter iter = failedNodes.begin(); iter != failedNodes.end();
         iter++)
      {
         TiXmlElement *nodeElement = new TiXmlElement("node");
         nodeElement->LinkEndChild(new TiXmlText(*iter));
         rootElement->LinkEndChild(nodeElement);

         nodes.append(*iter);
      }

      if(!nodes.empty() )
         Logger::getLogger()->log(Log_ERR, __func__, "SSH connection to some nodes "
            "failed. Nodes:" + nodes);

      webtk::writeDoc(conn, request_info, doc);
   }
   catch (HttpVarException &e)
   {
      xmlhelper::sendError(conn, request_info, e.what());
   }
}

/*
 * stores or gets the basic configuration for the beegfs daemons and the client
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void xmlhelper::configBasicConfig(struct mg_connection *conn,
   const struct mg_request_info *request_info, const char* postBuf, size_t postBufLen)
{
#ifdef MONGOOSE_DEBUG
   webtk::debugMongoose(conn,request_info, postBuf, postBufLen);
#endif

   Logger* log = Logger::getLogger();

   try
   {
      StringMap paramsMap;
      std::string saveConfig = webtk::getVar(conn, "saveConfig", "", postBuf, postBufLen);

      TiXmlDocument doc;
      TiXmlElement *rootElement = new TiXmlElement("data");
      doc.LinkEndChild(rootElement);

      if (saveConfig.compare("Save") == 0)
      {
         webtk::paramsToMap(conn, &paramsMap, "saveConfig", postBuf, postBufLen);
         TiXmlElement *errorElement = new TiXmlElement("errors");
         rootElement->LinkEndChild(errorElement);

         if (setuphelper::writeConfigFile(&paramsMap) != 0)
         {
            TiXmlElement *entryElement = new TiXmlElement("entry");
            entryElement->LinkEndChild(new TiXmlText("Could not write the configuration file.") );
            errorElement->LinkEndChild(entryElement);

            log->log(Log_ERR, __func__, "Could not write the configuration file. File: " +
               CONFIG_FILE_PATH);
         }
      }
      else
      {
         if(setuphelper::readConfigFile(&paramsMap) )
            log->log(Log_ERR, __func__, "Could not read the configuration file. File: " +
               CONFIG_FILE_PATH);

         for (std::map<std::string, std::string>::iterator
            iter = paramsMap.begin(); iter != paramsMap.end(); iter++)
         {
            std::string key = iter->first;
            std::string value = iter->second;
            if (key.compare("") != 0)
            {
               TiXmlElement *element = new TiXmlElement(key);
               element->LinkEndChild(new TiXmlText(value));
               rootElement->LinkEndChild(element);
            }
         }
      }
      webtk::writeDoc(conn, request_info, doc);
   }
   catch (HttpVarException &e)
   {
      xmlhelper::sendError(conn, request_info, e.what());
   }
}

/*
 * stores or gets the Infiniband configuration for the beegfs daemons and the client
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void xmlhelper::configIBConfig(struct mg_connection *conn,
   const struct mg_request_info *request_info, const char* postBuf, size_t postBufLen)
{
#ifdef MONGOOSE_DEBUG
   webtk::debugMongoose(conn,request_info, postBuf, postBufLen);
#endif

   Logger* log = Logger::getLogger();

   try
   {
      std::string saveIBConfig = webtk::getVar(conn, "saveIBConfig", "", postBuf, postBufLen);

      TiXmlDocument doc;
      TiXmlElement *rootElement = new TiXmlElement("data");
      doc.LinkEndChild(rootElement);

      if (saveIBConfig.compare("Save") == 0)
      {
         StringMap paramsMap;
         webtk::paramsToMap(conn, &paramsMap, "saveIBConfig", postBuf, postBufLen);
         TiXmlElement *errorElement = new TiXmlElement("errors");
         rootElement->LinkEndChild(errorElement);

         if (setuphelper::writeIBFile(&paramsMap) != 0)
         {
            TiXmlElement *entryElement = new TiXmlElement("entry");
            entryElement->LinkEndChild(new TiXmlText("Could not write the "
               "configuration file."));
            errorElement->LinkEndChild(entryElement);

            log->log(Log_ERR, __func__, "Could not write the configuration file. File: " +
               IB_CONFIG_FILE_PATH);
         }
      }
      else
      {
         StringMap ibConf;
         StringList hosts;
         if (setuphelper::readIBFile(&hosts, &ibConf) == 0)
         {
            for (StringListIter iter = hosts.begin(); iter != hosts.end();
               iter++)
            {
               TiXmlElement *element = new TiXmlElement(*iter);
               rootElement->LinkEndChild(element);

               TiXmlElement *kernelIncPathElem = new TiXmlElement(
                  "kernelIncPath");
               kernelIncPathElem->LinkEndChild(new TiXmlText(
                  ibConf[std::string((*iter) + "_kernelibincpath")]));
               element->LinkEndChild(kernelIncPathElem);
            }
         }
         else
         {
            TiXmlElement *errorElement = new TiXmlElement("error");
            rootElement->LinkEndChild(errorElement);
            TiXmlElement *entryElement = new TiXmlElement("entry");
            entryElement->LinkEndChild(new TiXmlText("Could not read the"
               "configuration file."));
            errorElement->LinkEndChild(entryElement);

            log->log(Log_ERR, __func__, "Could not read the configuration file. File: " +
               IB_CONFIG_FILE_PATH);
         }
      }
      webtk::writeDoc(conn, request_info, doc);
   }
   catch (HttpVarException &e)
   {
      xmlhelper::sendError(conn, request_info, e.what());
   }
}

/*
 * stores or gets the role configuration for the beegfs daemons and the clients
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void xmlhelper::configRoles(struct mg_connection *conn,
   const struct mg_request_info *request_info, const char* postBuf, size_t postBufLen)
{
#ifdef MONGOOSE_DEBUG
   webtk::debugMongoose(conn,request_info, postBuf, postBufLen);
#endif

   Logger* log = Logger::getLogger();

   TiXmlDocument doc;
   TiXmlElement *rootElement = new TiXmlElement("data");
   doc.LinkEndChild(rootElement);

   try
   {
      std::string saveRoles = webtk::getVar(conn, "saveRoles", "false", postBuf, postBufLen);
      if (StringTk::strToBool(saveRoles))
      {
         std::string mgmtdNode;
         StringList mgmtdList;
         StringList metaList;
         StringList storageList;
         StringList clientList;
         mgmtdNode = webtk::getVar(conn, std::string("mgmtd"), postBuf, postBufLen);
         mgmtdList.push_back(mgmtdNode);
         webtk::getVarList(conn, "meta", &metaList, postBuf, postBufLen);
         webtk::getVarList(conn, "storage", &storageList, postBuf, postBufLen);
         webtk::getVarList(conn, "client", &clientList, postBuf, postBufLen);
         TiXmlElement *errorElement = new TiXmlElement("error");
         rootElement->LinkEndChild(errorElement);

         if (setuphelper::writeRolesFile(ROLETYPE_Mgmtd, &mgmtdList) != 0)
         {
            TiXmlElement *entryElement = new TiXmlElement("entry");
            entryElement->LinkEndChild(new TiXmlText("Failed to save management nodes."));
            errorElement->LinkEndChild(entryElement);

            log->log(Log_ERR, __func__, "Failed to save management nodes. File: " +
               MGMTD_ROLE_FILE_PATH);
         }

         if (setuphelper::writeRolesFile(ROLETYPE_Meta, &metaList) != 0)
         {
            TiXmlElement *entryElement = new TiXmlElement("entry");
            entryElement->LinkEndChild(new TiXmlText("Failed to save meta nodes."));
            errorElement->LinkEndChild(entryElement);

            log->log(Log_ERR, __func__, "Failed to save meta nodes. File: " +
               META_ROLE_FILE_PATH);
         }

         if (setuphelper::writeRolesFile(ROLETYPE_Storage, &storageList) != 0)
         {
            TiXmlElement *entryElement = new TiXmlElement("entry");
            entryElement->LinkEndChild(new TiXmlText("Failed to save storage nodes."));
            errorElement->LinkEndChild(entryElement);

            log->log(Log_ERR, __func__, "Failed to save storage nodes. File: " +
               STORAGE_ROLE_FILE_PATH);
         }

         if (setuphelper::writeRolesFile(ROLETYPE_Client, &clientList) != 0)
         {
            TiXmlElement *entryElement = new TiXmlElement("entry");
            entryElement->LinkEndChild(new TiXmlText("Failed to save client nodes."));
            errorElement->LinkEndChild(entryElement);

            log->log(Log_ERR, __func__, "Failed to save client nodes. File: " +
               CLIENT_ROLE_FILE_PATH);
         }
      }
      else
      {
         StringList metaList;
         if(setuphelper::readRolesFile(ROLETYPE_Meta, &metaList) )
            log->log(Log_ERR, __func__, "Could not read the configuration file. File: " +
               META_ROLE_FILE_PATH);

         StringList storageList;
         if(setuphelper::readRolesFile(ROLETYPE_Storage, &storageList) )
            log->log(Log_ERR, __func__, "Could not read the configuration file. File: " +
               STORAGE_ROLE_FILE_PATH);

         StringList clientList;
         if(setuphelper::readRolesFile(ROLETYPE_Client, &clientList) )
            log->log(Log_ERR, __func__, "Could not read the configuration file. File: " +
               CLIENT_ROLE_FILE_PATH);

         StringList mgmtdList;
         if(setuphelper::readRolesFile(ROLETYPE_Mgmtd, &mgmtdList) )
            log->log(Log_ERR, __func__, "Could not read the configuration file. File: " +
               MGMTD_ROLE_FILE_PATH);

         TiXmlElement *metaElement = new TiXmlElement("meta");
         rootElement->LinkEndChild(metaElement);
         for (StringListIter iter = metaList.begin(); iter != metaList.end();
            iter++)
         {
            TiXmlElement *nodeElement = new TiXmlElement("node");
            nodeElement->LinkEndChild(new TiXmlText(*iter));
            metaElement->LinkEndChild(nodeElement);
         }
         TiXmlElement *storageElement = new TiXmlElement("storage");
         rootElement->LinkEndChild(storageElement);
         for (StringListIter iter = storageList.begin();
            iter != storageList.end(); iter++)
         {
            TiXmlElement *nodeElement = new TiXmlElement("node");
            nodeElement->LinkEndChild(new TiXmlText(*iter));
            storageElement->LinkEndChild(nodeElement);
         }
         TiXmlElement *clientElement = new TiXmlElement("client");
         rootElement->LinkEndChild(clientElement);
         for (StringListIter iter = clientList.begin();
            iter != clientList.end(); iter++)
         {
            TiXmlElement *nodeElement = new TiXmlElement("node");
            nodeElement->LinkEndChild(new TiXmlText(*iter));
            clientElement->LinkEndChild(nodeElement);
         }
         TiXmlElement *mgmtdElement = new TiXmlElement("mgmtd");
         rootElement->LinkEndChild(mgmtdElement);
         for (StringListIter iter = mgmtdList.begin(); iter != mgmtdList.end();
            iter++)
         {
            TiXmlElement *nodeElement = new TiXmlElement("node");
            nodeElement->LinkEndChild(new TiXmlText(*iter));
            mgmtdElement->LinkEndChild(nodeElement);
         }
      }
      webtk::writeDoc(conn, request_info, doc);
   }
   catch (HttpVarException &e)
   {
      xmlhelper::sendError(conn, request_info, e.what());
   }
}

/*
 * gets all files and directories of a beegfs
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void xmlhelper::fileBrowser(struct mg_connection *conn,
   const struct mg_request_info *request_info, const char* postBuf, size_t postBufLen)
{
#ifdef MONGOOSE_DEBUG
   webtk::debugMongoose(conn,request_info, postBuf, postBufLen);
#endif

   Logger* log = Logger::getLogger();

   TiXmlDocument doc;
   TiXmlElement *rootElement = new TiXmlElement("data");
   doc.LinkEndChild(rootElement);

   try
   {
      std::string pathStr = webtk::getVar(conn, "pathStr", "/", postBuf, postBufLen);
      int64_t offset = StringTk::strToInt64(webtk::getVar(conn, "offset", "0", postBuf,
         postBufLen) );
      int entriesAtOnce = StringTk::strToInt(webtk::getVar(conn, "entriesAtOnce", "25", postBuf,
         postBufLen) );

      FileEntryList content;
      short retVal = -1;
      uint64_t totalSize = 0;

      retVal = managementhelper::listDirFromOffset(pathStr, &offset, &content, &totalSize,
         entriesAtOnce);
      if (retVal != 0)
      {
         content.clear();
         TiXmlElement *successElement = new TiXmlElement("success");
         successElement->LinkEndChild(new TiXmlText("false"));
         rootElement->LinkEndChild(successElement);

         log->log(Log_ERR, __func__, "Could not list file of directory: " + pathStr +
            " . Offset: " + StringTk::int64ToStr(offset) );
      }
      else
      {
         TiXmlElement *successElement = new TiXmlElement("success");
         successElement->LinkEndChild(new TiXmlText("true"));
         rootElement->LinkEndChild(successElement);

         std::string unit;

         TiXmlElement *generalElement = new TiXmlElement("general");
         rootElement->LinkEndChild(generalElement);

         TiXmlElement *pathElement = new TiXmlElement("path");
         pathElement->LinkEndChild(new TiXmlText(pathStr));
         generalElement->LinkEndChild(pathElement);

         TiXmlElement *offsetElement = new TiXmlElement("offset");
         offsetElement->LinkEndChild(new TiXmlText(
            StringTk::int64ToStr(offset)));
         generalElement->LinkEndChild(offsetElement);

         TiXmlElement *totalFilesElement = new TiXmlElement("totalFiles");
         totalFilesElement->LinkEndChild(new TiXmlText(
            StringTk::uintToStr(content.size())));
         generalElement->LinkEndChild(totalFilesElement);

         TiXmlElement *totalSizeElement = new TiXmlElement("totalSize");
         totalSizeElement->LinkEndChild(new TiXmlText(
            StringTk::int64ToStr(totalSize)));
         generalElement->LinkEndChild(totalSizeElement);

         TiXmlElement *entriesElement = new TiXmlElement("entries");
         rootElement->LinkEndChild(entriesElement);

         for (FileEntryListIter iter = content.begin(); iter != content.end();
            iter++)
         {
            TiXmlElement *entryElement = new TiXmlElement("entry");
            entriesElement->LinkEndChild(entryElement);

            FileEntry entry = *iter;
            entryElement->LinkEndChild(new TiXmlText(entry.name));

            if (S_ISDIR(entry.statData.getMode()))
            {
               entryElement->SetAttribute("isDir", "true");
            }
            else
            {
               entryElement->SetAttribute("isDir", "false");
            }
            entryElement->SetAttribute("filesize", StringTk::int64ToStr(
               entry.statData.getFileSize()));
            entryElement->SetAttribute("mode", managementhelper::formatFileMode(
               entry.statData.getMode()));
            entryElement->SetAttribute("ctime", StringTk::int64ToStr(
               entry.statData.getAttribChangeTimeSecs() * 1000));
            entryElement->SetAttribute("mtime", StringTk::int64ToStr(
               entry.statData.getModificationTimeSecs() * 1000));
            entryElement->SetAttribute("atime", StringTk::int64ToStr(
               entry.statData.getLastAccessTimeSecs() * 1000));
            entryElement->SetAttribute("ownerUser", StringTk::uintToStr(
               entry.statData.getUserID()));
            entryElement->SetAttribute("ownerGroup", StringTk::uintToStr(
               entry.statData.getGroupID()));
         }
      }

      webtk::writeDoc(conn, request_info, doc);
   }
   catch (HttpVarException &e)
   {
      xmlhelper::sendError(conn, request_info, e.what());
   }
}

/*
 * uploads the logfile about the client dependency check
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void xmlhelper::getCheckClientDependenciesLog(
   struct mg_connection *conn, const struct mg_request_info *request_info,
   const char* postBuf, size_t postBufLen)
{
#ifdef MONGOOSE_DEBUG
   webtk::debugMongoose(conn,request_info, postBuf, postBufLen);
#endif

   Logger* log = Logger::getLogger();

   TiXmlDocument doc;
   TiXmlElement *rootElement = new TiXmlElement("data");
   doc.LinkEndChild(rootElement);

   try
   {
      std::string logStr;
      TiXmlElement *successElement = new TiXmlElement("success");
      rootElement->LinkEndChild(successElement);

      if (managementhelper::getFileContents(SETUP_CLIENT_WARNING, &logStr) == 0)
      {
         successElement->LinkEndChild(new TiXmlText("true"));
         TiXmlElement *logElement = new TiXmlElement("log");
         logElement->LinkEndChild(new TiXmlText(logStr));
         rootElement->LinkEndChild(logElement);
      }
      else
      {
         successElement->LinkEndChild(new TiXmlText("false"));

         log->log(Log_ERR, __func__, "Could not read client warnings file: " +
            SETUP_CLIENT_WARNING);
      }

      webtk::writeDoc(conn, request_info, doc);
   }
   catch (HttpVarException &e)
   {
      xmlhelper::sendError(conn, request_info, e.what());
   }
}

/*
 * stores or gets the group configuration for the beegfs daemons and the clients (in the moment
 * not in use)
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void xmlhelper::getGroups(struct mg_connection *conn,
   const struct mg_request_info *request_info, const char* postBuf, size_t postBufLen)
{
#ifdef MONGOOSE_DEBUG
   webtk::debugMongoose(conn,request_info, postBuf, postBufLen);
#endif

   Logger* log = Logger::getLogger();

   Database *db = Program::getApp()->getDB();
   TiXmlDocument doc;
   TiXmlElement *rootElement = new TiXmlElement("data");
   doc.LinkEndChild(rootElement);

   try
   {
      std::string move = webtk::getVar(conn, "move", "false", postBuf, postBufLen);
      std::string add = webtk::getVar(conn, "add", "false", postBuf, postBufLen);
      std::string remove = webtk::getVar(conn, "remove", "false", postBuf, postBufLen);
      std::string nodetype = webtk::getVar(conn, "nodetype", postBuf, postBufLen);
      std::string source = webtk::getVar(conn, "source", "", postBuf, postBufLen);
      std::string target = webtk::getVar(conn, "target", "", postBuf, postBufLen);
      std::string group = webtk::getVar(conn, "group", "", postBuf, postBufLen);
      UInt16List nodes;
      webtk::getVarList(conn, "nodes", &nodes, postBuf, postBufLen);

      NodeStoreMetaEx* metaNodes = Program::getApp()->getMetaNodes();
      NodeStoreStorageEx* storageServer = Program::getApp()->getStorageNodes();

      if (StringTk::strToBool(move))
      {
         TiXmlElement *errorElement = new TiXmlElement("errors");
         rootElement->LinkEndChild(errorElement);

         if ((source.compare("") != 0) && (target.compare("") != 0))
         {
            // check if groups exist
            if ((db->getGroupID(source) != -1) && (db->getGroupID(target) != -1))
            {
               for (UInt16ListIter iter = nodes.begin(); iter != nodes.end(); ++iter)
               {
                  if (nodetype.compare("meta") == 0)
                  {
                     auto tmpNode = metaNodes->referenceNode(NumNodeID(*iter) );
                     std::string nodeID = tmpNode->getID();

                     if ((db->delMetaNodeFromGroup(NumNodeID(*iter), source) != 0) ||
                        (db->addMetaNodeToGroup(nodeID, NumNodeID(*iter), source) != 0))
                     {
                        TiXmlElement *entryElement = new TiXmlElement("entry");
                        entryElement->LinkEndChild(new TiXmlText("An error occurred on the server "
                           "side while moving the node(s)") );
                        errorElement->LinkEndChild(entryElement);

                        log->log(Log_ERR, __func__, "An error occurred on the server side while "
                           "moving the node(s)");
                     }
                  }
                  else
                  if (nodetype.compare("storage") == 0)
                  {
                     auto tmpNode = storageServer->referenceNode(NumNodeID(*iter) );
                     std::string nodeID = tmpNode->getID();

                     if ((db->delStorageNodeFromGroup(NumNodeID(*iter), source) != 0)
                        || (db->addStorageNodeToGroup(nodeID, NumNodeID(*iter), source) != 0))
                     {
                        TiXmlElement *entryElement = new TiXmlElement("entry");
                        entryElement->LinkEndChild(new TiXmlText("An error occurred on the server "
                           "side while moving the node(s)"));
                        errorElement->LinkEndChild(entryElement);

                        log->log(Log_ERR, __func__, "An error occurred on the server side while "
                           "moving the node(s)");
                     }
                  }
               }
            }
            else
            {
               TiXmlElement *entryElement = new TiXmlElement("entry");
               entryElement->LinkEndChild(
                  new TiXmlText(
                     "Either the source or the target group does not exist. "
                     "Please reload the groups from server in case someone "
                     "else modified them in the meantime"));
               errorElement->LinkEndChild(entryElement);

               log->log(Log_ERR, __func__, "Either the source or the target group does not exist. "
                  "Please reload the groups from server in case someone else modified them in the "
                  "meantime");
            }
         }
      }
      else
      if (StringTk::strToBool(add))
      {
         TiXmlElement *errorElement = new TiXmlElement("errors");
         rootElement->LinkEndChild(errorElement);
         if (group.compare("") != 0)
         {
            if (db->addGroup(group) != 0)
            {
               TiXmlElement *entryElement = new TiXmlElement("entry");
               entryElement->LinkEndChild(new TiXmlText(
                  "An error occurred on the server side while adding the "
                  "group"));
               errorElement->LinkEndChild(entryElement);

               log->log(Log_ERR, __func__, "An error occurred on the server side while adding the "
                  "group");
            }
         }
      }
      else
      if (StringTk::strToBool(remove))
      {
         TiXmlElement *errorElement = new TiXmlElement("errors");
         rootElement->LinkEndChild(errorElement);

         if (group.compare("") != 0)
         {
            if (db->delGroup(group) != 0)
            {
               TiXmlElement *entryElement = new TiXmlElement("entry");
               entryElement->LinkEndChild(new TiXmlText("An error occurred on the server side "
                  "while removing the group"));
               errorElement->LinkEndChild(entryElement);

               log->log(Log_ERR, __func__, "An error occurred on the server side while removing "
                  "the group");
            }
         }
      }
      else
      {
         StringList groups;
         db->getGroups(&groups);
         for (StringListIter iter = groups.begin(); iter != groups.end();
            iter++)
         {
            TiXmlElement *groupElement = new TiXmlElement("group");
            groupElement->LinkEndChild(new TiXmlText(*iter));
            rootElement->LinkEndChild(groupElement);
         }
      }

      webtk::writeDoc(conn, request_info, doc);
   }
   catch (HttpVarException &e)
   {
      xmlhelper::sendError(conn, request_info, e.what());
   }
}

/*
 * uploads the logfile from a client or beegfs daemon
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void xmlhelper::getRemoteLogFile(struct mg_connection *conn,
   const struct mg_request_info *request_info, const char* postBuf, size_t postBufLen)
{
#ifdef MONGOOSE_DEBUG
   webtk::debugMongoose(conn,request_info, postBuf, postBufLen);
#endif

   Logger* log = Logger::getLogger();

   TiXmlDocument doc;
   TiXmlElement *rootElement = new TiXmlElement("data");
   doc.LinkEndChild(rootElement);

   try
   {
      std::string nodeID = webtk::getVar(conn, "node", "", postBuf, postBufLen);
      std::string service = webtk::getVar(conn, "service", "", postBuf, postBufLen);
      uint16_t nodeNumID = StringTk::strToUInt(
         webtk::getVar(conn, "nodeNumID", "", postBuf, postBufLen) );
      uint lines = StringTk::strToUInt(webtk::getVar(conn, "lines", "0", postBuf, postBufLen) );

      NodeType nodeType;

      if (service.compare("mgmtd") == 0)
         nodeType = NODETYPE_Mgmt;
      else
      if (service.compare("meta") == 0)
         nodeType = NODETYPE_Meta;
      else
      if (service.compare("storage") == 0)
         nodeType = NODETYPE_Storage;
      else
      if (service.compare("admon") == 0)
         nodeType = NODETYPE_Admon;
      else
      if (service.compare("client") == 0)
         nodeType = NODETYPE_Client;
      else
         nodeType = NODETYPE_Invalid;


      std::string logFile;
      if (nodeType == NODETYPE_Admon)
      {
         std::string logFilePath = Program::getApp()->getConfig()->getLogStdFile();
         if(managementhelper::getFileContents(logFilePath, &logFile) )
         {
            logFile = "Internal error: Could not load log file. File: " + logFilePath;
            log->log(Log_ERR, __func__, logFile);
         }
      }
      else
      if (nodeType != NODETYPE_Invalid)
      {
         if(managementhelper::getLogFile(nodeType, nodeNumID, lines, &logFile, nodeID) )
         {
            logFile = "Internal error: Could not load log file from node: " + nodeID +
               ". Node type: " + Node::nodeTypeToStr(nodeType);
            log->log(Log_ERR, __func__, logFile);
         }
      }
      else // error occurred
      {
         logFile = "Internal error: Could not load log file. Unknown node type.";
         log->log(Log_ERR, __func__, logFile);
      }

      TiXmlElement *element = new TiXmlElement("log");
      element->LinkEndChild(new TiXmlText(logFile));
      rootElement->LinkEndChild(element);

      webtk::writeDoc(conn, request_info, doc);
   }
   catch (HttpVarException &e)
   {
      xmlhelper::sendError(conn, request_info, e.what());
   }
}

/*
 * installs the packages on the remote hosts
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void xmlhelper::install(struct mg_connection *conn,
   const struct mg_request_info *request_info, const char* postBuf, size_t postBufLen)
{
#ifdef MONGOOSE_DEBUG
   webtk::debugMongoose(conn,request_info, postBuf, postBufLen);
#endif

   Logger* log = Logger::getLogger();

   TiXmlDocument doc;
   TiXmlElement *rootElement = new TiXmlElement("data");
   doc.LinkEndChild(rootElement);

   try
   {
      std::string package = webtk::getVar(conn, "package", "", postBuf, postBufLen);
      std::string nonceID = webtk::getVar(conn, "nonceID", "-1", postBuf, postBufLen);
      std::string secret = webtk::getVar(conn, "secret", "", postBuf, postBufLen);

      Auth::init();
      if(SecurityTk::checkReply(secret, Auth::getPassword("admin"), StringTk::strToInt(nonceID) ) )
      {
         TiXmlElement *authenticatedElement = new TiXmlElement("authenticated");
         authenticatedElement->LinkEndChild(new TiXmlText("true"));
         rootElement->LinkEndChild(authenticatedElement);

         TiXmlElement *failedElement = new TiXmlElement("failedHosts");
         rootElement->LinkEndChild(failedElement);

         // write the repository files
         StringList failedRepoHosts;
         StringList failedHosts;

         if(setuphelper::createRepos(&failedRepoHosts) )
         {
            for (StringListIter iter = failedRepoHosts.begin(); iter != failedRepoHosts.end();
               iter++)
            {
               TiXmlElement *entryElement = new TiXmlElement("entry");
               entryElement->LinkEndChild(new TiXmlText(*iter));
               failedElement->LinkEndChild(entryElement);
            }
         }
         else if(setuphelper::installPackage(package, &failedHosts) )
         {
            for (StringListIter iter = failedHosts.begin();
               iter != failedHosts.end(); iter++)
            {
               TiXmlElement *entryElement = new TiXmlElement("entry");
               entryElement->LinkEndChild(new TiXmlText(*iter));
               failedElement->LinkEndChild(entryElement);
            }
         }

         if(setuphelper::updateAdmonConfig() )
            log->log(Log_ERR, __func__, "Failed to update admon configuration.");

         webtk::writeDoc(conn, request_info, doc);
      }
      else
      {
         TiXmlElement *authenticatedElement = new TiXmlElement("authenticated");
         authenticatedElement->LinkEndChild(new TiXmlText("false"));
         rootElement->LinkEndChild(authenticatedElement);
         webtk::writeDoc(conn, request_info, doc);

         log->log(Log_ERR, __func__, "Authentication error.");
      }
   }
   catch (HttpVarException &e)
   {
      xmlhelper::sendError(conn, request_info, e.what());
   }
}

/*
 * gets the installation infos about the remote hosts (role, arch, distro, ...)
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void xmlhelper::installInfo(struct mg_connection *conn,
   const struct mg_request_info *request_info, const char* postBuf, size_t postBufLen)
{
#ifdef MONGOOSE_DEBUG
   webtk::debugMongoose(conn,request_info, postBuf, postBufLen);
#endif

   TiXmlDocument doc;
   TiXmlElement *rootElement = new TiXmlElement("data");
   doc.LinkEndChild(rootElement);

   try
   {
      std::string check = webtk::getVar(conn, "check", "false", postBuf, postBufLen);
      std::string package = webtk::getVar(conn, "package", "", postBuf, postBufLen);
      std::string nonceID = webtk::getVar(conn, "nonceID", "-1", postBuf, postBufLen);
      std::string secret = webtk::getVar(conn, "secret", "", postBuf, postBufLen);

      InstallNodeInfoList metaNodes;
      InstallNodeInfoList storageNodes;
      InstallNodeInfoList clientNodes;
      InstallNodeInfoList mgmtNodes;
      setuphelper::getInstallNodeInfo(&metaNodes, &storageNodes, &clientNodes, &mgmtNodes);

      TiXmlElement *mgmtElement = new TiXmlElement("mgmtd");
      rootElement->LinkEndChild(mgmtElement);

      if (!mgmtNodes.empty())
      {
         InstallNodeInfo info = mgmtNodes.front();
         std::string arch;

         switch (info.arch)
         {
            case ARCH_32:
               arch = "x86 32bit";
               break;
            case ARCH_64:
               arch = "x86 64bit";
               break;
            case ARCH_ppc32:
               arch = "PowerPC 32bit";
               break;
            case ARCH_ppc64:
               arch = "PowerPC 64bit";
               break;
         }
         TiXmlElement *nodeElement = new TiXmlElement("node");
         nodeElement->SetAttribute("arch", arch);
         nodeElement->SetAttribute("dist", info.distributionName);
         nodeElement->LinkEndChild(new TiXmlText(info.name));
         mgmtElement->LinkEndChild(nodeElement);
      }

      TiXmlElement *metaElement = new TiXmlElement("meta");
      rootElement->LinkEndChild(metaElement);
      for (InstallNodeInfoListIter iter = metaNodes.begin();
         iter != metaNodes.end(); iter++)
      {
         InstallNodeInfo info = *iter;
         std::string arch;

         switch (info.arch)
         {
            case ARCH_32:
               arch = "x86 32bit";
               break;
            case ARCH_64:
               arch = "x86 64bit";
               break;
            case ARCH_ppc32:
               arch = "PowerPC 32bit";
               break;
            case ARCH_ppc64:
               arch = "PowerPC 64bit";
               break;
         }

         TiXmlElement *nodeElement = new TiXmlElement("node");
         nodeElement->SetAttribute("arch", arch);
         nodeElement->SetAttribute("dist", info.distributionName);
         nodeElement->LinkEndChild(new TiXmlText(info.name));
         metaElement->LinkEndChild(nodeElement);
      }

      TiXmlElement *storageElement = new TiXmlElement("storage");
      rootElement->LinkEndChild(storageElement);
      for (InstallNodeInfoListIter iter = storageNodes.begin();
         iter != storageNodes.end(); iter++)
      {
         InstallNodeInfo info = *iter;
         std::string arch;

         switch (info.arch)
         {
            case ARCH_32:
               arch = "x86 32bit";
               break;
            case ARCH_64:
               arch = "x86 64bit";
               break;
            case ARCH_ppc32:
               arch = "PowerPC 32bit";
               break;
            case ARCH_ppc64:
               arch = "PowerPC 64bit";
               break;
         }

         TiXmlElement *nodeElement = new TiXmlElement("node");
         nodeElement->SetAttribute("arch", arch);
         nodeElement->SetAttribute("dist", info.distributionName);
         nodeElement->LinkEndChild(new TiXmlText(info.name));
         storageElement->LinkEndChild(nodeElement);
      }

      TiXmlElement *clientElement = new TiXmlElement("client");
      rootElement->LinkEndChild(clientElement);
      for (InstallNodeInfoListIter iter = clientNodes.begin();
         iter != clientNodes.end(); iter++)
      {
         InstallNodeInfo info = *iter;
         std::string arch;

         switch (info.arch)
         {
            case ARCH_32:
               arch = "x86 32bit";
               break;
            case ARCH_64:
               arch = "x86 64bit";
               break;
            case ARCH_ppc32:
               arch = "PowerPC 32bit";
               break;
            case ARCH_ppc64:
               arch = "PowerPC 64bit";
               break;
         }

         TiXmlElement *nodeElement = new TiXmlElement("node");
         nodeElement->SetAttribute("arch", arch);
         nodeElement->SetAttribute("dist", info.distributionName);
         nodeElement->LinkEndChild(new TiXmlText(info.name));
         clientElement->LinkEndChild(nodeElement);
      }

      webtk::writeDoc(conn, request_info, doc);
   }
   catch (HttpVarException &e)
   {
      xmlhelper::sendError(conn, request_info, e.what());
   }
}

/*
 * removes the packages on the remote hosts
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void xmlhelper::uninstall(struct mg_connection *conn,
   const struct mg_request_info *request_info, const char* postBuf, size_t postBufLen)
{
#ifdef MONGOOSE_DEBUG
   webtk::debugMongoose(conn,request_info, postBuf, postBufLen);
#endif

   Logger* log = Logger::getLogger();

   TiXmlDocument doc;
   TiXmlElement *rootElement = new TiXmlElement("data");
   doc.LinkEndChild(rootElement);

   try
   {
      std::string package = webtk::getVar(conn, "package", "", postBuf, postBufLen);
      std::string nonceID = webtk::getVar(conn, "nonceID", "-1", postBuf, postBufLen);
      std::string secret = webtk::getVar(conn, "secret", "", postBuf, postBufLen);

      Auth::init();
      if (SecurityTk::checkReply(secret, Auth::getPassword("admin"),
         StringTk::strToInt(nonceID)))
      {
         TiXmlElement *authenticatedElement = new TiXmlElement("authenticated");
         authenticatedElement->LinkEndChild(new TiXmlText("true"));
         rootElement->LinkEndChild(authenticatedElement);

         StringList failedHosts;
         TiXmlElement *failedElement = new TiXmlElement("failedHosts");
         rootElement->LinkEndChild(failedElement);
         if (setuphelper::uninstallPackage(package, &failedHosts) != 0)
         {
            for (StringListIter iter = failedHosts.begin();
               iter != failedHosts.end(); iter++)
            {
               TiXmlElement *entryElement = new TiXmlElement("entry");
               entryElement->LinkEndChild(new TiXmlText(*iter));
               failedElement->LinkEndChild(entryElement);
            }
         }
      }
      else
      {
         TiXmlElement *authenticatedElement = new TiXmlElement("authenticated");
         authenticatedElement->LinkEndChild(new TiXmlText("false"));
         rootElement->LinkEndChild(authenticatedElement);

         log->log(Log_ERR, __func__, "Authentication error.");
      }

      webtk::writeDoc(conn, request_info, doc);
   }
   catch (HttpVarException &e)
   {
      xmlhelper::sendError(conn, request_info, e.what());
   }
}

/*
 * gets all known problems of the beegfs
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void xmlhelper::knownProblems(struct mg_connection *conn,
   const struct mg_request_info *request_info, const char* postBuf, size_t postBufLen)
{
#ifdef MONGOOSE_DEBUG
   webtk::debugMongoose(conn,request_info, postBuf, postBufLen);
#endif

   TiXmlDocument doc;
   TiXmlElement *rootElement = new TiXmlElement("data");
   doc.LinkEndChild(rootElement);

   try
   {
      NodeStoreMetaEx* metaNodeStore = Program::getApp()->getMetaNodes();
      auto metaNodesList = metaNodeStore->referenceAllNodes();
      StringList deadMetaNodes;
      std::string info;
      for (auto iter = metaNodesList.begin(); iter != metaNodesList.end(); iter++)
      {
         if (!(metaNodeStore->statusMeta((*iter)->getNumID(), &info)))
         {
            deadMetaNodes.push_back((*iter)->getID());
         }
      }

      NodeStoreStorageEx *storageNodeStore = Program::getApp()->getStorageNodes();
      auto storageNodesList = storageNodeStore->referenceAllNodes();
      StringList deadStorageNodes;
      for (auto iter = storageNodesList.begin(); iter != storageNodesList.end(); iter++)
      {
         if (!(storageNodeStore->statusStorage((*iter)->getNumID(), &info) ) )
         {
            deadStorageNodes.push_back((*iter)->getID());
         }
      }

      TiXmlElement *metaElement = new TiXmlElement("deadMetaNodes");
      rootElement->LinkEndChild(metaElement);

      if (!deadMetaNodes.empty())
      {
         for (StringListIter iter = deadMetaNodes.begin();
            iter != deadMetaNodes.end(); iter++)
         {
            TiXmlElement *nodeElement = new TiXmlElement("node");
            nodeElement->LinkEndChild(new TiXmlText(*iter));
            metaElement->LinkEndChild(nodeElement);
         }
      }

      TiXmlElement *storageElement = new TiXmlElement("deadStorageNodes");
      rootElement->LinkEndChild(storageElement);

      if (!deadStorageNodes.empty())
      {
         for (StringListIter iter = deadStorageNodes.begin();
            iter != deadStorageNodes.end(); iter++)
         {
            TiXmlElement *nodeElement = new TiXmlElement("node");
            nodeElement->LinkEndChild(new TiXmlText(*iter));
            storageElement->LinkEndChild(nodeElement);
         }
      }

      webtk::writeDoc(conn, request_info, doc);
   }
   catch (HttpVarException &e)
   {
      xmlhelper::sendError(conn, request_info, e.what());
   }
}

/*
 * uploads the setup logfile
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void xmlhelper::getLocalLogFile(struct mg_connection *conn,
   const struct mg_request_info *request_info, const char* postBuf, size_t postBufLen)
{
#ifdef MONGOOSE_DEBUG
   webtk::debugMongoose(conn,request_info, postBuf, postBufLen);
#endif

   Logger* log = Logger::getLogger();

   TiXmlDocument doc;
   TiXmlElement *rootElement = new TiXmlElement("data");
   doc.LinkEndChild(rootElement);

   try
   {
      std::string logStr;
      TiXmlElement *successElement = new TiXmlElement("success");
      rootElement->LinkEndChild(successElement);

      if (managementhelper::getFileContents(SETUP_LOG_PATH, &logStr) == 0)
      {
         successElement->LinkEndChild(new TiXmlText("true"));
         TiXmlElement *logElement = new TiXmlElement("log");
         logElement->LinkEndChild(new TiXmlText(logStr));
         rootElement->LinkEndChild(logElement);
      }
      else
      {
         successElement->LinkEndChild(new TiXmlText("false") );
         log->log(Log_ERR, __func__, "Failed to load logfile: " + SETUP_LOG_PATH);
      }

      webtk::writeDoc(conn, request_info, doc);
   }
   catch (HttpVarException &e)
   {
      xmlhelper::sendError(conn, request_info, e.what());
   }
}

/*
 * gets or stores the eMail notification settings
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void xmlhelper::mailNotification(struct mg_connection *conn,
   const struct mg_request_info *request_info, const char* postBuf, size_t postBufLen)
{
#ifdef MONGOOSE_DEBUG
   webtk::debugMongoose(conn,request_info, postBuf, postBufLen);
#endif

   Logger* log = Logger::getLogger();

   Database *db = Program::getApp()->getDB();
   TiXmlDocument doc;
   TiXmlElement *rootElement = new TiXmlElement("data");
   doc.LinkEndChild(rootElement);

   try
   {
      std::string change = webtk::getVar(conn, "change", "false", postBuf, postBufLen);
      std::string mailEnabled = webtk::getVar(conn, "mailEnabled", "false", postBuf, postBufLen);
      std::string sendType = webtk::getVar(conn, "sendType", "", postBuf, postBufLen);
      std::string sendMailPath = webtk::getVar(conn, "sendmailPath", "", postBuf, postBufLen);
      std::string smtpServer = webtk::getVar(conn, "smtpServer", "", postBuf, postBufLen);
      std::string sender = webtk::getVar(conn, "sender", "", postBuf, postBufLen);
      std::string recipient = webtk::getVar(conn, "recipient", "", postBuf, postBufLen);
      int mailMinDownTimeSec = StringTk::strToInt(
         webtk::getVar(conn, "mailMinDownTimeSec", "0", postBuf, postBufLen) );
      int mailResendMailTimeMin = StringTk::strToInt(
         webtk::getVar(conn, "mailResendMailTimeMin", "0", postBuf, postBufLen) );
      std::string nonceID = webtk::getVar(conn, "nonceID", "-1", postBuf, postBufLen);
      std::string secret = webtk::getVar(conn, "secret", "", postBuf, postBufLen);

      RuntimeConfig *runtimeCfg = Program::getApp()->getRuntimeConfig();
      Config *cfg = Program::getApp()->getConfig();

      if (StringTk::strToBool(change))
      {
         TiXmlElement *errorElement = new TiXmlElement("errors");
         rootElement->LinkEndChild(errorElement);

         bool enableMail = StringTk::strToBool(mailEnabled);
         Auth::init();

         if (!SecurityTk::checkReply(secret, Auth::getPassword("admin"),
            StringTk::strToInt(nonceID)))
         {
            TiXmlElement *entryElement = new TiXmlElement("entry");
            entryElement->LinkEndChild(new TiXmlText(
               "The system could not authenticate you as administrative user."));
            errorElement->LinkEndChild(entryElement);

            log->log(Log_ERR, __func__, "Authentication for admin user failed.");
         }
         else if(enableMail && ( sender.empty() || recipient.empty() ||
            ( (StringTk::strToInt(sendType) == SmtpSendType_SOCKET) && smtpServer.empty() ) ||
            ( (StringTk::strToInt(sendType) == SmtpSendType_SENDMAIL) && sendMailPath.empty() ) ) )
         {
            bool notFirstEmptyValue = false;

            std::string errorMessage = "Could not write new config. Please be aware that if you"
               "activate eMail support, you MUST specify a ";

            if(sender.empty() )
            {
               errorMessage.append("sender");
               notFirstEmptyValue = true;
            }

            if(recipient.empty() )
            {
               if(notFirstEmptyValue)
               {
                  errorMessage.append(", recipient");
               }
               else
               {
                  errorMessage.append("recipient");
                  notFirstEmptyValue = true;
               }

            }

            if( (StringTk::strToInt(sendType) == SmtpSendType_SOCKET) && smtpServer.empty() )
            {
               if(notFirstEmptyValue)
               {
                  errorMessage.append(", SMTP Server");
               }
               else
               {
                  errorMessage.append("SMTP Server");
                  notFirstEmptyValue = true;
               }
            }

            if( (StringTk::strToInt(sendType) == SmtpSendType_SENDMAIL) && sendMailPath.empty() )
            {
               if(notFirstEmptyValue)
               {
                  errorMessage.append(", path to sendmail");
               }
               else
               {
                  errorMessage.append("path to sendmail");
               }
            }
            errorMessage.append(".");

            TiXmlElement *entryElement = new TiXmlElement("entry");
            entryElement->LinkEndChild(new TiXmlText(errorMessage) );
            errorElement->LinkEndChild(entryElement);
         }
         else
         {
            runtimeCfg->setMailEnabled(enableMail);
            runtimeCfg->setMailSmtpSendType( (SmtpSendType)StringTk::strToInt(sendType) );
            runtimeCfg->setMailSendmailPath(sendMailPath);
            runtimeCfg->setMailSmtpServer(smtpServer);
            runtimeCfg->setMailSender(sender);
            runtimeCfg->setMailRecipient(recipient);
            runtimeCfg->setMailMinDownTimeSec(mailMinDownTimeSec);
            runtimeCfg->setMailResendMailTimeMin(mailResendMailTimeMin);

            if (db->writeRuntimeConfig() != 0)
            {
               TiXmlElement *entryElement = new TiXmlElement("entry");
               entryElement->LinkEndChild(new TiXmlText(
                  "Could not write new config. Please be aware that if you activate eMail support, "
                  "you MUST specify a sender and a recipient.") );
               errorElement->LinkEndChild(entryElement);

               log->log(Log_ERR, __func__, "Failed to write runtime configuration to database.");
            }
         }
      }
      else
      {
         TiXmlElement *enabledElement = new TiXmlElement("enabled");
         if (runtimeCfg->getMailEnabled())
         {
            enabledElement->LinkEndChild(new TiXmlText("true"));
         }
         else
         {
            enabledElement->LinkEndChild(new TiXmlText("false"));
         }
         rootElement->LinkEndChild(enabledElement);

         TiXmlElement *overrideActiveElement = new TiXmlElement("overrideActive");
         if (cfg->isMailConfigOverrideActive())
         {
            overrideActiveElement->LinkEndChild(new TiXmlText("true"));
         }
         else
         {
            overrideActiveElement->LinkEndChild(new TiXmlText("false"));
         }
         rootElement->LinkEndChild(overrideActiveElement);

         TiXmlElement *sendTypeElement = new TiXmlElement("sendType");
         sendTypeElement->LinkEndChild(new TiXmlText(
            StringTk::intToStr(runtimeCfg->getMailSmtpSendType() ) ) );
         rootElement->LinkEndChild(sendTypeElement);

         TiXmlElement *sendmailPathElement = new TiXmlElement("sendmailPath");
         sendmailPathElement->LinkEndChild(new TiXmlText(runtimeCfg->getMailSendmailPath() ) );
         rootElement->LinkEndChild(sendmailPathElement);

         TiXmlElement *smtpElement = new TiXmlElement("smtpServer");
         smtpElement->LinkEndChild(new TiXmlText(runtimeCfg->getMailSmtpServer() ) );
         rootElement->LinkEndChild(smtpElement);

         TiXmlElement *senderElement = new TiXmlElement("sender");
         senderElement->LinkEndChild(new TiXmlText(runtimeCfg->getMailSender() ) );
         rootElement->LinkEndChild(senderElement);

         TiXmlElement *recipientElement = new TiXmlElement("recipient");
         recipientElement->LinkEndChild(new TiXmlText(runtimeCfg->getMailRecipient() ) );
         rootElement->LinkEndChild(recipientElement);

         TiXmlElement *delayElement = new TiXmlElement("delay");
         delayElement->LinkEndChild(new TiXmlText(StringTk::intToStr(
            runtimeCfg->getMailMinDownTimeSec() ) ) );
         rootElement->LinkEndChild(delayElement);

         TiXmlElement *resendElement = new TiXmlElement("resendTime");
         resendElement->LinkEndChild(new TiXmlText(StringTk::intToStr(
            runtimeCfg->getMailResendMailTimeMin() ) ) );
         rootElement->LinkEndChild(resendElement);
      }

      webtk::writeDoc(conn, request_info, doc);
   }
   catch (HttpVarException &e)
   {
      xmlhelper::sendError(conn, request_info, e.what());
   }
}

/*
 * gets the details of one metadata node
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void xmlhelper::metaNode(struct mg_connection *conn,
   const struct mg_request_info *request_info, const char* postBuf, size_t postBufLen)
{
#ifdef MONGOOSE_DEBUG
   webtk::debugMongoose(conn,request_info, postBuf, postBufLen);
#endif

   Logger* log = Logger::getLogger();

   NodeStoreMetaEx* metaNodes = Program::getApp()->getMetaNodes();

   TiXmlDocument doc;
   TiXmlElement *rootElement = new TiXmlElement("data");
   doc.LinkEndChild(rootElement);

   try
   {
      uint timeSpanRequests = StringTk::strToUInt(webtk::getVar(
         conn, "timeSpanRequests", "10", postBuf, postBufLen).c_str() );

      std::string node = webtk::getVar(conn, "node", postBuf, postBufLen);
      NumNodeID nodeNumID(StringTk::strToUInt(webtk::getVar(conn, "nodeNumID", postBuf,
         postBufLen) ) );
      TiXmlElement *generalElement = new TiXmlElement("general");
      rootElement->LinkEndChild(generalElement);

      TiXmlElement *nodeIDElement = new TiXmlElement("nodeID");
      nodeIDElement->LinkEndChild(new TiXmlText(node));
      generalElement->LinkEndChild(nodeIDElement);
      TiXmlElement *nodeNumIDElement = new TiXmlElement("nodeNumID");
      nodeNumIDElement->LinkEndChild(new TiXmlText(nodeNumID.str()));
      generalElement->LinkEndChild(nodeNumIDElement);

      std::string outInfo;
      if (metaNodes->statusMeta(nodeNumID, &outInfo))
      {
         TiXmlElement *statusElement = new TiXmlElement("status");
         statusElement->LinkEndChild(new TiXmlText("true"));
         generalElement->LinkEndChild(statusElement);
      }
      else
      {
         TiXmlElement *statusElement = new TiXmlElement("status");
         statusElement->LinkEndChild(new TiXmlText("false"));
         generalElement->LinkEndChild(statusElement);
      }

      GeneralNodeInfo info;

      if (metaNodes->getGeneralMetaNodeInfo(nodeNumID, &info))
      {
         TiXmlElement *cpuModelElement = new TiXmlElement("cpuModel");
         cpuModelElement->LinkEndChild(new TiXmlText(info.cpuName));
         generalElement->LinkEndChild(cpuModelElement);

         TiXmlElement *cpuCountElement = new TiXmlElement("cpuCount");
         cpuCountElement->LinkEndChild(new TiXmlText(
            StringTk::intToStr(info.cpuCount)));
         generalElement->LinkEndChild(cpuCountElement);

         char cpuSpeedStr[32];
         sprintf(cpuSpeedStr, "%0.1lf", (double) (info.cpuSpeed) / 1000);
         TiXmlElement *cpuSpeedElement = new TiXmlElement("cpuSpeed");
         cpuSpeedElement->LinkEndChild(
            new TiXmlText(std::string(cpuSpeedStr) + std::string(" GHz")));
         generalElement->LinkEndChild(cpuSpeedElement);

         char ramStr[32];
         sprintf(ramStr, "%0.1lf", (double) (info.memTotal) / 1000);
         TiXmlElement *ramSizeElement = new TiXmlElement("ramSize");
         ramSizeElement->LinkEndChild(new TiXmlText(std::string(ramStr) +
            std::string(" GB")));
         generalElement->LinkEndChild(ramSizeElement);
      }
      else
      {
         TiXmlElement *cpuModelElement = new TiXmlElement("cpuModel");
         cpuModelElement->LinkEndChild(new TiXmlText(""));
         generalElement->LinkEndChild(cpuModelElement);

         TiXmlElement *cpuCountElement = new TiXmlElement("cpuCount");
         cpuCountElement->LinkEndChild(new TiXmlText(""));
         generalElement->LinkEndChild(cpuCountElement);

         TiXmlElement *cpuSpeedElement = new TiXmlElement("cpuSpeed");
         cpuSpeedElement->LinkEndChild(new TiXmlText(""));
         generalElement->LinkEndChild(cpuSpeedElement);

         TiXmlElement *ramSizeElement = new TiXmlElement("ramSize");
         ramSizeElement->LinkEndChild(new TiXmlText(""));
         generalElement->LinkEndChild(ramSizeElement);

         log->log(Log_ERR, __func__, "Failed to get general node info of metadata node: " + node);
      }

      TiXmlElement *rootNodeElement = new TiXmlElement("rootNode");
      rootNodeElement->LinkEndChild(new TiXmlText(metaNodes->isRootMetaNode(nodeNumID)));
      generalElement->LinkEndChild(rootNodeElement);

      TiXmlElement *lastMessageElement = new TiXmlElement("lastMessage");
      lastMessageElement->LinkEndChild(new TiXmlText(
         metaNodes->timeSinceLastMessageMeta(nodeNumID)));
      generalElement->LinkEndChild(lastMessageElement);

      TiXmlElement *openSessionsElement = new TiXmlElement("openSessions");
      openSessionsElement->LinkEndChild(new TiXmlText(StringTk::uintToStr(
         metaNodes->sessionCountMeta(nodeNumID))));
      generalElement->LinkEndChild(openSessionsElement);

      StringList times;
      StringList workRequests;
      StringList queuedRequests;
      metaNodes->metaDataRequests(nodeNumID, timeSpanRequests, &times, &queuedRequests,
         &workRequests);

      TiXmlElement *workRequestsElement = new TiXmlElement("workRequests");
      rootElement->LinkEndChild(workRequestsElement);
      TiXmlElement *queuedRequestsElement = new TiXmlElement("queuedRequests");
      rootElement->LinkEndChild(queuedRequestsElement);

      StringListIter timesIter = times.begin();
      StringListIter workRequestsIter = workRequests.begin();
      StringListIter queuedRequestsIter = queuedRequests.begin();

      while (timesIter != times.end())
      {
         if (workRequestsIter != workRequests.end())
         {
            TiXmlElement *workRequestsValueElement =new TiXmlElement("value");
            workRequestsValueElement->SetAttribute("time", *timesIter);
            workRequestsValueElement->LinkEndChild(new TiXmlText(*workRequestsIter));
            workRequestsElement->LinkEndChild(workRequestsValueElement);

            workRequestsIter++;
         }

         if (queuedRequestsIter != queuedRequests.end())
         {
            TiXmlElement *queuedRequestsValueElement = new TiXmlElement("value");
            queuedRequestsValueElement->SetAttribute("time", *timesIter);
            queuedRequestsValueElement->LinkEndChild(new TiXmlText(*queuedRequestsIter));
            queuedRequestsElement->LinkEndChild(queuedRequestsValueElement);
            queuedRequestsIter++;
         }
         timesIter++;
      }

      webtk::writeDoc(conn, request_info, doc);
   }
   catch (HttpVarException &e)
   {
      xmlhelper::sendError(conn, request_info, e.what());
   }
}

/*
 * gets the overview about all metadata nodes
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void xmlhelper::metaNodesOverview(struct mg_connection *conn,
   const struct mg_request_info *request_info, const char* postBuf, size_t postBufLen)
{
#ifdef MONGOOSE_DEBUG
   webtk::debugMongoose(conn,request_info, postBuf, postBufLen);
#endif

   NodeStoreMetaEx* metaNodes = Program::getApp()->getMetaNodes();
   Database *db = Program::getApp()->getDB();

   TiXmlDocument doc;
   TiXmlElement *rootElement = new TiXmlElement("data");
   doc.LinkEndChild(rootElement);

   try
   {
      uint timeSpanRequests = StringTk::strToUInt(
         webtk::getVar(conn, "timeSpanRequests", "10", postBuf, postBufLen).c_str());

      std::string group = webtk::getVar(conn, "group", "", postBuf, postBufLen);

      NumNodeIDList nodeNumIDs;
      StringList nodes;

      if (group.compare("") == 0)
      {
         Program::getApp()->getMetaNodeNumIDs(&nodeNumIDs);
         Program::getApp()->getMetaNodesAsStringList(&nodes);
      }
      else
      {
         db->getMetaNodesInGroup(group, &nodeNumIDs);
         db->getMetaNodesInGroup(group, &nodes);
      }

      TiXmlElement *generalElement = new TiXmlElement("general");
      rootElement->LinkEndChild(generalElement);

      TiXmlElement *nodeCountElement = new TiXmlElement("nodeCount");
      nodeCountElement->LinkEndChild(new TiXmlText(
         StringTk::intToStr(metaNodes->getSize() ) ) );
      generalElement->LinkEndChild(nodeCountElement);

      TiXmlElement *rootNodeElement = new TiXmlElement("rootNode");
      rootNodeElement->LinkEndChild(new TiXmlText(metaNodes->getRootMetaNodeAsTypedNodeID() ) );
      generalElement->LinkEndChild(rootNodeElement);

      TiXmlElement *statusElement = new TiXmlElement("status");
      rootElement->LinkEndChild(statusElement);

      NumNodeIDListIter nodeNumIDIter = nodeNumIDs.begin();
      for (StringListIter nodeIDIter = nodes.begin(); nodeIDIter != nodes.end(); nodeIDIter++,
            nodeNumIDIter++)
      {
         std::string outInfo;
         std::string nodeID = *nodeIDIter;
         NumNodeID nodeNumID = *nodeNumIDIter;

         TiXmlElement *valueElement = new TiXmlElement("value");
         statusElement->LinkEndChild(valueElement);
         valueElement->SetAttribute("node", nodeID);
         valueElement->SetAttribute("nodeNumID", nodeNumID.str());

         if (metaNodes->statusMeta(nodeNumID, &outInfo))
            valueElement->LinkEndChild(new TiXmlText("true"));
         else
            valueElement->LinkEndChild(new TiXmlText("false"));
      }

      StringList times;
      StringList workRequests;
      StringList queuedRequests;
      metaNodes->metaDataRequestsSum(timeSpanRequests, &times, &queuedRequests, &workRequests);

      TiXmlElement *workRequestsElement = new TiXmlElement("workRequests");
      rootElement->LinkEndChild(workRequestsElement);
      TiXmlElement *queuedRequestsElement = new TiXmlElement("queuedRequests");
      rootElement->LinkEndChild(queuedRequestsElement);

      StringListIter timesIter = times.begin();
      StringListIter workRequestsIter = workRequests.begin();
      StringListIter queuedRequestsIter = queuedRequests.begin();

      while (timesIter != times.end())
      {
         if (workRequestsIter != workRequests.end())
         {
            TiXmlElement *workRequestsValueElement = new TiXmlElement("value");
            workRequestsValueElement->SetAttribute("time", *timesIter);
            workRequestsValueElement->LinkEndChild(
               new TiXmlText(*workRequestsIter));
            workRequestsElement->LinkEndChild(workRequestsValueElement);

            workRequestsIter++;
         }

         if (queuedRequestsIter != queuedRequests.end())
         {
            TiXmlElement *queuedRequestsValueElement =
               new TiXmlElement("value");
            queuedRequestsValueElement->SetAttribute("time", *timesIter);
            queuedRequestsValueElement->LinkEndChild(
               new TiXmlText(*queuedRequestsIter));
            queuedRequestsElement->LinkEndChild(queuedRequestsValueElement);

            queuedRequestsIter++;
         }

         timesIter++;
      }

      webtk::writeDoc(conn, request_info, doc);
   }
   catch (HttpVarException &e)
   {
      xmlhelper::sendError(conn, request_info, e.what());
   }
}

/*
 * gets a node list with the group informations
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void xmlhelper::nodeList(struct mg_connection *conn,
   const struct mg_request_info *request_info, const char* postBuf, size_t postBufLen)
{
#ifdef MONGOOSE_DEBUG
   webtk::debugMongoose(conn,request_info, postBuf, postBufLen);
#endif

   App* app = Program::getApp();
   Database *db = app->getDB();

   TiXmlDocument doc;
   TiXmlElement *rootElement = new TiXmlElement("data");
   doc.LinkEndChild(rootElement);

   try
   {
      bool clients = StringTk::strToBool(
         webtk::getVar(conn, "clients", "false", postBuf, postBufLen));
      bool admon = StringTk::strToBool(
         webtk::getVar(conn, "admon", "false", postBuf, postBufLen));


      TiXmlElement *mgmtdElement = new TiXmlElement("mgmtd");
      rootElement->LinkEndChild(mgmtdElement);

      NodeStoreMgmtEx *mgmtdNodeStore = app->getMgmtNodes();
      auto node = mgmtdNodeStore->referenceFirstNode();
      while (node)
      {
         std::string groupName = "Default";
         std::string nodeID = node->getID();
         NumNodeID nodeNumID = node->getNumID();

         TiXmlElement *nodeElement = new TiXmlElement("node");
         nodeElement->SetAttribute("group", groupName);
         nodeElement->SetAttribute("nodeNumID", nodeNumID.str() );
         nodeElement->LinkEndChild(new TiXmlText(nodeID));
         mgmtdElement->LinkEndChild(nodeElement);

         node = mgmtdNodeStore->referenceNextNode(node);
      }

      TiXmlElement *metaElement = new TiXmlElement("meta");
      rootElement->LinkEndChild(metaElement);

      NodeStoreMetaEx *metaNodeStore = app->getMetaNodes();
      node = metaNodeStore->referenceFirstNode();
      while (node != NULL)
      {
         std::string groupName = "Default";
         std::string nodeID = node->getID();
         NumNodeID nodeNumID = node->getNumID();

         int groupID = db->metaNodeGetGroup(nodeNumID);
         if (groupID >= 0)
            groupName = db->getGroupName(groupID);

         TiXmlElement *nodeElement = new TiXmlElement("node");
         nodeElement->SetAttribute("group", groupName);
         nodeElement->SetAttribute("nodeNumID", nodeNumID.str());
         nodeElement->LinkEndChild(new TiXmlText(nodeID));
         metaElement->LinkEndChild(nodeElement);

         node = metaNodeStore->referenceNextNode(node);
      }

      TiXmlElement *storageElement = new TiXmlElement("storage");
      rootElement->LinkEndChild(storageElement);
      NodeStoreStorageEx *storageNodeStore = app->getStorageNodes();
      node = storageNodeStore->referenceFirstNode();
      while (node)
      {
         std::string groupName = "Default";
         std::string nodeID = node->getID();
         NumNodeID nodeNumID = node->getNumID();

         int groupID = db->storageNodeGetGroup(nodeNumID);
         if (groupID >= 0)
            groupName = db->getGroupName(groupID);

         TiXmlElement *nodeElement = new TiXmlElement("node");
         nodeElement->SetAttribute("group", groupName);
         nodeElement->SetAttribute("nodeNumID", nodeNumID.str());
         nodeElement->LinkEndChild(new TiXmlText(nodeID));
         storageElement->LinkEndChild(nodeElement);

         node = storageNodeStore->referenceNextNode(node);
      }

      if (clients)
      {
         TiXmlElement *clientElement = new TiXmlElement("client");
         rootElement->LinkEndChild(clientElement);

         NodeStoreClients *clientNodeStore = app->getClientNodes();

         auto node = clientNodeStore->referenceFirstNode();
         while (node)
         {
            std::string groupName = "Default";
            std::string nodeID = node->getID();
            NumNodeID nodeNumID = node->getNumID();

            TiXmlElement *nodeElement = new TiXmlElement("node");
            nodeElement->SetAttribute("group", groupName);
            nodeElement->SetAttribute("nodeNumID", nodeNumID.str());
            nodeElement->LinkEndChild(new TiXmlText(nodeID));
            clientElement->LinkEndChild(nodeElement);

            node = clientNodeStore->referenceNextNode(node);
         }
      }

      if(admon)
      {
         TiXmlElement *admonElement = new TiXmlElement("admon");
         rootElement->LinkEndChild(admonElement);

         Node& localNode = app->getLocalNode();
         std::string groupName = "Default";
         std::string nodeID = localNode.getID();
         NumNodeID nodeNumID = localNode.getNumID();

         TiXmlElement *nodeElement = new TiXmlElement("node");
         nodeElement->SetAttribute("group", groupName);
         nodeElement->SetAttribute("nodeNumID", nodeNumID.str());
         nodeElement->LinkEndChild(new TiXmlText(nodeID));
         admonElement->LinkEndChild(nodeElement);
      }

      webtk::writeDoc(conn, request_info, doc);
   }
   catch (HttpVarException &e)
   {
      xmlhelper::sendError(conn, request_info, e.what());
   }
}

/*
 * checks if the passwordless access for the info users is enabled
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void xmlhelper::preAuthInfo(struct mg_connection *conn,
   const struct mg_request_info *request_info, const char* postBuf, size_t postBufLen)
{
#ifdef MONGOOSE_DEBUG
   webtk::debugMongoose(conn,request_info, postBuf, postBufLen);
#endif

   TiXmlDocument doc;
   TiXmlElement *rootElement = new TiXmlElement("data");
   doc.LinkEndChild(rootElement);

   try
   {
      TiXmlElement *infoDisabledElement = new TiXmlElement("infoDisabled");
      rootElement->LinkEndChild(infoDisabledElement);

      if (Auth::getInformationDisabled())
         infoDisabledElement->LinkEndChild(new TiXmlText("true"));
      else
         infoDisabledElement->LinkEndChild(new TiXmlText("false"));

      webtk::writeDoc(conn, request_info, doc);
   }
   catch (HttpVarException &e)
   {
      xmlhelper::sendError(conn, request_info, e.what());
   }
}

/*
 * gets or stores the script settings
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void xmlhelper::scriptSettings(struct mg_connection *conn,
   const struct mg_request_info *request_info, const char* postBuf, size_t postBufLen)
{
#ifdef MONGOOSE_DEBUG
   webtk::debugMongoose(conn,request_info, postBuf, postBufLen);
#endif

   Logger* log = Logger::getLogger();
   Database *db = Program::getApp()->getDB();

   TiXmlDocument doc;
   TiXmlElement *rootElement = new TiXmlElement("data");
   doc.LinkEndChild(rootElement);

   try
   {
      std::string change = webtk::getVar(conn, "change", "false", postBuf, postBufLen);
      std::string scriptPath = webtk::getVar(conn, "scriptPath", "", postBuf, postBufLen);
      std::string nonceID = webtk::getVar(conn, "nonceID", "-1", postBuf, postBufLen);
      std::string secret = webtk::getVar(conn, "secret", "", postBuf, postBufLen);

      RuntimeConfig *cfg = Program::getApp()->getRuntimeConfig();
      if (StringTk::strToBool(change))
      {
         TiXmlElement *errorElement = new TiXmlElement("errors");
         rootElement->LinkEndChild(errorElement);

         Auth::init();

         if (!SecurityTk::checkReply(secret, Auth::getPassword("admin"),
            StringTk::strToInt(nonceID)))
         {
            TiXmlElement *entryElement = new TiXmlElement("entry");
            entryElement->LinkEndChild(new TiXmlText(
               "The system could not authenticate you as administrative user"));
            errorElement->LinkEndChild(entryElement);

            log->log(Log_ERR, __func__, "Authentication for admin user failed.");
         }
         else
         {
            if ((!scriptPath.empty()) &&
               (access(scriptPath.c_str(), F_OK) != 0))
            {
               // path does not exist or is not a file
               TiXmlElement *entryElement = new TiXmlElement("entry");
               entryElement->LinkEndChild(new TiXmlText("New config not written (path does not "
                  "exist or is not a file)."));
               errorElement->LinkEndChild(entryElement);

               log->log(Log_ERR, __func__, "New config not written (path does not exist or is not "
                  "a file). File: " + scriptPath);
            }
            else
            if ((!scriptPath.empty()) && (access(scriptPath.c_str(),
               X_OK) != 0))
            {
               // script not executable
               TiXmlElement *entryElement = new TiXmlElement("entry");
               entryElement->LinkEndChild(new TiXmlText(
                  "New config not written (script is not executable).") );
               errorElement->LinkEndChild(entryElement);

               log->log(Log_ERR, __func__, "New config not written (script is not executable). "
                  "File: " + scriptPath);
            }
            else
            {
               cfg->setScriptPath(scriptPath);

               if (db->writeRuntimeConfig() != 0)
               {
                  TiXmlElement *entryElement = new TiXmlElement("entry");
                  entryElement->LinkEndChild(new TiXmlText(
                     "Could not write new config (database error)."));
                  errorElement->LinkEndChild(entryElement);

                  log->log(Log_ERR, __func__, "Could not store runtime configuration to database.");
               }
            }
         }
      }
      else
      {
         TiXmlElement *scriptPathElement = new TiXmlElement("scriptPath");
         scriptPathElement->LinkEndChild(new TiXmlText(cfg->getScriptPath()));
         rootElement->LinkEndChild(scriptPathElement);
      }

      webtk::writeDoc(conn, request_info, doc);
   }
   catch (HttpVarException &e)
   {
      xmlhelper::sendError(conn, request_info, e.what());
   }
}

/*
 * creates a new setup logfile
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void xmlhelper::createNewSetupLogfile(struct mg_connection *conn,
   const struct mg_request_info *request_info, const char* postBuf, size_t postBufLen)
{
#ifdef MONGOOSE_DEBUG
   webtk::debugMongoose(conn,request_info, postBuf, postBufLen);
#endif

   Logger* log = Logger::getLogger();

   TiXmlDocument doc;
   TiXmlElement *rootElement = new TiXmlElement("data");
   doc.LinkEndChild(rootElement);

   try
   {
      std::string nonceID = webtk::getVar(conn, "nonceID", "-1", postBuf, postBufLen);
      std::string secret = webtk::getVar(conn, "secret", "", postBuf, postBufLen);
      std::string type = webtk::getVar(conn, "type", "", postBuf, postBufLen);

      Auth::init();

      if (SecurityTk::checkReply(secret, Auth::getPassword("admin"),
         StringTk::strToInt(nonceID)))
      {
         TiXmlElement *authenticatedElement = new TiXmlElement("authenticated");
         authenticatedElement->LinkEndChild(new TiXmlText("true"));
         rootElement->LinkEndChild(authenticatedElement);
         webtk::writeDoc(conn, request_info, doc);
         setuphelper::createNewSetupLogfile(type);
      }
      else
      {
         TiXmlElement *authenticatedElement = new TiXmlElement("authenticated");
         authenticatedElement->LinkEndChild(new TiXmlText("false"));
         rootElement->LinkEndChild(authenticatedElement);
         webtk::writeDoc(conn, request_info, doc);

         log->log(Log_ERR, __func__, "Authentication for admin user failed.");
      }
   }
   catch (HttpVarException &e)
   {
      xmlhelper::sendError(conn, request_info, e.what());
   }
}

/*
 * starts or stops beegfs daemons or clients
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void xmlhelper::startStop(struct mg_connection *conn,
   const struct mg_request_info *request_info, const char* postBuf, size_t postBufLen)
{
#ifdef MONGOOSE_DEBUG
   webtk::debugMongoose(conn,request_info, postBuf, postBufLen);
#endif

   Logger* log = Logger::getLogger();

   TiXmlDocument doc;
   TiXmlElement *rootElement = new TiXmlElement("data");
   doc.LinkEndChild(rootElement);

   try
   {
      std::string nonceID = webtk::getVar(conn, "nonceID", "-1", postBuf, postBufLen);
      std::string secret = webtk::getVar(conn, "secret", "", postBuf, postBufLen);
      std::string start = webtk::getVar(conn, "start", "false", postBuf, postBufLen);
      std::string stop = webtk::getVar(conn, "stop", "false", postBuf, postBufLen);
      std::string startAll = webtk::getVar(conn, "startAll", "false", postBuf, postBufLen);
      std::string stopAll = webtk::getVar(conn, "stopAll", "false", postBuf, postBufLen);
      std::string restartAdmon = webtk::getVar(conn, "restartAdmon", "false", postBuf, postBufLen);
      std::string host = webtk::getVar(conn, "host", "", postBuf, postBufLen);
      std::string service = webtk::getVar(conn, "service", "", postBuf, postBufLen);

      TiXmlElement *authElement = new TiXmlElement("authenticated");
      rootElement->LinkEndChild(authElement);

      if (StringTk::strToBool(restartAdmon))
      {
         Auth::init();

         if (SecurityTk::checkReply(secret, Auth::getPassword("admin"),
            StringTk::strToInt(nonceID)))
         {
            authElement->LinkEndChild(new TiXmlText("true"));
            managementhelper::restartAdmon();
         }
         else
         {
            authElement->LinkEndChild(new TiXmlText("false"));
            log->log(Log_ERR, __func__, "Authentication for admin user failed.");
         }
      }
      else
      if (StringTk::strToBool(startAll))
      {
         Auth::init();

         if (SecurityTk::checkReply(secret, Auth::getPassword("admin"),
            StringTk::strToInt(nonceID)))
         {
            authElement->LinkEndChild(new TiXmlText("true"));
            StringList failedNodes;
            if(managementhelper::startService(service, &failedNodes) )
               log->log(Log_ERR, __func__, "Failed to execute start all services script. "
                  "Service: " + service);

            TiXmlElement *failedHostsElement = new TiXmlElement("failedHosts");
            rootElement->LinkEndChild(failedHostsElement);

            for (StringListIter iter = failedNodes.begin(); iter != failedNodes.end(); iter++)
            {
               TiXmlElement *nodeElement = new TiXmlElement("node");
               nodeElement->LinkEndChild(new TiXmlText(*iter));
               failedHostsElement->LinkEndChild(nodeElement);
            }
         }
         else
         {
            authElement->LinkEndChild(new TiXmlText("false"));
            log->log(Log_ERR, __func__, "Authentication for admin user failed.");
         }
      }
      else
      if (StringTk::strToBool(stopAll))
      {
         Auth::init();

         if (SecurityTk::checkReply(secret, Auth::getPassword("admin"),
            StringTk::strToInt(nonceID)))
         {
            authElement->LinkEndChild(new TiXmlText("true"));
            StringList failedNodes;
            if(managementhelper::stopService(service, &failedNodes) )
               log->log(Log_ERR, __func__, "Failed to execute stop all services script. Service: " +
                  service);

            TiXmlElement *failedHostsElement = new TiXmlElement("failedHosts");
            rootElement->LinkEndChild(failedHostsElement);

            for (StringListIter iter = failedNodes.begin();
               iter != failedNodes.end(); iter++)
            {
               TiXmlElement *nodeElement = new TiXmlElement("node");
               nodeElement->LinkEndChild(new TiXmlText(*iter));
               failedHostsElement->LinkEndChild(nodeElement);
            }
         }
         else
         {
            authElement->LinkEndChild(new TiXmlText("false"));
            log->log(Log_ERR, __func__, "Authentication for admin user failed.");
         }
      }
      else
      if (StringTk::strToBool(start))
      {
         Auth::init();

         if (SecurityTk::checkReply(secret, Auth::getPassword("admin"),
            StringTk::strToInt(nonceID)))
         {
            authElement->LinkEndChild(new TiXmlText("true"));

            if (managementhelper::startService(service, host) != 0)
            {
               TiXmlElement *failedHostsElement =
                  new TiXmlElement("failedHosts");
               rootElement->LinkEndChild(failedHostsElement);
               TiXmlElement *nodeElement = new TiXmlElement("node");
               nodeElement->LinkEndChild(new TiXmlText(host));
               failedHostsElement->LinkEndChild(nodeElement);
            }
         }
         else
         {
            authElement->LinkEndChild(new TiXmlText("false"));
            log->log(Log_ERR, __func__, "Authentication for admin user failed.");
         }
      }
      else
      if (StringTk::strToBool(stop))
      {
         Auth::init();

         if (SecurityTk::checkReply(secret, Auth::getPassword("admin"),
            StringTk::strToInt(nonceID)))
         {
            authElement->LinkEndChild(new TiXmlText("true"));

            if (managementhelper::stopService(service, host) != 0)
            {
               TiXmlElement *failedHostsElement = new TiXmlElement("failedHosts");
               rootElement->LinkEndChild(failedHostsElement);
               TiXmlElement *nodeElement = new TiXmlElement("node");
               nodeElement->LinkEndChild(new TiXmlText(host));
               failedHostsElement->LinkEndChild(nodeElement);
            }
         }
         else
         {
            authElement->LinkEndChild(new TiXmlText("false"));
            log->log(Log_ERR, __func__, "Authentication for admin user failed.");
         }
      }
      else
      {
         authElement->LinkEndChild(new TiXmlText("true"));

         StringList runningHosts;
         StringList stoppedHosts;
         managementhelper::checkStatus(service, &runningHosts, &stoppedHosts);
         {
            log->log(Log_ERR, __func__, "Failed to execute status of all services script. "
               "Service: " + service);
         }

         TiXmlElement *runningElement = new TiXmlElement("runningHosts");
         rootElement->LinkEndChild(runningElement);
         for (StringListIter iter = runningHosts.begin();
            iter != runningHosts.end(); iter++)
         {
            TiXmlElement *nodeElement = new TiXmlElement("node");
            nodeElement->LinkEndChild(new TiXmlText(*iter));
            runningElement->LinkEndChild(nodeElement);
         }

         TiXmlElement *stoppedElement = new TiXmlElement("stoppedHosts");
         rootElement->LinkEndChild(stoppedElement);
         for (StringListIter iter = stoppedHosts.begin();
            iter != stoppedHosts.end(); iter++)
         {
            TiXmlElement *nodeElement = new TiXmlElement("node");
            nodeElement->LinkEndChild(new TiXmlText(*iter));
            stoppedElement->LinkEndChild(nodeElement);
         }
      }

      webtk::writeDoc(conn, request_info, doc);
   }
   catch (HttpVarException &e)
   {
      xmlhelper::sendError(conn, request_info, e.what());
   }
}

/*
 * gets the details of one storage node
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void xmlhelper::storageNode(struct mg_connection *conn,
   const struct mg_request_info *request_info, const char* postBuf, size_t postBufLen)
{
#ifdef MONGOOSE_DEBUG
   webtk::debugMongoose(conn,request_info, postBuf, postBufLen);
#endif

   Logger* log = Logger::getLogger();
   NodeStoreStorageEx* storageNodes = Program::getApp()->getStorageNodes();

   TiXmlDocument doc;
   TiXmlElement *rootElement = new TiXmlElement("data");
   doc.LinkEndChild(rootElement);

   try
   {
      std::string node = webtk::getVar(conn, "node", postBuf, postBufLen);
      NumNodeID nodeNumID(StringTk::strToUInt(webtk::getVar(conn, "nodeNumID", postBuf,
         postBufLen) ) );
      uint64_t timeSpanPerf = StringTk::strToUInt64(
         webtk::getVar(conn, "timeSpanPerf", "10", postBuf, postBufLen).c_str());

      long endTimestamp;
      long startTimestamp = calculateStartTime(timeSpanPerf, endTimestamp);

      TiXmlElement *generalElement = new TiXmlElement("general");
      rootElement->LinkEndChild(generalElement);

      TiXmlElement *nodeIDElement = new TiXmlElement("nodeID");
      nodeIDElement->LinkEndChild(new TiXmlText(node));
      generalElement->LinkEndChild(nodeIDElement);
      TiXmlElement *nodeNumIDElement = new TiXmlElement("nodeNumID");
      nodeNumIDElement->LinkEndChild(new TiXmlText(nodeNumID.str()));
      generalElement->LinkEndChild(nodeNumIDElement);

      TiXmlElement *statusElement = new TiXmlElement("status");
      generalElement->LinkEndChild(statusElement);

      std::string outInfo;
      if (storageNodes->statusStorage(nodeNumID, &outInfo))
      {
         statusElement->LinkEndChild(new TiXmlText("true"));
      }
      else
      {
         statusElement->LinkEndChild(new TiXmlText("false"));
      }

      GeneralNodeInfo info;

      if (storageNodes->getGeneralStorageNodeInfo(nodeNumID, &info))
      {
         TiXmlElement *cpuModelElement = new TiXmlElement("cpuModel");
         cpuModelElement->LinkEndChild(new TiXmlText(info.cpuName));
         generalElement->LinkEndChild(cpuModelElement);

         TiXmlElement *cpuCountElement = new TiXmlElement("cpuCount");
         cpuCountElement->LinkEndChild(new TiXmlText(
            StringTk::intToStr(info.cpuCount)));
         generalElement->LinkEndChild(cpuCountElement);

         char cpuSpeedStr[32];
         sprintf(cpuSpeedStr, "%0.1lf", (double) (info.cpuSpeed) / 1000);

         TiXmlElement *cpuSpeedElement = new TiXmlElement("cpuSpeed");
         cpuSpeedElement->LinkEndChild(new TiXmlText(
            std::string(cpuSpeedStr) + "GHz"));
         generalElement->LinkEndChild(cpuSpeedElement);

         char ramStr[32];
         sprintf(ramStr, "%0.1lf", (double) (info.memTotal) / 1000);
         TiXmlElement *ramSizeElement = new TiXmlElement("ramSize");
         ramSizeElement->LinkEndChild(
            new TiXmlText(std::string(ramStr) + "GB"));
         generalElement->LinkEndChild(ramSizeElement);
      }
      else
      {
         TiXmlElement *cpuModelElement = new TiXmlElement("cpuModel");
         cpuModelElement->LinkEndChild(new TiXmlText(""));
         generalElement->LinkEndChild(cpuModelElement);

         TiXmlElement *cpuCountElement = new TiXmlElement("cpuCount");
         cpuCountElement->LinkEndChild(new TiXmlText(""));
         generalElement->LinkEndChild(cpuCountElement);

         TiXmlElement *cpuSpeedElement = new TiXmlElement("cpuSpeed");
         cpuSpeedElement->LinkEndChild(new TiXmlText(""));
         generalElement->LinkEndChild(cpuSpeedElement);

         TiXmlElement *ramSizeElement = new TiXmlElement("ramSize");
         ramSizeElement->LinkEndChild(new TiXmlText(""));
         generalElement->LinkEndChild(ramSizeElement);

         log->log(Log_ERR, __func__, "Failed to get general node info of storage node: " + node);
      }

      TiXmlElement *lastMessageElement = new TiXmlElement("lastMessage");
      lastMessageElement->LinkEndChild(new TiXmlText(
         storageNodes->timeSinceLastMessageStorage(nodeNumID)));
      generalElement->LinkEndChild(lastMessageElement);

      TiXmlElement *openSessionsElement = new TiXmlElement("openSessions");
      openSessionsElement->LinkEndChild(new TiXmlText(StringTk::uintToStr(
         storageNodes->sessionCountStorage(nodeNumID) ) ) );
      generalElement->LinkEndChild(openSessionsElement);

      TiXmlElement *storageTargetsElement = new TiXmlElement("storageTargets");
      rootElement->LinkEndChild(storageTargetsElement);
      StorageTargetInfoList storageTargets;
      storageNodes->storageTargetsInfo(nodeNumID, &storageTargets);

      for (StorageTargetInfoListIter iter = storageTargets.begin();
         iter != storageTargets.end(); iter++)
      {
         StorageTargetInfo targetInfo = *iter;
         TiXmlElement *targetElement = new TiXmlElement("target");
         storageTargetsElement->LinkEndChild(targetElement);
         targetElement->LinkEndChild(new TiXmlText(StringTk::uintToStr(targetInfo.getTargetID())));
         targetElement->SetAttribute("diskSpaceTotal", StringTk::int64ToStr(
            targetInfo.getDiskSpaceTotal()));
         targetElement->SetAttribute("diskSpaceFree",
            StringTk::int64ToStr(targetInfo.getDiskSpaceFree()));
         targetElement->SetAttribute("pathStr", targetInfo.getPathStr());
      }

      std::string unit;

      TiXmlElement *diskPerfSummaryElement =
         new TiXmlElement("diskPerfSummary");
      rootElement->LinkEndChild(diskPerfSummaryElement);

      TiXmlElement *diskReadSumElement = new TiXmlElement("diskReadSum");
      diskReadSumElement->LinkEndChild(new TiXmlText(storageNodes->diskRead(nodeNumID,
         timeSpanPerf, &unit) + " " + unit));
      diskPerfSummaryElement->LinkEndChild(diskReadSumElement);

      TiXmlElement *diskWriteSumElement = new TiXmlElement("diskWriteSum");
      diskWriteSumElement->LinkEndChild(new TiXmlText(storageNodes->diskWrite(nodeNumID,
         timeSpanPerf, &unit) + " " + unit));
      diskPerfSummaryElement->LinkEndChild(diskWriteSumElement);

      UInt64List timesRead;
      UInt64List read;
      UInt64List timesAverageRead;
      UInt64List averageRead;
      storageNodes->diskPerfRead(nodeNumID, timeSpanPerf, &timesRead, &read,
         &timesAverageRead, &averageRead, startTimestamp, endTimestamp);

      UInt64List timesWrite;
      UInt64List write;
      UInt64List timesAverageWrite;
      UInt64List averageWrite;
      storageNodes->diskPerfWrite(nodeNumID, timeSpanPerf, &timesWrite, &write,
         &timesAverageWrite, &averageWrite, startTimestamp, endTimestamp);

      TiXmlElement *diskPerfReadElement = new TiXmlElement("diskPerfRead");
      rootElement->LinkEndChild(diskPerfReadElement);

      for (ZipIterRange<UInt64List, UInt64List> timesReadIter(timesRead, read);
           !timesReadIter.empty(); ++timesReadIter)
      {
         TiXmlElement *valueElement = new TiXmlElement("value");
         valueElement->SetAttribute("time", StringTk::uint64ToStr(*(timesReadIter()->first) ) );
         valueElement->LinkEndChild(
            new TiXmlText(StringTk::uint64ToStr(*(timesReadIter()->second) ) ) );
         diskPerfReadElement->LinkEndChild(valueElement);
      }

      TiXmlElement *diskPerfAverageReadElement =
         new TiXmlElement("diskPerfAverageRead");
      rootElement->LinkEndChild(diskPerfAverageReadElement);

      for (ZipIterRange<UInt64List, UInt64List> timesAverageReadIter(timesAverageRead, averageRead);
           !timesAverageReadIter.empty(); ++timesAverageReadIter)
      {
         TiXmlElement *valueElement = new TiXmlElement("value");
         valueElement->SetAttribute("time",
            StringTk::uint64ToStr(*(timesAverageReadIter()->first) ) );
         valueElement->LinkEndChild(
            new TiXmlText(StringTk::uint64ToStr(*(timesAverageReadIter()->second) ) ) );
         diskPerfAverageReadElement->LinkEndChild(valueElement);
      }

      TiXmlElement *diskPerfWriteElement = new TiXmlElement("diskPerfWrite");
      rootElement->LinkEndChild(diskPerfWriteElement);

      for (ZipIterRange<UInt64List, UInt64List> timesWriteIter(timesWrite, write);
           !timesWriteIter.empty(); ++timesWriteIter)
      {
         TiXmlElement *valueElement = new TiXmlElement("value");
         valueElement->SetAttribute("time", StringTk::uint64ToStr(*(timesWriteIter()->first) ) );
         valueElement->LinkEndChild(
            new TiXmlText(StringTk::uint64ToStr(*(timesWriteIter()->second) ) ) );
         diskPerfWriteElement->LinkEndChild(valueElement);
      }

      TiXmlElement *diskPerfAverageWriteElement =
         new TiXmlElement("diskPerfAverageWrite");
      rootElement->LinkEndChild(diskPerfAverageWriteElement);

      for (ZipIterRange<UInt64List, UInt64List>
           timesAverageWriteIter(timesAverageWrite, averageWrite);
           !timesAverageWriteIter.empty(); ++timesAverageWriteIter)
      {
         TiXmlElement *valueElement = new TiXmlElement("value");
         valueElement->SetAttribute("time",
            StringTk::uint64ToStr(*(timesAverageWriteIter()->first)));
         valueElement->LinkEndChild(
            new TiXmlText(StringTk::uint64ToStr(*(timesAverageWriteIter()->second) ) ) );
         diskPerfAverageWriteElement->LinkEndChild(valueElement);
      }

      webtk::writeDoc(conn, request_info, doc);
   }
   catch (HttpVarException &e)
   {
      xmlhelper::sendError(conn, request_info, e.what());
   }
}

/*
 * gets the overview about all metadata nodes
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void xmlhelper::storageNodesOverview(struct mg_connection *conn,
   const struct mg_request_info *request_info, const char* postBuf, size_t postBufLen)
{
#ifdef MONGOOSE_DEBUG
   webtk::debugMongoose(conn,request_info, postBuf, postBufLen);
#endif

   NodeStoreStorageEx* storageNodes = Program::getApp()->getStorageNodes();
   Database *db = Program::getApp()->getDB();

   TiXmlDocument doc;
   TiXmlElement *rootElement = new TiXmlElement("data");
   doc.LinkEndChild(rootElement);

   try
   {
      std::string group = webtk::getVar(conn, "group", "", postBuf, postBufLen);
      uint timeSpanPerf = StringTk::strToUInt(
         webtk::getVar(conn, "timeSpanPerf", "10", postBuf, postBufLen) );

      long endTimestamp;
      long startTimestamp = calculateStartTime(timeSpanPerf, endTimestamp);

      StringList nodes;
      NumNodeIDList nodeNumIDs;
      if (group.compare("") == 0)
      {
         Program::getApp()->getStorageNodesAsStringList(&nodes);
         Program::getApp()->getStorageNodeNumIDs(&nodeNumIDs);
      }
      else
      {
         db->getStorageNodesInGroup(group, &nodes);
         db->getStorageNodesInGroup(group, &nodeNumIDs);
      }

      TiXmlElement *statusElement = new TiXmlElement("status");
      rootElement->LinkEndChild(statusElement);

      for (ZipIterRange<NumNodeIDList, StringList> nodesIter(nodeNumIDs, nodes);
           !nodesIter.empty(); ++nodesIter)
      {
         std::string outInfo;
         std::string nodeID = *(nodesIter()->second);
         NumNodeID nodeNumID = *(nodesIter()->first);

         TiXmlElement *valueElement = new TiXmlElement("value");
         statusElement->LinkEndChild(valueElement);
         valueElement->SetAttribute("node", nodeID);
         valueElement->SetAttribute("nodeNumID", nodeNumID.str());

         if (storageNodes->statusStorage(nodeNumID, &outInfo))
            valueElement->LinkEndChild(new TiXmlText("true"));
         else
            valueElement->LinkEndChild(new TiXmlText("false"));
      }

      TiXmlElement *diskSpaceElement = new TiXmlElement("diskSpace");
      rootElement->LinkEndChild(diskSpaceElement);
      std::string unit;

      TiXmlElement *diskSpaceTotalElement = new TiXmlElement("diskSpaceTotal");
      diskSpaceTotalElement->LinkEndChild(new TiXmlText(
         storageNodes->diskSpaceTotalSum(&unit) + " " + unit));
      diskSpaceElement->LinkEndChild(diskSpaceTotalElement);

      TiXmlElement *diskSpaceFreeElement = new TiXmlElement("diskSpaceFree");
      diskSpaceFreeElement->LinkEndChild(
         new TiXmlText(storageNodes->diskSpaceFreeSum(&unit) + " " + unit));
      diskSpaceElement->LinkEndChild(diskSpaceFreeElement);

      TiXmlElement *diskSpaceUsedElement = new TiXmlElement("diskSpaceUsed");
      diskSpaceUsedElement->LinkEndChild(
         new TiXmlText(storageNodes->diskSpaceUsedSum(&unit) + " " + unit));
      diskSpaceElement->LinkEndChild(diskSpaceUsedElement);

      TiXmlElement *diskPerfSummaryElement = new TiXmlElement("diskPerfSummary");
      rootElement->LinkEndChild(diskPerfSummaryElement);

      TiXmlElement *diskReadSumElement = new TiXmlElement("diskReadSum");
      diskReadSumElement->LinkEndChild(new TiXmlText(storageNodes->diskReadSum(
         timeSpanPerf, &unit) + " " + unit));
      diskPerfSummaryElement->LinkEndChild(diskReadSumElement);

      TiXmlElement *diskWriteSumElement = new TiXmlElement("diskWriteSum");
      diskWriteSumElement->LinkEndChild(new TiXmlText(storageNodes->diskWriteSum(
         timeSpanPerf, &unit) + " " + unit));

      diskPerfSummaryElement->LinkEndChild(diskWriteSumElement);

      UInt64List timesRead;
      UInt64List read;
      UInt64List timesAverageRead;
      UInt64List averageRead;
      storageNodes->diskPerfReadSum(timeSpanPerf, &timesRead, &read,
         &timesAverageRead, &averageRead, startTimestamp, endTimestamp);

      UInt64List timesWrite;
      UInt64List write;
      UInt64List timesAverageWrite;
      UInt64List averageWrite;
      storageNodes->diskPerfWriteSum(timeSpanPerf, &timesWrite, &write,
         &timesAverageWrite, &averageWrite, startTimestamp, endTimestamp);

      TiXmlElement *diskPerfReadElement = new TiXmlElement("diskPerfRead");
      rootElement->LinkEndChild(diskPerfReadElement);

      for (ZipIterRange<UInt64List, UInt64List> timesReadIter(timesRead, read);
           !timesReadIter.empty(); ++timesReadIter)
      {
         TiXmlElement *valueElement = new TiXmlElement("value");
         valueElement->SetAttribute("time", StringTk::uint64ToStr(*(timesReadIter()->first) ) );
         valueElement->LinkEndChild(
            new TiXmlText(StringTk::uint64ToStr(*(timesReadIter()->second) ) ) );
         diskPerfReadElement->LinkEndChild(valueElement);
      }

      TiXmlElement *diskPerfAverageReadElement = new TiXmlElement("diskPerfAverageRead");
      rootElement->LinkEndChild(diskPerfAverageReadElement);
      for (ZipIterRange<UInt64List, UInt64List> timesAverageReadIter(timesAverageRead, averageRead);
           !timesAverageReadIter.empty(); ++timesAverageReadIter)
      {
         TiXmlElement *valueElement = new TiXmlElement("value");
         valueElement->SetAttribute("time",
            StringTk::uint64ToStr(*(timesAverageReadIter()->first) ) );
         valueElement->LinkEndChild(
            new TiXmlText(StringTk::uint64ToStr(*(timesAverageReadIter()->second) ) ) );
         diskPerfAverageReadElement->LinkEndChild(valueElement);
      }

      TiXmlElement *diskPerfWriteElement = new TiXmlElement("diskPerfWrite");
      rootElement->LinkEndChild(diskPerfWriteElement);
      for (ZipIterRange<UInt64List, UInt64List> timesWriteIter(timesWrite, write);
           !timesWriteIter.empty(); ++timesWriteIter)
      {
         TiXmlElement *valueElement = new TiXmlElement("value");
         valueElement->SetAttribute("time", StringTk::uint64ToStr(*(timesWriteIter()->first) ) );
         valueElement->LinkEndChild(
            new TiXmlText(StringTk::uint64ToStr(*(timesWriteIter()->second) ) ) );
         diskPerfWriteElement->LinkEndChild(valueElement);
      }

      TiXmlElement *diskPerfAverageWriteElement = new TiXmlElement("diskPerfAverageWrite");
      rootElement->LinkEndChild(diskPerfAverageWriteElement);

      for (ZipIterRange<UInt64List, UInt64List>
         timesAverageWriteIter(timesAverageWrite, averageWrite);
         !timesAverageWriteIter.empty(); ++timesAverageWriteIter)
      {
         TiXmlElement *valueElement = new TiXmlElement("value");
         valueElement->SetAttribute("time",
            StringTk::uint64ToStr(*(timesAverageWriteIter()->first) ) );
         valueElement->LinkEndChild(
            new TiXmlText(StringTk::uint64ToStr(*(timesAverageWriteIter()->second) ) ) );
         diskPerfAverageWriteElement->LinkEndChild(valueElement);
      }

      webtk::writeDoc(conn, request_info, doc);
   }
   catch (HttpVarException &e)
   {
      xmlhelper::sendError(conn, request_info, e.what());
   }
}

/**
 * Gets and sets a stripe pattern for a path (depending on whether
 * "change" is set).
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void xmlhelper::striping(struct mg_connection *conn,
   const struct mg_request_info *request_info, const char* postBuf, size_t postBufLen)
{
#ifdef MONGOOSE_DEBUG
   webtk::debugMongoose(conn,request_info, postBuf, postBufLen);
#endif

   App* app = Program::getApp();
   Logger* log = Logger::getLogger();
   NodeStoreMgmtEx* mgmtNodes = app->getMgmtNodes();
   NodeStoreMetaEx* metaNodes = app->getMetaNodes();
   NodeStoreStorageEx* storageNodes = app->getStorageNodes();
   TargetMapper* targetMapper = app->getTargetMapper();

   TiXmlDocument doc;
   TiXmlElement *rootElement = new TiXmlElement("data");
   doc.LinkEndChild(rootElement);

   bool responseErr = false; // will be contained in the error element of the response

   try
   {
      std::string pathStr = webtk::getVar(conn, "pathStr", "/", postBuf, postBufLen);
      std::string change = webtk::getVar(conn, "change", "false", postBuf, postBufLen);
      std::string dnn = webtk::getVar(conn, "dnn", "0", postBuf, postBufLen);
      std::string cs = webtk::getVar(conn, "cs", "0", postBuf, postBufLen);
      std::string metaMirroring = webtk::getVar(conn, "metaMirroring", "false", postBuf,
         postBufLen);
      std::string patternID = webtk::getVar(conn, "pattern", "0", postBuf, postBufLen);
      std::string nonceID = webtk::getVar(conn, "nonceID", "-1", postBuf, postBufLen);
      std::string secret = webtk::getVar(conn, "secret", "", postBuf, postBufLen);

      // note: the change element defines whether we only get or set+get a stripe pattern with this

      if ((StringTk::strToBool(change)) && (pathStr.compare("") != 0))
      { // set new stripe pattern
         TiXmlElement *authElement = new TiXmlElement("authenticated");
         rootElement->LinkEndChild(authElement);
         Auth::init();
         if (SecurityTk::checkReply(secret, Auth::getPassword("admin"),
            StringTk::strToInt(nonceID) ) )
         {
            authElement->LinkEndChild(new TiXmlText("true"));
            bool setPatternRes = managementhelper::setPattern(pathStr,
               StringTk::strToUInt(cs), StringTk::strToUInt(dnn),
               StringTk::strToUInt(patternID), StringTk::strToBool(metaMirroring));

            if(!setPatternRes)
               responseErr = true; // setting of pattern failed
         }
         else
         {
            authElement->LinkEndChild(new TiXmlText("false") );
            log->log(Log_ERR, __func__, "Authentication for admin user failed.");
         }
      }
      else
      { // retrieve current stripe pattern
         TiXmlElement *settingsElement = new TiXmlElement("settings");
         rootElement->LinkEndChild(settingsElement);

         if(!pathStr.empty() )
         {
            std::string chunkSize;
            std::string defaultNumNodes;
            UInt16Vector primaryTargetNumIDs;
            UInt16Vector secondaryTargetNumIDs;
            UInt16Vector storageBMGs;
            NumNodeID primaryMetaNodeNumID;
            NumNodeID secondaryMetaNodeNumID;
            uint16_t metaBMG;
            unsigned pattern;

            bool getInfoRes = managementhelper::getEntryInfo(pathStr, &chunkSize,
               &defaultNumNodes, &primaryTargetNumIDs, &pattern, &secondaryTargetNumIDs,
               &storageBMGs, &primaryMetaNodeNumID, &secondaryMetaNodeNumID, &metaBMG);

            if(!getInfoRes)
               responseErr = true; // error on entry info retrieval

            // if path has no storage nodes assigned, assume it's a directory
            bool isDir = primaryTargetNumIDs.empty();

            TiXmlElement *pathElement = new TiXmlElement("path");
            settingsElement->LinkEndChild(pathElement);
            pathElement->LinkEndChild(new TiXmlText(pathStr) );

            TiXmlElement *dirElement = new TiXmlElement("isDir");
            settingsElement->LinkEndChild(dirElement);
            dirElement->LinkEndChild(new TiXmlText(isDir ? "true" : "false") );

            TiXmlElement *chunksizeElement = new TiXmlElement("chunksize");
            settingsElement->LinkEndChild(chunksizeElement);
            chunksizeElement->LinkEndChild(new TiXmlText(chunkSize) );

            TiXmlElement *patternElement = new TiXmlElement("pattern");
            settingsElement->LinkEndChild(patternElement);
            patternElement->LinkEndChild(new TiXmlText(StringTk::uintToStr(pattern) ) );

            TiXmlElement *defaultNumNodesElement = new TiXmlElement("defaultNumNodes");
            settingsElement->LinkEndChild(defaultNumNodesElement);
            defaultNumNodesElement->LinkEndChild(
               new TiXmlText(defaultNumNodes) );

            TiXmlElement *metaElement = new TiXmlElement("meta");
            rootElement->LinkEndChild(metaElement);
            metaElement->LinkEndChild(new TiXmlText(
               metaNodes->getNodeIDWithTypeStr(primaryMetaNodeNumID) ) );

            TiXmlElement *metaMirroringElement = new TiXmlElement("metaMirroring");
            settingsElement->LinkEndChild(metaMirroringElement);

            if(metaBMG)
            {
               TiXmlElement *metaMirrorElement = new TiXmlElement("metaMirror");
               rootElement->LinkEndChild(metaMirrorElement);
               metaMirrorElement->LinkEndChild(new TiXmlText(
                  metaNodes->getNodeIDWithTypeStr(secondaryMetaNodeNumID) ) );

               TiXmlElement *metaBuddyGroupElement = new TiXmlElement("metaMirrorBuddyGroup");
               rootElement->LinkEndChild(metaBuddyGroupElement);
               metaBuddyGroupElement->LinkEndChild(new TiXmlText(StringTk::uintToStr(metaBMG) ) );

               metaMirroringElement->LinkEndChild(new TiXmlText("true") );
            }
            else
            {
               metaMirroringElement->LinkEndChild(new TiXmlText("false") );
            }


            if(!isDir)
            { // file => add stripe nodes
               UInt16List mappedTargetIDs;
               NumNodeIDList mappedNodeIDs;
               bool targetMappingSuccess = false;

               auto mgmtNode = mgmtNodes->referenceFirstNode();

               if( (mgmtNode != NULL) &&
                  NodesTk::downloadTargetMappings(*mgmtNode, &mappedTargetIDs, &mappedNodeIDs,
                     false) )
               {
                  targetMapper->syncTargetsFromLists(mappedTargetIDs, mappedNodeIDs);
                  targetMappingSuccess = true;
               }
               else
               {
                  log->log(Log_ERR, __func__, "Failed to download target mappings from mgmtd.");
               }

               TiXmlElement *targetElement = new TiXmlElement("storageTargets");
               rootElement->LinkEndChild(targetElement);
               for(UInt16VectorIter iter = primaryTargetNumIDs.begin();
                  iter != primaryTargetNumIDs.end(); iter++)
               {
                  std::string node = "<unknown>";
                  if(targetMappingSuccess)
                  {
                     NumNodeID nodeID = targetMapper->getNodeID(*iter);
                     node = storageNodes->getNodeIDWithTypeStr(nodeID);
                  }

                  TiXmlElement *nodeElement = new TiXmlElement("target");
                  nodeElement->LinkEndChild(new TiXmlText(
                     StringTk::uintToStr(*iter) + " @ " + node) );
                  targetElement->LinkEndChild(nodeElement);
               }

               TiXmlElement *mirrorTargetElement = new TiXmlElement("mirrorStorageTargets");
               rootElement->LinkEndChild(mirrorTargetElement);
               for(UInt16VectorIter iter = secondaryTargetNumIDs.begin();
                  iter != secondaryTargetNumIDs.end(); iter++)
               {
                  std::string node = "<unknown>";
                  if(targetMappingSuccess)
                  {
                     NumNodeID nodeID = targetMapper->getNodeID(*iter);
                     node = storageNodes->getNodeIDWithTypeStr(nodeID);
                  }

                  TiXmlElement *nodeElement = new TiXmlElement("target");
                  nodeElement->LinkEndChild(new TiXmlText(
                     StringTk::uintToStr(*iter) + " @ " + node) );
                  mirrorTargetElement->LinkEndChild(nodeElement);
               }

               TiXmlElement *mirrorBuddyGroupsElement =
                  new TiXmlElement("storageMirrorBuddyGroups");
               rootElement->LinkEndChild(mirrorBuddyGroupsElement);
               for(UInt16VectorIter iter = storageBMGs.begin(); iter != storageBMGs.end(); iter++)
               {
                  TiXmlElement *nodeElement = new TiXmlElement("id");
                  nodeElement->LinkEndChild(new TiXmlText(StringTk::uintToStr(*iter) ) );
                  mirrorBuddyGroupsElement->LinkEndChild(nodeElement);
               }
            }
         }
      }

      // add the general error element
      TiXmlElement* errorElement = new TiXmlElement("error");
      rootElement->LinkEndChild(errorElement);
      errorElement->LinkEndChild(
         new TiXmlText(responseErr ? "true" : "false") );

      webtk::writeDoc(conn, request_info, doc);
   }
   catch (HttpVarException &e)
   {
      xmlhelper::sendError(conn, request_info, e.what());
   }
}

/**
 * gets the beegfs version
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void xmlhelper::getVersionString(struct mg_connection *conn,
   const struct mg_request_info *request_info, const char* postBuf, size_t postBufLen)
{
#ifdef MONGOOSE_DEBUG
   webtk::debugMongoose(conn,request_info, postBuf, postBufLen);
#endif

   TiXmlDocument doc;
   TiXmlElement *rootElement = new TiXmlElement("data");
   doc.LinkEndChild(rootElement);

   TiXmlElement *element = new TiXmlElement("version");
   TiXmlText *elementText = new TiXmlText(BEEGFS_VERSION);
   element->LinkEndChild(elementText);
   rootElement->LinkEndChild(element);

   webtk::writeDoc(conn, request_info, doc);
}

/**
 * send a test eMail to test the eMail notification settings
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void xmlhelper::testEMail(struct mg_connection *conn,
   const struct mg_request_info * request_info, const char* postBuf, size_t postBufLen)
{
#ifdef MONGOOSE_DEBUG
   webtk::debugMongoose(conn,request_info, postBuf, postBufLen);
#endif

   Logger* log = Logger::getLogger();
   App* app  = Program::getApp();
   RuntimeConfig *runtimeCfg = app->getRuntimeConfig();
   Mailer* mailer = app->getMailer();

   std::string msg = "Test email from beegfs-admon.";
   std::string subject = "BeeGFS : test email";

   int sendToSmtpServerSuccsessfull = mailer->sendMail(subject, msg);

   TiXmlDocument doc;
   TiXmlElement *rootElement = new TiXmlElement("data");
   doc.LinkEndChild(rootElement);

   TiXmlElement *successElement = new TiXmlElement("success");
   rootElement->LinkEndChild(successElement);

   if (sendToSmtpServerSuccsessfull == 0)
   {
      TiXmlText *successElementText = new TiXmlText("true");
      successElement->LinkEndChild(successElementText);
   }
   else
   {
      TiXmlText *successElementText = new TiXmlText("false");
      successElement->LinkEndChild(successElementText);

      log->log(Log_ERR, __func__, "Failed to send eMail to " + runtimeCfg->getMailRecipient() +
         " with subject " + subject);
   }

   webtk::writeDoc(conn, request_info, doc);
}

/**
 * Collects the client stats.
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void xmlhelper::clientStats(struct mg_connection *conn,
   const struct mg_request_info * request_info, const char* postBuf, size_t postBufLen)
{
#ifdef MONGOOSE_DEBUG
   webtk::debugMongoose(conn, request_info, postBuf, postBufLen);
#endif

   xmlhelper::stats(false, conn, request_info, postBuf, postBufLen);
}

/**
 * Collects the user stats.
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void xmlhelper::userStats(struct mg_connection *conn,
   const struct mg_request_info * request_info, const char* postBuf, size_t postBufLen)
{
#ifdef MONGOOSE_DEBUG
   webtk::debugMongoose(conn, request_info, postBuf, postBufLen);
#endif

   xmlhelper::stats(true, conn, request_info, postBuf, postBufLen);
}

/**
 * Collects the client stats.
 *
 * @param doUserStats if true user stats are requested, if false client stats are requested
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void xmlhelper::stats(bool doUserStats, struct mg_connection *conn,
   const struct mg_request_info * request_info, const char* postBuf, size_t postBufLen)
{
#ifdef MONGOOSE_DEBUG
   webtk::debugMongoose(conn,request_info, postBuf, postBufLen);
#endif

   StatsOperator* cStatsOperator = Program::getApp()->getClientStatsOperator();

   try
   {
      uint64_t currentDataSequenceID;

      int type = StringTk::strToInt(webtk::getVar(conn, "nodetype", "1", postBuf,
         postBufLen) );

      NodeType nodeType;
      if(type == NODETYPE_Meta)
         nodeType = NODETYPE_Meta;
      else if(type == NODETYPE_Storage)
         nodeType = NODETYPE_Storage;
      else
         nodeType = NODETYPE_Invalid;

      unsigned intervalSecs = StringTk::strToUInt(webtk::getVar(conn, "interval", "10", postBuf,
         postBufLen) );
      unsigned numClients = StringTk::strToUInt(webtk::getVar(conn, "numLines", "10", postBuf,
         postBufLen) );
      unsigned requestorID = StringTk::strToUInt(webtk::getVar(conn, "requestorID", "0", postBuf,
         postBufLen) );
      uint64_t nextDataSequenceID = StringTk::strToUInt64(webtk::getVar(conn, "nextDataSequenceID",
         postBuf, postBufLen).c_str() );

      if (cStatsOperator->needUpdate(requestorID, intervalSecs, numClients, nodeType, doUserStats) )
      {
         requestorID = cStatsOperator->addOrUpdate(requestorID, intervalSecs, numClients, nodeType,
            doUserStats);
      }

      StatsAdmon* stats = cStatsOperator->getStats(requestorID, currentDataSequenceID);

      TiXmlDocument doc;

      TiXmlElement *rootElement = new TiXmlElement("data");
      doc.LinkEndChild(rootElement);

      TiXmlElement *requestorIDElement = new TiXmlElement("requestorID");
      rootElement->LinkEndChild(requestorIDElement);
      requestorIDElement->LinkEndChild(new TiXmlText(StringTk::uintToStr(requestorID) ) );

      TiXmlElement *dataSequenceIDElement = new TiXmlElement("dataSequenceID");
      rootElement->LinkEndChild(dataSequenceIDElement);
      dataSequenceIDElement->LinkEndChild(new TiXmlText(StringTk::uint64ToStr(
         currentDataSequenceID)));

      if (nextDataSequenceID <= currentDataSequenceID)
         stats->addStatsToXml(rootElement, doUserStats);

      SAFE_DELETE(stats);

      webtk::writeDoc(conn, request_info, doc);
   }
   catch (HttpVarException &e)
   {
      xmlhelper::sendError(conn, request_info, e.what());
   }
}
