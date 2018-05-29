#include "GetClientNodesWork.h"
#include <program/Program.h>

void GetClientNodesWork::process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen)
{
   std::vector<NodeHandle> clientsList;
   NumNodeIDList addedClients;
   NumNodeIDList removedClients;

   // download the nodes from the mgmtd and sync them with the local node store
   if (NodesTk::downloadNodes(*mgmtdNode, NODETYPE_Client, clientsList, false))
   {
      clients->syncNodes(clientsList, &addedClients, &removedClients, true);

      if(addedClients.size())
         log.log(Log_WARNING, std::string("Nodes added (sync results): ")
               + StringTk::uintToStr(addedClients.size())
               + " (Type: " + Node::nodeTypeToStr(NODETYPE_Client) + ")");

      if(removedClients.size())
         log.log(Log_WARNING, std::string("Nodes removed (sync results): ")
               + StringTk::uintToStr(removedClients.size()) + " (Type: "
               + Node::nodeTypeToStr(NODETYPE_Client) + ")");
   }
   else
   {
      log.log(Log_ERR, "Couldn't download client list from management daemon.");
   }
}
