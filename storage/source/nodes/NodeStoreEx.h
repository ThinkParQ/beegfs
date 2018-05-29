#ifndef NODESTOREEX_H_
#define NODESTOREEX_H_

/**
 * New code should not use type NodeStore/NodeStoreEx and this header file!
 *
 * This file and typedef exists only to help ease migration from the previous single NodeStoreEx
 * class to the splitted NodeStoreClients and NodeStoreServers class.
 */

#include <common/nodes/NodeStore.h>
#include "NodeStoreServersEx.h"


typedef class NodeStoreServersEx NodeStoreEx;




#endif /* NODESTOREEX_H_ */
