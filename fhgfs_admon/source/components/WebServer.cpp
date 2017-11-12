#include <common/toolkit/StringTk.h>
#include <program/Program.h>
#include <toolkit/webtk.h>
#include "WebServer.h"



#define MONGOOSE_DOCUMENT_ROOT "/var/lib/beegfs/www" // default document root for the moongose
                                                    // webserver, it's required for security reason
#define MONGOOSE_DISABLE_DIR_LISTING "no" // disable directory listing for security reasons
#define MONGOOSE_NUM_THREADS "8" // number of worker threads for mongoose


/*
 * function to generate a generic page, which offers the user to download the Java GUI
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 */
static void downloadGuiPage(struct mg_connection *conn,
   const struct mg_request_info *request_info)
{
   std::string output = std::string("") +
      std::string("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" ") +
      std::string("\"http://www.w3.org/TR/html4/loose.dtd\">\n") +
      std::string("<html>\n") +
      std::string("    <head>\n") +
      std::string("       <meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\">\n") +
      std::string("       <title>BeeGFS Admon GUI download</title>\n") +
      std::string("    </head>\n") +
      std::string("    <body id=\"beegfs-download\" class=\"lang-en\">\n") +
      std::string("        <p align=\"center\">&nbsp;<br>&nbsp;<br>\n") +
      std::string("        <a href=\"beegfs-admon-gui.jar\">Download BeeGFS Admon GUI (Java .jar)</a>\n") +
      std::string("        </p>&nbsp;<br>&nbsp;<br>\n") +
      std::string("    </body>\n") +
      std::string("</html>\n");

   int contentLength = output.length();
   mg_printf(conn, "HTTP/1.1 200 OK\r\n"
                   "Cache: no-cache\r\n"
                   "Content-Type: text/html\r\n"
                   "Content-Length: %d\r\n"
                   "\r\n",
                   contentLength);
   mg_write(conn, output.c_str(), contentLength);


   Logger::getLogger()->log(Log_DEBUG, "WebSrv", "Requested URI: " +
      std::string(request_info->uri) + " from client IP: " + webtk::getIP(request_info));
}

/*
 * @param port The Port for the HTTP server to listen for
 */
WebServer::WebServer(int port) : PThread("WebSrv"),
   log("WebSrv")
{
   strcpy(this->port, StringTk::intToStr(port).c_str() );
   memset(&callbacks, 0, sizeof(callbacks));

   this->callbacks.begin_request = requestHandler;
   this->callbacks.http_error = errorHandler;
   this->callbacks.websocket_connect = websocketConnectHandler;
}

void WebServer::run()
{
   try
   {
      log.log(Log_DEBUG, "Component started.");
      registerSignalHandler();

      const char* options[] = {"listening_ports", this->port,
                               "num_threads", MONGOOSE_NUM_THREADS,
                               "enable_directory_listing", MONGOOSE_DISABLE_DIR_LISTING,
                               "document_root", MONGOOSE_DOCUMENT_ROOT,
                               NULL};

      // start a mongoose context
      this->ctx = mg_start(&this->callbacks, NULL, options);

      if (this->ctx == NULL)
         throw ComponentInitException("Couldn't start mongoose thread.");

      // do nothing, just wait for termination and let the mongoose thread do the work
      PThread::waitForSelfTerminateOrder();

      mg_stop(ctx);
      log.log(Log_DEBUG, std::string("Component stopped."));
   }
   catch (std::exception& e)
   {
      Program::getApp()->handleComponentException(e);
   }
}

/*
 * The default request handler for the webserver, it accepts only a set of defined URIs and returns
 * an HTTP 404 error for all other URIs
 *
 * @param conn The mg_connection from mongoose
 */
int WebServer::requestHandler(struct mg_connection *conn)
{
   struct mg_request_info *requestInfo = mg_get_request_info(conn);
   char* postData = NULL;
   size_t postDataLen = 0;

   // need to initialize a fake thread here, because the some handler function will
   // call PThread functions later
   PThread::initFakeThread("Mongoose Thread " + StringTk::int64ToStr(
      pthread_self()), Program::getApp());

   // Read the POST data once and store the POST buffer until the message is processed. Provide the
   // POST buffer to all methods which need the POST data. It is only possible to read the POST data
   // from the message body once.
   bool isPostRequest = !strcmp(requestInfo->request_method, "POST");
   if (isPostRequest)
   {
      const char* contenLengthChar = mg_get_header(conn, "Content-Length");
      if (contenLengthChar != NULL)
      {
         postDataLen = atoi(contenLengthChar);
         postData = (char*) malloc(postDataLen + 1); // +1 for "/0" termination

         int dataReadLen = mg_read(conn, postData, postDataLen);
         if (dataReadLen <= 0)
         {
            if (dataReadLen == 0 && postDataLen != 0)
               LOG_DEBUG("WebSrv", Log_WARNING, "Mismatch of message header Content-Length and the "
                  "amount of data which could read from the message body."
                  "Content-Length: " + StringTk::uintToStr(postDataLen) +
                  "amount of data: " + StringTk::intToStr(dataReadLen) +
                  ", URI: " + std::string(requestInfo->uri) );
            else if (dataReadLen == 0)
               LOG_DEBUG("WebSrv", Log_WARNING, "POST request doesn't contain data in the message "
                  "body. Peer closed the connection. URI: " + std::string(requestInfo->uri) );
            else
               LOG_DEBUG("WebSrv", Log_WARNING, "POST request doesn't contain data in the message "
                  "body. I/0 error. URI: " + std::string(requestInfo->uri) );

            postData[0] = '\0';
         }
         else
         {
            postData[dataReadLen] = '\0';
         }
      }
   }

   // each page that mongoose shall display needs to be specified here setting a callback
   // do not change the order of the URIs. The order was set for performance reasons.
   if (strcmp(requestInfo->uri, "/XML_ClientStats") == 0)
   { // a URI which is polled every x seconds
      xmlhelper::clientStats(conn, requestInfo, postData, postDataLen);
   }
   else if (strcmp(requestInfo->uri, "/XML_UserStats") == 0)
   { // a URI which is polled every x seconds
      xmlhelper::userStats(conn, requestInfo, postData, postDataLen);
   }
   else if (strcmp(requestInfo->uri, "/XML_Storagenode") == 0)
   { // a URI which is polled every x seconds
      xmlhelper::storageNode(conn, requestInfo, postData, postDataLen);
   }
   else if (strcmp(requestInfo->uri, "/XML_StoragenodesOverview") == 0)
   { // a URI which is polled every x seconds
      xmlhelper::storageNodesOverview(conn, requestInfo, postData, postDataLen);
   }
   else if (strcmp(requestInfo->uri, "/XML_Metanode") == 0)
   { // a URI which is polled every x seconds
      xmlhelper::metaNode(conn, requestInfo, postData, postDataLen);
   }
   else if (strcmp(requestInfo->uri, "/XML_MetanodesOverview") == 0)
   { // a URI which is polled every x seconds
      xmlhelper::metaNodesOverview(conn, requestInfo, postData, postDataLen);
   }
   else if (strcmp(requestInfo->uri, "/XML_KnownProblems") == 0)
   { // a URI which is polled every x seconds
      xmlhelper::knownProblems(conn, requestInfo, postData, postDataLen);
   }
   else if (strcmp(requestInfo->uri, "/XML_GetNonce") == 0)
   {
      xmlhelper::getNonce(conn, requestInfo, postData, postDataLen);
   }
   else if (strcmp(requestInfo->uri, "/XML_AuthInfo") == 0)
   {
      xmlhelper::authInfo(conn, requestInfo, postData, postDataLen);
   }
   else if (strcmp(requestInfo->uri, "/XML_ChangePW") == 0)
   {
      xmlhelper::changePassword(conn, requestInfo, postData, postDataLen);
   }
   else if (strcmp(requestInfo->uri, "/XML_CheckDistriArch") == 0)
   {
      xmlhelper::checkDistriArch(conn, requestInfo, postData, postDataLen);
   }
   else if (strcmp(requestInfo->uri, "/XML_CheckSSH") == 0)
   {
      xmlhelper::checkSSH(conn, requestInfo, postData, postDataLen);
   }
   else if (strcmp(requestInfo->uri, "/XML_ConfigBasicConfig") == 0)
   {
      xmlhelper::configBasicConfig(conn, requestInfo, postData, postDataLen);
   }
   else if (strcmp(requestInfo->uri, "/XML_ConfigIBConfig") == 0)
   {
      xmlhelper::configIBConfig(conn, requestInfo, postData, postDataLen);
   }
   else if (strcmp(requestInfo->uri, "/XML_ConfigRoles") == 0)
   {
      xmlhelper::configRoles(conn, requestInfo, postData, postDataLen);
   }
   else if (strcmp(requestInfo->uri, "/XML_FileBrowser") == 0)
   {
      xmlhelper::fileBrowser(conn, requestInfo, postData, postDataLen);
   }
   else if (strcmp(requestInfo->uri, "/XML_GetCheckClientDependenciesLog") == 0)
   {
      xmlhelper::getCheckClientDependenciesLog(conn, requestInfo, postData, postDataLen);
   }
   else if (strcmp(requestInfo->uri, "/XML_GetGroups") == 0)
   {
      xmlhelper::getGroups(conn, requestInfo, postData, postDataLen);
   }
   else if (strcmp(requestInfo->uri, "/XML_Install") == 0)
   {
      xmlhelper::install(conn, requestInfo, postData, postDataLen);
   }
   else if (strcmp(requestInfo->uri, "/XML_InstallInfo") == 0)
   {
      xmlhelper::installInfo(conn, requestInfo, postData, postDataLen);
   }
   else if (strcmp(requestInfo->uri, "/XML_Uninstall") == 0)
   {
      xmlhelper::uninstall(conn, requestInfo, postData, postDataLen);
   }
   else if (strcmp(requestInfo->uri, "/XML_LogFile") == 0)
   {
      xmlhelper::getLocalLogFile(conn, requestInfo, postData, postDataLen);
   }
   else if (strcmp(requestInfo->uri, "/XML_MailNotification") == 0)
   {
      xmlhelper::mailNotification(conn, requestInfo, postData, postDataLen);
   }
   else if (strcmp(requestInfo->uri, "/XML_NodeList") == 0)
   {
      xmlhelper::nodeList(conn, requestInfo, postData, postDataLen);
   }
   else if (strcmp(requestInfo->uri, "/XML_PreAuthInfo") == 0)
   {
      xmlhelper::preAuthInfo(conn, requestInfo, postData, postDataLen);
   }
   else if (strcmp(requestInfo->uri, "/XML_StartStop") == 0)
   {
      xmlhelper::startStop(conn, requestInfo, postData, postDataLen);
   }
   else if (strcmp(requestInfo->uri, "/XML_Striping") == 0)
   {
      xmlhelper::striping(conn, requestInfo, postData, postDataLen);
   }
   else if (strcmp(requestInfo->uri, "/XML_ScriptSettings") == 0)
   {
      xmlhelper::scriptSettings(conn, requestInfo, postData, postDataLen);
   }
   else if (strcmp(requestInfo->uri, "/XML_CreateNewSetupLogfile") == 0)
   {
      xmlhelper::createNewSetupLogfile(conn, requestInfo, postData, postDataLen);
   }
   else if (strcmp(requestInfo->uri, "/XML_GetLogFile") == 0)
   {
      xmlhelper::getRemoteLogFile(conn, requestInfo, postData, postDataLen);
   }
   else if (strcmp(requestInfo->uri, "/beegfs-admon-gui.jar") == 0)
   {
      xmlhelper::admonGui(conn, requestInfo);
   }
   else if (strcmp(requestInfo->uri, "/") == 0)
   {
      downloadGuiPage(conn, requestInfo);
   }
   else if (strcmp(requestInfo->uri, "/XML_GetVersionString") == 0)
   {
      xmlhelper::getVersionString(conn, requestInfo, postData, postDataLen);
   }
   else if (strcmp(requestInfo->uri, "/XML_TestEMail") == 0)
   {
      xmlhelper::testEMail(conn, requestInfo, postData, postDataLen);
   }
   else
   {
      // send HTTP error code "Not Found"
      const int httpErrorCodeNotFound = 404;
      std::string errorMessage = std::string("Requested URI: ") + requestInfo->uri + " not found.";

      webtk::sendHttpError(conn, requestInfo, httpErrorCodeNotFound, errorMessage);
   }

   // free the meomry from the post data buffer
   if (isPostRequest)
      SAFE_FREE(postData);

   // Mark request as processed, if the method returns 0 Mongoose will process the request
   return 1;
}

/*
 * The error request handler for the webserver, it offers a GUI download
 *
 * @param conn The mg_connection from mongoose
 * @param status The error code (ignored in this implementation)
 */
int WebServer::errorHandler(struct mg_connection *conn, int status)
{
   // if a HTTP error occurs (i.e. mainly page does not exist), then offer a GUI download
   downloadGuiPage(conn, mg_get_request_info(conn));

   return 0;
}

/*
 * The websocket connect handler for the webserver, it ignores all requests for a websocket,
 * implemented for security reasons
 *
 * @param conn The mg_connection from mongoose
 */
int WebServer::websocketConnectHandler(const struct mg_connection *conn)
{
   // do not accept websocket connections, mongoose close the connection if this method returns a
   // value != 0
   return -1;
}
