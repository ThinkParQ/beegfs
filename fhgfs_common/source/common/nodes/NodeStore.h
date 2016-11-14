#ifndef NODESTORE_H_
#define NODESTORE_H_

/**
 * New code should not use type NodeStore and this header file!
 *
 * This file and typedef exists only to help ease migration from the previous single NodeStore class
 * to the splitted NodeStoreClients and NodeStoreServers class.
 */


#include "NodeStoreServers.h"


typedef class NodeStoreServers NodeStore;




#endif /* NODESTORE_H_ */
