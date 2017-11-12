#ifndef WEBTK_H_
#define WEBTK_H_

/*
 * static helper functions for dealing with http variables (parsing the HTTP variables that were
 * passed to mongoose) in several ways
 */

#include <common/toolkit/StringTk.h>
#include <exception/HttpVarException.h>

#include <mongoose.h>
#include <string>



namespace webtk
{
   void debugMongoose(struct mg_connection *conn, const struct mg_request_info *request_info,
      const char* postBuf, size_t postBufLen);
   void writeDoc(struct mg_connection *conn, const struct mg_request_info *request_info,
      TiXmlDocument doc);
   void sendHttpError(struct mg_connection *conn, const struct mg_request_info *request_info,
      const int errorCode, std::string errorMessage);
   void serveFile(struct mg_connection *conn, const struct mg_request_info *request_info,
      std::string filename);

   std::string getVar(struct mg_connection *conn, std::string name, const char* postBuf,
      size_t postBufLen) throw (HttpVarException);
   std::string getVar(struct mg_connection *conn, std::string name,std::string defaultVal,
      const char* postBuf, size_t postBufLen);
   void paramsToMap(struct mg_connection *conn, StringMap *outParamsMap, const char* postBuf,
      size_t postBufLen);
   void paramsToMap(struct mg_connection *conn, StringMap *outParamsMap, std::string ignoreParam,
      const char* postBuf, size_t postBufLen);
   void paramsToMap(std::string paramsStr, StringMap *outParamsMap);
   void getVarList(struct mg_connection *conn, std::string name, StringList *outList,
      const char* postBuf, size_t postBufLen);
   void getVarList(struct mg_connection *conn, std::string name, UInt16List *outList,
      const char* postBuf, size_t postBufLen);
   std::string getIP(const struct mg_request_info *request_info);

   std::string getVarFromBuf(const char *buf, size_t bufLen, std::string name);
   std::string getVarFromGet(struct mg_connection *conn, std::string name);
   std::string getVarFromPost(struct mg_connection *conn, std::string name);
   std::string getVarStringBuffer(struct mg_connection *conn, const char* postBuf,
      size_t postBufLen);
};

#endif /* WEBTK_H_ */
