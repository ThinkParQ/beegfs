#include <common/app/log/Logger.h>
#include <common/net/sock/Socket.h>
#include <program/Program.h>
#include "webtk.h"


static const int smallBufferSize = 128;      // buffer size for the value of one variable
static const int bigBufferSize = 1024;       // buffer size for the value of one big variable, only
                                             // if smallBufferSize is too small



/*
 * Print the raw information on the HTTP request to stdout;
 * Only for debugging! Will be called from every function,
 * when DEBUG_MONGOOSE is defined
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void webtk::debugMongoose(struct mg_connection *conn,
   const struct mg_request_info *request_info, const char* postBuf, size_t postBufLen)
{
   std::string logMessage("Request method: ");
   logMessage.append(request_info->request_method);
   logMessage.append(". URI: ");
   logMessage.append(request_info->uri);

   if (request_info->query_string != NULL)
   {
      logMessage.append(". Query string: ");
      logMessage.append(request_info->query_string);
   }

   if (postBuf != NULL)
   {
      logMessage.append(". Post data: ");
      logMessage.append(postBuf);
   }

   Logger::getLogger()->log(Log_DEBUG, __func__, logMessage);
}

/*
 * write a XML document to mongoose output stream
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param doc document to send to the remote host
 */
void webtk::writeDoc(struct mg_connection *conn, const struct mg_request_info *request_info,
   TiXmlDocument doc)
{
#ifdef MONGOOSE_DEBUG
   Logger::getLogger()->log(Log_DEBUG, __func__, "Requested URI: " +
      std::string(request_info->uri) + " from client IP: " + webtk::getIP(request_info) );
#endif

   std::string xmlHeader = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\r\n";
   std::stringstream outStream;
   outStream << xmlHeader;
   outStream << doc;
   std::string outStr = outStream.str();
   int contentLength = outStr.length();

   //send http header
   mg_printf(conn, "HTTP/1.1 200 OK\r\n"
                "Cache: no-cache\r\n"
                "Content-Type: text/xml\r\n"
                "Content-Length: %d\r\n"
                "\r\n",
                contentLength);

   //send data
   mg_write(conn, outStr.c_str(), contentLength);

#ifdef BEEGFS_DEBUG_XML
   Logger::getLogger()->log(Log_DEBUG, __func__, "XML content: " + outStr);
#endif
}

/*
 * error page with a given error code and an given error message
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param errorCode The error code to send
 * @param errorMessage The error message to display
 */
void webtk::sendHttpError(struct mg_connection *conn,
   const struct mg_request_info *request_info, const int errorCode, std::string errorMessage)
{
   std::string errorText = "Error";
   if (errorMessage.size() != 0)
      errorText = errorMessage;

   mg_printf(conn, "HTTP/1.1 %d Error\r\n\r\n%s", errorCode, errorText.c_str());
}

/*
 * serve a particular file from the file system via HTTP;
 * is used to provide the Admon GUI via HTTP, but could be used for every
 * other file
 *
 * @param conn The mg_connection from mongoose
 * @param request_info The mg_request_info from mongoose
 * @param filename The filename to serve to the remote host
 */
void webtk::serveFile(struct mg_connection *conn, const struct mg_request_info *request_info,
   std::string filename)
{
#ifdef MONGOOSE_DEBUG
   debugMongoose(conn,request_info, NULL, 0);
#endif

   Logger* log = Logger::getLogger();
   log->log(Log_DEBUG, __func__, "Requested URI: " + std::string(request_info->uri) +
      " from client IP: " + webtk::getIP(request_info));

   std::ifstream ifs(filename.c_str(), std::ifstream::in | std::ifstream::binary);

   if (!ifs.good())
   {
      std::string outStr = std::string("") +
         std::string("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 "
            "Transitional//EN\" ") +
         std::string("\"http://www.w3.org/TR/html4/loose.dtd\">\n") +
         std::string("<html>\n") +
         std::string("    <head>\n") +
         std::string("       <meta http-equiv=\"Content-Type\" "
            "content=\"text/html; charset=UTF-8\">\n") +
         std::string("       <title>BeeGFS Admon GUI download</title>\n") +
         std::string("    </head>\n") +
         std::string("    <body id=\"downloadError\" class=\"lang-en\">\n") +
         std::string("        <p align=\"center\">&nbsp;<br>&nbsp;<br>\n") +
         std::string("        Could not find requested file...\n") +
         std::string("        </p>&nbsp;<br>&nbsp;<br>\n") +
         std::string("    </body>\n") +
         std::string("</html>\n");

      int contentLength = outStr.length();
      mg_printf(conn, "HTTP/1.1 404 Not Found\r\n"
               "Cache: no-cache\r\n"
               "Content-Type: text/html\r\n"
               "Content-Length: %d\r\n"
               "\r\n",
               contentLength);
      mg_write(conn, outStr.c_str(), outStr.length());

      log->log(Log_ERR, __func__, "Couldn't load file: " + filename);
   }
   else
   {
      // get length of file:
      ifs.seekg(0, std::ios::end);
      int length = ifs.tellg();
      ifs.seekg(0, std::ios::beg);

      char *output = new char[length];
      ifs.read(output, length);
      ifs.close();

      mg_printf(conn, "HTTP/1.1 200 OK\r\n"
               "Cache: no-cache\r\n"
               "Content-Type: application/java-archive\r\n"
               "Content-Length: %d\r\n"
               "\r\n",
               length);
      mg_write(conn, output, length);
   }
}

