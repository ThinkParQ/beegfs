#ifndef COMMON_NODES_NODETYPE_H
#define COMMON_NODES_NODETYPE_H

enum NodeType
{
   NODETYPE_Invalid = 0,
   NODETYPE_Meta = 1,
   NODETYPE_Storage = 2,
   NODETYPE_Client = 3,
   NODETYPE_Mgmt = 4,
   NODETYPE_Helperd = 5,
   NODETYPE_Admon = 6,
   NODETYPE_CacheDaemon = 7,
   NODETYPE_CacheLib = 8
};

#endif
