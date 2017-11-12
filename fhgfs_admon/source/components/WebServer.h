#ifndef WEBSERVER_H_
#define WEBSERVER_H_

/*
 * The integrated webserver; runs as a thread and internally starts a mongoose
 * (http://code.google.com/p/mongoose/) thread to listen for incoming HTTP connections.
 * The Admon GUI communicates with this webserver
 *
 * mongoose is a single .c file included in the source folder "thirdparty"
 */


#include <common/app/log/LogContext.h>
#include <common/Common.h>
#include <common/components/ComponentInitException.h>
#include <common/threading/PThread.h>
#include <toolkit/xml/xmlhelper.h>

#include <mongoose.h>


class WebServer : public PThread
{
   private:
      LogContext log;
      char port[10];
      struct mg_context *ctx;
      struct mg_callbacks callbacks;

      static int requestHandler(struct mg_connection *conn);
      static int errorHandler(struct mg_connection *conn, int status);
      static int websocketConnectHandler(const struct mg_connection *conn);


   public:
      WebServer(int port);
      virtual ~WebServer() { };
      virtual void run();
};

#endif /*WEBSERVER_H_*/
