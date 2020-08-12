#ifndef COMMON_NODES_NODETYPE_H
#define COMMON_NODES_NODETYPE_H

#include <iostream>
#include <sstream>

#include <boost/io/ios_state.hpp>

enum NodeType
{
   NODETYPE_Invalid = 0,
   NODETYPE_Meta = 1,
   NODETYPE_Storage = 2,
   NODETYPE_Client = 3,
   NODETYPE_Mgmt = 4,
   NODETYPE_Helperd = 5,
};

inline std::ostream& operator<<(std::ostream& os, NodeType nodeType)
{
   const boost::io::ios_all_saver ifs(os);

   os.flags(std::ios_base::dec);
   os.width(0);

   switch (nodeType)
   {
      case NODETYPE_Invalid:     return os << "<undefined/invalid>";
      case NODETYPE_Meta:        return os << "beegfs-meta";
      case NODETYPE_Storage:     return os << "beegfs-storage";
      case NODETYPE_Client:      return os << "beegfs-client";
      case NODETYPE_Mgmt:        return os << "beegfs-mgmtd";
      case NODETYPE_Helperd:     return os << "beegfs-helperd";
      default:
         return os << "<unknown(" << int(nodeType) << ")>";
   }
}

#endif
