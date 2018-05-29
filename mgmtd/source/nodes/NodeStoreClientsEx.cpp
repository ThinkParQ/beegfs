#include <common/threading/SafeMutexLock.h>
#include <common/toolkit/Random.h>
#include <common/toolkit/serialization/Serialization.h>
#include <program/Program.h>
#include "NodeStoreClientsEx.h"


#define NODESTOREEX_TMPFILE_EXT  ".tmp" /* temporary extension for saved files until we rename */


NodeStoreClientsEx::NodeStoreClientsEx() : NodeStoreClients(false)
{
   this->storeDirty = false;
}

bool NodeStoreClientsEx::addOrUpdateNode(NodeHandle node)
{
   bool nodeAdded = NodeStoreClients::addOrUpdateNode(std::move(node));

   // Note: NodeStore might be dirty even if nodeAdded==false
   //    (e.g. because the ports might have changed)

   SafeMutexLock mutexLock(&storeMutex);
   storeDirty = true;
   mutexLock.unlock();
   
   return nodeAdded;
}

bool NodeStoreClientsEx::deleteNode(NumNodeID nodeNumID)
{
   bool delRes = NodeStoreClients::deleteNode(nodeNumID);
   
   if(delRes)
   {
      SafeMutexLock mutexLock(&storeMutex);
      storeDirty = true;
      mutexLock.unlock();
   }
   
   return delRes;
}

/**
 * Note: setStorePath must be called before using this.
 */
bool NodeStoreClientsEx::loadFromFile(bool longNodeIDs)
{
   /* note: we use a separate storeMutex here because we don't want to keep the standard mutex
      locked during disk access. */

   LogContext log("NodeStoreClientsEx (load)");
   
   bool retVal = false;
   boost::scoped_array<char> buf;
   int readRes;

   if(!this->storePath.length() )
      return false;

   SafeMutexLock mutexLock(&storeMutex);
   
   int fd = open(storePath.c_str(), O_RDONLY, 0);
   if(fd == -1)
   { // open failed
      LOG_DEBUG_CONTEXT(log, 4, "Unable to open nodes file: " + storePath + ". " +
         "SysErr: " + System::getErrString() );
         
      goto err_unlock;
   }

   struct stat stat;
   if (fstat(fd, &stat) != 0)
   {
      LOG(GENERAL, ERR, "Could not stat nodes file", storePath, sysErr());
      goto err_close;
   }

   buf.reset(new char[stat.st_size]);
   readRes = read(fd, buf.get(), stat.st_size);
   if(readRes <= 0)
   { // reading failed
      LOG_DEBUG_CONTEXT(log, 4, "Unable to read nodes file: " + storePath + ". " +
         "SysErr: " + System::getErrString() );
   }
   else
   { // parse contents
      retVal = loadFromBuf(buf.get(), readRes, longNodeIDs);
   }

err_close:
   close(fd);

err_unlock:
   mutexLock.unlock();
   
   return retVal;
}


/**
 * Note: setStorePath must be called before using this.
 */
bool NodeStoreClientsEx::saveToFile()
{
   /* note: we use a separate storeMutex here because we don't want to keep the standard mutex
      locked during disk access. */

   LogContext log("NodeStoreClientsEx (save)");
   
   bool retVal = false;

   boost::scoped_array<char> buf;
   ssize_t bufLen;
   ssize_t writeRes;
   int renameRes;

   if(!this->storePath.length() )
      return false;
   
   std::string storePathTmp(storePath + NODESTOREEX_TMPFILE_EXT);

   SafeMutexLock mutexLock(&storeMutex); // L O C K

   // create/trunc file
   int openFlags = O_CREAT|O_TRUNC|O_WRONLY;

   int fd = open(storePathTmp.c_str(), openFlags, 0666);
   if(fd == -1)
   { // error
      log.logErr("Unable to create nodes file: " + storePathTmp + ". " +
         "SysErr: " + System::getErrString() );
         
      goto err_unlock;
   }

   // file created => store data
   bufLen = saveToBuf(buf);
   if (bufLen < 0)
   {
      log.logErr("Unable to serialize nodes file: " + storePathTmp + ".");
      goto err_closefile;
   }

   writeRes = write(fd, buf.get(), bufLen);
   if(writeRes != (ssize_t)bufLen)
   {
      log.logErr("Unable to store nodes file: " + storePathTmp + ". " +
         "SysErr: " + System::getErrString() );

      goto err_closefile;
   }

   close(fd);
   
   renameRes = rename(storePathTmp.c_str(), storePath.c_str() );
   if(renameRes == -1)
   {
      log.logErr("Unable to rename nodes file: " + storePathTmp + ". " +
         "SysErr: " + System::getErrString() );

      goto err_unlink;
   }

   storeDirty = false;
   retVal = true;

   LOG_DEBUG_CONTEXT(log, Log_DEBUG, "Nodes file stored: " + storePath);

   mutexLock.unlock(); // U N L O C K

   return retVal;

   
   // error compensation
err_closefile:
   close(fd);

err_unlink:
   unlink(storePathTmp.c_str() );

err_unlock:
   mutexLock.unlock(); // U N L O C K
   
   return retVal;
}

/**
 * Note: Does not require locking (uses the locked addOrUpdate() method).
 */
bool NodeStoreClientsEx::loadFromBuf(const char* buf, unsigned bufLen, bool longNodeIDs)
{
   LogContext log("NodeStoreClientsEx (load)");

   Deserializer des(buf, bufLen);

   // nodeList
   std::vector<NodeHandle> nodeList;

   if (longNodeIDs)
      des % nodeList;
   else
      des % Node::vectorWithShortIDs(nodeList);

   if (!des.good())
   { // preprocessing failed
      log.logErr("Unable to deserialize node from buffer (preprocessing error)");
      return false;
   }

   // add all nodes
   for (auto iter = nodeList.begin(); iter != nodeList.end(); iter++)
   {
      auto& node = *iter;
      std::string nodeID = node->getID();

      // set local nic capabilities...

      Node& localNode = Program::getApp()->getLocalNode(); /* (note: not the one from this store) */
      NicAddressList localNicList(localNode.getNicList() );
      NicListCapabilities localNicCaps;

      NetworkInterfaceCard::supportedCapabilities(&localNicList, &localNicCaps);
      node->getConnPool()->setLocalNicCaps(&localNicCaps);

      // actually add the node to the store...

      /* note: using NodeStoreClientsEx::addOrUpdate() here would deadlock, because it would try to
         acquire the storeMutex. */

      bool nodeAdded = NodeStoreClients::addOrUpdateNode(std::move(node));

      LOG_DEBUG_CONTEXT(log, Log_DEBUG, "Added new node from buffer: " + nodeID);
      IGNORE_UNUSED_VARIABLE(nodeAdded);
      IGNORE_UNUSED_VARIABLE(&nodeID);
   }

   return true;
}

/**
 * Note: The NodeStoreClients mutex may not be acquired when this is called (because this uses the
 * locked referenceAllNodes() method).
 */
ssize_t NodeStoreClientsEx::saveToBuf(boost::scoped_array<char>& buf)
{
   return serializeIntoNewBuffer(referenceAllNodes(), buf);
}
