package com.beegfs.admon.gui.common.nodes;


import com.beegfs.admon.gui.common.enums.NodeTypesEnum;
import static com.beegfs.admon.gui.common.enums.NodeTypesEnum.ADMON;
import static com.beegfs.admon.gui.common.enums.NodeTypesEnum.CLIENT;
import static com.beegfs.admon.gui.common.enums.NodeTypesEnum.MANAGMENT;
import static com.beegfs.admon.gui.common.enums.NodeTypesEnum.METADATA;
import static com.beegfs.admon.gui.common.enums.NodeTypesEnum.STORAGE;
import com.beegfs.admon.gui.common.exceptions.CommunicationException;
import com.beegfs.admon.gui.common.threading.UpdateThread;
import com.beegfs.admon.gui.common.tools.HttpTk;
import static java.lang.Integer.parseInt;
import java.util.ArrayList;
import java.util.TreeMap;
import java.util.concurrent.locks.Condition;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;
import java.util.logging.Level;
import java.util.logging.Logger;



public class NodeEnvironment
{
   static final Logger LOGGER = Logger.getLogger(NodeEnvironment.class.getCanonicalName());
   private static final String THREAD_NAME = "NodeListUpdater";

   private static final String BASE_URL = HttpTk.generateAdmonUrl("/XML_NodeList");
   private static final String URL_OPTIONS = "?clients=true&admon=true";
   private static final int UPDATE_INTERVAL_MS = 5000;

   private final TypedNodes mgmtdNodes;
   private final TypedNodes metaNodes;
   private final TypedNodes storageNodes;
   private final TypedNodes clientNodes;
   private final TypedNodes admonNodes;

   private final Lock updateLock;
   private final Condition newData;
   private final NodesUpdateThread updateThread;


   public NodeEnvironment()
   {
      this.mgmtdNodes = new TypedNodes(MANAGMENT);
      this.metaNodes = new TypedNodes(METADATA);
      this.storageNodes = new TypedNodes(STORAGE);
      this.clientNodes = new TypedNodes(CLIENT);
      this.admonNodes = new TypedNodes(ADMON);

      this.updateLock = new ReentrantLock();
      this.newData = updateLock.newCondition();
      updateThread = new NodesUpdateThread();
   }

   public boolean add(NodeTypesEnum type, Nodes newNodes)
   {
      TypedNodes nodes = getNodes(type);
      return nodes.addNodes(newNodes);
   }

   public int syncNodes(NodeTypesEnum type, Nodes newNodes)
   {
      Nodes nodes = getNodes(type);
      return nodes.syncNodes(newNodes);
   }

   public boolean removeNodes(NodeTypesEnum type, Nodes newNodes)
   {
      TypedNodes nodes = getNodes(type);
      return nodes.removeNodes(newNodes);
   }

   public TypedNodes getNodes(NodeTypesEnum type)
   {
      switch(type)
      {
         case MANAGMENT:
            return mgmtdNodes;
         case METADATA:
            return metaNodes;
         case STORAGE:
            return storageNodes;
         case CLIENT:
            return clientNodes;
         case ADMON:
            return admonNodes;
         default:
            return null;
      }
   }

   public Node getNode(NodeTypesEnum type, int nodeNumID)
   {
      switch(type)
      {
         case MANAGMENT:
            return mgmtdNodes.getNode(nodeNumID);
         case METADATA:
            return metaNodes.getNode(nodeNumID);
         case STORAGE:
            return storageNodes.getNode(nodeNumID);
         case CLIENT:
            return clientNodes.getNode(nodeNumID);
         case ADMON:
            return admonNodes.getNode(nodeNumID);
         default:
            return null;
      }
   }

   public void startUpdateThread()
   {
      updateThread.start();
   }

   public void stopUpdateThread()
   {
      updateThread.shouldStop();
   }

   public Lock getUpdateLock()
   {
      return updateLock;
   }

   public Condition getNewDataCondition()
   {
      return newData;
   }

   private class NodesUpdateThread extends UpdateThread
   {
      NodesUpdateThread()
      {
         super(BASE_URL + URL_OPTIONS, false, UPDATE_INTERVAL_MS, THREAD_NAME);
      }

       @Override
      public void run()
      {
         while (!stop)
         {
            lock.lock();
            try
            {
               gotData.await();

               if(!stop)
               {
                  int numMergedNodes = mergeUpdateToNodes();

                  if(numMergedNodes < 0)
                  {
                     LOGGER.log(Level.FINEST, "Could not merge updated nodes.");
                  }
                  else if(numMergedNodes > 0)
                  {
                     updateLock.lock();
                     try
                     {
                        newData.signalAll();
                     }
                     finally
                     {
                        updateLock.unlock();
                     }
                  }
               }
            }
            catch (InterruptedException ex)
            {
               LOGGER.log(Level.FINEST, "Internal error.", ex);
            }
            catch (CommunicationException ex)
            {
               LOGGER.log(Level.FINEST, "Communication error.", ex);
            }
            finally
            {
               lock.unlock();
            }
         }
      }