/*
 * get a buffer from HTTP request and returns it as string,
 * GET request: returns the variable and value part from the URI
 * POST request: returns the message body
 *
 * @param conn The mg_connection from mongoose
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 *
 * @return the parameters of a request as a string
 */
std::string webtk::getVarStringBuffer(struct mg_connection *conn, const char* postBuf,
   size_t postBufLen)
{
   std::string query = "";
   const struct mg_request_info *requestInfo = mg_get_request_info(conn);

   if (strcmp(requestInfo->request_method, "GET") == 0)
   {
      if (requestInfo->query_string != NULL)
         query = requestInfo->query_string;
   }
   // if request method was HTTP POST the postBuf needs to be parsed
   else
   {
      if (postBuf != NULL)
         query = postBuf;
   }

   return query;
}

/*
 * get a specific variable from a buffer, variable and value must be stored in HTTP request style
 *
 * Example :
 * if the buffer contains fooVar=fooVal&barVar=barVal then a call to
 * getVarFromBuf(buf, bufLen, "fooVar") will return "fooVal"
 *
 * @param buf the buffer which contains the variable and value
 * @param bufLen the length of the buffer
 * @param name the name of the variable to fetch
 *
 * @return the value of the variable
 */
std::string webtk::getVarFromBuf(const char *buf, size_t bufLen, std::string name)
{
   std::string retVal = "";
   char *charVal = (char*) malloc(sizeof(char) * smallBufferSize);

   int length = mg_get_var(buf, bufLen, name.c_str(), charVal, sizeof(char) * smallBufferSize);

   if(length > 0)
   {
      retVal = charVal;
   }
   else if (length == -2)
   {
      charVal = (char*) realloc(charVal, sizeof(char) * bigBufferSize);

      int bigLength = mg_get_var(buf, bufLen, name.c_str(), charVal, sizeof(char) * bigBufferSize);
      if (bigLength > 0)
      {
         retVal = charVal;
      }
   }

   free(charVal);

   return retVal;
}

/*
 * get a specific variable from HTTP request (POST and GET) and returns its value as string
 *
 * Example :
 * if the HTTP_request was http://someHost/XML_SomePage?fooVar=fooVal&barVar=barVal
 * then a call to getVar(conn,"fooVar") will return "fooVal"
 *
 * @param conn The mg_connection from mongoose
 * @param name the name of the variable to fetch
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 *
 * @return the value of the variable *
 */
std::string webtk::getVar(struct mg_connection *conn, std::string name,
   const char* postBuf, size_t postBufLen)
{
   const struct mg_request_info *requestInfo = mg_get_request_info(conn);

   std::string varValue;
   if (strcmp(requestInfo->request_method, "GET") == 0)
   {
      if (requestInfo->query_string != NULL)
      {
         const char *queryString = requestInfo->query_string;
         varValue = getVarFromBuf(queryString, strlen(queryString), name);
      }
   }
   else
   {
      varValue = getVarFromBuf(postBuf, postBufLen, name);
   }

   if (varValue.size() == 0)
      throw HttpVarException(std::string("The parameter '") + name + std::string(
         "' is invalid or empty"));

   return varValue;
}

/*
 * get a specific variable from HTTP request and returns its value as string or the default value
 *
 * Example :
 * if the HTTP_request was http://someHost/XML_SomePage?fooVar=fooVal&barVar=barVal
 * then a call to getVar(conn,"fooVar") will return "fooVal"
 *
 * @param conn The mg_connection from mongoose
 * @param name the name of the variable to fetch
 * @param defaultVal a default value that will be returned if the parameter does not exist or is
 *        empty
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 *
 * @return the value of the variable
 */
std::string webtk::getVar(struct mg_connection *conn, std::string name, std::string defaultVal,
   const char* postBuf, size_t postBufLen)
{
   std::string retVal;
   try
   {
      retVal = webtk::getVar(conn, name, postBuf, postBufLen);
   }
   catch (HttpVarException &e)
   {
      retVal = defaultVal;
   }
   return retVal;
}

