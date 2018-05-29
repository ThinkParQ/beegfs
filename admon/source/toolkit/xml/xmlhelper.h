#ifndef XMLHELPER_H
#define XMLHELPER_H

/*
 * Xmlhelper is a collection of a lot static functions used as callback functions by WebServer.
 * The class makes use of "Tiny XML C++" (http://code.google.com/p/ticpp/), which is directly
 * included in thirdparty/ticpp, to create XML pages, that are delivered to the GUI.
 *
 * Example usage of ticcp in XMLCreator functions:
 *
 * // create XML document
 * TiXmlDocument doc;
 *
 * //create a root element with the name data
 * TiXmlElement *rootElement = new TiXmlElement("data");
 *
 * // link the rootElement to the doc (each node needs to be linked to a parent)
 * doc.LinkEndChild(rootElement);
 *
 * // create another node, named foo and link it to the rootElement
 * TiXmlElement *fooElement = new TiXmlElement("foo");
 * rootElement->LinkEndChild(fooElement);
 *
 * // create a TextElement and directly link it to the rootElement
 * fooElement->LinkEndChild(new TiXmlText("Text for foo"));
 *
 *
 *
 * This function creates the following XML document :
 * <data>
 *   <foo>Text for foo</foo>
 * </data>
 *
 * The document can be printed to the mongoose output stream using
 * XmlCreator::writeDoc() afterwards
 */

#include <common/toolkit/TimeAbs.h>

#include <fstream>
#include <grp.h>
#include <mongoose.h>
#include <pwd.h>
#include <sstream>
#include <ticpp.h>


#define MONGOOSE_DEBUG

namespace xmlhelper
{
   void sendError(struct mg_connection *conn, const struct mg_request_info *request_info,
      std::string errorMessage);

   void admonGui(struct mg_connection *conn, const struct mg_request_info *request_info);

   // install
   void checkDistriArch(struct mg_connection *conn, const struct mg_request_info *request_info,
      const char* postBuf, size_t postBufLen);
   void checkSSH(struct mg_connection *conn, const struct mg_request_info *request_info,
      const char* postBuf, size_t postBufLen);
   void configBasicConfig(struct mg_connection *conn, const struct mg_request_info *request_info,
      const char* postBuf, size_t postBufLen);
   void configIBConfig(struct mg_connection *conn, const struct mg_request_info *request_info,
      const char* postBuf, size_t postBufLen);
   void configRoles(struct mg_connection *conn, const struct mg_request_info *request_info,
      const char* postBuf, size_t postBufLen);
   void getCheckClientDependenciesLog(struct mg_connection *conn,
      const struct mg_request_info *request_info, const char* postBuf, size_t postBufLen);
   void install(struct mg_connection *conn, const struct mg_request_info *request_info,
      const char* postBuf, size_t postBufLen);
   void installInfo(struct mg_connection *conn, const struct mg_request_info *request_info,
      const char* postBuf, size_t postBufLen);
   void uninstall(struct mg_connection *conn, const struct mg_request_info *request_info,
      const char* postBuf, size_t postBufLen);
   void createNewSetupLogfile(struct mg_connection *conn,
      const struct mg_request_info *request_info, const char* postBuf, size_t postBufLen);

   // management
   void changePassword(struct mg_connection *conn, const struct mg_request_info *request_info,
      const char* postBuf, size_t postBufLen);
   void fileBrowser(struct mg_connection *conn, const struct mg_request_info *request_info,
      const char* postBuf, size_t postBufLen);
   void getRemoteLogFile(struct mg_connection *conn, const struct mg_request_info *request_info,
      const char* postBuf, size_t postBufLen);
   void getGroups(struct mg_connection *conn, const struct mg_request_info *request_info,
      const char* postBuf, size_t postBufLen);
   void knownProblems(struct mg_connection *conn, const struct mg_request_info *request_info,
      const char* postBuf, size_t postBufLen);
   void getLocalLogFile(struct mg_connection *conn, const struct mg_request_info *request_info,
      const char* postBuf, size_t postBufLen);
   void mailNotification(struct mg_connection *conn, const struct mg_request_info *request_info,
      const char* postBuf, size_t postBufLen);
   void nodeList(struct mg_connection *conn, const struct mg_request_info *request_info,
      const char* postBuf, size_t postBufLen);
   void scriptSettings(struct mg_connection *conn, const struct mg_request_info *request_info,
      const char* postBuf, size_t postBufLen);
   void startStop(struct mg_connection *conn, const struct mg_request_info *request_info,
      const char* postBuf, size_t postBufLen);
   void striping(struct mg_connection *conn, const struct mg_request_info *request_info,
      const char* postBuf, size_t postBufLen);
   void getVersionString(struct mg_connection *conn, const struct mg_request_info *request_info,
      const char* postBuf, size_t postBufLen);
   void testEMail(struct mg_connection *conn, const struct mg_request_info *request_info,
      const char* postBuf, size_t postBufLen);

   // security
   void getNonce(struct mg_connection *conn, const struct mg_request_info *request_info,
      const char* postBuf, size_t postBufLen);
   void authInfo(struct mg_connection *conn, const struct mg_request_info *request_info,
      const char* postBuf, size_t postBufLen);
   void preAuthInfo(struct mg_connection *conn, const struct mg_request_info *request_info,
         const char* postBuf, size_t postBufLen);

   // stats
   void metaNode(struct mg_connection *conn, const struct mg_request_info *request_info,
      const char* postBuf, size_t postBufLen);
   void metaNodesOverview(struct mg_connection *conn, const struct mg_request_info *request_info,
      const char* postBuf, size_t postBufLen);
   void storageNode(struct mg_connection *conn, const struct mg_request_info *request_info,
      const char* postBuf, size_t postBufLen);
   void storageNodesOverview(struct mg_connection *conn, const struct mg_request_info *request_info,
      const char* postBuf, size_t postBufLen);
   void clientStats(struct mg_connection *conn, const struct mg_request_info * request_info,
      const char* postBuf, size_t postBufLen);
   void userStats(struct mg_connection *conn, const struct mg_request_info * request_info,
      const char* postBuf, size_t postBufLen);
   void stats(bool doUserStats, struct mg_connection *conn,
      const struct mg_request_info * request_info, const char* postBuf, size_t postBufLen);

   inline long calculateStartTime(uint64_t timespanMinutes, long &timestampEndMS)
   {
      TimeAbs t;
      t.setToNow();
      timestampEndMS = t.getTimeMS();
      return (t.getTimeMS() - (timespanMinutes * 60 * 1000));
   }
};

#endif /* XMLHELPER_H */