      private int mergeUpdateToNodes() throws CommunicationException
      {
         TypedNodes newMgmtdNodes = new TypedNodes(MANAGMENT);
         TypedNodes newMetaNodes = new TypedNodes(METADATA);
         TypedNodes newStorageNodes = new TypedNodes(STORAGE);
         TypedNodes newClientNodes = new TypedNodes(CLIENT);
         TypedNodes newAdmonNodes = new TypedNodes(ADMON);

         ArrayList<TreeMap<String, String>> groupedNodesMgmtd =
            parser.getVectorOfAttributeTreeMaps("mgmtd");
         for (TreeMap<String, String> groupedNode : groupedNodesMgmtd)
         {
            String group = groupedNode.get("group");
            String nodeID = groupedNode.get("value");
            int nodeNumID = parseInt(groupedNode.get("nodeNumID"));
            Node node = new Node(nodeNumID, nodeID, group, NodeTypesEnum.MANAGMENT);
            boolean added = newMgmtdNodes.add(node);
            if(!added)
            {
               return -1;
            }
         }

         ArrayList<TreeMap<String, String>> groupedNodesMeta =
            parser.getVectorOfAttributeTreeMaps("meta");
         for (TreeMap<String, String> groupedNode : groupedNodesMeta)
         {
            String group = groupedNode.get("group");
            String nodeID = groupedNode.get("value");
            int nodeNumID = parseInt(groupedNode.get("nodeNumID"));
            Node node = new Node(nodeNumID, nodeID, group, NodeTypesEnum.METADATA);
            boolean added = newMetaNodes.add(node);
            if(!added)
            {
               return -1;
            }
         }

         ArrayList<TreeMap<String, String>> groupedNodesStorage =
            parser.getVectorOfAttributeTreeMaps("storage");
         for (TreeMap<String, String> groupedNode : groupedNodesStorage)
         {
            String group = groupedNode.get("group");
            String nodeID = groupedNode.get("value");
            int nodeNumID = parseInt(groupedNode.get("nodeNumID"));
            Node node = new Node(nodeNumID, nodeID, group, NodeTypesEnum.STORAGE);
            boolean added = newStorageNodes.add(node);
            if(!added)
            {
               return -1;
            }
         }

         ArrayList<TreeMap<String, String>> groupedNodesClient =
            parser.getVectorOfAttributeTreeMaps("client");
         for (TreeMap<String, String> groupedNode : groupedNodesClient)
         {
            String group = groupedNode.get("group");
            String nodeID = groupedNode.get("value");
            int nodeNumID = parseInt(groupedNode.get("nodeNumID"));
            Node node = new Node(nodeNumID, nodeID, group, NodeTypesEnum.CLIENT);
            boolean added = newClientNodes.add(node);
            if(!added)
            {
               return -1;
            }
         }

         ArrayList<TreeMap<String, String>> groupedNodesAdmon =
            parser.getVectorOfAttributeTreeMaps("admon");
         for (TreeMap<String, String> groupedNode : groupedNodesAdmon)
         {
            String group = groupedNode.get("group");
            String nodeID = groupedNode.get("value");
            int nodeNumID = parseInt(groupedNode.get("nodeNumID"));
            Node node = new Node(nodeNumID, nodeID, group, NodeTypesEnum.ADMON);
            boolean added = newAdmonNodes.add(node);
            if(!added)
            {
               return -1;
            }
         }

         int numSyncedNodesMgmtd = syncNodes(MANAGMENT, newMgmtdNodes);
         int numSyncedNodesMeta = syncNodes(METADATA, newMetaNodes);
         int numSyncedNodesStorage = syncNodes(STORAGE, newStorageNodes);
         int numSyncedNodesClient = syncNodes(CLIENT, newClientNodes);
         int numSyncedNodesAdmon = syncNodes(ADMON, newAdmonNodes);

         if((numSyncedNodesMgmtd < 0) || (numSyncedNodesMeta < 0) || (numSyncedNodesStorage < 0) ||
            (numSyncedNodesClient < 0) || (numSyncedNodesAdmon < 0))
         {
            return -1;
         }
         else
         {
            return (numSyncedNodesMgmtd + numSyncedNodesMeta + numSyncedNodesStorage +
               numSyncedNodesClient + numSyncedNodesAdmon);
         }
      }
   }
}
