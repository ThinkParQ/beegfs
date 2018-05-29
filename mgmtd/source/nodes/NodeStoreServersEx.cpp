#include <common/threading/SafeMutexLock.h>
#include <common/toolkit/Random.h>
#include <common/toolkit/serialization/Serialization.h>
#include <program/Program.h>
#include "NodeStoreServersEx.h"


#define NODESTOREEX_TMPFILE_EXT  ".tmp" /* temporary extension for saved files until we rename */


NodeStoreServersEx::NodeStoreServersEx(NodeType storeType) :
   NodeStoreServers(storeType, false), storeDirty(false)
{ }

bool NodeStoreServersEx::addOrUpdateNodeEx(std::shared_ptr<Node> node, NumNodeID* outNodeNumID)
{
   bool nodeAdded = NodeStore::addOrUpdateNodeEx(std::move(node), outNodeNumID);

   // Note: NodeStore might be dirty even if nodeAdded==false
   //    (e.g. because the ports might have changed)
   setStoreDirty();

   return nodeAdded;
}

bool NodeStoreServersEx::deleteNode(NumNodeID nodeID)
{
   bool delRes = NodeStore::deleteNode(nodeID);

   if(delRes)
      setStoreDirty();

   return delRes;
}

bool NodeStoreServersEx::setRootNodeNumID(NumNodeID nodeID, bool ignoreExistingRoot,
   bool isBuddyMirrored)
{
   bool setRes = NodeStore::setRootNodeNumID(nodeID, ignoreExistingRoot, isBuddyMirrored);

   if(setRes)
      setStoreDirty();

   return setRes;
}

/**
 * @return 0 as invalid ID if store is empty
 */
NumNodeID NodeStoreServersEx::getLowestNodeID()
{
   NumNodeID lowestID;

   SafeMutexLock mutexLock(&mutex);

   if(!activeNodes.empty() )
      lowestID = activeNodes.begin()->first;

   mutexLock.unlock();

   return lowestID;
}

/**
 * Note: setStorePath must be called before using this.
 */
bool NodeStoreServersEx::loadFromFile(bool longNodeIDs)
{
   /* note: we use a separate storeMutex here because we don't want to keep the standard mutex
      locked during disk access. */

   LogContext log("NodeStoreServersEx (load)");

   bool retVal = false;
   boost::scoped_array<char> buf;
   int readRes;

   if(!this->storePath.length() )
      return false;

   SafeMutexLock mutexLock(&storeMutex);

   int fd = open(storePath.c_str(), O_RDONLY, 0);
   if(fd == -1)
   { // open failed
      LOG_DEBUG_CONTEXT(log, Log_DEBUG, "Unable to open nodes file: " + storePath + ". " +
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
      LOG_DEBUG_CONTEXT(log, Log_DEBUG, "Unable to read nodes file: " + storePath + ". " +
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
bool NodeStoreServersEx::saveToFile()
{
   /* note: we use a separate storeMutex here because we don't want to keep the standard mutex
      locked during disk access. */

   LogContext log("NodeStoreServersEx (save)");

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
   if (!buf)
   {
      log.logErr("Unable to store nodes file: " + storePathTmp + ".");
      goto err_closefile;
   }

   writeRes = write(fd, buf.get(), bufLen);

   if(writeRes != bufLen)
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
 * Fill the attached state store with the list of servers loaded.
 * This is only useful for metadata nodes (since for storage servers, the state store containes
 * target IDs, not node IDs).
 */
void NodeStoreServersEx::fillStateStore()
{
   if (!stateStore)
   {
      LOG_DEBUG(__func__, Log_DEBUG, "No StateStore attached.");
      return;
   }

   UInt16List ids;
   UInt8List reachabilityStates;
   UInt8List consistencyStates;

   SafeMutexLock mutexLock(&mutex); // L O C K

   for (auto it = activeNodes.begin(); it != activeNodes.end(); ++it)
   {
      ids.push_back(it->first.val());
      reachabilityStates.push_back(TargetReachabilityState_POFFLINE);
      consistencyStates.push_back(TargetConsistencyState_GOOD);
   }

   mutexLock.unlock(); // U N L O C K

   stateStore->syncStatesFromLists(ids, reachabilityStates, consistencyStates);
}

/**
 * Note: Does not require locking (uses the locked addOrUpdate() method).
 */
bool NodeStoreServersEx::loadFromBuf(const char* buf, unsigned bufLen, bool longNodeIDs)
{
   LogContext log("NodeStoreServersEx (load)");

   Deserializer des(buf, bufLen);

   NumNodeID rootID;
   bool rootIsBuddyMirrored;

   if (longNodeIDs)
   {
      des % rootID;
   }
   else
   {
      uint16_t id;
      des % id;
      rootID = NumNodeID(id);
   }

   if (!des.good())
   {
      log.logErr("Unable to deserialize root ID from buffer");
      return false;
   }

   if (longNodeIDs)
   {
      des % rootIsBuddyMirrored;
      if (!des.good())
      {
         log.logErr("Unable to deserialize rootIsBuddyMirrored from buffer");
         return false;
      }
   }
   else
   {
      rootIsBuddyMirrored = false;
   }

   NodeStore::setRootNodeNumID(rootID, false, rootIsBuddyMirrored); /* be careful not to call the
      virtual method of this object, because that would try to save the updated file (=> deadlock)*/

   // nodeList
   std::vector<NodeHandle> nodeList;

   if (longNodeIDs)
      des % nodeList;
   else
      des % Node::vectorWithShortIDs(nodeList);

   if(!des.good())
   {
      log.logErr("Unable to deserialize node from buffer");
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

      /* note: using NodeStoreServersEx::addOrUpdate() here would deadlock (because it would try to
         acquire the storeMutex), so we explicitly call the parent class method. */

      bool nodeAdded = NodeStoreServers::addOrUpdateNodeEx(std::move(node), NULL);

      LOG_DEBUG_CONTEXT(log, Log_DEBUG, "Added new node from buffer: " + nodeID);
      IGNORE_UNUSED_VARIABLE(nodeAdded);
      IGNORE_UNUSED_VARIABLE(&nodeID);
   }

   return true;
}

namespace {
struct NodeStoreData
{
   NumNodeID rootNodeID;
   bool rootIsBuddyMirrored;
   const std::vector<NodeHandle>& nodeList;

   static void serialize(const NodeStoreData* obj, Serializer& ser)
   {
      ser
         % obj->rootNodeID
         % obj->rootIsBuddyMirrored
         % obj->nodeList;
   }
};
}

/**
 * Note: The NodeStore mutex may not be acquired when this is called (because this uses the locked
 * referenceAllNodes() method).
 */
ssize_t NodeStoreServersEx::saveToBuf(boost::scoped_array<char>& buf)
{
   NumNodeID rootNodeID = getRootNodeNumID();

   auto nodeList = referenceAllNodes();

   NodeStoreData data = { rootNodeID, rootIsBuddyMirrored, nodeList };
   ssize_t result = serializeIntoNewBuffer(data, buf);

   return result;
}
