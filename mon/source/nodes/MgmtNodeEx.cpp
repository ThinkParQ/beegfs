#include "MgmtNodeEx.h"

MgmtNodeEx::MgmtNodeEx(std::string nodeID, NumNodeID nodeNumID, unsigned short portUDP,
      unsigned short portTCP, NicAddressList& nicList) :
      Node(NODETYPE_Mgmt, nodeID, nodeNumID, portUDP, portTCP, nicList)
{}