/*
 * get a set of parametes from a HTTP request as StringList
 *
 * Example :
 * if the HTTP_request was http://someHost/XML_SomePage?node=node01&node=node02&node=node03
 * then a call to getVar(request_info,"node",outList) will put the following values into outList
 * [node01,node02,node03]
 *
 * @param conn The mg_connection from mongoose
 * @param name the name of the parameters to fetch
 * @param outList a StringList to put the values in
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void webtk::getVarList(struct mg_connection *conn, std::string name, StringList *outList,
   const char* postBuf, size_t postBufLen)
{
   // get the request into a string variable to parse it
   std::string query = getVarStringBuffer(conn, postBuf, postBufLen);
   if(query.size() == 0)
      return;

   // split up the parametes and look for parameters matching the search parameter
   StringList partsList;
   StringTk::explode(query, '&', &partsList);
   for (StringListIter iter = partsList.begin(); iter != partsList.end(); iter++)
   {
      StringList paramAsList;
      StringTk::explode(*iter, '=', &paramAsList);
      if (paramAsList.front().compare(name) == 0)
      {
         outList->push_back(paramAsList.back());
      }
   }
}

/*
 * get a set of parametes from a HTTP request as UInt16List
 *
 * Example :
 * if the HTTP_request was http://someHost/XML_SomePage?node=123&node=124&node=125
 * then a call to getVar(request_info,"node",outList) will put the following values into outList
 * [123,124,125]
 *
 * @param conn The mg_connection from mongoose
 * @param name the name of the parameters to fetch
 * @param outList a StringList to put the values in
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void webtk::getVarList(struct mg_connection *conn, std::string name, UInt16List *outList,
   const char* postBuf, size_t postBufLen)
{
   // get the request into a string variable to parse it
   std::string query = getVarStringBuffer(conn, postBuf, postBufLen);
   if(query.size() == 0)
      return;

   // split up the parametes and look for parameters matching the search parameter
   StringList partsList;
   StringTk::explode(query, '&', &partsList);
   for (StringListIter iter = partsList.begin(); iter != partsList.end(); iter++)
   {
      StringList paramAsList;
      StringTk::explode(*iter, '=', &paramAsList);
      if (paramAsList.front().compare(name) == 0)
      {
         outList->push_back(StringTk::strToUInt(paramAsList.back()));
      }
   }
}

/*
 * puts all parametes from a HTTP request into a map
 *
 * Example :
 * URL : http://SomeHost/XML_SomePage?var1=val1&var2=val2&var3=val3
 *
 * Output of paramsToMap is a map like this : [ (var1,val1), (var2,val2), (var3,val3) ]
 *
 * @param conn The mg_connection from mongoose
 * @param outParamsMap the map with all variable value pairs
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void webtk::paramsToMap(struct mg_connection *conn, StringMap *outParamsMap, const char* postBuf,
   size_t postBufLen)
{
   paramsToMap(conn, outParamsMap, "", postBuf, postBufLen);
}

/*
 * puts all parametes from a HTTP request into a map, but lets you ignore specific parameters
 *
 * Example :
 * URL : http://SomeHost/XML_SomePage?var1=val1&var2=val2&var3=val3
 *
 * A call to the function with paramsToMap(request_info, outMap, "var2") would put the following
 * map into outMap : [ (var1,val1), (var3,val3) ]
 *
 * @param conn The mg_connection from mongoose
 * @param outParamsMap the map with all variable value pairs
 * @param ignoreParam the name of the variable to ignore
 * @param postBuf A buffer which contains the POST content
 * @param postBufLen The length of the POST buffer
 */
void webtk::paramsToMap(struct mg_connection *conn, StringMap *outParamsMap,
   std::string ignoreParam, const char* postBuf, size_t postBufLen)
{
   std::string query = getVarStringBuffer(conn, postBuf, postBufLen);
   if(query.size() == 0)
      return;

   StringList partsList;
   StringTk::explode(query, '&', &partsList);
   for (StringListIter iter = partsList.begin(); iter != partsList.end(); iter++)
   {
      StringList paramAsList;
      StringTk::explode(*iter, '=', &paramAsList);
      if ((!paramAsList.empty()) && (paramAsList.front().compare(ignoreParam) != 0))
      {
         if (paramAsList.size() > 1)
         {
            (*outParamsMap)[paramAsList.front()] = paramAsList.back();
         }
         else
         {
            (*outParamsMap)[paramAsList.front()] = "";
         }
      }
   }
}

/*
 * puts all parametes from a converted HTTP request into a map
 *
 * @param paramsStr the HTTP request, already parsed as string of parameters (i.e. var1=val1&var2=val2)
 * @param outParamsMap a reference to a map that will be used as output map
 */
void webtk::paramsToMap(std::string paramsStr, StringMap *outParamsMap)
{
   StringList partsList;
   StringTk::explode(paramsStr, '&', &partsList);
   for (StringListIter iter = partsList.begin(); iter != partsList.end(); iter++)
   {
      StringList paramAsList;
      StringTk::explode(*iter, '=', &paramAsList);
      (*outParamsMap)[paramAsList.front()] = paramAsList.back();
   }
}

/*
 * returns the IP of the remote host
 *
 * @param request_info The request_info from mongoose
 *
 * @return the IP of the remote host as readable string
 */
std::string webtk::getIP(const struct mg_request_info *request_info)
{
   long long_ip = request_info->remote_ip;
   struct in_addr addr;
   addr.s_addr = long_ip;
   unsigned char* cIP = (unsigned char*)&addr.s_addr;

   std::string ipString =
      StringTk::uintToStr(cIP[3]) + "." +
      StringTk::uintToStr(cIP[2]) + "." +
      StringTk::uintToStr(cIP[1]) + "." +
      StringTk::uintToStr(cIP[0]);

   return ipString;
}
