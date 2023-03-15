#include <common/toolkit/serialization/Serialization.h>
#include <common/toolkit/TempFileTk.h>
#include <program/Program.h>
#include "NodeStoreServersEx.h"

#include <mutex>

 /*
  * maximum ID for server nodes, 0 is reserved.
  * note: at the moment we restrict server node IDs to 16 bit to be compatible with targetIDs
  */
#define NODESTORESERVERS_MAX_ID (USHRT_MAX)


NodeStoreServersEx::NodeStoreServersEx(NodeType storeType) :
   NodeStoreServers(storeType, false)
{ }

/**
 * @return 0 as invalid ID if store is empty
 */
NumNodeID NodeStoreServersEx::getLowestNodeID()
{
   NumNodeID lowestID;

   const std::lock_guard<Mutex> lock(mutex);

   if(!activeNodes.empty() )
      lowestID = activeNodes.begin()->first;

   return lowestID;
}

/**
 * Note: setStorePath must be called before using this.
 */
bool NodeStoreServersEx::loadFromFile(RootInfo* root, bool v6Format)
{
   /* note: we use a separate storeMutex here because we don't want to keep the standard mutex
      locked during disk access. */

   LogContext log("NodeStoreServersEx (load)");

   bool retVal = false;
   boost::scoped_array<char> buf;
   int readRes;

   if(!this->storePath.length() )
      return false;

   const std::lock_guard<Mutex> lock(storeMutex);

   int fd = open(storePath.c_str(), O_RDONLY, 0);
   if(fd == -1)
   { // open failed
      LOG_DEBUG_CONTEXT(log, Log_DEBUG, "Unable to open nodes file: " + storePath + ". " +
         "SysErr: " + System::getErrString() );

      return false;
   }

   struct stat stat;
   if (fstat(fd, &stat) != 0)
   {
      LOG(GENERAL, ERR, "Could not stat nodes file", storePath, sysErr);
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
      retVal = loadFromBuf(buf.get(), readRes, root, v6Format);
   }

err_close:
   close(fd);

   return retVal;
}


/**
 * Note: setStorePath must be called before using this.
 */
bool NodeStoreServersEx::saveToFile(const RootInfo* root)
{
   /* note: we use a separate storeMutex here because we don't want to keep the standard mutex
      locked during disk access. */

   LogContext log("NodeStoreServersEx (save)");

   boost::scoped_array<char> buf;
   ssize_t bufLen;

   if(!this->storePath.length() )
      return false;

   const std::lock_guard<Mutex> lock(storeMutex);

   bufLen = saveToBuf(buf, root);
   if (!buf)
   {
      log.logErr("Unable to create buffer to store " + storePath + ".");
      return false;
   }

   if (TempFileTk::storeTmpAndMove(storePath, buf.get(), bufLen) == FhgfsOpsErr_SUCCESS)
   {
      LOG_DEBUG_CONTEXT(log, Log_DEBUG, "Nodes file stored: " + storePath);
      return true;
   }
   else
      return false;
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

   {
      const std::lock_guard<Mutex> lock(mutex);

      for (auto it = activeNodes.begin(); it != activeNodes.end(); ++it)
      {
         ids.push_back(it->first.val());
         reachabilityStates.push_back(TargetReachabilityState_POFFLINE);
         consistencyStates.push_back(TargetConsistencyState_GOOD);
      }
   }

   stateStore->syncStatesFromLists(ids, reachabilityStates, consistencyStates);
}

/**
 * Note: Does not require locking (uses the locked addOrUpdate() method).
 */
bool NodeStoreServersEx::loadFromBuf(const char* buf, unsigned bufLen, RootInfo* root,
      bool v6Format)
{
   LogContext log("NodeStoreServersEx (load)");

   Deserializer des(buf, bufLen);

   if (!v6Format) {
      uint32_t version;

      des % version;
      if (!des.good()) {
         LOG(GENERAL, ERR, "Unable to deserialize version from buffer.");
         return false;
      }

      if (version > BEEGFS_DATA_VERSION) {
         LOG(GENERAL, ERR, "Failed to load node store: version mismatch.");
         return false;
      }
   }

   NumNodeID rootID;
   bool rootIsBuddyMirrored;

   des % rootID;

   if (!des.good())
   {
      log.logErr("Unable to deserialize root ID from buffer");
      return false;
   }

   des % rootIsBuddyMirrored;
   if (!des.good())
   {
      log.logErr("Unable to deserialize rootIsBuddyMirrored from buffer");
      return false;
   }

   if (root)
      root->set(rootID, rootIsBuddyMirrored);

   // nodeList
   std::vector<NodeHandle> nodeList;

   if (v6Format)
      des % Node::inV6Format(nodeList);
   else
      des % nodeList;

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
      node->getConnPool()->setLocalNicList(localNicList, localNicCaps);

      // actually add the node to the store...

      /* note: using NodeStoreServersEx::addOrUpdate() here would deadlock (because it would try to
         acquire the storeMutex), so we explicitly call the parent class method. */

      bool nodeAdded = (NodeStoreServers::addOrUpdateNodeEx(std::move(node), NULL) == NodeStoreResult::Added);

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
         % uint32_t(BEEGFS_DATA_VERSION)
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
ssize_t NodeStoreServersEx::saveToBuf(boost::scoped_array<char>& buf, const RootInfo* root)
{
   NumNodeID rootNodeID = root ? root->getID() : NumNodeID();
   bool rootIsBuddyMirrored = root ? root->getIsMirrored() : false;

   auto nodeList = referenceAllNodes();

   NodeStoreData data = { rootNodeID, rootIsBuddyMirrored, nodeList };
   ssize_t result = serializeIntoNewBuffer(data, buf);

   return result;
}


/**
 * Generate a new numeric node ID and assign it to the node.
 * This method will also first check whether this node already has a numeric ID assigned and just
 * didn't know of it yet (e.g. because of thread races.)
 *
 * Note: Caller must hold lock.
 * Note: Caller is expected to add the node to the activeNodes map and assign the numeric ID
 *       (because new IDs are picked based on assigned IDs in the activeNodes map).
 *
 * @return 0 if all available numIDs are currently assigned, so none are left
 */
NumNodeID NodeStoreServersEx::generateID(Node& node) const
{
   // check whether this node's stringID is already associated with an active or deleted numID

   NumNodeID previousNumID = retrieveNumIDFromStringID(node.getID());
   if (previousNumID)
      return previousNumID;

   if (activeNodes.empty())
      return NumNodeID(1);

   if (activeNodes.rbegin()->first.val() < NODESTORESERVERS_MAX_ID)
      return NumNodeID(activeNodes.rbegin()->first.val() + 1);

   if (activeNodes.begin()->first.val() > 1)
      return NumNodeID(1);

   const auto gap = std::adjacent_find(
         activeNodes.begin(), activeNodes.end(),
         [] (const auto& a, const auto& b) {
            return a.first.val() + 1 < b.first.val();
         });

   if (gap != activeNodes.end())
      return NumNodeID(gap->first.val() + 1);

   // still no new ID => all IDs used!
   LOG(GENERAL, ERR, "Ran out of server node IDs!");
   return {};
}
